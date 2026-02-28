// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotPrivateUtils.h"



FString SkelotStructExportText(const UScriptStruct* StructPtr, const void* InstancePtr)
{
	FString DebugText;
	StructPtr->ExportText(DebugText, InstancePtr, nullptr, nullptr, 0, nullptr, true);
	return DebugText;
}


void SkelotMergeRanges(FIntRange* InOut, int32 NumRange, int32 MinIndex, int32 MaxIndex)
{
	for (int32 i = MinIndex - 1; i >= 0; i--)
	{
		InOut[MinIndex].Count += InOut[i].Count;
		InOut[MinIndex].Start = InOut[i].Start;
		InOut[i] = FIntRange{ 0, 0 };
	}

	for (int32 i = MaxIndex + 1; i < NumRange; i++)
	{
		InOut[MaxIndex].Count += InOut[i].Count;
		InOut[i] = FIntRange{ 0, 0 };
	}

}
