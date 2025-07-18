// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Graph/MovieGraphCustomizationUtils.h"
#include "IDetailCustomization.h"
#include "IDetailGroup.h"
#include "MoviePipelineQueue.h"
#include "PropertyHandle.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "MoviePipelineEditor"

/** Customize how properties for a job appear in the details panel. */
class FJobDetailsCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShared<FJobDetailsCustomization>();
	}

	virtual ~FJobDetailsCustomization() override
	{
	}

	virtual void PendingDelete() override
	{
		// Unregister delegates. It's important to do this in PendingDelete() vs the destructor because the destructor is not called before the next
		// details panel is created (via ForceRefreshDetails()), leading to an exponential increase in the number of delegates registered.
		
		UPackage::PackageSavedWithContextEvent.RemoveAll(this);

		if (SelectedJob.IsValid())
		{
			SelectedJob->OnJobGraphPresetChanged.RemoveAll(this);
		}
		
		if (SelectedShot.IsValid())
		{
			SelectedShot->OnShotGraphPresetChanged.RemoveAll(this);
		}
	}

protected:
	//~ Begin IDetailCustomization interface
	virtual void CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& InDetailBuilder) override
	{
		DetailBuilder = InDetailBuilder.Get();
		CustomizeDetails(*InDetailBuilder);
	}

	void RefreshLayout(const FString&, UPackage*, FObjectPostSaveContext) const
	{
		DetailBuilder->ForceRefreshDetails();
	}

	void RefreshLayout(UMoviePipelineExecutorShot*, UMovieGraphConfig*) const
	{
		DetailBuilder->ForceRefreshDetails();
	}

	void RefreshLayout(UMoviePipelineExecutorJob*, UMovieGraphConfig*) const
	{
		DetailBuilder->ForceRefreshDetails();
	}

	virtual void CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder) override
	{
		// Refresh the customization every time a save happens. Use this opportunity to update the variables in the UI. We could update the UI before
		// a save occurs, but this would be very difficult to get right when multiple subgraphs are involved.
		UPackage::PackageSavedWithContextEvent.AddSP(this, &FJobDetailsCustomization::RefreshLayout);
		
		TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
		InDetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);

		TArray<UMoviePipelineExecutorJob*> SelectedJobs;
		TArray<UMoviePipelineExecutorShot*> SelectedShots;

		for (const TWeakObjectPtr<UObject>& SelectedObject : ObjectsBeingCustomized)
		{
			if (UMoviePipelineExecutorJob* SelectedJobTemp = Cast<UMoviePipelineExecutorJob>(SelectedObject.Get()))
			{
				SelectedJobs.Add(SelectedJobTemp);
			}
			else if (UMoviePipelineExecutorShot* SelectedShotTemp = Cast<UMoviePipelineExecutorShot>(SelectedObject.Get()))
			{
				SelectedShots.Add(SelectedShotTemp);
			}
		}

		// Hide the original assignments properties (since they present an asset picker by default) for both jobs and shots
		const TSharedRef<IPropertyHandle> JobAssignmentsProperty =
			InDetailBuilder.GetProperty(TEXT("GraphVariableAssignments"), UMoviePipelineExecutorJob::StaticClass());
		const TSharedRef<IPropertyHandle> ShotAssignmentsProperty =
			InDetailBuilder.GetProperty(TEXT("GraphVariableAssignments"), UMoviePipelineExecutorShot::StaticClass());
		const TSharedRef<IPropertyHandle> ShotPrimaryGraphAssignmentsProperty =
			InDetailBuilder.GetProperty(TEXT("PrimaryGraphVariableAssignments"), UMoviePipelineExecutorShot::StaticClass());
		InDetailBuilder.HideProperty(JobAssignmentsProperty);
		InDetailBuilder.HideProperty(ShotAssignmentsProperty);
		InDetailBuilder.HideProperty(ShotPrimaryGraphAssignmentsProperty);

		// Only display the customized variables UI if there is one job or shot selected
		const bool bIsPrimaryJob = (SelectedJobs.Num() == 1) && SelectedShots.IsEmpty();
		const bool bIsShot = (SelectedShots.Num() == 1) && SelectedJobs.IsEmpty();
		if (!bIsPrimaryJob && !bIsShot)
		{
			return;
		}

		// Refresh the UI if the graph preset changes (so the new variable assignments are displayed)
		if (bIsShot)
		{
			SelectedShot = SelectedShots[0];
			SelectedShot->OnShotGraphPresetChanged.AddSP(this, &FJobDetailsCustomization::RefreshLayout);

			// Also listen for changes to the primary job. Changes to the primary job can trigger an update to shot variable assignments.
			if (UMoviePipelineExecutorJob* PrimaryJob = SelectedShot->GetTypedOuter<UMoviePipelineExecutorJob>())
			{
				SelectedJob = PrimaryJob;
				SelectedJob->OnJobGraphPresetChanged.AddSP(this, &FJobDetailsCustomization::RefreshLayout);
			}
		}
		else
		{
			SelectedJob = SelectedJobs[0];
			SelectedJob->OnJobGraphPresetChanged.AddSP(this, &FJobDetailsCustomization::RefreshLayout);
		}

		// Set up the categories for variable assignments.
		IDetailCategoryBuilder& PrimaryGraphVariablesCategory = InDetailBuilder.EditCategory(
			"PrimaryGraphVariables", LOCTEXT("PrimaryGraphVariablesCategory", "Primary Graph Variables"));
		IDetailCategoryBuilder& PrimaryGraphVariablesShotOverridesCategory = InDetailBuilder.EditCategory(
			"PrimaryGraphVariablesShotOverrides", LOCTEXT("PrimaryGraphVariablesShotOverridesCategory", "Primary Graph Variables (shot overrides)"));
		IDetailCategoryBuilder& ShotGraphVariablesCategory = InDetailBuilder.EditCategory(
			"ShotGraphVariables", LOCTEXT("ShotGraphVariablesCategory", "Shot Graph Variables"));

		// Set all categories as hidden by default. Individual categories will be made visible if variables are added under them.
		PrimaryGraphVariablesCategory.SetCategoryVisibility(false);
		PrimaryGraphVariablesShotOverridesCategory.SetCategoryVisibility(false);
		ShotGraphVariablesCategory.SetCategoryVisibility(false);
		
		if (bIsShot)
		{
			UE::MovieRenderPipelineEditor::Private::AddVariableAssignments(SelectedShot->GetGraphVariableAssignments(), ShotGraphVariablesCategory, DetailBuilder);
			UE::MovieRenderPipelineEditor::Private::AddVariableAssignments(SelectedShot->GetPrimaryGraphVariableAssignments(), PrimaryGraphVariablesShotOverridesCategory, DetailBuilder);
		}
		else
		{
			UE::MovieRenderPipelineEditor::Private::AddVariableAssignments(SelectedJob->GetGraphVariableAssignments(), PrimaryGraphVariablesCategory, DetailBuilder);
		}

		IDetailCategoryBuilder& MovieRenderPipelineCategory = InDetailBuilder.EditCategory("Movie Render Pipeline");

		// The Console Variable Overrides category should be collapsed by default. It's considered advanced, and most people won't need to use it.
		IDetailCategoryBuilder& ConsoleVariableOverridesCategory = InDetailBuilder.EditCategory("Console Variable Overrides");
		ConsoleVariableOverridesCategory.InitiallyCollapsed(true);

		// Give the categories a specific ordering.
		int32 SortOrder = 0;
		MovieRenderPipelineCategory.SetSortOrder(SortOrder);
		PrimaryGraphVariablesCategory.SetSortOrder(++SortOrder);
		PrimaryGraphVariablesShotOverridesCategory.SetSortOrder(++SortOrder);
		ShotGraphVariablesCategory.SetSortOrder(++SortOrder);
		ConsoleVariableOverridesCategory.SetSortOrder(++SortOrder);
	}
	//~ End IDetailCustomization interface

private:
	/** The details builder associated with the customization. */
	IDetailLayoutBuilder* DetailBuilder = nullptr;

	/** The primary job that's selected in the UI. There should always be a selected primary job. */
	TWeakObjectPtr<UMoviePipelineExecutorJob> SelectedJob;

	/** The shot that's selected in the UI (may be null if only a primary is selected). */
	TWeakObjectPtr<UMoviePipelineExecutorShot> SelectedShot;
};

#undef LOCTEXT_NAMESPACE