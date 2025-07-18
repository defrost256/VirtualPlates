/**
 * Author:  Ahmed Fawzy
 * Created:   11.04.2025
 * 
 * (c) Copyright by AN Games Studio.
 **/
// Ahmed Fawzy - AN Games Studio - 2025

#include "LandscapeSplineActorTool.h"

#define LOCTEXT_NAMESPACE "FLandscapeSplineActorToolModule"

void FLandscapeSplineActorToolModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FLandscapeSplineActorToolModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FLandscapeSplineActorToolModule, LandscapeSplineActorTool)