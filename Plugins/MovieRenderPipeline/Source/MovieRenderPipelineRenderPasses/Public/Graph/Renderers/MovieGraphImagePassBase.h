// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreTypes.h"
#include "Graph/MovieGraphDefaultRenderer.h"
#include "SceneView.h"

// Forward Declares
class UMovieGraphImagePassBaseNode;

namespace UE::MovieGraph::Rendering
{
	struct MOVIERENDERPIPELINERENDERPASSES_API FMovieGraphRenderDataAccumulationArgs : ::MoviePipeline::IMoviePipelineAccumulationArgs
	{
	public:
		TWeakPtr<::MoviePipeline::IMoviePipelineOverlappedAccumulator, ESPMode::ThreadSafe> ImageAccumulator;
		DefaultRenderer::FSurfaceAccumulatorPool::FInstancePtr AccumulatorInstance;
		TWeakPtr<IMovieGraphOutputMerger, ESPMode::ThreadSafe> OutputMerger;

		UE_DEPRECATED(5.6, "No longer used, calling code now looks at sample state directly.")
		bool bIsFirstSample;
		UE_DEPRECATED(5.6, "No longer used, calling code now looks at sample state directly.")
		bool bIsLastSample;


		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		FMovieGraphRenderDataAccumulationArgs() = default;
		~FMovieGraphRenderDataAccumulationArgs() = default;
		FMovieGraphRenderDataAccumulationArgs(const FMovieGraphRenderDataAccumulationArgs&) = default;
		FMovieGraphRenderDataAccumulationArgs(FMovieGraphRenderDataAccumulationArgs&&) = default;
		FMovieGraphRenderDataAccumulationArgs& operator=(const FMovieGraphRenderDataAccumulationArgs&) = default;
		FMovieGraphRenderDataAccumulationArgs& operator=(FMovieGraphRenderDataAccumulationArgs&&) = default;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	};

	// Forward Declare
	void AccumulateSample_TaskThread(TUniquePtr<FImagePixelData>&& InPixelData, const TSharedRef<::UE::MovieGraph::FMovieGraphSampleState> InSampleState, const TSharedRef<::MoviePipeline::IMoviePipelineAccumulationArgs> InAccumulatorArgs);

	struct MOVIERENDERPIPELINERENDERPASSES_API FViewFamilyInitData
	{
		FViewFamilyInitData()
			: RenderTarget(nullptr)
			, World(nullptr)
			, SceneCaptureSource(ESceneCaptureSource::SCS_MAX)
			, bWorldIsPaused(false)
			, FrameIndex(-1)
			, AntiAliasingMethod(EAntiAliasingMethod::AAM_None)
			, ShowFlags(ESFIM_Game)
			, ViewModeIndex(VMI_Lit)
		{
		}

		class FRenderTarget* RenderTarget;
		class UWorld* World;
		FMovieGraphTimeStepData TimeData;
		ESceneCaptureSource SceneCaptureSource;
		bool bWorldIsPaused;
		int32 FrameIndex;
		EAntiAliasingMethod AntiAliasingMethod;
		FEngineShowFlags ShowFlags;
		EViewModeIndex ViewModeIndex;
		TEnumAsByte<ECameraProjectionMode::Type> ProjectionMode;
	};
	


	struct MOVIERENDERPIPELINERENDERPASSES_API FMovieGraphImagePassBase
	{
		FMovieGraphImagePassBase() = default;
		virtual ~FMovieGraphImagePassBase() = default;

		virtual void Setup(TWeakObjectPtr<UMovieGraphDefaultRenderer> InRenderer, TWeakObjectPtr<UMovieGraphImagePassBaseNode> InRenderPassNode, const FMovieGraphRenderPassLayerData& InLayer);
		virtual void Teardown() {}
		virtual void Render(const FMovieGraphTraversalContext& InFrameTraversalContext, const FMovieGraphTimeStepData& InTimeData) {}
		virtual void GatherOutputPasses(UMovieGraphEvaluatedConfig* InConfig, TArray<FMovieGraphRenderDataIdentifier>& OutExpectedPasses) const {}
		virtual void AddReferencedObjects(FReferenceCollector& Collector) {}
		virtual FName GetBranchName() const { return NAME_None; }
		virtual TWeakObjectPtr<UMovieGraphDefaultRenderer> GetRenderer() const { return WeakGraphRenderer; }

	protected:
		typedef TFunction<void(TUniquePtr<FImagePixelData>&&, const TSharedRef<FMovieGraphSampleState>, const TSharedRef<::MoviePipeline::IMoviePipelineAccumulationArgs>)> FAccumulatorSampleFunc;
		
		/** Utility function for calculating a Projection Matrix (Orthographic or Perspective), and modify it based on the overscan percentage, aspect ratio, etc. */
		virtual void CalculateProjectionMatrix(UE::MovieGraph::DefaultRenderer::FCameraInfo& InOutCameraInfo, FSceneViewProjectionData& InOutProjectionData, const FIntPoint InBackbufferResolution, const FIntPoint InAccumulatorResolution) const;
		
		/** Utility function for modifying the projection matrix to match the given tiling parameters. */
		virtual void ModifyProjectionMatrixForTiling(const UE::MovieGraph::DefaultRenderer::FMovieGraphTilingParams& InTilingParams, const bool bInOrthographic, FMatrix& InOutProjectionMatrix, float& OutDoFSensorScale) const;
		
		/** Utility function for calculating the PrinciplePointOffset with the given tiling parameters. Used to make some effects (like vignette) work with tiling. */
		virtual FVector4f CalculatePrinciplePointOffsetForTiling(const UE::MovieGraph::DefaultRenderer::FMovieGraphTilingParams& InTilingParams) const;
		
		/** Utility function for creating FSceneViewInitOptions based on the specified camera info. */
		virtual FSceneViewInitOptions CreateViewInitOptions(const UE::MovieGraph::DefaultRenderer::FCameraInfo& InCameraInfo, FSceneViewFamilyContext* InViewFamily, FSceneViewStateInterface* InViewStateInterface) const;

		UE_DEPRECATED(5.6, "Use the version which takes a FSceneViewStateInterface pointer instead.")
		virtual FSceneViewInitOptions CreateViewInitOptions(const UE::MovieGraph::DefaultRenderer::FCameraInfo& InCameraInfo, FSceneViewFamilyContext* InViewFamily, FSceneViewStateReference& InViewStateRef) const;
		
		/** Utility function for creating a FSceneView for the given InitOptions, Family, and Camera. */
		virtual FSceneView* CreateSceneView(const FSceneViewInitOptions& InInitOptions, TSharedRef<FSceneViewFamilyContext> InViewFamily, const UE::MovieGraph::DefaultRenderer::FCameraInfo& InCameraInfo) const;

		/** Gets the render target init params that will be used when creating the render target. */
		virtual DefaultRenderer::FRenderTargetInitParams GetRenderTargetInitParams(const FMovieGraphTimeStepData& InTimeData, const FIntPoint& InResolution);

		virtual void ApplyCameraManagerPostProcessBlends(FSceneView* InView, const FMinimalViewInfo& InViewInfo, bool bApplyCameraManagerPostProcessBlends) const;
		virtual TSharedRef<FSceneViewFamilyContext> CreateSceneViewFamily(const FViewFamilyInitData& InInitData) const;
		virtual void ApplyMovieGraphOverridesToSceneView(TSharedRef<FSceneViewFamilyContext> InOutFamily, const FViewFamilyInitData& InInitData, const UE::MovieGraph::DefaultRenderer::FCameraInfo& InCameraInfo) const;
		virtual void ApplyMovieGraphOverridesToViewFamily(TSharedRef<FSceneViewFamilyContext> InOutFamily, const FViewFamilyInitData& InInitData) const;
		virtual void PostRendererSubmission(const UE::MovieGraph::FMovieGraphSampleState& InSampleState, const UE::MovieGraph::DefaultRenderer::FRenderTargetInitParams& InRenderTargetInitParams, FCanvas& InCanvas, const UE::MovieGraph::DefaultRenderer::FCameraInfo& InCameraInfo) const;
		virtual TFunction<void(TUniquePtr<FImagePixelData>&&)> MakeForwardingEndpoint(const FMovieGraphSampleState& InSampleState, const FMovieGraphTimeStepData& InTimeData);
		virtual bool ShouldDiscardOutput(const TSharedRef<FSceneViewFamilyContext>& InFamily, const UE::MovieGraph::DefaultRenderer::FCameraInfo& InCameraInfo) const { return false; }
		virtual void ApplyMovieGraphOverridesToSampleState(FMovieGraphSampleState& SampleState) const {}
		/** For this image pass, look up the associated node by type for the given config. */
		virtual UMovieGraphImagePassBaseNode* GetParentNode(UMovieGraphEvaluatedConfig* InConfig) const { return nullptr; }

		/** Gets an accumulator that's in use for the provided sample state (or creates a new one), and returns the accumulator args that are in use. */
		virtual TSharedRef<::MoviePipeline::IMoviePipelineAccumulationArgs> GetOrCreateAccumulator(TObjectPtr<UMovieGraphDefaultRenderer> InGraphRenderer, const FMovieGraphSampleState& InSampleState) const;

		/** Gets the function that the accumulator will use to perform accumulation. */
		virtual FAccumulatorSampleFunc GetAccumulateSampleFunction() const;

	protected:
		TWeakObjectPtr<UMovieGraphDefaultRenderer> WeakGraphRenderer;
	};
}