// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotObjectVersion.h"
#include "Serialization/CustomVersion.h"

const FGuid FSkelotObjectVersion::GUID(0x1C01A2C, 0xF25D4159, 0x84EE0, 0x0CAE6E1F3);
FCustomVersionRegistration GRegisterSkelotCustomVersion(FSkelotObjectVersion::GUID, FSkelotObjectVersion::LatestVersion, TEXT("SkelotVer"));
