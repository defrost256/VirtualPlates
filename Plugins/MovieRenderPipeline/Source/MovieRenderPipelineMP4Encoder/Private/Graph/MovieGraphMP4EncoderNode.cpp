// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MovieGraphMP4EncoderNode.h"

#include "Graph/MovieGraphBlueprintLibrary.h"
#include "Graph/MovieGraphConfig.h"
#include "Graph/MovieGraphOCIOHelper.h"
#include "Graph/MovieGraphPipeline.h"
#include "Graph/Nodes/MovieGraphGlobalOutputSettingNode.h"
#include "ImageWriteTask.h"
#include "MoviePipelineImageQuantization.h"
#include "MoviePipelineTelemetry.h"
#include "MovieRenderPipelineCoreModule.h"
#include "SampleBuffer.h"
#include "Styling/AppStyle.h"


UMovieGraphMP4EncoderNode::UMovieGraphMP4EncoderNode()
{
}

#if WITH_EDITOR
FText UMovieGraphMP4EncoderNode::GetNodeTitle(const bool bGetDescriptive) const
{
	static const FText NodeName = NSLOCTEXT("MovieGraphNodes", "NodeName_MP4", "H.264 MP4");
	return NodeName;
}

FText UMovieGraphMP4EncoderNode::GetMenuCategory() const
{
	return NSLOCTEXT("MovieGraphNodes", "WMFNode_Category", "Output Type");
}

FText UMovieGraphMP4EncoderNode::GetKeywords() const
{
	static const FText Keywords = NSLOCTEXT("MovieGraphNodes", "MP4_Keywords", "mp4 h264 h265 windows mpeg mov movie video");
	return Keywords;
}

FLinearColor UMovieGraphMP4EncoderNode::GetNodeTitleColor() const
{
	static const FLinearColor NodeColor = FLinearColor(0.047f, 0.654f, 0.537f);
	return NodeColor;
}

FSlateIcon UMovieGraphMP4EncoderNode::GetIconAndTint(FLinearColor& OutColor) const
{
	static const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Cinematics");

	OutColor = FLinearColor::White;
	return Icon;
}
#endif // WITH_EDITOR

TUniquePtr<MovieRenderGraph::IVideoCodecWriter> UMovieGraphMP4EncoderNode::Initialize_GameThread(const FMovieGraphVideoNodeInitializationContext& InInitializationContext)
{
	bool bIncludeCDOs = true;
	constexpr bool bExactMatch = true;
	UMovieGraphGlobalOutputSettingNode* OutputSetting =
		InInitializationContext.EvaluatedConfig->GetSettingForBranch<UMovieGraphGlobalOutputSettingNode>(GlobalsPinName, bIncludeCDOs, bExactMatch);

	bIncludeCDOs = false;
	const UMovieGraphMP4EncoderNode* EvaluatedNode = Cast<UMovieGraphMP4EncoderNode>(
		InInitializationContext.EvaluatedConfig->GetSettingForBranch(GetClass(), FName(InInitializationContext.PassData->Key.RootBranchName), bIncludeCDOs, bExactMatch));
	checkf(EvaluatedNode, TEXT("MP4 Encoder node could not be found in the graph in branch [%s]."), *InInitializationContext.PassData->Key.RootBranchName.ToString());
	
	const FFrameRate SourceFrameRate = InInitializationContext.Pipeline->GetDataSourceInstance()->GetDisplayRate();
	const FFrameRate EffectiveFrameRate = UMovieGraphBlueprintLibrary::GetEffectiveFrameRate(OutputSetting, SourceFrameRate);
	
	FMoviePipelineMP4EncoderOptions Options;
	Options.OutputFilename = InInitializationContext.FileName;
	Options.Width = InInitializationContext.Resolution.X;
	Options.Height = InInitializationContext.Resolution.Y;
	Options.FrameRate = EffectiveFrameRate;
	Options.CommonMeanBitRate = FMath::RoundToInt(EvaluatedNode->AverageBitrateInMbps*1024*1024);
	Options.CommonQualityVsSpeed = 100;
	Options.CommonConstantRateFactor = EvaluatedNode->ConstantRateFactor;
	Options.EncodingRateControl = EvaluatedNode->EncodingRateControl;
	Options.bIncludeAudio = EvaluatedNode->bIncludeAudio;
	
	// Optional properties that can be overridden by scripting if needed; not exposed to the UI currently
	if (EvaluatedNode->bOverride_MaxBitrateInMbps)
	{
		Options.CommonMaxBitRate = FMath::RoundToInt(EvaluatedNode->MaxBitrateInMbps*1024*1024);
	}
	if (EvaluatedNode->bOverride_EncodingProfile)
	{
		Options.EncodingProfile = EvaluatedNode->EncodingProfile;
	}
	if (EvaluatedNode->bOverride_EncodingLevel)
	{
		Options.EncodingLevel = EvaluatedNode->EncodingLevel;
	}
	
	TUniquePtr<FMP4CodecWriter> NewWriter = MakeUnique<FMP4CodecWriter>();
	NewWriter->Writer = MakeUnique<FMoviePipelineMP4Encoder>(Options);

	// If OCIO is enabled, don't do additional color conversion.
	NewWriter->bSkipColorConversions = (EvaluatedNode->bOverride_OCIOConfiguration && EvaluatedNode->OCIOConfiguration.bIsEnabled && InInitializationContext.bAllowOCIO);
	CachedPipeline = InInitializationContext.Pipeline;
	
	return NewWriter;
}

bool UMovieGraphMP4EncoderNode::Initialize_EncodeThread(MovieRenderGraph::IVideoCodecWriter* InWriter)
{
	const FMP4CodecWriter* CodecWriter = static_cast<FMP4CodecWriter*>(InWriter);
	if(!CodecWriter->Writer->Initialize())
	{
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("Failed to initialize Movie Pipeline MP4 writer. An encoder that supports the render resolution and requested MP4 encode options was not found. Try again with a different resolution and/or encoder settings."));
		return false;
	}
	
	return true;
}

void UMovieGraphMP4EncoderNode::WriteFrame_EncodeThread(MovieRenderGraph::IVideoCodecWriter* InWriter, FImagePixelData* InPixelData, TArray<FMovieGraphPassData>&& InCompositePasses, TObjectPtr<UMovieGraphEvaluatedConfig> InEvaluatedConfig, const FString& InBranchName)
{
	FMP4CodecWriter* CodecWriter = static_cast<FMP4CodecWriter*>(InWriter);
	if (!CodecWriter->Writer)
	{
		return;
	}

	bool bIncludeCDOs = false;
	constexpr bool bExactMatch = true;
	const UMovieGraphMP4EncoderNode* EvaluatedNode = Cast<UMovieGraphMP4EncoderNode>(
		InEvaluatedConfig->GetSettingForBranch(GetClass(), FName(InBranchName), bIncludeCDOs, bExactMatch));
	checkf(EvaluatedNode, TEXT("MP4 Encoder node could not be found in the graph in branch [%s]."), *InBranchName);

	bIncludeCDOs = true;
	const UMovieGraphGlobalOutputSettingNode* OutputSettingNode =
		InEvaluatedConfig->GetSettingForBranch<UMovieGraphGlobalOutputSettingNode>(GlobalsPinName, bIncludeCDOs, bExactMatch);
	
	const UE::MovieGraph::FMovieGraphSampleState* GraphPayload = InPixelData->GetPayload<UE::MovieGraph::FMovieGraphSampleState>();
	
	const bool bConvertToSrgb = !CodecWriter->bSkipColorConversions;
	const TUniquePtr<FImagePixelData> QuantizedPixelData = UE::MoviePipeline::QuantizeImagePixelDataToBitDepth(InPixelData, 8, nullptr, bConvertToSrgb);

	TArray<FPixelPreProcessor> PixelPreProcessors;

#if WITH_OCIO
	FMovieGraphOCIOHelper::GenerateOcioPixelPreProcessor(GraphPayload, CachedPipeline.Get(), InEvaluatedConfig, EvaluatedNode->OCIOConfiguration, EvaluatedNode->OCIOContext, PixelPreProcessors);
#endif

	// Do a quick composite of renders/burn-ins.
	for (const FMovieGraphPassData& CompositePass : InCompositePasses)
	{
		// We don't need to copy the data here (even though it's being passed to a async system) because we already made a unique copy of the
		// burn in/widget data when we decided to composite it.
		switch (QuantizedPixelData->GetType())
		{
		case EImagePixelType::Color:
			PixelPreProcessors.Add(TAsyncCompositeImage<FColor>(CompositePass.Value->MoveImageDataToNew()));
			break;
		case EImagePixelType::Float16:
			PixelPreProcessors.Add(TAsyncCompositeImage<FFloat16Color>(CompositePass.Value->MoveImageDataToNew()));
			break;
		case EImagePixelType::Float32:
			PixelPreProcessors.Add(TAsyncCompositeImage<FLinearColor>(CompositePass.Value->MoveImageDataToNew()));
			break;
		}
	}

	// This is done on the current thread for simplicity but the composite itself is parallelized.
	FImagePixelData* PixelData = QuantizedPixelData.Get();
	for (const FPixelPreProcessor& PreProcessor : PixelPreProcessors)
	{
		// PreProcessors are assumed to be valid.
		PreProcessor(PixelData);
	}

	const void* Data = nullptr;
	int64 DataSize;
	QuantizedPixelData->GetRawData(Data, DataSize);

	// WriteFrame expects Rec 709 8-bit data.
	CodecWriter->Writer->WriteFrame((uint8*)Data);
}

void UMovieGraphMP4EncoderNode::BeginFinalize_EncodeThread(MovieRenderGraph::IVideoCodecWriter* InWriter)
{
	// If the writer was not initialized, don't try to finalize anything.
	FMP4CodecWriter* CodecWriter = static_cast<FMP4CodecWriter*>(InWriter);
	if (!CodecWriter->Writer)
	{
		return;
	}

	if (!CodecWriter->Writer->IsInitialized())
	{
		return;
	}
	
	// Nothing to do here if audio isn't being generated. The "invalid shot index" warning below is legitimate *if audio is being rendered*, but if
	// no audio is being rendered (eg, with -nosound) then we don't want the warning to show up.
	if (!FApp::CanEverRenderAudio())
	{
		return;
	}

	const MoviePipeline::FAudioState& AudioData = CachedPipeline->GetAudioRendererInstance()->GetAudioState();

	for (const TPair<int32, MovieRenderGraph::IVideoCodecWriter::FLightweightSourceData>& SourceData : CodecWriter->LightweightSourceData)
	{
		if (!AudioData.FinishedSegments.IsValidIndex(SourceData.Key))
		{
			UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Invalid shot index was requested for audio data, skipping audio writes."));
			continue;
		}

		// Look up the audio segment for this shot
		const MoviePipeline::FAudioState::FAudioSegment& AudioSegment = AudioData.FinishedSegments[SourceData.Key];

		// Audio data isn't very sample accurate at this point, so we may have generated slightly more (or less) audio than we expect for
		// the number of frames, so we're actually going to trim down the view of data we provide to match the number of frames rendered,
		// to avoid any excess audio after the end of the video.
		const int32 NumFrames = SourceData.Value.SubmittedFrameCount;

		// ToDo: This is possibly dropping fractions of a sample (ie: 1/48,000th) if the audio sample rate can't be evenly divisible by
		// the frame rate.
		const int32 SamplesPerFrame = AudioSegment.SampleRate * CodecWriter->Writer->GetOptions().FrameRate.AsInterval();
		int32 ExpectedSampleCount = NumFrames * SamplesPerFrame * AudioSegment.NumChannels;

		ExpectedSampleCount = FMath::Min(ExpectedSampleCount, AudioSegment.SegmentData.Num());
		Audio::TSampleBuffer<int16> SampleBuffer = Audio::TSampleBuffer<int16>(AudioSegment.SegmentData.GetData(), ExpectedSampleCount, AudioSegment.NumChannels, AudioSegment.SampleRate);

		const TArrayView<int16> SampleData = SampleBuffer.GetArrayView();
		CodecWriter->Writer->WriteAudioSample(SampleData);
	}
}

void UMovieGraphMP4EncoderNode::Finalize_EncodeThread(MovieRenderGraph::IVideoCodecWriter* InWriter)
{
	// Write to disk
	const FMP4CodecWriter* CodecWriter = static_cast<FMP4CodecWriter*>(InWriter);
	if(!CodecWriter->Writer)
	{
		return;
	}
	CodecWriter->Writer->Finalize();
}

const TCHAR* UMovieGraphMP4EncoderNode::GetFilenameExtension() const
{
	return TEXT("mp4");
}

bool UMovieGraphMP4EncoderNode::IsAudioSupported() const
{
	return true;
}

void UMovieGraphMP4EncoderNode::UpdateTelemetry(FMoviePipelineShotRenderTelemetry* InTelemetry) const
{
	InTelemetry->bUsesMP4 = true;
}
