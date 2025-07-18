// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphVideoOutputNode.h"

#include "Graph/Nodes/MovieGraphGlobalOutputSettingNode.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "ImageWriteTask.h"
#include "Misc/Paths.h"
#include "MovieRenderPipelineCoreModule.h"
#include "MoviePipelineUtils.h"
#include "Graph/MovieGraphBlueprintLibrary.h"
#include "Graph/MovieGraphPipeline.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieGraphVideoOutputNode)

UMovieGraphVideoOutputNode::UMovieGraphVideoOutputNode()
	: bHasError(false)
{
	FileNameFormat = TEXT("{sequence_name}.{layer_name}");
}

void UMovieGraphVideoOutputNode::OnAllShotFramesSubmittedImpl(UMovieGraphPipeline* InPipeline, const UMoviePipelineExecutorShot* InShot, TObjectPtr<UMovieGraphEvaluatedConfig>& InShotEvaluatedGraph, const bool bFlushToDisk)
{
	constexpr bool bIncludeCDOs = true;
	constexpr bool bExactMatch = true;
	const UMovieGraphGlobalOutputSettingNode* OutputSettingsNode =
		InShotEvaluatedGraph->GetSettingForBranch<UMovieGraphGlobalOutputSettingNode>(GlobalsPinName, bIncludeCDOs, bExactMatch);
	
	if (bFlushToDisk)
	{
		// If they don't have {shot_name} or {camera_name} in their output path, this probably doesn't do what they expect
		// as it will finalize and then overwrite itself.
		const FString FullPath = OutputSettingsNode->OutputDirectory.Path / FileNameFormat;
		if (!(FullPath.Contains(TEXT("{shot_name}")) || FullPath.Contains(TEXT("{camera_name}"))))
		{
			UE_LOG(LogMovieRenderPipelineIO, Warning, TEXT("Asked Movie Render Graph to flush file writes to disk after each shot (via Global Output Settings), but filename format doesn't seem to separate video files per shot. This will cause the file to overwrite itself, is this intended?"));
		}

		UE_LOG(LogMovieRenderPipelineIO, Log, TEXT("MoviePipelineVideoOutputBase flushing %d tasks to disk..."), AllWriters.Num());
		const double FlushBeginTime = FPlatformTime::Seconds();
		
		// Finalize will clear the AllWriters array, so any subsequent requests to write to that shot will try to generate a new file.
		// Note: The evaluated graph provided here is technically incorrect (it's the shot graph instead of the job graph), but these methods
		// don't use the evaluated graph anyway.
		OnAllFramesSubmitted(InPipeline, InShotEvaluatedGraph);
		OnAllFramesFinalized(InPipeline, InShotEvaluatedGraph);

		const float ElapsedS = static_cast<float>(FPlatformTime::Seconds() - FlushBeginTime);
		UE_LOG(LogMovieRenderPipelineIO, Log, TEXT("Finished flushing tasks to disk after %2.2fs!"), ElapsedS);
	}
}

void UMovieGraphVideoOutputNode::OnReceiveImageDataImpl(UMovieGraphPipeline* InPipeline, UE::MovieGraph::FMovieGraphOutputMergerFrame* InRawFrameData, const TSet<FMovieGraphRenderDataIdentifier>& InMask)
{
	// Don't continue processing if there was an error
	if (bHasError)
	{
		return;
	}

	UMovieGraphEvaluatedConfig* EvaluatedConfig = InRawFrameData->EvaluatedConfig.Get();

	// Gather the composited passes (eg, Burn Ins and Widget Renderer) 
	TArray<FMovieGraphPassData> CompositedPasses = GetCompositedPasses(InRawFrameData);

	for (FMovieGraphPassData& RenderPassData : InRawFrameData->ImageOutputData)
	{
		// Don't write out a composited pass in this loop, as it will be composited by the encoder and not written separately. 
		if (CompositedPasses.ContainsByPredicate([&RenderPassData](const FMovieGraphPassData& CompositedPass)
			{
				return RenderPassData.Key == CompositedPass.Key;
			}))
		{
			continue;
		}

		// Skip this layer if it's not part of the mask provided (ie, this node isn't part of the branch the layer is on).
		if (!InMask.Contains(RenderPassData.Key))
		{
			continue;
		}

		// GetOrCreateOutputWriter() can set bHasError, so even if a writer was created, don't continue if there was an error initializing it
		const FMovieGraphCodecWriterWithPromise* OutputWriter = GetOrCreateOutputWriter(InPipeline, InRawFrameData, RenderPassData, CompositedPasses);
		if (!OutputWriter || bHasError)
		{
			continue;
		}

		FImagePixelData* RawRenderPassData = RenderPassData.Value.Get();
		const UE::MovieGraph::FMovieGraphSampleState* Payload = RawRenderPassData->GetPayload<UE::MovieGraph::FMovieGraphSampleState>();

		// We can have multiple shots in one file, so we find/add the shot in case this is a new shot, and the counter will start over at zero.
		MovieRenderGraph::IVideoCodecWriter::FLightweightSourceData& LightweightData = OutputWriter->CodecWriter->LightweightSourceData.FindOrAdd(Payload->TraversalContext.ShotIndex);
		LightweightData.SubmittedFrameCount++;

		// Pixel data pointer for the cropped result to ensure it stays in memory while the image write is processed
		TUniquePtr<FImagePixelData> CroppedPixelData;
		const bool bIsCropRectValid = !Payload->CropRectangle.IsEmpty();
		const bool bCanCropResolution = RenderPassData.Value->GetSize() == Payload->OverscannedResolution;
		if (ShouldCropOverscanImpl() && bIsCropRectValid && bCanCropResolution)
		{
			switch (RenderPassData.Value->GetType())
			{
			case EImagePixelType::Color:
				{
					TAsyncCropImage<FColor> CropImagePreprocessor(Payload->CropRectangle);
					CropImagePreprocessor(RawRenderPassData);
					CroppedPixelData = MoveTemp(CropImagePreprocessor.OutCroppedImage);
				}
				break;

			case EImagePixelType::Float16:
				{
					TAsyncCropImage<FFloat16Color> CropImagePreprocessor(Payload->CropRectangle);
					CropImagePreprocessor(RawRenderPassData);
					CroppedPixelData = MoveTemp(CropImagePreprocessor.OutCroppedImage);
				}
				break;

			case EImagePixelType::Float32:
				{
					TAsyncCropImage<FLinearColor> CropImagePreprocessor(Payload->CropRectangle);
					CropImagePreprocessor(RawRenderPassData);
					CroppedPixelData = MoveTemp(CropImagePreprocessor.OutCroppedImage);
				}
				break;
			}
			
			RawRenderPassData = CroppedPixelData.Get();
		}
		
		// Write this frame's beauty pass, and pass along other passes so the encoder can composite them
		if (RenderPassData.Key.SubResourceName == TEXT("beauty"))
		{
			TArray<FMovieGraphPassData> CompositesForThisCamera;
			for (FMovieGraphPassData& CompositePass : CompositedPasses)
			{
				// This pass may not allow other passes to be composited on it 
				if (!Payload->bAllowsCompositing)
				{
					continue;
				}
				
				// Match them up by camera name so multiple passes intended for different camera names work.
				if (RenderPassData.Key.CameraName == CompositePass.Key.CameraName)
				{
					// Create a new composite pass, otherwise when the main
					// loop tries to check if we should skip writing out this image pass in the future, it fails
					// because we've MoveTemp'd CompositePass and thus the name checks no longer pass.
					FMovieGraphPassData NewPassInfo;
					NewPassInfo.Key = CompositePass.Key;

					// Copy the pixel data if more than one node is using this composited pass
					TUniquePtr<FImagePixelData> PixelData;
					if (GetNumFileOutputNodes(*InRawFrameData->EvaluatedConfig, CompositePass.Key.RootBranchName) > 1)
					{
						NewPassInfo.Value = CompositePass.Value->CopyImageData();
					}
					else
					{
						NewPassInfo.Value = CompositePass.Value->MoveImageDataToNew();
					}
					
					CompositesForThisCamera.Add(MoveTemp(NewPassInfo));
				}
			}

			// Notify the encoder to write this frame
			WriteFrame_EncodeThread(OutputWriter->CodecWriter.Get(), RawRenderPassData, MoveTemp(CompositesForThisCamera), EvaluatedConfig, RenderPassData.Key.RootBranchName.ToString());
		}
		else
		{
			TArray<FMovieGraphPassData> Dummy;
			WriteFrame_EncodeThread(OutputWriter->CodecWriter.Get(), RawRenderPassData, MoveTemp(Dummy), EvaluatedConfig, RenderPassData.Key.RootBranchName.ToString());
		}
	}
}

bool UMovieGraphVideoOutputNode::IsFinishedWritingToDiskImpl() const
{
	// Most encoders should be writing to disk every frame, so by the time we get to this point, the video should be on disk. OnFramesFinalized()
	// will still be called to allow encoders to finish up.
	return true;
}

void UMovieGraphVideoOutputNode::OnAllFramesSubmittedImpl(UMovieGraphPipeline* InPipeline, TObjectPtr<UMovieGraphEvaluatedConfig>& InPrimaryJobEvaluatedGraph)
{
	for (const FMovieGraphCodecWriterWithPromise& Writer : AllWriters)
	{
		if (Writer.NodeType == GetClass())
		{
			MovieRenderGraph::IVideoCodecWriter* RawWriter = Writer.CodecWriter.Get();
			BeginFinalize_EncodeThread(RawWriter);
		}
	}
}

void UMovieGraphVideoOutputNode::OnAllFramesFinalizedImpl(UMovieGraphPipeline* InPipeline, TObjectPtr<UMovieGraphEvaluatedConfig>& InPrimaryJobEvaluatedGraph)
{
	for (FMovieGraphCodecWriterWithPromise& Writer : AllWriters)
	{
		if (Writer.NodeType != GetClass())
		{
			continue;
		}
		
		MovieRenderGraph::IVideoCodecWriter* RawWriter = Writer.CodecWriter.Get();

		Finalize_EncodeThread(RawWriter);

		// Note: If there was an error, the promise should already be set to false, so we only set it to true here if there's no error. This isn't
		// ideal and it would be nice to have another way of tracking this, but double-setting the promise will trigger a check().
		if (!bHasError)
		{
			Writer.Promise.SetValue(!bHasError);
		}
	}

	// AllWriters is static and shared across *all* video output node types. Only delete writers of the current node's type.
	AllWriters.RemoveAll([this](const FMovieGraphCodecWriterWithPromise& InWriter)
	{
		return InWriter.NodeType == GetClass();
	});
	
	bHasError = false;
}

UMovieGraphVideoOutputNode::FMovieGraphCodecWriterWithPromise::FMovieGraphCodecWriterWithPromise(TUniquePtr<MovieRenderGraph::IVideoCodecWriter>&& InWriter, TPromise<bool>&& InPromise, UClass* InNodeType)
	: CodecWriter(MoveTemp(InWriter))
	, Promise(MoveTemp(InPromise))
	, NodeType(InNodeType)
{
	
}

void UMovieGraphVideoOutputNode::GetOutputFilePaths(const UMovieGraphPipeline* InPipeline, const UE::MovieGraph::FMovieGraphOutputMergerFrame* InRawFrameData, FMovieGraphPassData& InRenderPassData, const TArray<FMovieGraphPassData>& InCompositedPasses, FString& OutFinalFilePath, FString& OutStableFilePath)
{
	const TObjectPtr<UMoviePipelineExecutorShot> Shot = InRawFrameData->TraversalContext.Shot;
	UMovieGraphEvaluatedConfig* EvaluatedConfig = InRawFrameData->EvaluatedConfig.Get();

	constexpr bool bIncludeCDOs = true;
	constexpr bool bExactMatch = true;
	UMovieGraphGlobalOutputSettingNode* OutputSettingsNode =
		EvaluatedConfig->GetSettingForBranch<UMovieGraphGlobalOutputSettingNode>(GlobalsPinName, bIncludeCDOs, bExactMatch);

	UMovieGraphVideoOutputNode* EvaluatedNode = Cast<UMovieGraphVideoOutputNode>(
		EvaluatedConfig->GetSettingForBranch(GetClass(), InRenderPassData.Key.RootBranchName, bIncludeCDOs, bExactMatch));

	const TMap<FString, FString> FileNameFormatOverrides =  {
		{TEXT("camera_name"), InRenderPassData.Key.CameraName},
		{TEXT("render_pass"), InRenderPassData.Key.RendererName},
		{TEXT("ext"), GetFilenameExtension()},
	};
	FMovieGraphFilenameResolveParams ResolveParams =
		FMovieGraphFilenameResolveParams::MakeResolveParams(InRenderPassData.Key, InPipeline, EvaluatedConfig, InRawFrameData->TraversalContext, FileNameFormatOverrides);
	ResolveParams.FileNameOverride = EvaluatedNode->FileNameFormat;
	ResolveParams.Version = Shot ? Shot->ShotInfo.VersionNumber : UMovieGraphBlueprintLibrary::ResolveVersionNumber(ResolveParams);
	
	FMovieGraphResolveArgs FinalFormatArgs;
	FString FileNameFormatString = EvaluatedNode->FileNameFormat;
	
	// Insert tokens like {layer_name} as appropriate to make sure outputs don't clash with each other.
	DisambiguateFilename(FileNameFormatString, InRawFrameData, EvaluatedNode->GetFName(), InRenderPassData);

	// Strip any frame number tags so we don't get one video file per frame.
	UE::MoviePipeline::RemoveFrameNumberFormatStrings(FileNameFormatString, true);

	const FString OutputDirectory = OutputSettingsNode->OutputDirectory.Path;
	const FString FullFilepathFormatString = OutputDirectory / FileNameFormatString;

	// This is a bit crummy but we don't have a good way to implement bOverwriteFiles = False for video files. When you don't have the
	// override file flag set, we need to increment the number by 1, and write to that. This works fine for image sequences as image
	// sequences are also differentiated by their {frame_number}, but for video files (which strip it all out), instead we end up
	// with each incoming image sample trying to generate the same filename, finding a file that already exists (from the previous
	// frame) and incrementing again, giving us 1 video file per frame. To work around this, we're going to force bOverwriteExisting
	// off so we can generate a "stable" filename for the incoming image sample, then compare it against existing writers, and
	// if we don't find a writer, then generate a new one (respecting Overwrite Existing) to add the sample to.
	{
		const bool bPreviousOverwriteExisting = OutputSettingsNode->bOverwriteExistingOutput;
		OutputSettingsNode->bOverwriteExistingOutput = true;

		OutStableFilePath = UMovieGraphBlueprintLibrary::ResolveFilenameFormatArguments(FullFilepathFormatString, ResolveParams, FinalFormatArgs);
		
		// Restore user setting
		OutputSettingsNode->bOverwriteExistingOutput = bPreviousOverwriteExisting;

		if (FPaths::IsRelative(OutStableFilePath))
		{
			OutStableFilePath = FPaths::ConvertRelativePathToFull(OutStableFilePath);
		}
	}

	// Then we add the OutputDirectory, and resolve the filename format arguments again so the arguments in the directory get resolved.
	OutFinalFilePath = UMovieGraphBlueprintLibrary::ResolveFilenameFormatArguments(FullFilepathFormatString, ResolveParams, FinalFormatArgs);

	if (FPaths::IsRelative(OutFinalFilePath))
	{
		OutFinalFilePath = FPaths::ConvertRelativePathToFull(OutFinalFilePath);
	}

	// Ensure the directory is created
	{
		const FString FolderPath = FPaths::GetPath(OutFinalFilePath);
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

		PlatformFile.CreateDirectoryTree(*FolderPath);
	}
}

UMovieGraphVideoOutputNode::FMovieGraphCodecWriterWithPromise* UMovieGraphVideoOutputNode::GetOrCreateOutputWriter(UMovieGraphPipeline* InPipeline, const UE::MovieGraph::FMovieGraphOutputMergerFrame* InRawFrameData, FMovieGraphPassData& InRenderPassData, const TArray<FMovieGraphPassData>& InCompositedPasses)
{
	FMovieGraphCodecWriterWithPromise* OutputWriter = nullptr;

	const UE::MovieGraph::FMovieGraphSampleState* Payload = InRenderPassData.Value->GetPayload<UE::MovieGraph::FMovieGraphSampleState>();
	UMovieGraphEvaluatedConfig* EvaluatedConfig = InRawFrameData->EvaluatedConfig.Get();
	
	FString FinalFilePath;
	FString StableFilePath;
	GetOutputFilePaths(InPipeline, InRawFrameData, InRenderPassData, InCompositedPasses, FinalFilePath, StableFilePath);
	
	for (int32 Index = 0; Index < AllWriters.Num(); Index++)
	{
		if (AllWriters[Index].CodecWriter->StableFileName == StableFilePath)
		{
			OutputWriter = &AllWriters[Index];
			break;
		}
	}
	
	if (!OutputWriter)
	{
		FIntPoint OutputResolution = InRenderPassData.Value->GetSize();
		
		const bool bIsCropRectValid = !Payload->CropRectangle.IsEmpty();
		const bool bCanCropResolution = InRenderPassData.Value->GetSize() == Payload->OverscannedResolution;
		if (ShouldCropOverscanImpl() && bIsCropRectValid && bCanCropResolution)
		{
			OutputResolution = Payload->CropRectangle.Size();
		}
		
		FMovieGraphVideoNodeInitializationContext InitializationContext;
		InitializationContext.Pipeline = InPipeline;
		InitializationContext.EvaluatedConfig = EvaluatedConfig;
		InitializationContext.TraversalContext = &InRawFrameData->TraversalContext;
		InitializationContext.PassData = &InRenderPassData;
		InitializationContext.Resolution = OutputResolution;
		InitializationContext.FileName = FinalFilePath;
		InitializationContext.bAllowOCIO = Payload->bAllowOCIO;

		// Create a new writer for this file name (and output settings)
		if (TUniquePtr<MovieRenderGraph::IVideoCodecWriter> NewWriter = Initialize_GameThread(InitializationContext))
		{
			// Store the stable filename this was generated with so we can match them up later.
			NewWriter->StableFileName = StableFilePath;
			
			UE::MovieGraph::FMovieGraphOutputFutureData OutputData;
			OutputData.Shot = InPipeline->GetActiveShotList()[Payload->TraversalContext.ShotIndex];
			OutputData.DataIdentifier = InRenderPassData.Key;
			OutputData.FilePath = FinalFilePath;
			OutputData.OriginNodeClass = GetClass();
			OutputData.RenderLayerIndex = Payload->RenderLayerIndex;

			TPromise<bool> Completed;
			InPipeline->AddOutputFuture(Completed.GetFuture(), OutputData);

			AllWriters.Add(FMovieGraphCodecWriterWithPromise(MoveTemp(NewWriter), MoveTemp(Completed), GetClass()));
			OutputWriter = &AllWriters.Last();

			// If it fails to initialize, immediately mark the promise as failed so the render queue stops.
			bHasError = !Initialize_EncodeThread(OutputWriter->CodecWriter.Get());
			if (bHasError)
			{
				OutputWriter->Promise.SetValue(false);
				UE_LOG(LogMovieRenderPipelineIO, Error, TEXT("Failed to initialize encoder for FileName: %s"), *FinalFilePath);
			}
		}
	}

	if (!OutputWriter)
	{
		UE_LOG(LogMovieRenderPipelineIO, Error, TEXT("Failed to generate writer for FileName: %s"), *FinalFilePath);
	}

	return OutputWriter;
}
