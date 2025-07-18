// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphFileOutputNode.h"

#include "Graph/MovieGraphConfig.h"
#include "MoviePipelineUtils.h"

int32 UMovieGraphFileOutputNode::GetNumFileOutputNodes(const UMovieGraphEvaluatedConfig& InEvaluatedConfig, const FName& InBranchName)
{
	return InEvaluatedConfig.GetSettingsForBranch(UMovieGraphFileOutputNode::StaticClass(), InBranchName, false /*bIncludeCDOs*/, false /*bExactMatch*/).Num();
}

void UMovieGraphFileOutputNode::DisambiguateFilename(FString& InOutFilenameFormatString, const UE::MovieGraph::FMovieGraphOutputMergerFrame* InRawFrameData, const FName& InNodeName, const FMovieGraphPassData& InRenderData)
{
	// ToDo: This is overly protective and could be relaxed later, for instance
	// if different file write nodes have chosen a separate filepath entirely.
	const UE::MovieGraph::FMovieGraphRenderDataValidationInfo ValidationInfo = InRawFrameData->GetValidationInfo(InRenderData.Key);

	// Since there can only be one layer per branch, we restrain layer/branch validation to multi-branch graphs.
	if (ValidationInfo.BranchCount > 1)
	{
		// We can run into the scenario where the users have given layers the same name, so layer_name token won't help differentiate.
		// To resolve this, we look to see if there's multiple branches with the same layer name, and if so we force the branch name into the token too.
		if (ValidationInfo.LayerCount < ValidationInfo.BranchCount)
		{
			UE::MoviePipeline::ConformOutputFormatStringToken(InOutFilenameFormatString, TEXT("{branch_name}"), InNodeName, InRenderData.Key.RootBranchName);
		}
		else
		{
			// Otherwise, we separate each branch by its unique layer name.
			UE::MoviePipeline::ConformOutputFormatStringToken(InOutFilenameFormatString, TEXT("{layer_name}"), InNodeName, InRenderData.Key.RootBranchName);
		}
	}

	// We only add the renderer name token if multiple (non-composited) renderers are present on the active branch (eg, in the case of optional PPMs).
	if (ValidationInfo.ActiveBranchRendererCount > 1)
	{
		UE::MoviePipeline::ConformOutputFormatStringToken(InOutFilenameFormatString, TEXT("{renderer_name}"), InNodeName, InRenderData.Key.RootBranchName);
	}

	// We only add the subresource token if a (non-composited) renderer on the active branch is producing more than one subresource (eg, in the case of optional PPMs).
	if (ValidationInfo.ActiveRendererSubresourceCount > 1)
	{
		UE::MoviePipeline::ConformOutputFormatStringToken(InOutFilenameFormatString, TEXT("{renderer_sub_name}"), InNodeName, InRenderData.Key.RootBranchName);
	}

	// We only add the camera name token if there are more than one cameras for the active branch
	if (ValidationInfo.ActiveCameraCount > 1)
	{
		UE::MoviePipeline::ConformOutputFormatStringToken(InOutFilenameFormatString, TEXT("{camera_name}"), InNodeName, InRenderData.Key.RootBranchName);
	}
}

TArray<FMovieGraphPassData> UMovieGraphFileOutputNode::GetCompositedPasses(UE::MovieGraph::FMovieGraphOutputMergerFrame* InRawFrameData)
{
	// Gather the passes that need to be composited
	TArray<FMovieGraphPassData> CompositedPasses;

	for (const FMovieGraphPassData& RenderData : InRawFrameData->ImageOutputData)
	{
		const UE::MovieGraph::FMovieGraphSampleState* Payload = RenderData.Value->GetPayload<UE::MovieGraph::FMovieGraphSampleState>();
		check(Payload);
		if (!Payload->bCompositeOnOtherRenders)
		{
			continue;
		}

		FMovieGraphPassData CompositePass;
		CompositePass.Key = RenderData.Key;
		CompositePass.Value = RenderData.Value->CopyImageData();
		CompositedPasses.Add(MoveTemp(CompositePass));
	}

	// Sort composited passes if multiple were found. Passes with a higher sort order go to the end of the array so they
	// get composited on top of passes with a lower sort order.
	CompositedPasses.Sort([](const FMovieGraphPassData& PassA, const FMovieGraphPassData& PassB)
	{
		const UE::MovieGraph::FMovieGraphSampleState* PayloadA = PassA.Value->GetPayload<UE::MovieGraph::FMovieGraphSampleState>();
		const UE::MovieGraph::FMovieGraphSampleState* PayloadB = PassB.Value->GetPayload<UE::MovieGraph::FMovieGraphSampleState>();
		check(PayloadA);
		check(PayloadB);

		return PayloadA->CompositingSortOrder < PayloadB->CompositingSortOrder;
	});

	return CompositedPasses;
}
