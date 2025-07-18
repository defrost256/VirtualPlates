// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Graph/Renderers/MovieGraphImagePassBase.h"

struct FSceneViewStateSystemMemoryMirror;

namespace UE::MovieGraph::Rendering
{
	struct FMovieGraphPostRendererSubmissionParams
	{
		UE::MovieGraph::FMovieGraphSampleState SampleState;
		UE::MovieGraph::DefaultRenderer::FRenderTargetInitParams RenderTargetInitParams;
		UE::MovieGraph::DefaultRenderer::FCameraInfo CameraInfo;
	};

	struct MOVIERENDERPIPELINERENDERPASSES_API FMovieGraphDeferredPass : public FMovieGraphImagePassBase
	{
		// FMovieGraphImagePassBase Interface
		virtual void Setup(TWeakObjectPtr<UMovieGraphDefaultRenderer> InRenderer, TWeakObjectPtr<UMovieGraphImagePassBaseNode> InRenderPassNode, const FMovieGraphRenderPassLayerData& InLayer) override;
		virtual void Teardown() override;
		virtual void Render(const FMovieGraphTraversalContext& InFrameTraversalContext, const FMovieGraphTimeStepData& InTimeData) override;
		virtual void GatherOutputPasses(UMovieGraphEvaluatedConfig* InConfig, TArray<FMovieGraphRenderDataIdentifier>& OutExpectedPasses) const override;
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
		virtual FName GetBranchName() const override;
		virtual UMovieGraphImagePassBaseNode* GetParentNode(UMovieGraphEvaluatedConfig* InConfig) const override;
		bool ShouldDiscardOutput(const TSharedRef<FSceneViewFamilyContext>& InFamily, const UE::MovieGraph::DefaultRenderer::FCameraInfo& InCameraInfo) const override;

		// End FMovieGraphImagePassBase
			
	protected:
		bool HasRenderResourceParametersChanged(const FIntPoint& AccumulatorResolution, const FIntPoint& BackbufferResolution) const;
		virtual void PostRendererSubmission(const UE::MovieGraph::FMovieGraphSampleState& InSampleState, const UE::MovieGraph::DefaultRenderer::FRenderTargetInitParams& InRenderTargetInitParams, FCanvas& InCanvas, const UE::MovieGraph::DefaultRenderer::FCameraInfo& InCameraInfo) const override;
		virtual FSceneViewStateInterface* GetSceneViewState(UMovieGraphImagePassBaseNode* ParentNodeThisFrame, int32_t TileX, int32_t TileY);

	protected:
		FMovieGraphRenderPassLayerData LayerData;

		/** Unique identifier passed in GatherOutputPasses and with each render that identifies the data produced by this renderer. */
		FMovieGraphRenderDataIdentifier RenderDataIdentifier;

		UE_DEPRECATED(5.6, "SceneViewState is no longer used. Use SceneViewStates instead with reference at FIntPoint(0,0).")
		FSceneViewStateReference SceneViewState;

		// Scene View history used by the renderer. When using an auto-exposure pass it'll use (-1, -1), otherwise one-per tile (and one at 0,0 if not using tiling).
		TMap<FIntPoint, FSceneViewStateReference> SceneViewStates;

		// Used when using Page to System Memory
		TPimplPtr<FSceneViewStateSystemMemoryMirror> SystemMemoryMirror;

		// The number of frames to delay to send frames from SubmissionQueue to post-render submission.
		int32 FramesToDelayPostSubmission;
		
		// If using cooldown, the number of cool-down frames we still need to process.
		int32 RemainingCooldownReadbackFrames;

		// FIFO queue of rendered frames. It allows frames to be sent to post-render submission with a delay if needed (e.g., when temporal denoising is used with path tracers).  
		TQueue<FMovieGraphPostRendererSubmissionParams> SubmissionQueue;

		// Did we initialize a auto-exposure sceneview history during setup?
		bool bHasAutoExposurePass = false;

		// Track the last Accumulator resolution we used, so that we can detect when it is changed and log that information.
		FIntPoint PrevAccumulatorResolution;
		// Track the last backbuffer resolution we used, so that we can detect when it is changed and log that information.
		FIntPoint PrevBackbufferResolution;
	}; 
}