/**
 * Author:  Ahmed Fawzy
 * Created:   11.04.2025
 * 
 * (c) Copyright by AN Games Studio.
 **/
// Ahmed Fawzy - AN Games Studio - 2025


#include "ASplineActor.h"
#include "Components/SceneComponent.h"

// Sets default values
AASplineActor::AASplineActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root Component"));

    SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("Path Spline"));
}

// Called when the game starts or when spawned
void AASplineActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AASplineActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}


int32 AASplineActor::FindNearestIndex(TArray<FLandscapeSplineInterpPoint> points, FLandscapeSplineInterpPoint point) {
    int32 nearestIndex = 0;
    double nearestDistSquared = std::numeric_limits<double>::infinity();
    UE_LOG(LogTemp, Warning, TEXT("Size is %d"), points.Num());
    for (int i = 0; i < points.Num(); i++) {
        FLandscapeSplineInterpPoint point2 = points[i];
        double distsq = FVector::Distance(point.Center, point2.Center);
        if (distsq < nearestDistSquared) {
            nearestDistSquared = distsq;
            nearestIndex = i;
        }

    }
    return nearestIndex;
}

// Pull the landscape spline from the level and copy it to the spline component.  Will clear the existing splinecomponent!
TArray<FVector> AASplineActor::GenerateSpline()
{
    TArray<FVector> returnpoints;
#if WITH_EDITOR

    if (Landscape != nullptr) {
        // Erase pre-existing spline points
        SplineComponent->ClearSplinePoints(true);

        // Get the spline data from the landscape.
        ULandscapeSplinesComponent* LandscapeSpline = Landscape->GetSplinesComponent();

        // Note:  ControlPoints is protected.  You will need to edit LandscapeSplinesComponent.h to make it public.
        //TArray<ULandscapeSplineControlPoint*> LandscapeSplinePoints = LandscapeSpline->ControlPoints;
        const TArray<TObjectPtr<ULandscapeSplineControlPoint>> LandscapeSplinePoints = LandscapeSpline->GetControlPoints();
        /*LandscapeSpline->ForEachControlPoint([&](ULandscapeSplineControlPoint* point)-> void {
            // Landscape spline points are stored relative to the origin of the landscape, so adjust accordingly.
            FVector Point_Worldspace = point->Location + Landscape->GetActorLocation();
            FSplinePoint
            // Add a new spline point to the component
            SplineComponent->AddSplinePoint(Point_Worldspace, ESplineCoordinateSpace::World, true);
        });*/
        //SplineComponent->AddPoints(LandscapeSpline->ControlPoints,true);
        /*for (ULandscapeSplineControlPoint* LandscapePoint : LandscapeSplinePoints) {
            // Separate out the given point
            //ULandscapeSplineControlPoint* LandscapePoint = LandscapeSplinePoints[i];

            // Landscape spline points are stored relative to the origin of the landscape, so adjust accordingly.
            FVector Point_Worldspace = LandscapePoint->Location + Landscape->GetActorLocation();

            // Add a new spline point to the component
            SplineComponent->AddSplinePoint(Point_Worldspace, ESplineCoordinateSpace::World, true);
        }*/
        TArray<FLandscapeSplineInterpPoint> points;
        for (ULandscapeSplineSegment* LanscapeSegment : Landscape->GetSplinesComponent()->GetSegments()) {
            for (FLandscapeSplineInterpPoint LandscapePoint : LanscapeSegment->GetPoints()) {
                points.Add(LandscapePoint);
            }
            points.RemoveAt(points.Num() - 1);
        }

        TArray<FLandscapeSplineInterpPoint> orderedPoints;
        orderedPoints.Add(points[0]);
        int size1 = points.Num();
        points.RemoveAt(0);
        int size2 = points.Num();
        if (size2 >= size1) {
            UE_LOG(LogTemp, Warning, TEXT("No Item Removed!!"));
        }
        while (points.Num() > 0) {
            int nearestIndex = FindNearestIndex(points, orderedPoints[orderedPoints.Num() - 1]);

            orderedPoints.Add(points[nearestIndex]);
            size1 = points.Num();
            points.RemoveAt(nearestIndex);
            size2 = points.Num();
            if (size2 >= size1) {
                UE_LOG(LogTemp, Warning, TEXT("No Item Removed222!!"));
            }
        }
        
        for (FLandscapeSplineInterpPoint point : orderedPoints) {
            // Landscape spline points are stored relative to the origin of the landscape, so adjust accordingly.
            FVector Point_Worldspace = point.Center + Landscape->GetActorLocation();

            // Add a new spline point to the component
            SplineComponent->AddSplinePoint(Point_Worldspace, ESplineCoordinateSpace::World, true);
            returnpoints.Add(Point_Worldspace);
        }
        /*for (int i = 1; i < LandscapeSplinePoints.Num(); i++) {
            // Separate out the given point
            ULandscapeSplineControlPoint* LandscapePoint = LandscapeSplinePoints[i];

            // Landscape spline points are stored relative to the origin of the landscape, so adjust accordingly.
            FVector Point_Worldspace = LandscapePoint->Location + Landscape->GetActorLocation();

            // Add a new spline point to the component
            SplineComponent->AddSplinePoint(Point_Worldspace, ESplineCoordinateSpace::World, true);
        }*/
        
    }
#endif
    return returnpoints;
}
