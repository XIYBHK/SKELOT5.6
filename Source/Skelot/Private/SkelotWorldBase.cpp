// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotWorldBase.h"

#include "SkelotPrivateUtils.h"


FString FSkelotInstanceHandle::ToDebugString() const
{
#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
	return FString();
#else
	return SkelotStructExportText<FSkelotInstanceHandle>(*this);
#endif
}


