// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieGraphFormatTokenCustomization.h"

#include "Graph/Nodes/MovieGraphDebugNode.h"
#include "Graph/Nodes/MovieGraphFileOutputNode.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Layout/WidgetPath.h"
#include "Widgets/SMoviePipelineFormatTokenAutoCompleteBox.h"

void FMovieGraphFormatTokenCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// This should work just fine for UMovieGraphFileOutputNode and UMovieGraphCommandLineEncoderNode as long as the property names stay in sync
	OutputFormatPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMovieGraphFileOutputNode, FileNameFormat));

	// If we can't find it, try the debug node
	if (!OutputFormatPropertyHandle->IsValidHandle())
	{
		OutputFormatPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMovieGraphDebugSettingNode, UnrealInsightsTraceFileNameFormat));
	}

	// If we still can't find it, early out
	if (!OutputFormatPropertyHandle->IsValidHandle())
	{
		return;
	}

	DetailBuilder.EditDefaultProperty(OutputFormatPropertyHandle)->CustomWidget()
		.PropertyHandleList({OutputFormatPropertyHandle})
		.NameContent()
		[
			OutputFormatPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(200.0f)
		[
			// We choose not to bind the text box text here because simultaneously setting and getting 
			// the handle value can cause the binding to stop working, so we manually update it when necessary 
			SAssignNew(AutoCompleteBox, SMoviePipelineFormatTokenAutoCompleteBox)
			.TextHandle(OutputFormatPropertyHandle)
			.Suggestions(SMoviePipelineFormatTokenAutoCompleteBox::GetFileNameFormatSuggestions())
		];
}