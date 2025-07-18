// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoviePipelinePostRenderSettings.h"

FMovieGraphPostRenderVideoPlayOptions::FMovieGraphPostRenderVideoPlayOptions()
	: PlaybackMethod(EMovieGraphPlaybackMethod::OperatingSystem)
	, JobPlayback(EMovieGraphJobPlaybackRange::FirstJobOnly)
	, RenderLayerPlayback(EMovieGraphRenderLayerPlaybackRange::FirstRenderLayerOnly)
{
	
}

FMovieGraphPostRenderImageSequencePlayOptions::FMovieGraphPostRenderImageSequencePlayOptions()
	: FrameRangeNotation(EMovieGraphFrameRangeNotation::HashWithStartEndFrame)
	, PlaybackRange(EMovieGraphImageSequencePlaybackRange::FirstFrameOnly)
{
	
}

FMovieGraphPostRenderSettings::FMovieGraphPostRenderSettings()
	: OutputTypePriorityOrder({TEXT("EXR"), TEXT("PNG"), TEXT("JPEG"), TEXT("MP4"), TEXT("BMP"), TEXT("MOV"), TEXT("MXF")})
	, OutputTypePlayback(EMovieGraphOutputTypePlayback::UsePriorityOrder)
{
	
}