// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SplineComponent.h"
#include "Landscape.h"
#include "SplineRider.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSplineRider, Log, Log);

UENUM(BlueprintType)
enum class ESmoothingMethod:uint8{CentralDifference, ToPrevious};


UCLASS()
class ASplineRider : public AActor
{
	GENERATED_BODY()


public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "SplineRider|Motion")
	float Speed;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "SplineRider|Motion|Speed")
	bool bAdjustSpeedByTurn;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "SplineRider|Motion|Speed")
	bool bAdjustSpeedBySlope;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "SplineRider|Motion|Speed")
	float Acceleration;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "SplineRider|Motion|Speed")
	float Deceleration;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "SplineRider|Motion|Speed")
	float TurnSlopeLookAhead;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "SplineRider|Motion|Speed")
	float MaxSlope;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "SplineRider|Motion|Speed")
	float MinSlope;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "SplineRider|Motion|Speed")
	float SlopeSpeedGain = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "SplineRider|Motion|Speed")
	float SlopeSpeedLoss = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "SplineRider|Motion|Speed")
	float MaxTurn;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "SplineRider|Motion|Speed")
	float MinTurn;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "SplineRider|Motion|Speed")
	float TurnSpeedLoss;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "SplineRider|Motion|Speed")
	float CurrentSlope;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "SplineRider|Motion|Speed")
	float CurrentTurn;
	
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "SplineRider|Motion")
	float CurrentSpeed;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "SplineRider|Motion")
	float CurrentTime;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "SplineRider|Motion")
	float CurrentPositionOnRail;

	/* If enabled, it compensates world time dilation in Speed/Duration mode so that the spline moves as intended speed regardless of recording time scale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SplineRider|Motion")
	bool bCompensateTimeScale = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SplineRider|Motion")
	bool bLooping;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Components")
	USplineComponent* Spline;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Components")
	USceneComponent* Mount;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "SplineRider|GroundCompensation")
	bool bGroundCompensation;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "SplineRider|GroundCompensation")
	float GroudTolerance = 20.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "SplineRider|GroundCompensation")
	float GroundOffset = 130.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "SplineRider|GroundCompensation")
	float GroundCompensationScale = 1.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "SplineRider|GroundCompensation")
	bool bAlignToGroundNormal;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "SplineRider|GroundCompensation")
	float NormalInfluece = 1.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "SplineRider|Motion")
	float TranslationSmoothing = 2.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "SplineRider|Motion")
	float RotationSmoothing = 2.f;

	//UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category = "SplineRider|Smoothing")
	//ESmoothingMethod SmoothingMethod;

	//UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category = "SplineRider|Smoothing")
	//float SmoothingAlignFactor;

	//UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category = "SplineRider|Smoothing")
	//float SmoothingScaleFactor;

	//UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category = "SplineRider|Smoothing")
	//float SmoothingPostStraighten;

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "SplineRider|Smoothing")
	//bool bDrawSmoothingDebug = false;

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "SplineRider|Smoothing")
	//bool bSmoothingDebugLiveUpdate = false;

public:	
	// Sets default values for this actor's properties
	ASplineRider();
	UFUNCTION(BlueprintCallable)
	void UpdateRide(float DeltaTime);

	UFUNCTION(BlueprintCallable)
	void UpdateSpeed(float DeltaTime);


	UFUNCTION(BlueprintCallable)
	void CopySplineFromActor(AActor* SplineToCopy, int SplineComponentIdx, ESplineCoordinateSpace::Type CopyCoordinateSpace);

	UFUNCTION(BlueprintCallable)
	void CopySplineFromLandscape(const ALandscape* landscape);

	/*UFUNCTION(BlueprintCallable)
	const TArray<FSplinePoint>& CalculateSmoothSplinePoints(float AlignFactor, float ScaleFactor, float PostStraighten, ESmoothingMethod method);*/
		
	UFUNCTION(BlueprintCallable)
	void DrawDebugSplinePoints(const TArray<FSplinePoint>& SplinePoints);
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	virtual bool ShouldTickIfViewportsOnly() const override;

	virtual class USceneComponent* GetDefaultAttachComponent() const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	void DrawDebugSpline(const FSplineCurves& SplineCurve);
private:

	FTimerHandle SequencerCheckHandle;
	/* Check if the rig rail is driven by Sequencer*/
	UFUNCTION()
	void OnSequencerCheck();
	//FSplinePoint CalculateSmoothTangents(const FSplinePoint& pt0, const FSplinePoint& pt1, const FSplinePoint& pt2, float AlignFactor, float ScaleFactor);
	//FSplinePoint CalculateSmoothToPrevious(const FSplinePoint& pt0, const FSplinePoint& pt1, const FSplinePoint& pt2, float AlignFactor, float ScaleFactor) ;
private:
	TArray<FSplinePoint> SmoothSplinePoints;
};
