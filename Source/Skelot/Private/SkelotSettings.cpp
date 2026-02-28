// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotSettings.h"



USkelotDeveloperSettings::USkelotDeveloperSettings(const FObjectInitializer& Initializer)
{
	CategoryName = TEXT("Plugins");
	MaxTransitionGenerationPerFrame = 200;
}