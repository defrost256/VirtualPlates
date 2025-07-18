// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/MovieGraphNode.h"
#include "MovieGraphRerouteNode.generated.h"

/** A node which is effectively a no-op/passthrough. Allows a connection to be routed untouched through this node to organize the graph. */
UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphRerouteNode : public UMovieGraphSettingNode
{
	GENERATED_BODY()

public:
	UMovieGraphRerouteNode();

	//~ UMovieGraphNode Interface
	virtual TArray<FMovieGraphPinProperties> GetInputPinProperties() const override;
	virtual TArray<FMovieGraphPinProperties> GetOutputPinProperties() const override;
	virtual bool CanBeDisabled() const override;
#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override;
	virtual FText GetMenuCategory() const override;
#endif
	//~ End UMovieGraphNode Interface

	/** Gets the pin opposite to the specified InFromPin. */
	virtual UMovieGraphPin* GetPassThroughPin(const UMovieGraphPin* InFromPin) const;

	/** Sets the pin properties for this reroute node. Note that this applies to both the input and output pin. */
	UFUNCTION(BlueprintPure, Category = "Movie Graph")
	FMovieGraphPinProperties GetPinProperties() const;

	/**
	 * Sets the pin properties for this reroute node (both the input and output pin have the same properties). This generally should not be called
	 * unless you know what you're doing; normal connection/disconnection should handle setting the properties correctly.
	 */
	UFUNCTION(BlueprintCallable, Category = "Movie Graph")
	void SetPinProperties(const FMovieGraphPinProperties& InPinProperties);

private:
	/** Pin properties that are shared with both the input and output pins. */
	UPROPERTY()
	FMovieGraphPinProperties InputOutputProperties;
};