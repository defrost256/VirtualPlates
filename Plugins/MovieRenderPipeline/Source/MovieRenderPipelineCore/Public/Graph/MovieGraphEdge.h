// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "MovieGraphEdge.generated.h"

class UMovieGraphPin;

UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphEdge : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UMovieGraphPin> InputPin;
	
	UPROPERTY()
	TObjectPtr<UMovieGraphPin> OutputPin;

	/** Whether the edge is valid or not. Being valid means it contains a non-null input and output pin. */
	bool IsValid() const;

	/**
	 * Gets the pin on the other side of the edge. If bFollowRerouteConnections is true, reroute nodes will be passthrough, and this method will
	 * continue traversing edges until a pin on a non-reroute node is found.
	 */
	UMovieGraphPin* GetOtherPin(const UMovieGraphPin* InFirstPin, const bool bFollowRerouteConnections = false);
};