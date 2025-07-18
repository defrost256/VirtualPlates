// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/MovieGraphNode.h"
#include "MovieRenderPipelineDataTypes.h" // For EMoviePipelineShutterTiming
#include "MovieGraphCameraNode.generated.h"

/** A node which configures global camera settings that are shared among all renders. */
UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphCameraSettingNode : public UMovieGraphSettingNode
{
	GENERATED_BODY()

public:
	UMovieGraphCameraSettingNode()
		: ShutterTiming(EMoviePipelineShutterTiming::FrameCenter)
		, OverscanPercentage(0.f)
		, bRenderAllCameras(false)
	{}

	virtual EMovieGraphBranchRestriction GetBranchRestriction() const override { return EMovieGraphBranchRestriction::Globals; }
	virtual void GetFormatResolveArgs(FMovieGraphResolveArgs& OutMergedFormatArgs, const FMovieGraphRenderDataIdentifier& InRenderDataIdentifier) const override;

#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override;
	virtual FText GetMenuCategory() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
#endif

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_ShutterTiming : 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_OverscanPercentage : 1;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_bRenderAllCameras : 1;

	/**
	* Shutter Timing allows you to bias the timing of your shutter angle to either be before, during, or after
	* a frame. When set to FrameClose, it means that the motion gathered up to produce frame N comes from 
	* before and right up to frame N. When set to FrameCenter, the motion represents half the time before the
	* frame and half the time after the frame. When set to FrameOpen, the motion represents the time from 
	* Frame N onwards.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (EditCondition = "bOverride_ShutterTiming"))
	EMoviePipelineShutterTiming ShutterTiming;

	/**
	* Overscan percent enables rendering render additional pixels beyond the set resolution and can be used in conjunction 
	* with EXR file output to add post-processing effects such as lens distortion.
	* Please note that using this feature might affect the results due to auto-exposure and other camera settings.
	* On EXR this will produce a 1080p image with extra pixel data hidden around the outside edges for use 
	* in post production. For all other formats this will increase the final resolution and no pixels will be hidden 
	* (ie: 1080p with 10.0 overscan will make a 2112x1188 jpg, but a 1080p exr /w 96/54 pixels hidden on each side).
	*
	* Note: This uses 0-100 and not 0-1 like the previous system did to bring it in-line with other usages
	* of overscan in the engine (nDisplay).
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Overscan Percentage Override", UIMin = "0", UIMax = "100", ClampMin = "0", ClampMax = "100"), Category = "Settings", meta = (EditCondition = "bOverride_OverscanPercentage"))
	float OverscanPercentage;

	/*
	* If enabled Movie Render Queue will examine your Level Sequence for additional cameras and create an additional render for each renderer for that camera.
	* The Camera Cut Track/Section is still used to determine the range of time to render, and then all Camera Actors that are in the level sequence adjacent
	* to the Camera Cut Track will be considered for rendering. They are expected to exist the entire time and do not support rendering sub-ranges.
	* 
	* This increases render duration (100% per camera) and has increased VRAM/RAM requirements. However all cameras are rendered on the same engine tick so they
	* should all see a consistent view of the world which can be useful for things like particle effects, etc.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (EditCondition = "bOverride_bRenderAllCameras"))
	bool bRenderAllCameras;
};