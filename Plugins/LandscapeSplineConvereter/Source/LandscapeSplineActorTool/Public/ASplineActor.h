/**
 * Author:  Ahmed Fawzy
 * Created:   11.04.2025
 * 
 * (c) Copyright by AN Games Studio.
 **/
// Ahmed Fawzy - AN Games Studio - 2025

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SplineComponent.h"
#include "LandscapeSplineActor.h"
#include "LandscapeSplineControlPoint.h"
#include "Misc/EngineVersionComparison.h"


#include "ASplineActor.generated.h"

UCLASS()
class LANDSCAPESPLINEACTORTOOL_API AASplineActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USplineComponent* SplineComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape")
	ALandscapeSplineActor* Landscape;

	// Sets default values for this actor's properties
	AASplineActor();

private:
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Landscape")
	TArray<FVector> GenerateSpline();
	int32 FindNearestIndex(TArray<FLandscapeSplineInterpPoint> points, FLandscapeSplineInterpPoint point);

public:
	UFUNCTION(BlueprintCallable, Category = LandscapeSplines)
	TArray<ULandscapeSplineControlPoint*> GetControlPoints() { 
		return Landscape->GetSplinesComponent()->GetControlPoints();
	}

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
