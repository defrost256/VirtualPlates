// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MovieGraphQuickRender.h"

#include "Camera/CameraActor.h"
#include "Editor.h"
#include "Engine/LevelStreaming.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "LevelUtils.h"
#include "Misc/MessageDialog.h"
#include "MoviePipelineEditorBlueprintLibrary.h"
#include "MoviePipelinePIEExecutor.h"
#include "MoviePipelinePostRenderFileDisplayProcessor.h"
#include "MoviePipelineQueueSubsystem.h"
#include "MoviePipelineTelemetry.h"
#include "MoviePipelineUtils.h"
#include "MovieRenderPipelineCoreModule.h"
#include "MovieRenderPipelineSettings.h"
#include "MovieScene.h"
#include "MovieSceneSpawnableAnnotation.h"
#include "ObjectEditorUtils.h"
#include "Scalability.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Sections/MovieSceneLevelVisibilitySection.h"
#include "SequencerUtilities.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Tracks/MovieSceneLevelVisibilityTrack.h"
#include "Tracks/MovieSceneSubTrack.h"

#include "Graph/MovieGraphRenderLayerSubsystem.h"
#include "Graph/MovieRenderGraphEditorSettings.h"
#include "Graph/Nodes/MovieGraphApplyViewportLookNode.h"
#include "Graph/Nodes/MovieGraphCollectionNode.h"
#include "Graph/Nodes/MovieGraphDeferredPassNode.h"
#include "Graph/Nodes/MovieGraphGlobalGameOverrides.h"
#include "Graph/Nodes/MovieGraphGlobalOutputSettingNode.h"
#include "Graph/Nodes/MovieGraphModifierNode.h"
#include "Graph/Nodes/MovieGraphVideoOutputNode.h"

#if WITH_EDITOR
#include "Editor/Transactor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "Selection.h"
#endif

#if WITH_OCIO
#include "IOpenColorIOModule.h"
#include "OpenColorIODisplayManager.h"
#endif

#define LOCTEXT_NAMESPACE "FMovieGraphQuickRender"

/**
 * Wraps the scope that this is created in with a dummy transaction. When going out of scope, the dummy transaction will be canceled, effectively
 * blocking any transactions that occurred while in scope.
 */
class FMovieGraphTransactionBlocker
{
public:
	FMovieGraphTransactionBlocker()
	{
#if WITH_EDITOR
		TransactionId = GEditor->BeginTransaction(FText::GetEmpty());
#endif
	}

	~FMovieGraphTransactionBlocker()
	{
#if WITH_EDITOR
		GEditor->CancelTransaction(TransactionId);
#endif
	}

private:
	int32 TransactionId = 0;
};

void UMovieGraphQuickRenderSubsystem::BeginQuickRender(const EMovieGraphQuickRenderMode InQuickRenderMode, const UMovieGraphQuickRenderModeSettings* InQuickRenderSettings)
{
	UMoviePipelineQueueSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>();
	check(Subsystem);

	if (Subsystem->IsRendering())
	{
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("Unable to Quick Render: Rendering already in progress."));
		return;
	}

	QuickRenderMode = InQuickRenderMode;

	// Store a reference to the settings used, so the executor callbacks can rely on them still existing.
	QuickRenderModeSettings = InQuickRenderSettings;

	// Set up the rendering and "utility" level sequences. These are both required to do a render successfully.
	if (!SetUpAllLevelSequences())
	{
		return;
	}

	// Don't do additional work if the selected mode doesn't have what it needs to do a render. AreModePrerequisitesMet() will output the appropriate warnings.
	if (!AreModePrerequisitesMet())
	{
		return;
	}

	// Find the editor world
	UWorld* EditorWorld = GetWorldOfType(EWorldType::Editor);
	if (!EditorWorld)
	{
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("Unable to Quick Render: Could not find a currently active editor world."));
		return;
	}

	// Generate a new queue, fully populated with a job to render from, the graph assigned to that job, and job-level variable overrides applied.
	TObjectPtr<UMoviePipelineExecutorJob> NewJob = nullptr;
	if (!GenerateAndPopulateQueue(TemporaryQueue, NewJob, QuickRenderModeSettings))
	{
		return;
	}
	
	// Evaluate the graph. Walking the graph to determine which nodes are present in each branch (especially taking into consideration
	// subgraphs) is difficult here, so evaluation is the best way to deal with this.
	//
	// Ideally we would not have to evaluate the graph here, and we could get the evaluated graph from elsewhere within the pipeline.
	// However, that would require exposing the inner workings of the pipeline to an outside API, which was deemed a bigger negative
	// than taking the (very small) performance hit of doing a graph evaluation here.
	FString OutError;
	FMovieGraphTraversalContext Context;
	Context.Job = NewJob;
	Context.RootGraph = TemporaryGraph.Get();
	TemporaryEvaluatedGraph = TemporaryGraph->CreateFlattenedGraph(Context, OutError);
	if (!TemporaryEvaluatedGraph)
	{
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("Unable to Quick Render: Could not create an evaluated graph. Reason: %s"), *OutError);
		return;
	}

	// Update the graph as needed in order to properly apply the settings specified by the user. This will perform a graph evaluation, so the job
	// should be fully initialized at this point.
	ApplyQuickRenderUpdatesToDuplicateGraph(EditorWorld);

	// Cache out anything necessary before PIE starts
	CachePrePieData(EditorWorld);

	// Create the executor. Note that it's allowed to use unsaved levels (many animators work off of unsaved levels). Because Quick Render is
	// creating a temporary queue and rendering locally, using an unsaved level is safe.
	// Always use the PIE executor for Quick Render, rather than the executor specified in the project settings for normal MRG.
	TemporaryExecutor = NewObject<UMoviePipelinePIEExecutor>(GetTransientPackage());
	TemporaryExecutor->SetAllowUsingUnsavedLevels(true);

	// Do any needed setup in the PIE world before rendering starts
	TemporaryExecutor->OnIndividualJobStarted().AddLambda([this, EditorWorld](UMoviePipelineExecutorJob* InJob)
	{
		PerformPreRenderSetup(EditorWorld);
	});

	// View the job's rendered frame(s)/video(s) if requested
	TemporaryExecutor->OnIndividualJobWorkFinished().AddLambda([this](FMoviePipelineOutputData OutputData)
	{
		HandleJobFinished(QuickRenderModeSettings, OutputData);
	});

	// Clean up
	TemporaryExecutor->OnExecutorFinished().AddLambda([this](UMoviePipelineExecutorBase* InExecutor, bool bSuccess)
	{
		RestorePreRenderState();
		PerformPostRenderCleanup();
	});

	FMoviePipelineTelemetry::SendQuickRenderRequestedTelemetry(InQuickRenderMode);

	// Do the render
	Subsystem->RenderQueueInstanceWithExecutorInstance(TemporaryQueue.Get(), TemporaryExecutor.Get());
}

void UMovieGraphQuickRenderSubsystem::PlayLastRender()
{
	OpenPostRenderFileDisplayProcessor(PreviousRenderOutputData);
}

bool UMovieGraphQuickRenderSubsystem::CanPlayLastRender()
{
	return !PreviousRenderOutputData.GraphData.IsEmpty();
}

void UMovieGraphQuickRenderSubsystem::OpenOutputDirectory(const UMovieGraphQuickRenderModeSettings* InQuickRenderSettings)
{
	if (!InQuickRenderSettings)
	{
		return;
	}

	// In order to properly resolve the output directory, we have to set up a valid job.
	TObjectPtr<UMoviePipelineExecutorJob> TempJob = nullptr;
	TObjectPtr<UMoviePipelineQueue> TempQueue = nullptr;
	if (!GenerateAndPopulateQueue(TempQueue, TempJob, InQuickRenderSettings))
	{
		return;
	}

	const FString ResolvedOutputDirectory = UMoviePipelineEditorBlueprintLibrary::ResolveOutputDirectoryFromJob(TempJob);

	if (!ResolvedOutputDirectory.IsEmpty())
	{
		// The directory might not exist yet. Create it (if needed) so ExploreFolder() can open it.
		IFileManager::Get().MakeDirectory(*ResolvedOutputDirectory, true);

		FPlatformProcess::ExploreFolder(*ResolvedOutputDirectory);
	}
}

UMovieGraphConfig* UMovieGraphQuickRenderSubsystem::GetQuickRenderGraph(UMovieGraphConfig* InUserSpecifiedGraph, TMap<TObjectPtr<UMovieGraphConfig>, TObjectPtr<UMovieGraphConfig>>& OutOriginalToDupeMap)
{
	UMovieGraphConfig* QuickRenderGraph = InUserSpecifiedGraph;
	if (!QuickRenderGraph)
	{
		if (const UMovieRenderPipelineProjectSettings* MoviePipelineProjectSettings = GetDefault<UMovieRenderPipelineProjectSettings>())
		{
			QuickRenderGraph = MoviePipelineProjectSettings->DefaultQuickRenderGraph.LoadSynchronous();
			if (!QuickRenderGraph)
			{
				UE_LOG(LogMovieRenderPipeline, Error, TEXT("Unable to Quick Render: The default graph specified in project settings could not be loaded."));
				return nullptr;
			}
		}
		else
		{
			UE_LOG(LogMovieRenderPipeline, Error, TEXT("Unable to Quick Render: Could not get the movie pipeline project settings."));
			return nullptr;
		}
	}

	if (!QuickRenderGraph)
	{
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("Unable to Quick Render: No valid graph could be loaded."));
		return nullptr;
	}

	// Duplicate the graph so changes can be made to it without affecting the graph asset being used
	UMovieGraphConfig* DuplicateGraph = MoviePipeline::DuplicateConfigRecursive(QuickRenderGraph, OutOriginalToDupeMap);
	TemporaryGraph = DuplicateGraph;

	return TemporaryGraph.Get();
}

bool UMovieGraphQuickRenderSubsystem::GenerateAndPopulateQueue(TObjectPtr<UMoviePipelineQueue>& OutQueue, TObjectPtr<UMoviePipelineExecutorJob>& OutJob, TObjectPtr<const UMovieGraphQuickRenderModeSettings> InQuickRenderSettings)
{
	if (!InQuickRenderSettings)
	{
		UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Quick Render: Invalid mode settings provided."));
		return false;
	}
	
	UWorld* EditorWorld = GetWorldOfType(EWorldType::Editor);
	if (!EditorWorld)
	{
		UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Quick Render: Could not find an editor world."));
		return false;
	}
	
	// Allocate a new queue that will be used temporarily
	OutQueue = NewObject<UMoviePipelineQueue>(GetTransientPackage());

	// Add a temp job to the queue, and give it the editor's level and the current level sequence being edited in Sequencer
	OutJob = OutQueue->AllocateNewJob(UMoviePipelineExecutorJob::StaticClass());
	OutJob->SetSequence(FSoftObjectPath(RenderingLevelSequence.Get()));
	OutJob->Map = FSoftObjectPath(EditorWorld);
	OutJob->JobName = FString(TEXT("QuickRender"));

	// Update the job to use the quick render graph (either the default, or the one specified in the settings)
	TMap<TObjectPtr<UMovieGraphConfig>, TObjectPtr<UMovieGraphConfig>> OriginalGraphToDupeMap;
	const UMovieGraphConfig* QuickRenderGraph = GetQuickRenderGraph(InQuickRenderSettings->GraphPreset.LoadSynchronous(), OriginalGraphToDupeMap);
	if (!QuickRenderGraph)
	{
		// GetQuickRenderGraph() will output an error to the log if a graph was not returned
		return false;
	}
	OutJob->SetGraphPreset(QuickRenderGraph);

	// Apply any job-level variable overrides, if they were specified
	for (const TObjectPtr<UMovieJobVariableAssignmentContainer>& IncomingVariableAssignment : InQuickRenderSettings->GraphVariableAssignments)
	{
		// Map the setting's graph to the duplicate graph
		const UMovieGraphConfig* AssignmentGraph = IncomingVariableAssignment->GetGraphConfig().LoadSynchronous();
		const TObjectPtr<UMovieGraphConfig>* DuplicateGraph = OriginalGraphToDupeMap.Find(AssignmentGraph);
		if (!DuplicateGraph)
		{
			UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Quick Render: Unable to properly set variable overrides for graph [%s]."), *AssignmentGraph->GetName());
			continue;
		}

		UMovieJobVariableAssignmentContainer* NewVariableAssignments = OutJob->GetOrCreateJobVariableAssignmentsForGraph(*DuplicateGraph);

		for (const UMovieGraphVariable* GraphVariable : (*DuplicateGraph)->GetVariables())
		{
			bool bIsVariableAssignmentEnabled = false;
			IncomingVariableAssignment->GetVariableAssignmentEnableState(GraphVariable, bIsVariableAssignmentEnabled);
			
			// Copying values by serialized string isn't ideal, but it's the easiest way to transfer values between the variable assignments
			NewVariableAssignments->SetValueSerializedString(GraphVariable, IncomingVariableAssignment->GetValueSerializedString(GraphVariable));
			NewVariableAssignments->SetVariableAssignmentEnableState(GraphVariable, bIsVariableAssignmentEnabled);
		}
	}

	return true;
}

void UMovieGraphQuickRenderSubsystem::PerformPostRenderCleanup()
{
	// The temp objects are part of the transient package, so manually release them when the render is finished
	TemporaryQueue = nullptr;
	TemporaryGraph = nullptr;
	TemporaryExecutor = nullptr;
	TemporaryEvaluatedGraph = nullptr;
	RenderingLevelSequence = nullptr;
	UtilityLevelSequence = nullptr;
	QuickRenderModeSettings = nullptr;
}

void UMovieGraphQuickRenderSubsystem::ApplyQuickRenderUpdatesToDuplicateGraph(const UWorld* InEditorWorld)
{
	check(IsValid(TemporaryGraph));

	// Apply changes to Global Output Settings
	{
		UMovieGraphGlobalOutputSettingNode* GlobalOutputSettings = Cast<UMovieGraphGlobalOutputSettingNode>(InjectNodeIntoBranch(UMovieGraphGlobalOutputSettingNode::StaticClass(), UMovieGraphNode::GlobalsPinName));

		auto SetStartFrame = [&GlobalOutputSettings](const int32 StartFrame) -> void
		{
			GlobalOutputSettings->bOverride_CustomPlaybackRangeStart = true;
			GlobalOutputSettings->CustomPlaybackRangeStart.Type = EMovieGraphSequenceRangeType::Custom;
			GlobalOutputSettings->CustomPlaybackRangeStart.Value = StartFrame;
		};

		auto SetEndFrame = [&GlobalOutputSettings](const int32 EndFrame) -> void
		{
			GlobalOutputSettings->bOverride_CustomPlaybackRangeEnd = true;
			GlobalOutputSettings->CustomPlaybackRangeEnd.Type = EMovieGraphSequenceRangeType::Custom;
			GlobalOutputSettings->CustomPlaybackRangeEnd.Value = EndFrame;
		};

		const TRange<FFrameNumber> PlaybackRange = GetPlaybackRange();
		if (!PlaybackRange.IsEmpty())
		{
			SetStartFrame(PlaybackRange.GetLowerBoundValue().Value);
			SetEndFrame(PlaybackRange.GetUpperBoundValue().Value);
		}
	}

	// Inject an Apply Viewport Look node if any viewport look flags were specified. Logic outside of Quick Render will for look this node in order to
	// determine how to apply some of its settings
	const EMovieGraphQuickRenderViewportLookFlags LookFlags = static_cast<EMovieGraphQuickRenderViewportLookFlags>(QuickRenderModeSettings->ViewportLookFlags);
	if (LookFlags != EMovieGraphQuickRenderViewportLookFlags::None)
	{
		UMovieGraphApplyViewportLookNode* ApplyViewportLookNode = Cast<UMovieGraphApplyViewportLookNode>(InjectNodeIntoBranch(UMovieGraphApplyViewportLookNode::StaticClass(), UMovieGraphNode::GlobalsPinName));
		check(ApplyViewportLookNode);

#define APPLY_VIEWPORT_LOOK_FLAG(EnumName, FlagName) \
		if (IsViewportLookFlagActive(EnumName)) \
		{ \
			ApplyViewportLookNode->bOverride_b##FlagName = true; \
			ApplyViewportLookNode->b##FlagName = true; \
		} \

		// Transfer the flags from the render settings to the Apply Viewport Look node
		APPLY_VIEWPORT_LOOK_FLAG(EMovieGraphQuickRenderViewportLookFlags::Ocio, Ocio);
		APPLY_VIEWPORT_LOOK_FLAG(EMovieGraphQuickRenderViewportLookFlags::ShowFlags, ShowFlags);
		APPLY_VIEWPORT_LOOK_FLAG(EMovieGraphQuickRenderViewportLookFlags::ViewMode, ViewMode);
		APPLY_VIEWPORT_LOOK_FLAG(EMovieGraphQuickRenderViewportLookFlags::Visibility, Visibility);
#undef APPLY_VIEWPORT_LOOK_FLAG
	}

	// Apply viewport/editor actor visibility
	if (IsViewportLookFlagActive(EMovieGraphQuickRenderViewportLookFlags::Visibility))
	{
		ApplyQuickRenderUpdatesToDuplicateGraph_ApplyEditorVisibility(InEditorWorld);
	}

	// Apply editor-only actor visibility. Normally editor-only actors won't be processed by the MRG modifier system.
	if (IsViewportLookFlagActive(EMovieGraphQuickRenderViewportLookFlags::EditorOnlyActors))
	{
		ApplyQuickRenderUpdatesToDuplicateGraph_ApplyEditorOnlyActorVisibility(InEditorWorld);
	}

	// Apply OCIO if it is activated on the viewport and enabled in the viewport look flags
	if (IsViewportLookFlagActive(EMovieGraphQuickRenderViewportLookFlags::Ocio))
	{
#if WITH_OCIO
		if (const FEditorViewportClient* ViewportClient = UMovieGraphApplyViewportLookNode::GetViewportClient())
		{
			if (const FOpenColorIODisplayConfiguration* OcioConfiguration = IOpenColorIOModule::Get().GetDisplayManager().GetDisplayConfiguration(ViewportClient))
			{
				if (OcioConfiguration->bIsEnabled)
				{
					ApplyQuickRenderUpdatesToDuplicateGraph_ApplyOcio(OcioConfiguration);
				}
			}
		}
#endif	// WITH_OCIO
	}

	// Apply viewport scalability settings if the graph didn't specify an explicit scalability setting
	ApplyQuickRenderUpdatesToDuplicateGraph_Scalability();
}

void UMovieGraphQuickRenderSubsystem::ApplyQuickRenderUpdatesToDuplicateGraph_ApplyEditorVisibility(const UWorld* InEditorWorld)
{
	TArray<FMovieGraphActorQueryEntry> VisibleActors;
	TArray<FMovieGraphActorQueryEntry> HiddenActors;
	
	// Determine the actors that are hidden and visible
	for (FActorIterator It(InEditorWorld); It; ++It)
	{
		AActor* Actor = *It;

		if (!Actor)
		{
			continue;
		}

		// Don't process ABrush actors (actors derived from ABrush are probably ok). The builder brush is problematic because of how it decides to
		// render itself. There's no API we can call to accurately determine if it's going to show up in the viewport, and they're hidden in game
		// by default, so just skip them so they don't show up in the render (needing them to show up in renders should be rare).
		if (Actor->GetClass() == ABrush::StaticClass())
		{
			continue;
		}

		if (Actor->IsHiddenEd())
		{
			FMovieGraphActorQueryEntry& HiddenActorEntry = HiddenActors.AddDefaulted_GetRef();
			HiddenActorEntry.ActorToMatch = Actor;
		}
		else
		{
			FMovieGraphActorQueryEntry& VisibleActorEntry = VisibleActors.AddDefaulted_GetRef();
			VisibleActorEntry.ActorToMatch = Actor;
		}
	}

	UMovieGraphConditionGroupQuery_Actor* VisibleActorQuery =
		AddNewCollectionWithVisibilityModifier<UMovieGraphConditionGroupQuery_Actor>(TEXT("VISIBLE_ACTORS"), false);
	VisibleActorQuery->ActorsAndComponentsToMatch = VisibleActors;
	
	UMovieGraphConditionGroupQuery_Actor* HiddenActorQuery =
		AddNewCollectionWithVisibilityModifier<UMovieGraphConditionGroupQuery_Actor>(TEXT("HIDDEN_ACTORS"), true);
	HiddenActorQuery->ActorsAndComponentsToMatch = HiddenActors;
}

void UMovieGraphQuickRenderSubsystem::ApplyQuickRenderUpdatesToDuplicateGraph_ApplyEditorOnlyActorVisibility(const UWorld* InEditorWorld)
{
	TArray<FMovieGraphActorQueryEntry> VisibleActors;
	
	// Determine the editor-only actors that should be made visible
	for (FActorIterator It(InEditorWorld); It; ++It)
	{
		AActor* Actor = *It;

		if (!Actor)
		{
			continue;
		}

		bool bHasEditorOnlyComponent = false;
		
		for (const UActorComponent* ActorComponent : Actor->GetComponents())
		{
			if (ActorComponent->IsEditorOnly())
			{
				bHasEditorOnlyComponent = true;
				break;
			}
		}

		if (Actor->IsEditorOnly() || Actor->bIsEditorPreviewActor || bHasEditorOnlyComponent)
		{
			FMovieGraphActorQueryEntry& VisibleActorEntry = VisibleActors.AddDefaulted_GetRef();
			VisibleActorEntry.ActorToMatch = Actor;
		}

		// The control rig actor class name is referenced by name here in order to prevent a dependency.
		static const FName ControlRigShapeActorName(TEXT("ControlRigShapeActor"));

		// The Control Rig manipulation gizmos are important to show in the editor-only actor mode, but they will not be copied into the PIE world
		// because they're marked as Transient. To work around this, we can temporarily mark them as RF_NonPIEDuplicateTransient which will inform
		// the serialization process that it's OK to copy these actors to PIE.
		if (Actor->HasAnyFlags(RF_Transient) && (Actor->GetClass()->GetName() == ControlRigShapeActorName))
		{
			// Since we need to un-set this flag after the render is done, only apply RF_NonPIEDuplicateTransient if it's not already applied (ie,
			// we don't want to remove RF_NonPIEDuplicateTransient after the render finishes if Quick Render didn't originally apply it).
			if (!Actor->HasAnyFlags(RF_NonPIEDuplicateTransient))
			{
				CachedPrePieData.TemporarilyNonTransientActors.Add(MakeWeakObjectPtr(Actor));
				Actor->SetFlags(RF_NonPIEDuplicateTransient);
			}

			for (UActorComponent* Component : Actor->GetComponents())
			{
				if (Component->HasAnyFlags(RF_Transient))
				{
					if (!Component->HasAnyFlags(RF_NonPIEDuplicateTransient))
					{
						CachedPrePieData.TemporarilyNonTransientComponents.Add(MakeWeakObjectPtr(Component));
						Component->SetFlags(RF_NonPIEDuplicateTransient);
					}
				}
			}
		}
	}

	constexpr bool bModifierShouldHide = false;
	constexpr bool bProcessEditorOnlyActors = true;
	UMovieGraphConditionGroupQuery_Actor* VisibleActorQuery =
		AddNewCollectionWithVisibilityModifier<UMovieGraphConditionGroupQuery_Actor>(TEXT("VISIBLE_EDITORONLY_ACTORS"), bModifierShouldHide, bProcessEditorOnlyActors);
	VisibleActorQuery->ActorsAndComponentsToMatch = VisibleActors;
}

void UMovieGraphQuickRenderSubsystem::ApplyQuickRenderUpdatesToDuplicateGraph_ApplyOcio(const FOpenColorIODisplayConfiguration* InOcioConfiguration)
{
	// For each branch, apply OCIO configuration.
	for (const FName& BranchName : TemporaryEvaluatedGraph->GetBranchNames())
	{
		constexpr bool bIncludeCDOs = false;
		constexpr bool bExactMatch = false;
		const TArray<UMovieGraphFileOutputNode*> FileOutputNodes =
			TemporaryEvaluatedGraph->GetSettingsForBranch<UMovieGraphFileOutputNode>(BranchName, bIncludeCDOs, bExactMatch);
		const TArray<UMovieGraphImagePassBaseNode*> ImagePassNodes =
			TemporaryEvaluatedGraph->GetSettingsForBranch<UMovieGraphImagePassBaseNode>(BranchName, bIncludeCDOs, bExactMatch);

		// Update all file output nodes to have the viewport's OCIO configuration
		for (UMovieGraphFileOutputNode* FileOutputNode : FileOutputNodes)
		{
			static const FName OcioOverrideName(TEXT("bOverride_OCIOConfiguration"));
			static const FName OcioConfigName(TEXT("OCIOConfiguration"));

			// The evaluated graph contains a node of this type, so insert a new node into the non-evaluated graph downstream to
			// override the OCIO settings.
			if (UMovieGraphNode* NewFileOutputNode = InjectNodeIntoBranch(FileOutputNode->GetClass(), BranchName))
			{
				// This is hacky, but OCIO properties are not uniformly inherited across image/video nodes. The properties are, however,
				// named the same.
				const FBoolProperty* OverrideProp = FindFProperty<FBoolProperty>(NewFileOutputNode->GetClass(), OcioOverrideName);
				const FStructProperty* OcioProp = FindFProperty<FStructProperty>(NewFileOutputNode->GetClass(), OcioConfigName);
				
				if (OverrideProp && OcioProp)
				{
					OverrideProp->SetPropertyValue_InContainer(NewFileOutputNode, true);
					OcioProp->SetValue_InContainer(NewFileOutputNode, InOcioConfiguration);
				}
			}
		}

		// Also enable OCIO on the renderer nodes
		for (UMovieGraphImagePassBaseNode* ImagePassNode : ImagePassNodes)
		{
			static const FName AllowOcioName(TEXT("bAllowOCIO"));
			static const FName DisableToneCurveOverrideName(TEXT("bOverride_bDisableToneCurve"));
			static const FName DisableToneCurveName(TEXT("bDisableToneCurve"));

			const FBoolProperty* AllowOcioProp = FindFProperty<FBoolProperty>(ImagePassNode->GetClass(), AllowOcioName);
			if (!AllowOcioProp)
			{
				// This node doesn't support OCIO
				continue;
			}

			// Skip nodes that have "Allow OCIO" explicitly turned off
			bool AllowOcioValue;
			AllowOcioProp->GetValue_InContainer(ImagePassNode, &AllowOcioValue);
			if (!AllowOcioValue)
			{
				UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Quick Render was set to apply viewport OCIO, but the renderer node [%s] has Allow OCIO turned off. OCIO will not be applied in this node's render."), *ImagePassNode->GetName());
				continue;
			}

			// Insert a new node of this type into the non-evaluated graph downstream to override the OCIO settings.
			if (UMovieGraphNode* NewNode = InjectNodeIntoBranch(ImagePassNode->GetClass(), BranchName))
			{
				// Like the file output nodes, OCIO properties are not uniformly inherited across renderer nodes. The properties are, however,
				// named the same.
				UMovieGraphImagePassBaseNode* NewRendererNode = Cast<UMovieGraphImagePassBaseNode>(NewNode);

				const FBoolProperty* DisableToneCurveOverrideProp = FindFProperty<FBoolProperty>(NewRendererNode->GetClass(), DisableToneCurveOverrideName);
				const FBoolProperty* DisableToneCurveProp = FindFProperty<FBoolProperty>(NewRendererNode->GetClass(), DisableToneCurveName);

				// At this point "Allow OCIO" is turned on, so we just need to make sure the tone curve is disabled.
				if (DisableToneCurveOverrideProp && DisableToneCurveProp)
				{
					DisableToneCurveOverrideProp->SetPropertyValue_InContainer(NewRendererNode, true);
					DisableToneCurveProp->SetPropertyValue_InContainer(NewRendererNode, true);
				}
			}
		}
	}
}

void UMovieGraphQuickRenderSubsystem::ApplyQuickRenderUpdatesToDuplicateGraph_Scalability()
{
	constexpr bool bIncludeCDOs = false;
	constexpr bool bExactMatch = true;
	const UMovieGraphGlobalGameOverridesNode* GameOverridesNode =
		TemporaryEvaluatedGraph->GetSettingForBranch<UMovieGraphGlobalGameOverridesNode>(UMovieGraphNode::GlobalsPinName, bIncludeCDOs, bExactMatch);

	// If the user specified an explicit scalability level to use, there's nothing to do here. Otherwise, we'll apply the value that the viewport
	// is using.
	if (GameOverridesNode && GameOverridesNode->bOverride_ScalabilityQualityLevel)
	{
		return;
	}
	
	if (UMovieGraphGlobalGameOverridesNode* NewGameOverridesNode =
			Cast<UMovieGraphGlobalGameOverridesNode>(InjectNodeIntoBranch(UMovieGraphGlobalGameOverridesNode::StaticClass(), UMovieGraphNode::GlobalsPinName)))
	{
		const Scalability::FQualityLevels QualityLevels = Scalability::GetQualityLevels();
		const int32 MinQualityLevel = QualityLevels.GetMinQualityLevel();
		const int32 QualityLevel = QualityLevels.GetSingleQualityLevel();

		int32 QualityLevelToUse;

		// Use the overall value set for scalability if it's not custom (-1). If that's custom, fall back to the minimum quality level. If that's
		// custom, then default to High.
		if (QualityLevel != -1)
		{
			QualityLevelToUse = QualityLevel;
		}
		else if (MinQualityLevel != -1)
		{
			UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Scalability settings are not all set to the same level, which is not supported by Quick Render. Using the minimum from all settings, which is [%s]."),
				*Scalability::GetScalabilityNameFromQualityLevel(MinQualityLevel).ToString());

			QualityLevelToUse = MinQualityLevel;
		}
		else
		{
			UE_LOG(LogMovieRenderPipeline, Warning, TEXT("One or more scalability settings are set to custom, which is not supported by Quick Render. Defaulting to High scalability"));

			QualityLevelToUse = 2;	// High
		}

		NewGameOverridesNode->bOverride_ScalabilityQualityLevel = true;
		NewGameOverridesNode->ScalabilityQualityLevel = static_cast<EMovieGraphScalabilityQualityLevel>(QualityLevelToUse);
	}
}

template <typename QueryType>
QueryType* UMovieGraphQuickRenderSubsystem::AddNewCollectionWithVisibilityModifier(const FString& InOperationName, const bool bModifierShouldHide, const bool bProcessEditorOnlyActors)
{
	const UMovieGraphCollectionNode* NewCollectionNode = Cast<UMovieGraphCollectionNode>(InjectNodeIntoBranch(UMovieGraphCollectionNode::StaticClass(), UMovieGraphNode::GlobalsPinName));

	const FString NewCollectionName(TEXT("__AUTOGEN_COLLECTION_") + InOperationName);
	NewCollectionNode->Collection->SetCollectionName(NewCollectionName);

	QueryType* NewConditionGroupQuery = Cast<QueryType>(NewCollectionNode->Collection->AddConditionGroup()->AddQuery(QueryType::StaticClass()));

	UMovieGraphModifierNode* NewModifierNode = Cast<UMovieGraphModifierNode>(InjectNodeIntoBranch(UMovieGraphModifierNode::StaticClass(), UMovieGraphNode::GlobalsPinName));
	NewModifierNode->ModifierName = TEXT("__AUTOGEN_MODIFIER_") + InOperationName;
	NewModifierNode->AddCollection(FName(NewCollectionName));

	UMovieGraphRenderPropertyModifier* RenderPropertyModifier = Cast<UMovieGraphRenderPropertyModifier>(NewModifierNode->GetModifier(UMovieGraphRenderPropertyModifier::StaticClass()));

	RenderPropertyModifier->bOverride_bIsHidden = true;
	RenderPropertyModifier->bIsHidden = bModifierShouldHide;

	if (bProcessEditorOnlyActors)
	{
		RenderPropertyModifier->bOverride_bProcessEditorOnlyActors = true;
		RenderPropertyModifier->bProcessEditorOnlyActors = true;
	}

	// The caller can modify the returned condition group as needed
	return NewConditionGroupQuery;
}

UMovieGraphNode* UMovieGraphQuickRenderSubsystem::InjectNodeIntoBranch(const TSubclassOf<UMovieGraphNode> NodeType, const FName& InBranchName) const
{
	check(IsValid(TemporaryGraph));

	UMovieGraphNode* NewNode = TemporaryGraph->CreateNodeByClass(NodeType);
	UMovieGraphNode* OutputNode = TemporaryGraph->GetOutputNode();

	const UMovieGraphPin* BranchPin = OutputNode->GetInputPin(InBranchName);
	if (!BranchPin)
	{
		UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Quick Render: Could not find branch [%s] to inject a node override into. The render may look different than expected."), *InBranchName.ToString());
		return nullptr;
	}

	// Get the node on the branch that is most downstream (if any)
	UMovieGraphNode* MostDownstreamNode = nullptr;
	if (const UMovieGraphPin* ConnectedPin = BranchPin->GetFirstConnectedPin())
	{
		MostDownstreamNode = ConnectedPin->Node;
	}

	// Add in the new node to the branch, downstream from the existing node that was previously furthest downstream
	TemporaryGraph->AddLabeledEdge(NewNode, FName(), OutputNode, InBranchName);

	// Re-connect the previously connected node upstream of the new node, if there was one
	if (MostDownstreamNode)
	{
		TemporaryGraph->AddLabeledEdge(MostDownstreamNode, FName(), NewNode, FName());
	}

	return NewNode;
}

bool UMovieGraphQuickRenderSubsystem::AreModePrerequisitesMet() const
{
	if (QuickRenderMode == EMovieGraphQuickRenderMode::SelectedCameras)
	{
		TArray<ACameraActor*> SelectedCameras;
		GEditor->GetSelectedActors()->GetSelectedObjects<ACameraActor>(SelectedCameras);

		if (SelectedCameras.IsEmpty())
		{
			FMessageDialog::Open(
				EAppMsgType::Ok,
				LOCTEXT("NoCamerasSelectedWarning", "The 'Selected Camera(s)' mode needs at least one camera selected. Select some cameras in the Outliner, then try again."));
			
			return false;
		}
	}

	return true;
}

void UMovieGraphQuickRenderSubsystem::CachePrePieData(UWorld* InEditorWorld)
{
	// Prevent all existing level sequences in the world from auto-playing. We need the level sequence from Quick Render to be in control. 
	for (auto It = TActorIterator<ALevelSequenceActor>(InEditorWorld); It; ++It)
	{
		const ALevelSequenceActor* LevelSequenceActor = *It;
		if (LevelSequenceActor->PlaybackSettings.bAutoPlay)
		{
			(*It)->PlaybackSettings.bAutoPlay = false;
			CachedPrePieData.ModifiedLevelSequenceActors.Add(MakeWeakObjectPtr(*It));
		}
	}

	// Cache out the selected camera actors before PIE starts. They'll be deselected once the PIE window shows up.
	TArray<ACameraActor*> CameraActors;
	GEditor->GetSelectedActors()->GetSelectedObjects<ACameraActor>(CameraActors);
	CopyToWeakArray(CachedPrePieData.SelectedCameras, CameraActors);

	// If the viewport is locked to an actor (likely a camera), cache it here. It may temporarily change while PIE is being started, so it needs to
	// be determined before PIE starts up.
	if (const FLevelEditorViewportClient* ViewportClient = UMovieGraphApplyViewportLookNode::GetViewportClient())
	{
		const FLevelViewportActorLock& ActorLock = ViewportClient->IsLockedToCinematic() ? ViewportClient->GetCinematicActorLock() : ViewportClient->GetActorLock();
		if (ActorLock.HasValidLockedActor())
		{
			CachedPrePieData.ViewportActorLock = ActorLock.LockedActor;
		}
		
		CachedPrePieData.ViewportActorLockCameraComponent = ViewportClient->GetCameraComponentForView();
	}
}

void UMovieGraphQuickRenderSubsystem::RestorePreRenderState()
{
	// Restore the modified level sequences to the way they were before the render.
	for (const TWeakObjectPtr<ALevelSequenceActor>& LevelSequenceActor : CachedPrePieData.ModifiedLevelSequenceActors)
	{
		if (LevelSequenceActor.IsValid())
		{
			LevelSequenceActor->PlaybackSettings.bAutoPlay = true;
		}
	}
	
	CachedPrePieData.SelectedCameras.Empty();
	
	// Remove the Utility subsequence that was added to the Rendering sequence
	UMovieSceneTrack* TrackToRemove = nullptr;
	for (UMovieSceneSection* Section : RenderingLevelSequence->GetMovieScene()->GetAllSections())
	{
		if (const UMovieSceneSubSection* SubsequenceSection = Cast<UMovieSceneSubSection>(Section))
		{
			if (SubsequenceSection->GetSequence() == UtilityLevelSequence.Get())
			{
				TrackToRemove = Section->GetTypedOuter<UMovieSceneTrack>();
				break;
			}
		}
	}

	if (TrackToRemove)
	{
		RenderingLevelSequence->GetMovieScene()->RemoveTrack(*TrackToRemove);
	}

	// Strip any transient flags off of relevant components and actors
	for (const TWeakObjectPtr<UActorComponent>& SceneComponent : CachedPrePieData.TemporarilyNonTransientComponents)
	{
		if (SceneComponent.IsValid())
		{
			SceneComponent->ClearFlags(RF_NonPIEDuplicateTransient);
		}
	}

	for (const TWeakObjectPtr<AActor>& Actor : CachedPrePieData.TemporarilyNonTransientActors)
	{
		if (Actor.IsValid())
		{
			Actor->ClearFlags(RF_NonPIEDuplicateTransient);
		}
	}

	CachedPrePieData.TemporarilyNonTransientComponents.Empty();
	CachedPrePieData.TemporarilyNonTransientActors.Empty();
	CachedPrePieData.ViewportActorLock.Reset();
	CachedPrePieData.ViewportActorLockCameraComponent = nullptr;
}

ULevelSequence* UMovieGraphQuickRenderSubsystem::SetUpRenderingLevelSequence() const
{
	auto GetCurrentLevelSequenceOrWarn = [](EMovieGraphQuickRenderMode ActiveQuickRenderMode) -> ULevelSequence*
	{
		ULevelSequence* CurrentLevelSequence = ULevelSequenceEditorBlueprintLibrary::GetCurrentLevelSequence();
		if (!CurrentLevelSequence)
		{
			const FText ModeName = StaticEnum<EMovieGraphQuickRenderMode>()->GetDisplayNameTextByValue(static_cast<int64>(ActiveQuickRenderMode));
			const FText MessageText = LOCTEXT("NoActiveSequenceWarning", "Quick Render needs a level sequence opened in Sequencer in order to function in the current mode [{0}]. Open a level sequence, then try again.");
		
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(MessageText, ModeName));

			return nullptr;
		}

		return CurrentLevelSequence;
	};
	
	// For the CurrentViewport and SelectedCameras modes, a temporary level sequence will be used to drive the render.
	if ((QuickRenderMode == EMovieGraphQuickRenderMode::CurrentViewport) || (QuickRenderMode == EMovieGraphQuickRenderMode::SelectedCameras))
	{
		// Create the new, temporary level sequence
		ULevelSequence* QuickRenderSequence = NewObject<ULevelSequence>(GetTransientPackage(), TEXT("QuickRenderSequence"), RF_Transient);
		QuickRenderSequence->Initialize();

		return QuickRenderSequence;
	}

	// Scripting may override the level sequence to use
	ULevelSequence* LevelSequenceOverride = QuickRenderModeSettings->LevelSequenceOverride.LoadSynchronous();
	if (IsValid(LevelSequenceOverride))
	{
		return LevelSequenceOverride;
	}

	// Otherwise, the active mode should use the level sequence that's currently active in Sequencer.
	ULevelSequence* CurrentLevelSequence = GetCurrentLevelSequenceOrWarn(QuickRenderMode);
	if (!CurrentLevelSequence)
	{
		return nullptr;
	}
	
	return CurrentLevelSequence;
}

ULevelSequence* UMovieGraphQuickRenderSubsystem::SetUpUtilityLevelSequence() const
{
	ULevelSequence* UtilitySequence = NewObject<ULevelSequence>(GetTransientPackage(), TEXT("QuickRenderUtilitySequence"), RF_Transient);
	UtilitySequence->Initialize();

	// If the currently active level sequence in Sequencer has spawnables, mirror those spawnable bindings into the utility level sequence. For modes
	// that don't use the active level sequence, we need to do this in order to match visibility.
	auto AddSpawnableTracks = [this, UtilitySequence]() -> void
	{
		const UWorld* EditorWorld = GetWorldOfType(EWorldType::Editor);
		const UMovieScene* UtilityMovieScene = UtilitySequence->GetMovieScene();

		for (FActorIterator It(EditorWorld); It; ++It)
		{
			AActor* Actor = *It;

			// There's no need to make the actor spawnable if it's currently not visible in the editor
			if (!Actor || Actor->IsHiddenEd())
			{
				continue;
			}

			TOptional<FMovieSceneSpawnableAnnotation> Spawnable = FMovieSceneSpawnableAnnotation::Find(Actor);
			if (Spawnable.IsSet())
			{
				FMovieGraphTransactionBlocker BlockTransactions;
				
				UE::Sequencer::FCreateBindingParams CreateBindingParams;
				CreateBindingParams.bAllowCustomBinding = true;
				CreateBindingParams.bSpawnable = true;
				FSequencerUtilities::CreateOrReplaceBinding(nullptr, UtilityMovieScene->GetTypedOuter<UMovieSceneSequence>(), Actor, CreateBindingParams);
			}
		}
	};

	if (IsViewportLookFlagActive(EMovieGraphQuickRenderViewportLookFlags::Visibility))
	{
		// Only modes that don't use the active level sequence need the spawnable bindings mirrored. Ideally this could be done in
		// PerformPreRenderSetup(), but at that point it's too late to get spawnable state because PIE has started.
		if ((QuickRenderMode == EMovieGraphQuickRenderMode::CurrentViewport) || (QuickRenderMode == EMovieGraphQuickRenderMode::SelectedCameras))
		{
			AddSpawnableTracks();
		}
	}

	return UtilitySequence;
}

bool UMovieGraphQuickRenderSubsystem::SetUpAllLevelSequences()
{
	// Get/create the level sequence that should be rendered from (varies depending on the Quick Render mode).
	RenderingLevelSequence = SetUpRenderingLevelSequence();

	// Also set up the utility level sequence (which is used, for example, for overriding level visibility)
	UtilityLevelSequence = SetUpUtilityLevelSequence();

	return IsValid(RenderingLevelSequence) && IsValid(UtilityLevelSequence);
}

UWorld* UMovieGraphQuickRenderSubsystem::GetWorldOfType(const EWorldType::Type WorldType) const
{
	UWorld* World = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == WorldType)
		{
			World = Context.World();
			break;
		}
	}

	return World;
}

TRange<FFrameNumber> UMovieGraphQuickRenderSubsystem::GetPlaybackRange()
{
	FFrameNumber StartFrame;
	FFrameNumber EndFrame;

	// If we're in Current Viewport or Selected Cameras mode, the playback range is always one frame long (only a single frame is rendered).
	if ((QuickRenderMode == EMovieGraphQuickRenderMode::CurrentViewport) || (QuickRenderMode == EMovieGraphQuickRenderMode::SelectedCameras))
	{
		return TRange<FFrameNumber>::Inclusive(0, 1);
	}

	if (QuickRenderModeSettings->FrameRangeType == EMovieGraphQuickRenderFrameRangeType::SelectionRange)
	{
		const int32 SelectionStartFrame = ULevelSequenceEditorBlueprintLibrary::GetSelectionRangeStart();
		const int32 SelectionEndFrame = ULevelSequenceEditorBlueprintLibrary::GetSelectionRangeEnd();

		if ((SelectionStartFrame != 0) || (SelectionEndFrame != 0))
		{
			StartFrame = ConvertSubSequenceFrameToRootFrame(SelectionStartFrame);
			EndFrame = ConvertSubSequenceFrameToRootFrame(SelectionEndFrame);
		}
	}
	else if (QuickRenderModeSettings->FrameRangeType == EMovieGraphQuickRenderFrameRangeType::Custom)
	{
		StartFrame = ConvertSubSequenceFrameToRootFrame(QuickRenderModeSettings->CustomStartFrame);
		EndFrame = ConvertSubSequenceFrameToRootFrame(QuickRenderModeSettings->CustomEndFrame);
	}
	else
	{
		// The frame range has not been overridden, so provide an empty range to indicate that the level sequence's normal
		// playback range should be used.
		return TRange<FFrameNumber>::Empty();
	}

	return TRange<FFrameNumber>::Inclusive(StartFrame, EndFrame);
}

FFrameNumber UMovieGraphQuickRenderSubsystem::ConvertSubSequenceFrameToRootFrame(const int32 FrameNum) const
{
	// The "current" level sequence is the root-most level sequence; the "focused" level sequence is the one that's currently visible in Sequencer
	// (and the level sequence that is having its frame number mapped back to the root)
	const ULevelSequence* CurrentLevelSequence = ULevelSequenceEditorBlueprintLibrary::GetCurrentLevelSequence();
	const ULevelSequence* FocusedLevelSequence = ULevelSequenceEditorBlueprintLibrary::GetFocusedLevelSequence();

	// There's nothing to convert if Sequencer is currently viewing the root-level sequence
	if (CurrentLevelSequence == FocusedLevelSequence)
	{
		return FrameNum;
	}

	// The hierarchy does not include the root, has the outermost subsequence at index 0, and the innermost subsequence at the end
	const TArray<UMovieSceneSubSection*> SubSequenceHierarchy = ULevelSequenceEditorBlueprintLibrary::GetSubSequenceHierarchy();
	if (SubSequenceHierarchy.IsEmpty())
	{
		// Shouldn't happen if the current and focused level sequences are different
		return FrameNum;
	}

	// Map the frame number into tick resolution; the OuterToInnerTransform() appears to need the frame time to be in this format
	const UMovieSceneSubSection* LastSubSection = SubSequenceHierarchy.Last();
	const FFrameRate SubSectionDisplayRate = LastSubSection->GetSequence()->GetMovieScene()->GetDisplayRate();
	const FFrameRate SubSectionTickResolution = LastSubSection->GetSequence()->GetMovieScene()->GetTickResolution();
	FFrameTime RootFrameTime = ConvertFrameTime(FrameNum, SubSectionDisplayRate, SubSectionTickResolution);

	// Walk the subsequence hierarchy in reverse, applying inverse transforms to get back to the outermost (root) frame time 
	for (int32 Index = SubSequenceHierarchy.Num() - 1; Index >= 0; --Index)
	{
		const UMovieSceneSubSection* SubSection = SubSequenceHierarchy[Index];
		if (!SubSection)
		{
			// GetSubSequenceHierarchy() can return nullptr for sections in some cases
			UE_LOG(LogMovieRenderPipeline, Error, TEXT("Quick Render: Found an invalid subsequence; rendered frame ranges may be incorrect."));
			continue;
		}

		if (TOptional<FFrameTime> TransformedTime = SubSection->OuterToInnerTransform().Inverse().TryTransformTime(RootFrameTime))
		{
			RootFrameTime = TransformedTime.GetValue();
		}
	}

	// Map the frame number back to the display rate
	const FFrameRate RootDisplayRate = CurrentLevelSequence->GetMovieScene()->GetDisplayRate();
	const FFrameRate RootTickResolution = CurrentLevelSequence->GetMovieScene()->GetTickResolution();
	RootFrameTime = ConvertFrameTime(RootFrameTime, RootTickResolution, RootDisplayRate);

	return RootFrameTime.GetFrame();
}

void UMovieGraphQuickRenderSubsystem::PerformPreRenderSetup(UWorld* InEditorWorld)
{
	// Adds the specified camera to the Camera Cut track in the given level sequence.
	// If there's already a camera cut section within the Camera Cut track, a new one will be added immediately after.
	// Each section is only one frame long.
	auto AddCameraToLevelSequence = [](ACameraActor* InCameraToAdd, const ULevelSequence* InDestLevelSequence, const TRange<FFrameNumber> OptionalFrameRangeOverride = TRange<FFrameNumber>::Empty())
	{
		FMovieGraphTransactionBlocker TransactionBlocker;
		
		UMovieScene* MovieScene = InDestLevelSequence->GetMovieScene();

		// Add a Camera Cut track if one hasn't already been added
		UMovieSceneCameraCutTrack* CameraCutTrack = Cast<UMovieSceneCameraCutTrack>(MovieScene->GetCameraCutTrack());
		if (!CameraCutTrack)
		{
			CameraCutTrack = CastChecked<UMovieSceneCameraCutTrack>(MovieScene->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass()));
		}

		const int32 NumCameraCutSections = CameraCutTrack->GetAllSections().Num();

		// Determine the playback range for the new camera cut. Each camera cut section has a playback range of one frame.
		const FFrameNumber StartFrame = MovieScene->GetPlaybackRange().GetLowerBound().GetValue();
		const FFrameNumber EndFrame = FFrameRate::TransformTime(FFrameTime(NumCameraCutSections + 1), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).GetFrame();

		// The entire playback range for the movie scene always starts a 0 and ends at the last camera cut section's end frame, unless there's an override.
		const bool bHasFrameRangeOverride = !OptionalFrameRangeOverride.IsEmpty();
		const TRange<FFrameNumber> PlaybackRange = bHasFrameRangeOverride
			? OptionalFrameRangeOverride
			: TRange<FFrameNumber>::Inclusive(FFrameNumber(0), EndFrame);
		MovieScene->SetPlaybackRange(PlaybackRange);
		
		UE::Sequencer::FCreateBindingParams CreateBindingParams;
		CreateBindingParams.bSpawnable = true;
		CreateBindingParams.bAllowCustomBinding = true;
		CreateBindingParams.BindingNameOverride = InCameraToAdd->GetActorLabel();

		// Create the binding for the camera
		const FGuid CameraBinding = FSequencerUtilities::CreateOrReplaceBinding(nullptr, MovieScene->GetTypedOuter<UMovieSceneSequence>(), InCameraToAdd, CreateBindingParams);

		const TRange<FFrameNumber> SectionRange = bHasFrameRangeOverride
			? OptionalFrameRangeOverride
			: TRange<FFrameNumber>::Inclusive(StartFrame, EndFrame);

		// Add the new camera to the camera cut track.
		UMovieSceneCameraCutSection* CameraCutSection = CameraCutTrack->AddNewCameraCut(UE::MovieScene::FRelativeObjectBindingID(CameraBinding), StartFrame);
		CameraCutSection->SetRange(SectionRange);
	};
	
	// Spawn a new camera which mimics the viewport's camera 
	auto DuplicateViewportCamera = [this]() -> ACameraActor*
	{
		// Get the PIE world, this is where we need to target the changes (not the editor world). Otherwise we'll have to deal with deleting actors
		// after the render finishes, which isn't ideal (and this will also create entries in the undo stack, also not ideal).
		UWorld* PieWorld = GetWorldOfType(EWorldType::PIE);
		
		FActorSpawnParameters CameraSpawnParams;
		CameraSpawnParams.ObjectFlags |= RF_Transient;
		CameraSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		// If the viewport is piloting a camera (and not another type of non-camera actor), we should just duplicate it. We can't just copy over
		// the basic transform + FOV, etc because this may be a cine camera with many properties set on it (like auto-exposure).
		// Note that this supports "cinematic locks" (like cameras piloted via Sequencer) and normal piloted cameras.
		if (CachedPrePieData.ViewportActorLock.IsValid() && IsValid(CachedPrePieData.ViewportActorLockCameraComponent))
		{
			AActor* ViewportPilotActor = CachedPrePieData.ViewportActorLock.Get();
			if (ViewportPilotActor->IsA<ACameraActor>())
			{
				// Use the current pilot cam as the template
				CameraSpawnParams.Template = ViewportPilotActor;

				return Cast<ACameraActor>(PieWorld->SpawnActor(ViewportPilotActor->GetClass(), nullptr, CameraSpawnParams));
			}
		}
		
		// Add a new camera that mimics the viewport's camera to the level sequence. Use a regular camera actor here rather than a cine camera. Using
		// a cine camera massively complicates things and creates a situation where it's difficult to match what the viewport looks like (eg, exposure).
		ACameraActor* NewCamera = Cast<ACameraActor>(PieWorld->SpawnActor<ACameraActor>(CameraSpawnParams));

		// Update the camera to look like the viewport's camera
		if (const FLevelEditorViewportClient* ViewportClient = UMovieGraphApplyViewportLookNode::GetViewportClient())
		{
			// Initialize the camera properties to reflect the (non-pilot) view shown in the viewport 
			FVector CameraPosition = ViewportClient->GetViewLocation();
			FRotator CameraRotation = ViewportClient->GetViewRotation();
			bool bIsPerspective = ViewportClient->IsPerspective();
			float CameraFov = ViewportClient->ViewFOV;
			float CameraAspectRatio = ViewportClient->AspectRatio;
			float CameraOrthoWidth = ViewportClient->Viewport->GetSizeXY().X * ViewportClient->GetOrthoUnitsPerPixel(ViewportClient->Viewport);
			
			// If the viewport is being piloted, then we should use the properties of that to render from instead of the viewport's
			// non-piloted camera properties. Note that this only covers non-camera pilot actors; camera pilot actors are covered above.
			if (CachedPrePieData.ViewportActorLock.IsValid())
			{
				const TStrongObjectPtr<AActor> LockedActorPin = CachedPrePieData.ViewportActorLock.Pin();
				CameraPosition = LockedActorPin->GetActorLocation();
				CameraRotation = LockedActorPin->GetActorRotation();
			}
			else
			{
				// The camera properties were initialized from the viewport camera properties, so all we need to do here is take into account the
				// special case of the viewport showing an axis-aligned ortho cam. Getting the actor rotation from the viewport client for these
				// ortho cams does not work properly, so their rotation is manually specified here. Sharing this logic with the viewport would involve
				// some fairly significant changes, so for now MRG calculates this separately.
				if (!bIsPerspective)
				{
					switch (ViewportClient->ViewportType)
					{
					case LVT_OrthoTop:
						CameraRotation = FRotator(-90.0f, -180.0f, 0.0f);
						break;
					case LVT_OrthoBottom:
						CameraRotation = FRotator(90.0f, 0.0f, 0.0f);
						break;
					case LVT_OrthoLeft:
						CameraRotation = FRotator(0.0f, -90.0f, 0.0f);
						break;
					case LVT_OrthoRight:
						CameraRotation = FRotator(0.0f, 90.0f, 0.0f);
						break;
					case LVT_OrthoBack:
						CameraRotation = FRotator(0.0f, 0.0f, 0.0f);
						break;
					case LVT_OrthoFront:
						CameraRotation = FRotator(0.0f, 180.0f, 0.0f);
						break;
					default:
						CameraRotation = FRotator(0.0f, 0.0f, 0.0f);
					}
				}
			}

			NewCamera->SetActorLocation(CameraPosition);
			NewCamera->SetActorRotation(CameraRotation);

			// Note: At extreme zoom levels, the ortho clipping planes don't match up exactly with the viewport. This is something to improve in the future.
			UCameraComponent* CameraComponent = NewCamera->GetCameraComponent();
			CameraComponent->SetProjectionMode(bIsPerspective ? ECameraProjectionMode::Perspective : ECameraProjectionMode::Orthographic);
			CameraComponent->SetAspectRatio(CameraAspectRatio);
			CameraComponent->SetFieldOfView(CameraFov);
			CameraComponent->SetOrthoWidth(CameraOrthoWidth);
			CameraComponent->SetAutoCalculateOrthoPlanes(true);
		}
		else
		{
			// This case should be exceedingly rare, but log about it anyway
			UE_LOG(LogMovieRenderPipeline, Error, TEXT("Quick Render: Could not determine the active viewport to render from, so the camera used will be incorrect."));
		}

		return NewCamera;
	};
	
	// Adds a Level Visibility track to the Utility subsequence to ensure that the visibility state of levels in the editor is reflected
	// in the render/PIE. Blueprint streamable levels, for example, may be shown in the editor, but will not typically show up in the
	// render unless made visible by code or in Sequencer.
	auto AddLevelVisibilityTracks = [this, InEditorWorld]() -> void
	{
		TArray<FName> VisibleLevelNames;
		TArray<FName> HiddenLevelNames;

		// Determine which levels are currently visible/hidden
		for (const ULevelStreaming* Level : InEditorWorld->GetStreamingLevels())
		{
			if (!Level)
			{
				continue;
			}
	
			const FName LevelName = FPackageName::GetShortFName(Level->GetWorldAssetPackageFName());
			if (FLevelUtils::IsStreamingLevelVisibleInEditor(Level))
			{
				VisibleLevelNames.Add(LevelName);
			}
			else
			{
				HiddenLevelNames.Add(LevelName);
			}
		}

		UMovieScene* UtilityMovieScene = UtilityLevelSequence->GetMovieScene();

		// Add a track to show visible levels
		UMovieSceneLevelVisibilityTrack* LevelVisibilityTrack_Visible = CastChecked<UMovieSceneLevelVisibilityTrack>(UtilityMovieScene->AddTrack(UMovieSceneLevelVisibilityTrack::StaticClass()));
		UMovieSceneLevelVisibilitySection* LevelVisibilitySection_Visible = CastChecked<UMovieSceneLevelVisibilitySection>(LevelVisibilityTrack_Visible->CreateNewSection());
		LevelVisibilityTrack_Visible->AddSection(*LevelVisibilitySection_Visible);
		LevelVisibilitySection_Visible->SetVisibility(ELevelVisibility::Visible);
		LevelVisibilitySection_Visible->SetRange(UtilityMovieScene->GetPlaybackRange());
		LevelVisibilitySection_Visible->SetLevelNames(VisibleLevelNames);

		// Add a track to hide hidden levels
		UMovieSceneLevelVisibilityTrack* LevelVisibilityTrack_Hidden = CastChecked<UMovieSceneLevelVisibilityTrack>(UtilityMovieScene->AddTrack(UMovieSceneLevelVisibilityTrack::StaticClass()));
		UMovieSceneLevelVisibilitySection* LevelVisibilitySection_Hidden = CastChecked<UMovieSceneLevelVisibilitySection>(LevelVisibilityTrack_Hidden->CreateNewSection());
		LevelVisibilityTrack_Hidden->AddSection(*LevelVisibilitySection_Hidden);
		LevelVisibilitySection_Hidden->SetVisibility(ELevelVisibility::Hidden);
		LevelVisibilitySection_Hidden->SetRange(UtilityMovieScene->GetPlaybackRange());
		LevelVisibilitySection_Hidden->SetLevelNames(HiddenLevelNames);
	};

	UMovieScene* RenderingMovieScene = RenderingLevelSequence->GetMovieScene();

	// Always add the Utility sequence. It's used for various things in different modes. Set the hierarchical bias of the new subsequence section so
	// its effects take precedence over the level sequence it's being added to. Use a very high number; the value used here is arbitrary, but should
	// be high enough to do the job. Note that the subsequence section is added with 0 duration, its length will be determined later.
	UMovieSceneSubTrack* NewSubsequenceTrack = CastChecked<UMovieSceneSubTrack>(RenderingMovieScene->AddTrack(UMovieSceneSubTrack::StaticClass()));
	UMovieSceneSubSection* SequenceSubsection = NewSubsequenceTrack->AddSequence(UtilityLevelSequence.Get(), 0, 0);
	SequenceSubsection->Parameters.HierarchicalBias = 1000000;

	// Track the frame range that should be used for rendering. By default this is the rendering sequence's playback range, but for many modes which
	// generate a dynamic level sequence for rendering (and the rendering sequence starts as empty), this will need to be adjusted.
	TRange<FFrameNumber> RenderingFrameRange = RenderingMovieScene->GetPlaybackRange();

	// Do mode-specific setup
	if (QuickRenderMode == EMovieGraphQuickRenderMode::CurrentViewport)
	{
		// Adding a camera will tweak the length of the utility sequence's playback range
		ACameraActor* NewCamera = DuplicateViewportCamera();
		AddCameraToLevelSequence(NewCamera, UtilityLevelSequence.Get());

		RenderingFrameRange = UtilityLevelSequence->GetMovieScene()->GetPlaybackRange();
	}
	else if (QuickRenderMode == EMovieGraphQuickRenderMode::UseViewportCameraInSequence)
	{
		// Add the viewport camera to the utility sequence to override any other cameras in use
		ACameraActor* NewCamera = DuplicateViewportCamera();
		AddCameraToLevelSequence(NewCamera, UtilityLevelSequence.Get(), RenderingMovieScene->GetPlaybackRange());
	}
	else if (QuickRenderMode == EMovieGraphQuickRenderMode::SelectedCameras)
	{
		for (const TWeakObjectPtr<ACameraActor>& SelectedCamera : CachedPrePieData.SelectedCameras)
		{
			if (SelectedCamera.IsValid())
			{
				// Adding a camera will tweak the length of the utility sequence's playback range
				AddCameraToLevelSequence(SelectedCamera.Get(), UtilityLevelSequence.Get());

				RenderingFrameRange = UtilityLevelSequence->GetMovieScene()->GetPlaybackRange();
			}
		}
	}

	// Now that cameras have been added and the length of the render has been established, make sure the playback ranges are correct
	RenderingLevelSequence->GetMovieScene()->SetPlaybackRange(RenderingFrameRange);
	UtilityLevelSequence->GetMovieScene()->SetPlaybackRange(RenderingFrameRange);
	SequenceSubsection->SetRange(RenderingFrameRange);

	// Add the Level Visibility tracks to the Utility sequence if matching the editor visibility was requested
	const EMovieGraphQuickRenderViewportLookFlags LookFlags = static_cast<EMovieGraphQuickRenderViewportLookFlags>(QuickRenderModeSettings->ViewportLookFlags);
	if (IsViewportLookFlagActive(EMovieGraphQuickRenderViewportLookFlags::Visibility))
	{
		AddLevelVisibilityTracks();
	}
}

void UMovieGraphQuickRenderSubsystem::OpenPostRenderFileDisplayProcessor(const FMoviePipelineOutputData& InOutputData) const
{
	FMoviePipelinePostRenderFileDisplayProcessor PostRenderFileDisplayProcessor(GetDefault<UMovieRenderGraphEditorSettings>()->PostRenderSettings);
	PostRenderFileDisplayProcessor.AddFiles(InOutputData);
	PostRenderFileDisplayProcessor.OpenFiles();
}

void UMovieGraphQuickRenderSubsystem::HandleJobFinished(const UMovieGraphQuickRenderModeSettings* InQuickRenderSettings, const FMoviePipelineOutputData& InGeneratedOutputData)
{
	if (InQuickRenderSettings->PostRenderBehavior == EMovieGraphQuickRenderPostRenderActionType::PlayRenderOutput)
	{
		OpenPostRenderFileDisplayProcessor(InGeneratedOutputData);
	}
	else if (InQuickRenderSettings->PostRenderBehavior == EMovieGraphQuickRenderPostRenderActionType::OpenOutputDirectory)
	{
		OpenOutputDirectory(InQuickRenderSettings);
	}

	PreviousRenderOutputData = InGeneratedOutputData;
}

bool UMovieGraphQuickRenderSubsystem::IsViewportLookFlagActive(const EMovieGraphQuickRenderViewportLookFlags ViewportLookFlag) const
{
	if (!IsValid(QuickRenderModeSettings))
	{
		return false;
	}
	
	const EMovieGraphQuickRenderViewportLookFlags LookFlags = static_cast<EMovieGraphQuickRenderViewportLookFlags>(QuickRenderModeSettings->ViewportLookFlags);
	
	return QuickRenderModeSettings->bOverride_ViewportLookFlags && EnumHasAnyFlags(LookFlags, ViewportLookFlag);
}

#undef LOCTEXT_NAMESPACE
