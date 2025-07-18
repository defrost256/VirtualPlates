// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphDeferredPanoramicPassNode.h"
#include "Graph/Renderers/MovieGraphDeferredPanoramicPass.h"
#include "MoviePipelineTelemetry.h"

UMovieGraphDeferredPanoramicNode::UMovieGraphDeferredPanoramicNode()
	: NumHorizontalSteps(8)
	, NumVerticalSteps(3)
	, bFollowCameraOrientation(true)
	, bAllocateHistoryPerPane(false)
	, bPageToSystemMemory(false)
	, SpatialSampleCount(1)
	, AntiAliasingMethod(EAntiAliasingMethod::AAM_TSR)
	, Filter(EMoviePipelinePanoramicFilterType::Bilinear)
	, bWriteAllSamples(false)
	, bDisableToneCurve(false)
	, bAllowOCIO(true)
	, ViewModeIndex(VMI_Lit)
{}

FEngineShowFlags UMovieGraphDeferredPanoramicNode::GetShowFlags() const
{
	FEngineShowFlags Flags = Super::GetShowFlags();
	Flags.SetVignette(false);
	Flags.SetSceneColorFringe(false);
	Flags.SetPhysicalMaterialMasks(false);
	Flags.SetDepthOfField(false);

	return Flags;
}

#if WITH_EDITOR
FText UMovieGraphDeferredPanoramicNode::GetNodeTitle(const bool bGetDescriptive) const
{
	return NSLOCTEXT("MovieRenderGraph", "DeferredPanoramicNodeTitle", "Panoramic Deferred Renderer");		
}

FSlateIcon UMovieGraphDeferredPanoramicNode::GetIconAndTint(FLinearColor& OutColor) const
{
	static const FSlateIcon DeferredRendererIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.SizeMap");
	OutColor = FLinearColor::White;
	
	return DeferredRendererIcon;
}
#endif	// WITH_EDITOR

void UMovieGraphDeferredPanoramicNode::UpdateTelemetry(FMoviePipelineShotRenderTelemetry* InTelemetry) const
{
	InTelemetry->bUsesPanoramic = true;
}

TUniquePtr<UE::MovieGraph::Rendering::FMovieGraphImagePassBase> UMovieGraphDeferredPanoramicNode::CreateInstance() const
{
	return MakeUnique<UE::MovieGraph::Rendering::FMovieGraphDeferredPanoramicPass>();
}

FString UMovieGraphDeferredPanoramicNode::GetRendererNameImpl() const
{
	return TEXT("DeferredPanoramic");
}

EViewModeIndex UMovieGraphDeferredPanoramicNode::GetViewModeIndex() const
{
	return ViewModeIndex;
}

bool UMovieGraphDeferredPanoramicNode::GetWriteAllSamples() const
{
	return bWriteAllSamples;
}

int32 UMovieGraphDeferredPanoramicNode::GetNumSpatialSamples() const
{
	return SpatialSampleCount;
}

bool UMovieGraphDeferredPanoramicNode::GetDisableToneCurve() const
{
	return bDisableToneCurve;
}

bool UMovieGraphDeferredPanoramicNode::GetAllowOCIO() const
{
	return bAllowOCIO;
}

EAntiAliasingMethod UMovieGraphDeferredPanoramicNode::GetAntiAliasingMethod() const
{
	return AntiAliasingMethod;
}