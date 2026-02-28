// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SkelotBase.h"
#include "Animation/AnimSequenceBase.h"
#include "Engine/World.h"
#include "Misc/MemStack.h"
#include "Chaos/Core.h"



template <typename TType, typename TSize> void SkelotElementCopy(TType* Dst, const TType* Src, TSize Count)
{
	for (TSize i = 0; i < Count; i++)
		Dst[i] = Src[i];
}

inline bool SkelotTerminatedArrayAddUnique(uint8* Array /*must be 0xFF terminated*/, uint8 MaxLen , uint8 NewItem)
{
	check(NewItem != 0xFF);
	auto End = Array + MaxLen;

	for (auto Iter = Array; Iter != End; Iter++)
	{
		if (Iter[0] == NewItem)	//already exist ?
			return true;

		if (Iter[0] == 0xFF)
		{
			Iter[0] = NewItem;
			Iter[1] = 0xFF;
			return true;
		}
	}

	return false;
}

inline bool SkelotTerminatedArrayRemoveShift(uint8* Array /*must be 0xFF terminated*/, uint8 MaxLen, uint8 ItemToRemove)
{
	check(ItemToRemove != 0xFF);
	auto End = Array + MaxLen;

	for (auto Iter = Array; Iter != End; Iter++)
	{
		if (Iter[0] == ItemToRemove) //found it ?
		{
			do
			{
				Iter[0] = Iter[1];
				Iter++;

			} while (Iter[0] != 0xFF);

			return true;
		}
	}

	return false;
}



template<typename T> void SkelotRemoveDuplicates(T* Elements, uint32 Len)
{
	// use nested for loop to find the duplicate elements in array  
	for (uint32 i = 0; i < Len; i++)
	{
		for (uint32 j = i + 1; j < Len; j++)
		{
			// use if statement to check duplicate element  
			if (Elements[i] == Elements[j])
			{
				// delete the current position of the duplicate element  
				for (uint32 k = j; k < Len - 1; k++)
				{
					Elements[k] = Elements[k + 1];
				}
				// decrease the size of array after removing duplicate element  
				Len--;
				// if the position of the elements is changes, don't increase the index j  
				j--;
			}
		}
	}
}


template<typename T> T SkelotGetEven(T value)
{
	return value & ~T(1);
}


inline void SkelotCalcGridSize(float W, float H, int CellCount, int& OutX, int& OutY)
{
	check(W > 0 && H > 0 && CellCount > 1);
	const bool bHightHigher = H > W;
	const float Ratio = bHightHigher ? (W / H) : (H / W);
	int A = FMath::TruncToInt32(FMath::Sqrt(FMath::Max(1.0f, CellCount * Ratio)));
	check(A > 0);
	int B = CellCount / A;
	check(B > 0);
	check(A * B <= CellCount);
	if (bHightHigher)
	{
		OutX = A;
		OutY = B;
	}
	else
	{
		OutX = B;
		OutY = A;
	}
}


//copied from RadixSort32 to send distances as a separate array
template< typename ValueType, typename CountType> void SkelotRadixSort32(ValueType* RESTRICT Dst, ValueType* RESTRICT Src, uint32* RESTRICT DstDist, uint32* RESTRICT SrcDist, CountType Num)
{
	CountType Histograms[1024 + 2048 + 2048];
	CountType* RESTRICT Histogram0 = Histograms + 0;
	CountType* RESTRICT Histogram1 = Histogram0 + 1024;
	CountType* RESTRICT Histogram2 = Histogram1 + 2048;

	FMemory::Memzero(Histograms, sizeof(Histograms));

	{
		// Parallel histogram generation pass
		//const ValueType* RESTRICT s = (const ValueType * RESTRICT)Src;
		for (CountType i = 0; i < Num; i++)
		{
			uint32 Key = SrcDist[i];// SortKey(s[i]);
			Histogram0[(Key >> 0) & 1023]++;
			Histogram1[(Key >> 10) & 2047]++;
			Histogram2[(Key >> 21) & 2047]++;
		}
	}
	{
		// Prefix sum
		// Set each histogram entry to the sum of entries preceding it
		CountType Sum0 = 0;
		CountType Sum1 = 0;
		CountType Sum2 = 0;
		for (CountType i = 0; i < 1024; i++)
		{
			CountType t;
			t = Histogram0[i] + Sum0; Histogram0[i] = Sum0 - 1; Sum0 = t;
			t = Histogram1[i] + Sum1; Histogram1[i] = Sum1 - 1; Sum1 = t;
			t = Histogram2[i] + Sum2; Histogram2[i] = Sum2 - 1; Sum2 = t;
		}
		for (CountType i = 1024; i < 2048; i++)
		{
			CountType t;
			t = Histogram1[i] + Sum1; Histogram1[i] = Sum1 - 1; Sum1 = t;
			t = Histogram2[i] + Sum2; Histogram2[i] = Sum2 - 1; Sum2 = t;
		}
	}
	{
		// Sort pass 1
		const ValueType* RESTRICT s = (const ValueType * RESTRICT)Src;
		ValueType* RESTRICT d = Dst;
		const uint32* RESTRICT sd = SrcDist;
		uint32* RESTRICT dd = DstDist;

		for (CountType i = 0; i < Num; i++)
		{
			ValueType Value = s[i];
			uint32 Key = sd[i]; //SortKey(Value);
			CountType Idx = ++Histogram0[((Key >> 0) & 1023)];
			d[Idx] = Value;
			dd[Idx] = Key;
		}
	}
	{
		// Sort pass 2
		const ValueType* RESTRICT s = (const ValueType * RESTRICT)Dst;
		ValueType* RESTRICT d = Src;
		const uint32* RESTRICT sd = DstDist;
		uint32* RESTRICT dd = SrcDist;

		for (CountType i = 0; i < Num; i++)
		{
			ValueType Value = s[i];
			uint32 Key = sd[i];//SortKey(Value);
			CountType Idx = ++Histogram1[((Key >> 10) & 2047)];
			d[Idx] = Value;
			dd[Idx] = Key;
		}
	}
	{
		// Sort pass 3
		const ValueType* RESTRICT s = (const ValueType * RESTRICT)Src;
		ValueType* RESTRICT d = Dst;
		const uint32* RESTRICT sd = SrcDist;
		uint32* RESTRICT dd = DstDist;

		for (CountType i = 0; i < Num; i++)
		{
			ValueType Value = s[i];
			uint32 Key = sd[i];//SortKey(Value);
			CountType Idx = ++Histogram2[((Key >> 21) & 2047)];
			d[Idx] = Value;
			dd[Idx] = Key;
		}
	}
}

inline void SkelotArrayAndSSE(void* Data, int NumPack, int Value)
{
	VectorRegister4Int MaskReg = VectorIntSet1(Value);
	VectorRegister4Int* FlagPacks = (VectorRegister4Int*)Data;
	for (int i = 0; i < NumPack; i++)
		VectorIntStore(VectorIntAnd(VectorIntLoad(FlagPacks + i), MaskReg), FlagPacks + i);
}

inline void SkelotArrayOrSSE(void* Data, int NumPack, int Value)
{
	VectorRegister4Int MaskReg = VectorIntSet1(Value);
	VectorRegister4Int* FlagPacks = (VectorRegister4Int*)Data;
	for (int i = 0; i < NumPack; i++)
		VectorIntStore(VectorIntOr(VectorIntLoad(FlagPacks + i), MaskReg), FlagPacks + i);
}

//because FMatrix3x4.SetMatrixTranspose takes FMatrix
inline void SkelotSetMatrix3x4Transpose(FMatrix3x4& DstMatrix, const FMatrix44f& SrcMatrix)
{
	const float* RESTRICT Src = &(SrcMatrix.M[0][0]);
	float* RESTRICT Dest = &(DstMatrix.M[0][0]);

	Dest[0] = Src[0];   // [0][0]
	Dest[1] = Src[4];   // [1][0]
	Dest[2] = Src[8];   // [2][0]
	Dest[3] = Src[12];  // [3][0]

	Dest[4] = Src[1];   // [0][1]
	Dest[5] = Src[5];   // [1][1]
	Dest[6] = Src[9];   // [2][1]
	Dest[7] = Src[13];  // [3][1]

	Dest[8]  = Src[2];  // [0][2]
	Dest[9]  = Src[6];  // [1][2]
	Dest[10] = Src[10]; // [2][2]
	Dest[11] = Src[14]; // [3][2]
}



template<typename ElementT> ElementT SkelotGetElementRandomWeighted(const TMap<ElementT, float>& Items, float RandAlpha)
{
	if (Items.Num() == 0)
		return ElementT();

	TArray<TTuple<float, ElementT>, TInlineAllocator<32>> Sums;
	float Sum = 0;

	for (const auto& Pair : Items)
	{
		float Weight = Pair.Value;
		if (Weight <= 0)
			continue;

		Sum += Weight;
		Sums.Add(MakeTuple(Sum, Pair.Key));
	}

	float ChoiceValue = RandAlpha * Sum;
	for (int i = 0; i < Sums.Num(); i++)
	{
		if (Sums[i].Key >= ChoiceValue)
			return Sums[i].Value;
	}

	return ElementT();
}

//same as FAnimationRuntime::AdvanceTime but we dont support negative play scale
inline ETypeAdvanceAnim SkelotAnimAdvanceTime(bool bAllowLooping, float MoveDelta, float& InOutTime, float EndTime)
{
	InOutTime += MoveDelta;

	if (InOutTime > EndTime)
	{
		if (bAllowLooping)
		{
			InOutTime = FMath::Fmod(InOutTime, EndTime);
			return ETAA_Looped;
		}
		else
		{
			// If not, snap time to end of sequence and stop playing.
			InOutTime = EndTime;
			return ETAA_Finished;
		}
	}

	return ETAA_Default;
}


template<typename T, typename TSize> T* NewPureArray(TSize Count)
{
	T* Elements = (T*)FMemory::Malloc(sizeof(T) * Count);
	DefaultConstructItems<T>(Elements, Count);
	return Elements;
}

template<typename T, typename TSize> void DeletePureArray(T* Elements, TSize Count)
{
	DestructItems<T>(Elements, Count);
	FMemory::Free(Elements);
}

template<typename T> struct TNullTerminatedIter
{
	T* Ptr;

	TNullTerminatedIter(T* In) : Ptr(In) {}

	void operator ++() { Ptr++; }
	operator bool () const { return *Ptr != nullptr; }
	T operator -> () const { return *Ptr; }
	T operator * () const  { return *Ptr; }
	
};

/*
*/
template<typename TElement, int32 NumItemPerChunk> struct TStackIncrementalArray
{
	struct FChunk
	{
		TElement Items[NumItemPerChunk];
		FChunk* Next = nullptr;
	};

	FChunk* First = nullptr;
	FChunk* Last = nullptr;
	int32 Counter = NumItemPerChunk;
	
	void* AddUninitialized()
	{
		if (Counter == NumItemPerChunk) //full or first time ?
		{
			Counter = 0;
			FChunk* NewChunk = new (FMemStack::Get().PushBytes(sizeof(FChunk), alignof(FChunk))) FChunk();
			if (Last)
			{
				Last->Next = NewChunk;
				Last = NewChunk;
			}
			else
			{
				First = Last = NewChunk;
			}
		}

		return &Last->Items[Counter++];
	}
	/** Adds an element to the array. */
	TElement* Add(const TElement& Element)
	{
		return new (AddUninitialized()) TElement(Element);
	}

	/** Adds an element to the array. */
	TElement* Add(TElement&& Element)
	{
		return new (AddUninitialized()) TElement(MoveTemp(Element));
	}

	bool IsEmpty() const { return First == nullptr; }

	template<typename TLambda> void ForEach(TLambda Proc)
	{
		FChunk* Iter = First;
		while (Iter)
		{
			const int32 ElemCount = Iter->Next == nullptr ? Counter : NumItemPerChunk;

			for(int32 i = 0; i < ElemCount; i++)
				Proc(Iter->Items[i]);

			Iter = Iter->Next;
		}
	}
};

template<typename TElement, int32 NumItemPerChunk> struct TStackIncrementalArrayWithMark : TStackIncrementalArray<TElement,NumItemPerChunk>
{
	FMemMark Marker = FMemStack::Get();
};


template<typename TStruct> const FString SkelotStructExportText(const TStruct& In)
{
	return SkelotStructExportText(TStruct::StaticStruct(), &In);
}

FString SkelotStructExportText(const UScriptStruct* StructPtr, const void* InstancePtr);






//see Chaos.TImplicitObjectScaled.Raycast 
template<typename TRayCastFunc> bool SkelotScaledRaycast(Chaos::FVec3 MScale, Chaos::FVec3 MInvScale, const Chaos::FVec3& StartPoint, const Chaos::FVec3& Dir, const Chaos::FReal Length, const Chaos::FReal Thickness, Chaos::FReal& OutTime, Chaos::FVec3& OutPosition, Chaos::FVec3& OutNormal, TRayCastFunc RayCastFunc)
{
	ensure(Length > 0);
	ensure(FMath::IsNearlyEqual(Dir.SizeSquared(), (Chaos::FReal)1, (Chaos::FReal)UE_KINDA_SMALL_NUMBER));
	ensure(Thickness == 0 || (FMath::IsNearlyEqual(MScale[0], MScale[1]) && FMath::IsNearlyEqual(MScale[0], MScale[2])));	//non uniform turns sphere into an ellipsoid so no longer a raycast and requires a more expensive sweep

	const Chaos::FVec3 UnscaledStart = MInvScale * StartPoint;
	const Chaos::FVec3 UnscaledDirDenorm = MInvScale * Dir;
	const Chaos::FReal LengthScale = UnscaledDirDenorm.Size();
	if (ensure(LengthScale > TNumericLimits<Chaos::FReal>::Min()))
	{
		const Chaos::FReal LengthScaleInv = Chaos::FReal(1) / LengthScale;
		const Chaos::FReal UnscaledLength = Length * LengthScale;
		const Chaos::FVec3 UnscaledDir = UnscaledDirDenorm * LengthScaleInv;

		Chaos::FVec3 UnscaledPosition;
		Chaos::FVec3 UnscaledNormal;
		Chaos::FReal UnscaledTime;

		if (RayCastFunc(UnscaledStart, UnscaledDir, UnscaledLength, Thickness * MInvScale[0], UnscaledTime, UnscaledPosition, UnscaledNormal))
		{
			//We double check that NewTime < Length because of potential precision issues. When that happens we always keep the shortest hit first
			const Chaos::FReal NewTime = LengthScaleInv * UnscaledTime;
			if (NewTime == 0) // Normal/Position output may be uninitialized with TOI 0 so we use ray information for that as the ray origin is likely inside the shape
			{
				OutPosition = StartPoint;
				OutNormal = -Dir;
				OutTime = NewTime;
				return true;
			}
			else if (NewTime < Length)
			{
				OutPosition = MScale * UnscaledPosition;
				OutNormal = (MInvScale * UnscaledNormal).GetSafeNormal(TNumericLimits<Chaos::FReal>::Min());
				OutTime = NewTime;
				return true;
			}
		}
	}

	return false;
}

template<typename TRayCastFunc> bool SkelotRotatedRaycast(const Chaos::FRigidTransform3& MTransform, const Chaos::FVec3& StartPoint, const Chaos::FVec3& Dir, const Chaos::FReal Length, const Chaos::FReal Thickness, Chaos::FReal& OutTime, Chaos::FVec3& OutPosition, Chaos::FVec3& OutNormal, TRayCastFunc RayFunc)
{
	Chaos::FVec3 LocalStart = MTransform.InverseTransformPositionNoScale(StartPoint);
	Chaos::FVec3 LocalDir = MTransform.InverseTransformVectorNoScale(Dir);
	Chaos::FVec3 LocalPosition;
	Chaos::FVec3 LocalNormal;

	if (RayFunc(LocalStart, LocalDir, Length, Thickness, OutTime, LocalPosition, LocalNormal))
	{
		if (OutTime != 0.0f)
		{
			OutPosition = MTransform.TransformPositionNoScale(LocalPosition);
			OutNormal = MTransform.TransformVectorNoScale(LocalNormal);
		}
		return true;
	}

	return false;
}

//handles rotation  translation scale
template<typename TRayCastFunc> bool SkelotTransformedRaycast(const Chaos::FRigidTransform3& MTransform, const Chaos::FVec3& StartPoint, const Chaos::FVec3& Dir, const Chaos::FReal Length, const Chaos::FReal Thickness, Chaos::FReal& OutTime, Chaos::FVec3& OutPosition, Chaos::FVec3& OutNormal, TRayCastFunc RayFunc)
{
	auto LRC = [&MTransform, MovedRayFunc = MoveTemp(RayFunc)](const Chaos::FVec3& StartPoint1, const Chaos::FVec3& Dir1, const Chaos::FReal Length1, const Chaos::FReal Thickness1, Chaos::FReal& OutTime1, Chaos::FVec3& OutPosition1, Chaos::FVec3& OutNormal1) {

		const bool bHasScale = !MTransform.GetScale3D().Equals(Chaos::FVec3::OneVector);
		if (bHasScale)
		{
			return SkelotScaledRaycast(MTransform.GetScale3D(), MTransform.GetScale3D().Reciprocal(), StartPoint1, Dir1, Length1, Thickness1, OutTime1, OutPosition1, OutNormal1, MovedRayFunc);
		}
		else
		{
			return MovedRayFunc(StartPoint1, Dir1, Length1, Thickness1, OutTime1, OutPosition1, OutNormal1);
		}
		};

	return SkelotRotatedRaycast(MTransform, StartPoint, Dir, Length, Thickness, OutTime, OutPosition, OutNormal, MoveTemp(LRC));




}





struct FIntRange
{
	int32 Start, Count;
};

void SkelotMergeRanges(FIntRange* InOut, int32 NumRange, int32 MinIndex, int32 MaxIndex);