// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphDeferredPassNode.h"

#include "Graph/Renderers/MovieGraphDeferredPass.h"
#include "MoviePipelineTelemetry.h"

TUniquePtr<UE::MovieGraph::Rendering::FMovieGraphImagePassBase> UMovieGraphDeferredRenderPassNode::CreateInstance() const
{
	return MakeUnique<UE::MovieGraph::Rendering::FMovieGraphDeferredPass>();
}

UMovieGraphDeferredRenderPassNode::UMovieGraphDeferredRenderPassNode()
	: SpatialSampleCount(1)
	, AntiAliasingMethod(EAntiAliasingMethod::AAM_TSR)
	, bWriteAllSamples(false)
	, bDisableToneCurve(false)
	, bAllowOCIO(true)
	, ViewModeIndex(VMI_Lit)
	, bEnableHighResolutionTiling(false)
	, TileCount(1)
	, OverlapPercentage(0.f)
	, bAllocateHistoryPerTile(false)
	, bPageToSystemMemory(false)
{

	// To help user knowledge we pre-seed the additional post processing materials with an array of potentially common passes.
	TArray<FString> DefaultPostProcessMaterials;
	DefaultPostProcessMaterials.Add(DefaultDepthAsset);
	DefaultPostProcessMaterials.Add(DefaultMotionVectorsAsset);

	for (FString& MaterialPath : DefaultPostProcessMaterials)
	{
		FMoviePipelinePostProcessPass& NewPass = AdditionalPostProcessMaterials.AddDefaulted_GetRef();
		NewPass.Material = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(MaterialPath));
		NewPass.bEnabled = false;
		NewPass.bHighPrecisionOutput = MaterialPath.Equals(DefaultDepthAsset);
	}
}

void UMovieGraphDeferredRenderPassNode::GetFormatResolveArgs(FMovieGraphResolveArgs& OutMergedFormatArgs, const FMovieGraphRenderDataIdentifier& InRenderDataIdentifier) const
{
	// ToDo: This should be split to write things per renderer as they can be different for each layer/camera/render pass.
	// We intentionally skip some settings for not being very meaningful to output, ie: bAllowOCIO, bPageToSystemMemory, bWriteAllSamples
	OutMergedFormatArgs.FilenameArguments.Add(TEXT("ss_count"), FString::FromInt(SpatialSampleCount));
	OutMergedFormatArgs.FileMetadata.Add(TEXT("unreal/sampling/spatialSampleCount"), FString::FromInt(SpatialSampleCount));

	OutMergedFormatArgs.FilenameArguments.Add(TEXT("disable_tonecurve"), FString::FromInt(bDisableToneCurve));
	OutMergedFormatArgs.FileMetadata.Add(TEXT("unreal/disableTonecurve"), FString::FromInt(bDisableToneCurve));

	OutMergedFormatArgs.FilenameArguments.Add(TEXT("overlap_percentage"), FString::SanitizeFloat(OverlapPercentage));
	OutMergedFormatArgs.FileMetadata.Add(TEXT("unreal/highres/overlapPercentage"), FString::SanitizeFloat(OverlapPercentage));

	OutMergedFormatArgs.FilenameArguments.Add(TEXT("history_per_tile"), FString::FromInt(bAllocateHistoryPerTile));
	OutMergedFormatArgs.FileMetadata.Add(TEXT("unreal/highres/historyPerTile"), FString::FromInt(bAllocateHistoryPerTile));
}

void UMovieGraphDeferredRenderPassNode::UpdateTelemetry(FMoviePipelineShotRenderTelemetry* InTelemetry) const
{
	InTelemetry->bUsesDeferred = true;
	InTelemetry->bUsesPPMs |= Algo::AnyOf(AdditionalPostProcessMaterials, [](const FMoviePipelinePostProcessPass& Pass) { return Pass.bEnabled; });
	InTelemetry->SpatialSampleCount = FMath::Max(InTelemetry->SpatialSampleCount, SpatialSampleCount);
	InTelemetry->HighResTileCount = FMath::Max(InTelemetry->HighResTileCount, TileCount);
	InTelemetry->HighResOverlap = FMath::Max(InTelemetry->HighResOverlap, OverlapPercentage);
	InTelemetry->bUsesPageToSystemMemory |= bPageToSystemMemory;
}

#if WITH_EDITOR
FText UMovieGraphDeferredRenderPassNode::GetNodeTitle(const bool bGetDescriptive) const
{
	return NSLOCTEXT("MovieGraphNodes", "DeferredRenderPassGraphNode_Description", "Deferred Renderer");
}

FSlateIcon UMovieGraphDeferredRenderPassNode::GetIconAndTint(FLinearColor& OutColor) const
{
	static const FSlateIcon DeferredRendererIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelViewport.HighResScreenshot");
	
	OutColor = FLinearColor::White;
	return DeferredRendererIcon;
}
#endif

FString UMovieGraphDeferredRenderPassNode::GetRendererNameImpl() const
{
	static const FString RendererNameImpl(TEXT("Deferred"));
	return RendererNameImpl;
}

EViewModeIndex UMovieGraphDeferredRenderPassNode::GetViewModeIndex() const
{
	return ViewModeIndex;
}

bool UMovieGraphDeferredRenderPassNode::GetWriteAllSamples() const
{
	return bWriteAllSamples;
}

TArray<FMoviePipelinePostProcessPass> UMovieGraphDeferredRenderPassNode::GetAdditionalPostProcessMaterials() const
{
	return AdditionalPostProcessMaterials;
}

int32 UMovieGraphDeferredRenderPassNode::GetNumSpatialSamples() const
{
	return SpatialSampleCount;
}

bool UMovieGraphDeferredRenderPassNode::GetDisableToneCurve() const
{
	return bDisableToneCurve;
}

bool UMovieGraphDeferredRenderPassNode::GetAllowOCIO() const
{
	return bAllowOCIO;
}

EAntiAliasingMethod UMovieGraphDeferredRenderPassNode::GetAntiAliasingMethod() const
{
	return AntiAliasingMethod;
}