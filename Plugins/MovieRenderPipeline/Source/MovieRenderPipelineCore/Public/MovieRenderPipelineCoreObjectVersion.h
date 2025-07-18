// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/Guid.h"

/** Custom serialization version for changes made to Movie Render Pipeline Core objects. */
struct MOVIERENDERPIPELINECORE_API FMovieRenderPipelineCoreObjectVersion
{
	enum Type : int32
	{
		PreVersioning = 0,

		/** Added bOnlyMatchComponents to some conditions within graph collections, defaulting to true. */
		OnlyMatchComponentsAdded,

		// --- Add new versions above this line ---
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	static const FGuid GUID;

	FMovieRenderPipelineCoreObjectVersion() = delete;
};