// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieGraphCustomizationUtils.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Graph/MovieGraphConfig.h"
#include "IDetailGroup.h"
#include "MovieJobVariableAssignmentContainer.h"

namespace UE::MovieRenderPipelineEditor::Private
{
	void AddVariableAssignments(TArray<TObjectPtr<UMovieJobVariableAssignmentContainer>>& InVariableAssignments, IDetailCategoryBuilder& InCategory, IDetailLayoutBuilder* InDetailBuilder)
	{
		// Add a sub-category for each graph (including subgraphs). Each entry in the array represents the assignments for one graph.
		for (TObjectPtr<UMovieJobVariableAssignmentContainer>& VariableAssignment : InVariableAssignments)
		{
			// Skip if the graph associated with this container has no variables in it
			if (VariableAssignment->GetNumAssignments() <= 0)
			{
				continue;
			}

			// If the graph can be found, display its variable assignments under its own category (group)
			TSoftObjectPtr<UMovieGraphConfig> SoftGraphConfig = VariableAssignment->GetGraphConfig();
			if (const UMovieGraphConfig* GraphConfig = SoftGraphConfig.Get())
			{
				constexpr bool bForAdvanced = false;
				constexpr bool bStartExpanded = true;
				IDetailGroup& GraphGroup = InCategory.AddGroup(GraphConfig->GetFName(), FText::FromString(GraphConfig->GetName()), bForAdvanced, bStartExpanded);

				// "Value" is private so we can't use GET_MEMBER_NAME_CHECKED unfortunately
				TSharedPtr<IPropertyHandle> ValueProperty = InDetailBuilder->AddObjectPropertyData({VariableAssignment}, FName("Value"));
				GraphGroup.AddPropertyRow(ValueProperty.ToSharedRef());

				// Un-hide the category if it's currently visible
				InCategory.SetCategoryVisibility(true);
			}
		}
	}
}
