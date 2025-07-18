// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieGraphModifiersCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Graph/MovieGraphConfig.h"
#include "Graph/MovieGraphSharedWidgets.h"
#include "Graph/MovieGraphRenderLayerSubsystem.h"
#include "Graph/Nodes/MovieGraphCollectionNode.h"
#include "Graph/Nodes/MovieGraphModifierNode.h"
#include "ScopedTransaction.h"
#include "Widgets/Images/SLayeredImage.h"

#define LOCTEXT_NAMESPACE "MovieGraphModifiersCustomization"

/** Discovers collections that are pickable from a specific graph, and presents them in a list. */
class SMovieGraphCollectionPicker final : public SMovieGraphSimplePicker<FName>
{
public:
	DECLARE_DELEGATE_OneParam(FOnCollectionPicked, FName);
	DECLARE_DELEGATE_RetVal_OneParam(bool, FOnFilter, FName);

	SLATE_BEGIN_ARGS(SMovieGraphCollectionPicker)
		{}
		/** The graph to begin discovering collections from. */
		SLATE_ATTRIBUTE(UMovieGraphConfig*, Graph)

		/** Called when a collection is picked in the list. */
		SLATE_EVENT(FOnCollectionPicked, OnCollectionPicked);

		/** Optional filter that can prevent discovered collections from showing up in the list. */
		SLATE_EVENT(FOnFilter, OnFilter);
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		CurrentGraph = InArgs._Graph.Get();
		OnCollectionPicked = InArgs._OnCollectionPicked;
		OnFilter = InArgs._OnFilter;

		SMovieGraphSimplePicker::Construct(SMovieGraphSimplePicker::FArguments()
			.OnGetRowText_Lambda([](FName InCollectionName) { return FText::FromName(InCollectionName); })
			.Title(LOCTEXT("PickCollectionHelpText", "Pick a Collection"))
			.DataSourceEmptyMessage(LOCTEXT("NoCollectionsFoundWarning", "No collections found."))
			.OnItemPicked(OnCollectionPicked));

		// Update the data source *after* calling Construct() above because Construct() will populate DataSource based on the widget arguments, but
		// we want to control/update it manually.
		UpdateDataSource();
	}

private:
	/** Discovers all available collections, and refreshes the data the list is displaying. */
	TArray<UMovieGraphCollectionNode*> UpdateDataSource()
	{
		TArray<UMovieGraphCollectionNode*> CollectionNodes;
		
		if (!CurrentGraph)
		{
			return CollectionNodes;
		}

		TSet<UMovieGraphConfig*> Subgraphs;
		CurrentGraph->GetAllContainedSubgraphs(Subgraphs);

		TArray<UMovieGraphConfig*> AllGraphs = { CurrentGraph };
		for (UMovieGraphConfig* Subgraph : Subgraphs)
		{
			AllGraphs.Add(Subgraph);
		}

		for (const UMovieGraphConfig* Graph : AllGraphs)
		{
			for (const TObjectPtr<UMovieGraphNode>& Node : Graph->GetNodes())
			{
				if (const TObjectPtr<UMovieGraphCollectionNode> CollectionNode = Cast<UMovieGraphCollectionNode>(Node))
				{
					const FName CollectionName = FName(CollectionNode->Collection->GetCollectionName());
					bool bIncludeCollection = true;

					if (OnFilter.IsBound())
					{
						bIncludeCollection = OnFilter.Execute(CollectionName);
					}

					if (bIncludeCollection)
					{
						DataSource.AddUnique(CollectionName);
					}
				}
			}
		}

		return CollectionNodes;
	}

private:
	/** The current graph being viewed. The data source will be populated from this graph. */
	UMovieGraphConfig* CurrentGraph = nullptr;
	
	FOnCollectionPicked OnCollectionPicked;
	FOnFilter OnFilter;
};

TSharedRef<IDetailCustomization> FMovieGraphModifiersCustomization::MakeInstance()
{
	return MakeShared<FMovieGraphModifiersCustomization>();
}

void FMovieGraphModifiersCustomization::CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder)
{
	// The customization only supports editing a single Modifier node
	const TWeakObjectPtr<UMovieGraphModifierNode> ModifierNode = GetSelectedModifierNode();
	if (!ModifierNode.IsValid())
	{
		return;
	}

	// Update the data source
	RefreshListDataSource();
	
	// Generate a (multi-layered) icon for the "Add" menu
	const TSharedRef<SLayeredImage> AddIcon =
		SNew(SLayeredImage)
		.ColorAndOpacity(FSlateColor::UseForeground())
		.Image(FAppStyle::GetBrush("LevelEditor.OpenAddContent.Background"));
	AddIcon->AddLayer(FAppStyle::GetBrush("LevelEditor.OpenAddContent.Overlay"));

	// Replace the "Collection" category row with a custom whole-row widget which includes an add-collection button
	IDetailCategoryBuilder& CollectionCategory = InDetailBuilder.EditCategory(FName("Collection"), FText::GetEmpty(), ECategoryPriority::Uncommon);
	CollectionCategory.HeaderContent
	(
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(FText::FromString("Collections"))
			.Font(FAppStyle::Get().GetFontStyle("DetailsView.CategoryFontStyle"))
		]

		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(5.f, 0, 0, 0)
		[
			SNew(SComboButton)
			.ToolTipText(LOCTEXT("AddCollectionToModifierTooltip", "Add a collection that will be affected by the configured modifiers."))
			.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
			.ContentPadding(0)
			.HasDownArrow(false)
			.OnGetMenuContent_Lambda([ModifierNode, this]()
			{
				return
					SNew(SMovieGraphCollectionPicker)
					.Graph(ModifierNode->GetTypedOuter<UMovieGraphConfig>())
					.OnFilter_Lambda([ModifierNode](const FName CollectionName)
					{
						if (ModifierNode.IsValid())
						{
							return !ModifierNode.Get()->GetCollections().Contains(CollectionName);
						}

						return false;
					})
					.OnCollectionPicked_Lambda([ModifierNode, this](const FName PickedCollectionName)
					{
						if (ModifierNode.IsValid())
						{
							const FScopedTransaction Transaction(LOCTEXT("AddCollectionToModifier", "Add Collection to Modifier"));
							
							ModifierNode->AddCollection(PickedCollectionName);
							ListDataSource = ModifierNode->GetCollections();
							CollectionsList->Refresh();
						}
					});
			})
			.ButtonContent()
			[
				SNew(SBox)
				.WidthOverride(16)
				.HeightOverride(16)
				[
					AddIcon
				]
			]
		]
	, /* bWholeRowContent */ true);

	// Add a collections browser
	CollectionCategory.AddCustomRow(FText::GetEmpty())
	.WholeRowWidget
	[
		SAssignNew(CollectionsList, SMovieGraphSimpleList<FName>)
		.DataSource(&ListDataSource)
		.DataType(FText::FromString("Collection"))
		.DataTypePlural(FText::FromString("Collections"))
		.OnDelete_Lambda([this, ModifierNode](const TArray<FName> DeletedCollectionNames)
		{
			if (ModifierNode.IsValid())
			{
				const FScopedTransaction Transaction(LOCTEXT("RemoveCollectionsFromModifier", "Remove Collections from Modifier"));

				for (const FName& DeletedCollectionName : DeletedCollectionNames)
				{
					ModifierNode.Get()->RemoveCollection(DeletedCollectionName);
				}
				
				ListDataSource = ModifierNode->GetCollections();
				CollectionsList->Refresh();
			}
		})
		.OnGetRowText_Static(&GetCollectionRowText)
		.ShowEnableDisable(true)
		.OnGetRowEnableState_Lambda([ModifierNode](FName InCollectionName)
		{
			return ModifierNode.IsValid() ? ModifierNode.Get()->IsCollectionEnabled(InCollectionName) : true;
		})
		.OnSetRowEnableState_Lambda([ModifierNode](FName InCollectionName, bool bNewEnableState)
		{
			if (ModifierNode.IsValid())
			{
				const FScopedTransaction Transaction(LOCTEXT("ChangeCollectionEnableState", "Change Collection Enable State"));
				
				ModifierNode.Get()->SetCollectionEnabled(InCollectionName, bNewEnableState);
			}
		})
		.OnRefreshDataSourceRequested(this, &FMovieGraphModifiersCustomization::RefreshListDataSource)
	];

	// For all modifiers added to the node, add a category for each, and add each modifier's EditAnywhere properties to the category
	for (UMovieGraphCollectionModifier* Modifier : ModifierNode->GetModifiers())
	{
		if (!Modifier)
		{
			continue;
		}
		
		const UClass* ModifierClass = Modifier->GetClass();
		const FText DisplayName = ModifierClass->GetDisplayNameText();

		// Add category as "Uncommon" to display after the general modifier properties
		IDetailCategoryBuilder& Category = InDetailBuilder.EditCategory(FName(DisplayName.ToString()), DisplayName, ECategoryPriority::Uncommon);

		for (TFieldIterator<FProperty> PropertyIterator(ModifierClass); PropertyIterator; ++PropertyIterator)
		{
			// Add any EditAnywhere properties, but skip the bOverride_* properties.
			const FProperty* ModifierProperty = *PropertyIterator;
			if (ModifierProperty && ModifierProperty->HasAnyPropertyFlags(CPF_Edit) && !ModifierProperty->HasMetaData(TEXT("InlineEditConditionToggle")))
			{
				Category.AddExternalObjectProperty({Modifier}, ModifierProperty->GetFName());
			}
		}
	}
}

void FMovieGraphModifiersCustomization::CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& InDetailBuilder)
{
	DetailBuilder = InDetailBuilder;
	CustomizeDetails(*InDetailBuilder);
}

const FSlateBrush* FMovieGraphModifiersCustomization::GetCollectionRowIcon(const FName CollectionName)
{
	return FAppStyle::GetBrush("Icons.FilledCircle");
}

FText FMovieGraphModifiersCustomization::GetCollectionRowText(const FName CollectionName)
{
	return FText::FromName(CollectionName);
}

TWeakObjectPtr<UMovieGraphModifierNode> FMovieGraphModifiersCustomization::GetSelectedModifierNode() const
{
	if (const TSharedPtr<IDetailLayoutBuilder> DetailBuilderPin = DetailBuilder.Pin())
	{
		TArray<TWeakObjectPtr<UMovieGraphModifierNode>> ModifierNodes =
			DetailBuilderPin->GetObjectsOfTypeBeingCustomized<UMovieGraphModifierNode>();
		if (ModifierNodes.Num() != 1)
		{
			return nullptr;
		}

		const TWeakObjectPtr<UMovieGraphModifierNode> ModifierNode = ModifierNodes[0];
		if (ModifierNode.IsValid())
		{
			return ModifierNode;
		}
	}

	return nullptr;
}

void FMovieGraphModifiersCustomization::RefreshListDataSource()
{
	const TWeakObjectPtr<UMovieGraphModifierNode> ModifierNode = GetSelectedModifierNode();
	
	if (const TStrongObjectPtr<UMovieGraphModifierNode> ModifierNodePin = ModifierNode.Pin())
	{
		// Update the data source
		ListDataSource = ModifierNodePin->GetCollections();
	}
}

#undef LOCTEXT_NAMESPACE