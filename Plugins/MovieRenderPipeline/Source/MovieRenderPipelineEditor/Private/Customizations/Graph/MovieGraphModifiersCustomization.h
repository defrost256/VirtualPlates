// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "Graph/MovieGraphSharedWidgets.h"

class UMovieGraphCollectionNode;
class UMovieGraphModifierNode;

/** Customize how the Modifier node appears in the details panel. */
class FMovieGraphModifiersCustomization final : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

protected:
	//~ Begin IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder) override;
	virtual void CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& InDetailBuilder) override;
	//~ End IDetailCustomization interface

private:
	static const FSlateBrush* GetCollectionRowIcon(const FName CollectionName);
	static FText GetCollectionRowText(const FName CollectionName);

	/** Gets the selected modifier node that this customization is displaying. */
	TWeakObjectPtr<UMovieGraphModifierNode> GetSelectedModifierNode() const;
	
	/** Refreshes the list's data source to reflect the data model. */
	void RefreshListDataSource();

private:
	/** The details builder associated with the customization. */
	TWeakPtr<IDetailLayoutBuilder> DetailBuilder;

	/** The data source for the list view. Modifiers don't expose a direct pointer to the array of collections, which is required by the list view. */
	TArray<FName> ListDataSource;

	/** Displays the collections which have been chosen. */
	TSharedPtr<SMovieGraphSimpleList<FName>> CollectionsList;
};