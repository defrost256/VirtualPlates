// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Graph/MovieGraphQuickRenderSettings.h"
#include "HAL/Platform.h"

class UMovieGraphEvaluatedConfig;
class UMoviePipelineExecutorJob;
class UMoviePipelineExecutorShot;

/** Telemetry data that is captured when a shot begins rendering. Only for use with settings/nodes shipped with Movie Render Queue/Graph. */
struct MOVIERENDERPIPELINECORE_API FMoviePipelineShotRenderTelemetry
{
	bool bIsGraph = false;
	bool bIsQuickRender = false;
	bool bUsesDeferred = false;
	bool bUsesPathTracer = false;
	bool bUsesPanoramic = false;
	bool bUsesHighResTiling = false;
	bool bUsesNDisplay = false;
	bool bUsesObjectID = false;
	bool bUsesMultiCamera = false;
	bool bUsesScripting = false;
	bool bUsesSubgraphs = false;
	bool bUsesPPMs = false;
	bool bUsesAudio = false;
	bool bUsesAvid = false;
	bool bUsesProRes = false;
	bool bUsesMP4 = false;
	int32 ResolutionX = 0;
	int32 ResolutionY = 0;
	int32 HandleFrameCount = 0;
	int32 TemporalSampleCount = 0;
	int32 SpatialSampleCount = 0;
	int32 RenderLayerCount = 0;
	int32 HighResTileCount = 0;
	float HighResOverlap = 0;
	bool bUsesPageToSystemMemory = false;

	// Note: If adding an entry here, make sure to also update FMoviePipelineTelemetry::SendBeginShotRenderTelemetry()
	// Also remember to track the telemetry in both the graph and legacy.
};

/** Responsible for sending out telemetry for Movie Render Queue, Movie Render Graph, and Quick Render. */
class MOVIERENDERPIPELINECORE_API FMoviePipelineTelemetry
{
public:
	/** Sends out telemetry that captures queue-level information. Called by either MRQ or MRG, not Quick Render. */
	static void SendRendersRequestedTelemetry(const bool bIsLocal, TArray<UMoviePipelineExecutorJob*>&& InJobs);

	/** Sends out telemetry that captures Quick Render information for the provided mode. */
	static void SendQuickRenderRequestedTelemetry(const EMovieGraphQuickRenderMode QuickRenderMode);

	/** Sends out telemetry that includes information about the shot being rendered (the type of rendering being done, which types of settings/nodes are being used, etc). */
	static void SendBeginShotRenderTelemetry(UMoviePipelineExecutorShot* InShot, UMovieGraphEvaluatedConfig* InEvaluatedConfig = nullptr);

	/** Sends out telemetry that indicates whether the shot was rendered successfully. */
	static void SendEndShotRenderTelemetry(const bool bIsGraph, const bool bWasSuccessful, const bool bWasCanceled);

private:
	/** Tracks whether the current render request originated from Quick Render. If false, the render originated from a queue in either MRQ or MRG. */
	inline static bool bIsCurrentRenderRequestQuickRender = false;
	
	/** Gets the current session type (Editor, DashGame, Shipping). */
	static FString GetSessionType();

	/** Returns a populated telemetry object for the shot and graph that's specified. */
	static FMoviePipelineShotRenderTelemetry GatherShotRenderTelemetryForGraph(const UMoviePipelineExecutorShot* InShot, UMovieGraphEvaluatedConfig* InEvaluatedConfig);

	/** Returns a populated telemetry object for the shot that's specified. For use with the legacy system, not the graph. */
	static FMoviePipelineShotRenderTelemetry GatherShotRenderTelemetryForLegacy(UMoviePipelineExecutorShot* InShot);
};