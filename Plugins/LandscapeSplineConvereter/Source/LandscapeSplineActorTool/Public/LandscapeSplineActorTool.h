/**
 * Author:  Ahmed Fawzy
 * Created:   11.04.2025
 * 
 * (c) Copyright by AN Games Studio.
 **/
// Ahmed Fawzy - AN Games Studio - 2025

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FLandscapeSplineActorToolModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
