// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieRenderPipelineEditorUtils.h"

#include "Editor.h"
#include "MoviePipelineQueueSubsystem.h"
#include "MoviePipelineTelemetry.h"
#include "MovieRenderPipelineSettings.h"

namespace UE::MovieRenderPipelineEditor::Private
{
	bool PerformLocalRender()
	{
		if (!CanPerformLocalRender())
		{
			return false;
		}
		
		UMoviePipelineQueueSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>();
		check(Subsystem);

		const UMovieRenderPipelineProjectSettings* ProjectSettings = GetDefault<UMovieRenderPipelineProjectSettings>();
		
		const TSubclassOf<UMoviePipelineExecutorBase> ExecutorClass = ProjectSettings->DefaultLocalExecutor.TryLoadClass<UMoviePipelineExecutorBase>();
		check(ExecutorClass != nullptr);
		
		Subsystem->RenderQueueWithExecutor(ExecutorClass);

		constexpr bool bIsLocal = true;
		FMoviePipelineTelemetry::SendRendersRequestedTelemetry(bIsLocal, Subsystem->GetQueue()->GetJobs());

		return true;
	}

	bool CanPerformLocalRender()
	{
		const UMoviePipelineQueueSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>();
		check(Subsystem);

		const UMovieRenderPipelineProjectSettings* ProjectSettings = GetDefault<UMovieRenderPipelineProjectSettings>();
		const bool bHasExecutor = ProjectSettings->DefaultLocalExecutor.TryLoadClass<UMoviePipelineExecutorBase>() != nullptr;
		const bool bNotRendering = !Subsystem->IsRendering();

		bool bAtLeastOneJobAvailable = false;
		for (const UMoviePipelineExecutorJob* Job : Subsystem->GetQueue()->GetJobs())
		{
			if (!Job->IsConsumed() && Job->IsEnabled())
			{
				bAtLeastOneJobAvailable = true;
				break;
			}
		}

		const bool bWorldIsActive = GEditor->IsPlaySessionInProgress();
		
		return bHasExecutor && bNotRendering && bAtLeastOneJobAvailable && !bWorldIsActive;
	}
}
