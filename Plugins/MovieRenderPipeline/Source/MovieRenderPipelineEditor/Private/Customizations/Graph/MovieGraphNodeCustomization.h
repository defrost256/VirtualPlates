// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "IDetailCustomization.h"
#include "Graph/MovieGraphConfig.h"
#include "PropertyHandle.h"

#define LOCTEXT_NAMESPACE "MoviePipelineEditor"

/** Customize how graph nodes appear in the details panel. */
class FMovieGraphNodeCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShared<FMovieGraphNodeCustomization>();
	}

protected:
	//~ Begin IDetailCustomization interface
	virtual void CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder) override
	{
		CustomizeDetails(*DetailBuilder);
	}

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
	{
		TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
		DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);

		// Hide the "Properties" category, which houses the dynamic properties, if there are no dynamic properties
		// on the node. An empty category in the details panel is messy/confusing.
		for (const TWeakObjectPtr<UObject>& CustomizedObject : ObjectsBeingCustomized)
		{
			if (const UMovieGraphNode* Node = Cast<UMovieGraphNode>(CustomizedObject))
			{
				if (Node->GetDynamicPropertyDescriptions().IsEmpty())
				{
					DetailBuilder.HideCategory("Properties");
					break;
				}
			}
		}

		// Also hide the "Properties" category if more than one object is selected. There needs to be some more work done
		// in property bags to be able to handle this condition without crashing in many cases.
		if (ObjectsBeingCustomized.Num() > 1)
		{
			DetailBuilder.HideCategory("Properties");
		}

		// Collapse the Tags category by default because it's not used by most users. Set the sort order to make Tags appear last in the list of
		// categories (1000 * Uncommon == starting index for all categories with "Uncommon" sort order, and the +1000 is to move the category past all
		// other categories with "Uncommon" sort order).
		DetailBuilder.EditCategory("Tags").InitiallyCollapsed(true).SetSortOrder(1000 * ECategoryPriority::Uncommon + 1000);

		// The File Output category should be shown first in all output nodes that have this category.
		DetailBuilder.EditCategory("File Output").SetSortOrder(ECategoryPriority::Important);
	}
	//~ End IDetailCustomization interface
};

#undef LOCTEXT_NAMESPACE