// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/Renderers/MovieGraphImagePassBase.h"
#include "Graph/Nodes/MovieGraphDeferredPanoramicPassNode.h"

struct FSceneViewStateSystemMemoryMirror;

namespace UE::MovieGraph::Rendering
{
	struct MOVIERENDERPIPELINERENDERPASSES_API FMovieGraphDeferredPanoramicPass : public FMovieGraphImagePassBase
	{
		FMovieGraphDeferredPanoramicPass();

		// FMovieGraphImagePassBase Interface
		virtual void Setup(TWeakObjectPtr<UMovieGraphDefaultRenderer> InRenderer, TWeakObjectPtr<UMovieGraphImagePassBaseNode> InRenderPassNode, const FMovieGraphRenderPassLayerData& InLayer) override;
		virtual void Teardown() override;
		virtual UMovieGraphImagePassBaseNode* GetParentNode(UMovieGraphEvaluatedConfig* InConfig) const override;
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
		virtual TSharedRef<::MoviePipeline::IMoviePipelineAccumulationArgs> GetOrCreateAccumulator(TObjectPtr<UMovieGraphDefaultRenderer> InGraphRenderer, const UE::MovieGraph::FMovieGraphSampleState& InSampleState) const override;
		virtual FAccumulatorSampleFunc GetAccumulateSampleFunction() const override;
		virtual void GatherOutputPasses(UMovieGraphEvaluatedConfig* InConfig, TArray<FMovieGraphRenderDataIdentifier>& OutExpectedPasses) const override;
		virtual FName GetBranchName() const;
		// ~FMovieGraphImagePassBase Interface

		// FMovieGraphDeferredPass Interface
		virtual void Render(const FMovieGraphTraversalContext& InFrameTraversalContext, const FMovieGraphTimeStepData& InTimeData) override;
		// ~FMovieGraphDeferredPass Interface

	protected:
		virtual bool ShouldDiscardOutput(const TSharedRef<FSceneViewFamilyContext>& InFamily, const UE::MovieGraph::DefaultRenderer::FCameraInfo& InCameraInfo) const;
		virtual FSceneViewStateInterface* GetSceneViewState(UMovieGraphDeferredPanoramicNode* ParentNodeThisFrame, int32_t PaneX, int32_t PaneY);

	protected:
		// A view state for each Pane (if History Per Pane is enabled)
		TArray<FSceneViewStateReference> PaneViewStates;

		// When using an auto exposure render pass, holds view states for 6 cube faces
		TArray<FSceneViewStateReference> AutoExposureViewStates;

		// Used when using Page to System Memory
		TPimplPtr<FSceneViewStateSystemMemoryMirror> SystemMemoryMirror;

		bool bHasPrintedRenderingInfo = false;
		bool bHasPrintedWarnings = false;
		FMovieGraphRenderDataIdentifier RenderDataIdentifier;
		FMovieGraphRenderPassLayerData LayerData;
		TSharedPtr<UE::MovieGraph::IMovieGraphOutputMerger> PanoramicOutputBlender;
		
	};
}