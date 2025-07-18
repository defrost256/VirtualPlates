// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"

#include "MovieGraphDataTypes.h"
#include "MovieGraphQuickRenderSettings.h"
#include "MoviePipelinePostRenderSettings.h"

#include "MovieRenderGraphEditorSettings.generated.h"

UCLASS(BlueprintType, Config=EditorPerProjectUserSettings, meta=(DisplayName="Movie Render Graph"))
class MOVIERENDERPIPELINECORE_API UMovieRenderGraphEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UMovieRenderGraphEditorSettings();

	virtual FName GetCategoryName() const override;

#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

	/** If PostRenderBehavior is set to PlayRenderOutput, these settings are used to determine how to play media. */
	UPROPERTY(Config, EditAnywhere, Category="Play Render Output (Quick Render only)", meta=(ShowOnlyInnerProperties))
	FMovieGraphPostRenderSettings PostRenderSettings;
};
