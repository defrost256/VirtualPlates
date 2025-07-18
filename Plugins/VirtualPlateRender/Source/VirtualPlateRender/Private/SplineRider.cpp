// Fill out your copyright notice in the Description page of Project Settings.


#include "SplineRider.h"
#include "Math/RotationMatrix.h"
#include "LandscapeSplinesComponent.h"
#include "LandscapeSplineSegment.h"
#include "Editor.h"

DEFINE_LOG_CATEGORY(LogSplineRider);

// Sets default values
ASplineRider::ASplineRider()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("RailSplineComponent"));
	Spline->SetupAttachment(RootComponent);

	Mount = CreateDefaultSubobject<USceneComponent>(TEXT("RailCameraMount"));
	Mount->SetupAttachment(Spline);

	CurrentPositionOnRail = 0.f;
//
//#if WITH_EDITOR
//	if (!IsTemplate() && GEditor)
//	{
//		GEditor->GetTimerManager()->SetTimer(SequencerCheckHandle, this, &ASplineRider::OnSequencerCheck, 1.0, true);
//	}
//#endif
}

void ASplineRider::UpdateRide(float DeltaTime)
{
	const float TimeDilation = FMath::Max(GetActorTimeDilation(), UE_KINDA_SMALL_NUMBER);
	const float AdjustedDeltaTime = bCompensateTimeScale ? DeltaTime / TimeDilation : DeltaTime;
	const float TotalTime = Spline->GetSplineLength() / FMath::Abs(CurrentSpeed);
	if (FMath::IsNaN(CurrentPositionOnRail)) {
		CurrentPositionOnRail = 0.f;
		UE_LOG(LogSplineRider, Warning, TEXT("Undefined position, resetting"));
	}
	CurrentTime = TotalTime * CurrentPositionOnRail + FMath::Sign(CurrentSpeed) * AdjustedDeltaTime;

	CurrentPositionOnRail = bLooping ? FMath::Frac(CurrentTime/TotalTime)  : FMath::Clamp(CurrentTime / TotalTime, 0.0f, 1.0f);
	const float CurrentIKey = Spline->GetInputKeyValueAtDistanceAlongSpline(CurrentPositionOnRail * Spline->GetSplineLength());
	const FVector SplinePos = Spline->GetLocationAtSplineInputKey(CurrentIKey, ESplineCoordinateSpace::World);
	const FQuat SplineQuat = Spline->GetQuaternionAtSplineInputKey(CurrentIKey, ESplineCoordinateSpace::World);
	FTransform TargetTransform(SplineQuat, SplinePos);
	UWorld* WLD = GetWorld();
	if (WLD != nullptr && !WLD->IsPreviewWorld()) {
		FHitResult GroundHit;
		FVector up = SplineQuat.GetUpVector();
		if (WLD->LineTraceSingleByChannel(GroundHit, SplinePos + up * GroudTolerance, SplinePos - up * 1000, ECollisionChannel::ECC_Camera)) {
			if (bGroundCompensation) {
				TargetTransform.SetLocation(FMath::Lerp(SplinePos, GroundHit.Location, GroundCompensationScale) + up * GroundOffset);
			}
			if (bAlignToGroundNormal) {
				TargetTransform.SetRotation(FQuat::Slerp(SplineQuat, UE::Math::TRotationMatrix<double>::MakeFromXZ(SplineQuat.GetForwardVector(), GroundHit.Normal).ToQuat(), NormalInfluece));
			}
		}
		else {
			UE_LOG(LogSplineRider, Warning, TEXT("Missed ground hit"));
		}
	}
	FTransform xForm = Mount->GetComponentTransform();
	xForm.SetLocation(FMath::Lerp(xForm.GetLocation(), TargetTransform.GetLocation(), FMath::Clamp(TranslationSmoothing * DeltaTime, 0.f, 1.f)));
	xForm.SetRotation(FQuat::Slerp(xForm.GetRotation(), TargetTransform.GetRotation(), FMath::Clamp(RotationSmoothing * DeltaTime, 0.f, 1.f)));
	Mount->SetWorldTransform(xForm);
}

void ASplineRider::UpdateSpeed(float DeltaTime)
{
	const float TimeDilation = FMath::Max(GetActorTimeDilation(), UE_KINDA_SMALL_NUMBER);
	const float AdjustedDeltaTime = bCompensateTimeScale ? DeltaTime / TimeDilation : DeltaTime;
	float TargetSpeed = Speed;

	const float CurrentIKey = Spline->GetInputKeyValueAtDistanceAlongSpline(CurrentPositionOnRail * Spline->GetSplineLength());
	const float LookaheadIKey = Spline->GetInputKeyValueAtDistanceAlongSpline(CurrentPositionOnRail * Spline->GetSplineLength() + TurnSlopeLookAhead);

	const FVector CurrentDir = Spline->GetDirectionAtSplineInputKey(CurrentIKey, ESplineCoordinateSpace::Local);
	const FVector LookaheadDir = Spline->GetDirectionAtSplineInputKey(LookaheadIKey, ESplineCoordinateSpace::Local);
	CurrentTurn = 1.f - FMath::Abs(CurrentDir.Dot(LookaheadDir));
	if (bAdjustSpeedByTurn)
		TargetSpeed -= FMath::GetMappedRangeValueClamped(TRange<float>(MinTurn, MaxTurn), TRange<float>(0.f, TurnSpeedLoss), CurrentTurn);

	const FVector CurrentPos = Spline->GetLocationAtSplineInputKey(CurrentIKey, ESplineCoordinateSpace::Local);
	const FVector LookaheadPos = Spline->GetLocationAtSplineInputKey(LookaheadIKey, ESplineCoordinateSpace::Local);
	CurrentSlope = LookaheadDir.Z;

	if(bAdjustSpeedBySlope)
		TargetSpeed += FMath::GetMappedRangeValueClamped(TRange<float>(MinSlope, MaxSlope), TRange<float>(SlopeSpeedGain, -TurnSpeedLoss), CurrentSlope);

	else if (TargetSpeed < CurrentSpeed)
		CurrentSpeed -= FMath::Min(Deceleration * AdjustedDeltaTime, CurrentSpeed - TargetSpeed);
	else 
		CurrentSpeed += FMath::Min(Acceleration * AdjustedDeltaTime, TargetSpeed - CurrentSpeed);
}

void ASplineRider::CopySplineFromActor(AActor* SplineToCopy, int SplineComponentIdx, ESplineCoordinateSpace::Type CopyCoordinateSpace)
{
	TArray<USplineComponent*> SplineComps;
	USplineComponent* CopySpline;
	SplineToCopy->GetComponents(SplineComps, false);
	if (SplineComps.IsValidIndex(SplineComponentIdx)) {
		CopySpline = SplineComps[SplineComponentIdx];
		if (CopySpline != nullptr)
		{
			Spline->ClearSplinePoints(false);
			int ptCount = CopySpline->GetNumberOfSplinePoints();
			for (int i = 0; i < ptCount; i++) {
				Spline->AddPoint(CopySpline->GetSplinePointAt(i, CopyCoordinateSpace), false);
			}
			Spline->UpdateSpline();
		}
	}
}
void ASplineRider::CopySplineFromLandscape(const ALandscape* landscape)
{
	const ULandscapeSplinesComponent* landscapeSplines = landscape->GetSplinesComponent();
	const TArray<TObjectPtr<ULandscapeSplineSegment>> segments = landscapeSplines->GetSegments();
	for (const TObjectPtr<ULandscapeSplineSegment>& seg : segments) {
		const TArray<FLandscapeSplineMeshEntry>& meshes = seg.Get()->SplineMeshes;
		for (FLandscapeSplineMeshEntry mesh : meshes) {
			UE_LOG(LogSplineRider, Log, TEXT("Landscape segment %s has static mesh %s"), *seg.Get()->GetName(), *mesh.Mesh->GetName());
		}
	}
}
//
//const TArray<FSplinePoint>& ASplineRider::CalculateSmoothSplinePoints(float AlignFactor, float ScaleFactor, float PostStraighten, ESmoothingMethod method)
//{
//	int ptCount = Spline->GetNumberOfSplinePoints();
//	SmoothSplinePoints.Reset();
//	for (int pt = 0; pt < ptCount; pt++) {
//		FSplinePoint pt0, pt1, pt2;
//		pt0 = pt1 = pt2 = Spline->GetSplinePointAt(pt, ESplineCoordinateSpace::World);
//		if (pt > 0) {	//if we have previous pt we store it
//			pt0 = Spline->GetSplinePointAt(pt - 1, ESplineCoordinateSpace::World);
//		}
//		else if (Spline->IsClosedLoop()) {	//In case of a closed loop we store the last point as the previous for the first point
//			pt0 = Spline->GetSplinePointAt(ptCount - 1, ESplineCoordinateSpace::World);
//		}
//		else {	//In case we don't have a previous point we store a point whose leave tangent is the same as our arrive tangent so we don't modify the end tangents
//			pt0.LeaveTangent = pt0.ArriveTangent;
//		}
//		if (pt < ptCount - 1) {
//			pt2 = Spline->GetSplinePointAt(pt + 1, ESplineCoordinateSpace::World);
//		}
//		else if (Spline->IsClosedLoop()) {
//			pt2 = Spline->GetSplinePointAt(0, ESplineCoordinateSpace::World);
//		}
//		else {
//			pt2.ArriveTangent = pt2.LeaveTangent;
//		}
//
//		FSplinePoint newPt;
//		if (method == ESmoothingMethod::CentralDifference) {
//			newPt = CalculateSmoothTangents(pt0, pt1, pt2, AlignFactor, ScaleFactor);
//		}
//		else if (method == ESmoothingMethod::ToPrevious) {
//			newPt = CalculateSmoothToPrevious(pt == 0 ? pt0 : newPt, pt1, pt2, AlignFactor, ScaleFactor);
//		}
//		SmoothSplinePoints.Add(newPt);
//	}
//
//	for (FSplinePoint& pt : SmoothSplinePoints) {
//		pt.ArriveTangent = FVector::SlerpVectorToDirection(pt.ArriveTangent, FVector::SlerpVectorToDirection(pt.ArriveTangent, -pt.LeaveTangent, .5f), PostStraighten);
//		pt.LeaveTangent = FVector::SlerpVectorToDirection(pt.LeaveTangent, FVector::SlerpVectorToDirection(pt.LeaveTangent, -pt.ArriveTangent, .5f), PostStraighten);
//	}
//
//	return SmoothSplinePoints;
//}

void ASplineRider::DrawDebugSplinePoints(const TArray<FSplinePoint>& SplinePoints)
{
	FSplineCurves curves;
	for (FSplinePoint pt : SplinePoints) {
		curves.Position.Points.Add(FInterpCurvePointVector(pt.InputKey, pt.Position, pt.ArriveTangent, pt.LeaveTangent, EInterpCurveMode::CIM_CurveBreak));
		curves.Rotation.Points.Add(FInterpCurvePointQuat(pt.InputKey, pt.Rotation.Quaternion(), FQuat::Identity, FQuat::Identity, EInterpCurveMode::CIM_CurveAuto));
		curves.Scale.Points.Add(FInterpCurvePointVector(pt.InputKey, pt.Scale, FVector::ZeroVector, FVector::ZeroVector, EInterpCurveMode::CIM_CurveAuto));
	}
	curves.UpdateSpline();
	DrawDebugSpline(curves);
}

// Called when the game starts or when spawned
void ASplineRider::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void ASplineRider::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateSpeed(DeltaTime);
	UpdateRide(DeltaTime);
}

bool ASplineRider::ShouldTickIfViewportsOnly() const
{
	return true;
}

USceneComponent* ASplineRider::GetDefaultAttachComponent() const
{
	return Mount;
}

void ASplineRider::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	const FName propName = PropertyChangedEvent.GetPropertyName();
	if (PropertyChangedEvent.Property == nullptr)
		return;
	/*if (bSmoothingDebugLiveUpdate) {
		const TMap<FName, FString>* meta = PropertyChangedEvent.Property->GetMetaDataMap();
		if (meta != nullptr) {
			const FString* category = meta->Find(FName("Category"));
			if (category != nullptr && category->Contains("smooth")) {
				FlushPersistentDebugLines(GetWorld());
				CalculateSmoothSplinePoints(SmoothingAlignFactor, SmoothingScaleFactor, SmoothingPostStraighten, SmoothingMethod);
				DrawDebugSplinePoints(SmoothSplinePoints);
				UE_LOG(LogSplineRider, Log, TEXT("Spline Rider smoothing parameter %s in category %s changed"), *propName.ToString(), **category);
			}
		}
	}*/
	/*TArray<FName> MetaKeys;
	PropertyChangedEvent.Property->GetMetaDataMap()->GetKeys(MetaKeys);
	FStringBuilderBase sb;
	for (const FName& metaName : MetaKeys) {
		sb.Append(metaName.ToString());
		sb.Append(", ");
	}
	const FString metaListStr(sb);
	UE_LOG(LogSplineRider, Log, TEXT("Rider Meta %s changed: %s"), *propName.ToString(), *metaListStr);*/
}

void ASplineRider::DrawDebugSpline(const FSplineCurves& SplineCurve)
{
	const UWorld* Wld = GetWorld();
	if (Wld == nullptr) {
		return;
	}
	const int ptCount = SplineCurve.Position.Points.Num();
	const int segmentCount = SplineCurve.Position.bIsLooped ? ptCount : ptCount - 1;
	FVector LastPos(0);
#if WITH_EDITOR
	const float SegmentLineThickness = GetDefault<ULevelEditorViewportSettings>()->SplineLineThicknessAdjustment;
#endif
	for (int KeyIdx = 0; KeyIdx < segmentCount + 1; KeyIdx++) {
		FVector CurrentPos = SplineCurve.Position.Eval(static_cast<float>(KeyIdx), FVector::Zero());
		if (KeyIdx < ptCount)
		{
			DrawDebugPoint(Wld, CurrentPos, 6, FColor::Yellow, true);
			const FInterpCurvePointVector pt = SplineCurve.Position.Points[KeyIdx];
			const FVector LeaveTanPt = CurrentPos + pt.LeaveTangent;
			const FVector ArriveTanPt = CurrentPos + pt.ArriveTangent;
#if WITH_EDITOR
			DrawDebugDirectionalArrow(Wld, CurrentPos, ArriveTanPt, 4.f, FColor::Cyan, true, -1.f, 0, SegmentLineThickness);
			DrawDebugDirectionalArrow(Wld, CurrentPos, LeaveTanPt, 4.f, FColor::Green, true, -1.f, 0, SegmentLineThickness);
#else
			DrawDebugDirectionalArrow(Wld, CurrentPos, ArriveTanPt, 4.f, FColor::Cyan, true);
			DrawDebugDirectionalArrow(Wld, CurrentPos, LeaveTanPt, 4.f, FColor::Green, true);
#endif
		}

		if (KeyIdx > 0) {
			FVector LastPosRolling = LastPos;
			const int stepCount = 20;

			for (int step = 1; step <= stepCount; step++) {
				const float key = (KeyIdx - 1) + (step / static_cast<float>(stepCount));
				const FVector CurrentPosRolling = SplineCurve.Position.Eval(key, FVector::Zero());
#if WITH_EDITOR
				DrawDebugLine(Wld, LastPosRolling, CurrentPosRolling, FColor::Yellow, true, -1.f, 0, SegmentLineThickness);
#else
				DrawDebugLine(Wld, LastPosRolling, CurrentPosRolling, FColor::Yellow, true);
#endif
				LastPosRolling = CurrentPosRolling;
			}
		}
		LastPos = CurrentPos;
	}
}

void ASplineRider::OnSequencerCheck()
{
}

//FSplinePoint ASplineRider::CalculateSmoothTangents(const FSplinePoint& pt0, const FSplinePoint &pt1, const FSplinePoint& pt2, float AlignFactor, float ScaleFactor)
//{
//	//Nearest points on tangent lines: https://en.wikipedia.org/wiki/Skew_lines#Nearest_points
//	const FVector nArrive = pt0.LeaveTangent.Cross(pt1.ArriveTangent);
//	const FVector n0Leave = pt0.LeaveTangent.Cross(nArrive);
//	const FVector n1Arrive = pt1.ArriveTangent.Cross(nArrive);
//	const FVector c01 = pt0.Position + pt0.LeaveTangent * ((pt1.Position - pt0.Position).Dot(n1Arrive) / pt0.LeaveTangent.Dot(n1Arrive));	//Point on pt0-leave closest to pt1-arrive
//	const FVector c10 = pt1.Position + pt1.ArriveTangent * ((pt0.Position - pt1.Position).Dot(n0Leave) / pt1.ArriveTangent.Dot(n0Leave));	//Point on pt1-arrive closest to pt0-leave
//
//	const FVector nLeave = pt1.LeaveTangent.Cross(pt2.ArriveTangent);
//	const FVector n1Leave = pt1.LeaveTangent.Cross(nLeave);
//	const FVector n2Arrive = pt2.ArriveTangent.Cross(nLeave);
//	const FVector c12 = pt1.Position + pt1.LeaveTangent * ((pt2.Position - pt1.Position).Dot(n2Arrive) / pt1.LeaveTangent.Dot(n2Arrive));	//Point on pt1-leave closest to pt2-arrive
//	const FVector c21 = pt2.Position + pt2.ArriveTangent * ((pt1.Position - pt2.Position).Dot(n1Leave) / pt2.ArriveTangent.Dot(n1Leave));	//Point on pt2-arrive closest to pt1-leave
//
//	const FVector ArriveTarget = ((c01 + c10) / 2) - pt1.Position;
//	const FVector LeaveTarget = ((c12 + c21) / 2) - pt1.Position;
//
//	FVector NewArrive = FMath::Lerp(pt1.ArriveTangent, ArriveTarget, AlignFactor);
//	NewArrive = NewArrive.GetSafeNormal() * FMath::Lerp(pt1.ArriveTangent.Length(), ArriveTarget.Length(), ScaleFactor);
//	FVector NewLeave = FMath::Lerp(pt1.LeaveTangent, LeaveTarget, AlignFactor);
//	NewLeave = NewLeave.GetSafeNormal() * FMath::Lerp(pt1.LeaveTangent.Length(), LeaveTarget.Length(), ScaleFactor);
//
//	FSplinePoint newPt(pt1);
//	newPt.ArriveTangent = NewArrive;
//	newPt.LeaveTangent = NewLeave;
//	newPt.Type = ESplinePointType::CurveCustomTangent;
//	return newPt;
//}
//
//FSplinePoint ASplineRider::CalculateSmoothToPrevious(const FSplinePoint& pt0, const FSplinePoint& pt1, const FSplinePoint& pt2, float AlignFactor, float ScaleFactor)
//{
//	FVector newArrive = FVector::SlerpVectorToDirection(pt1.ArriveTangent, pt0.LeaveTangent, AlignFactor);
//    newArrive = newArrive.GetSafeNormal() * FMath::Lerp(pt1.ArriveTangent.Length(), pt0.LeaveTangent.Length(), ScaleFactor);
//
//	FSplinePoint newPt(pt1);
//	newPt.ArriveTangent = newArrive;
//	newPt.Type = ESplinePointType::CurveCustomTangent;
//	return newPt;
//}
