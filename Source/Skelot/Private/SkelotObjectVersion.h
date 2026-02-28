// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/Guid.h"

struct SKELOT_API FSkelotObjectVersion
{
	enum Type
	{
		// Before any version changes were made
		BeforeCustomVersionWasAdded = 0,


		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	static const FGuid GUID;
};

