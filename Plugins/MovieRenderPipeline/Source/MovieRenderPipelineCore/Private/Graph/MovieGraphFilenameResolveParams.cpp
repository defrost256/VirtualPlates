// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MovieGraphFilenameResolveParams.h"

#include "Graph/MovieGraphPipeline.h"
#include "Graph/Nodes/MovieGraphGlobalOutputSettingNode.h"
#include "MoviePipelineQueue.h"

FMovieGraphFilenameResolveParams FMovieGraphFilenameResolveParams::MakeResolveParams(
	const FMovieGraphRenderDataIdentifier& InRenderId,
	const UMovieGraphPipeline* InPipeline,
	const TObjectPtr<UMovieGraphEvaluatedConfig>& InEvaluatedConfig,
	const FMovieGraphTraversalContext& InTraversalContext,
	const TMap<FString, FString>& InAdditionalFormatArgs)
{
    FMovieGraphFilenameResolveParams Params = FMovieGraphFilenameResolveParams();
	
	int32 RootFrameNumberRelOffset = 0;
	int32 ShotFrameNumberRelOffset = 0;
	if (ensureAlwaysMsgf(InPipeline, TEXT("InPipeline is not valid - ResolveParams will be created, but will be missing critical information.")))
	{
		Params.InitializationTime = InPipeline->GetInitializationTime();
		Params.InitializationTimeOffset = InPipeline->GetInitializationTimeOffset();
		Params.Job = InPipeline->GetCurrentJob();
		
		if (InPipeline->GetActiveShotList().IsValidIndex(InTraversalContext.ShotIndex))
		{
			const TObjectPtr<UMoviePipelineExecutorShot>& Shot = InPipeline->GetActiveShotList()[InTraversalContext.ShotIndex];
			Params.Version = Shot->ShotInfo.VersionNumber;
			Params.Shot = Shot;
			
			// Calculate the offset (in frames) that relative frame numbers need to add to themselves so that they are correctly
			// offset by the starting frame of the root sequence/shot (to match new updated relative behavior).
			RootFrameNumberRelOffset = FFrameRate::TransformTime(Shot->ShotInfo.InitialTimeInRoot, Shot->ShotInfo.CachedTickResolution, Shot->ShotInfo.CachedFrameRate).FloorToFrame().Value;
			ShotFrameNumberRelOffset = FFrameRate::TransformTime(Shot->ShotInfo.InitialTimeInShot, Shot->ShotInfo.CachedTickResolution, Shot->ShotInfo.CachedFrameRate).FloorToFrame().Value;
		}
	}
        
	Params.RenderDataIdentifier = InRenderId;
	
	Params.RootFrameNumber = InTraversalContext.Time.RootFrameNumber.Value;
	Params.ShotFrameNumber = InTraversalContext.Time.ShotFrameNumber.Value;

	// Starting in MRG, relative file numbers are now relative to the first frame of the shot/sequence, and not
	// to zero. To do this, we use our zero-relative numbers and offset them by the starting point of the shot/sequence.
	Params.RootFrameNumberRel = InTraversalContext.Time.OutputFrameNumber + RootFrameNumberRelOffset;
	Params.ShotFrameNumberRel = InTraversalContext.Time.ShotOutputFrameNumber + ShotFrameNumberRelOffset;
	//Params.FileMetadata = ToDo: Track File Metadata

	if (InEvaluatedConfig)
	{
		const UMovieGraphGlobalOutputSettingNode* OutputSettingNode = InEvaluatedConfig->GetSettingForBranch<UMovieGraphGlobalOutputSettingNode>(UMovieGraphNode::GlobalsPinName);
		if (IsValid(OutputSettingNode))
		{
			Params.ZeroPadFrameNumberCount = OutputSettingNode->ZeroPadFrameNumbers;
			Params.FrameNumberOffset = OutputSettingNode->FrameNumberOffset;
		}
		Params.EvaluatedConfig = InEvaluatedConfig;
	}

	const bool bTimeDilationUsed = !FMath::IsNearlyEqual(InTraversalContext.Time.WorldTimeDilation, 1.f) || InTraversalContext.Time.bHasRelativeTimeBeenUsed;
    Params.bForceRelativeFrameNumbers = bTimeDilationUsed;
	Params.bEnsureAbsolutePath = true;
	Params.FileNameFormatOverrides = InAdditionalFormatArgs;

	return Params;
}
