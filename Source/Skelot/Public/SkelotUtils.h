// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "VectorTypes.h"
#include "LineTypes.h"



namespace FSkelotUtils
{

	//////////////////////////////////////////////////////////////////////////
	template<class ActorT> ActorT* GetActorOfClass(UWorld* World, TSubclassOf<ActorT> Class = ActorT::StaticClass())
	{
		for (TActorIterator<ActorT> It(World, Class); It; ++It)
		{
			ActorT* Actor = *It;
			return Actor;
		}
		return nullptr;
	}
	//////////////////////////////////////////////////////////////////////////
	template<class ActorT, typename AllocT> void GetAllActorOfClass(UWorld* World, TArray<ActorT*, AllocT>& OutActors, TSubclassOf<ActorT> Class = ActorT::StaticClass())
	{
		for (TActorIterator<ActorT> It(World, Class); It; ++It)
		{
			OutActors.Add(*It);
		}
	}
	//////////////////////////////////////////////////////////////////////////
	template<class ActorT> TArray<ActorT*> GetAllActorOfClass(UWorld* World, TSubclassOf<ActorT> Class = ActorT::StaticClass())
	{
		TArray<ActorT*> Actors;
		for (TActorIterator<ActorT> It(World, Class); It; ++It)
			Actors.Add(*It);
		
		return Actors;
	}
	//////////////////////////////////////////////////////////////////////////
	template<class ActorT> ActorT* GetClosestActorOfClass2D(UWorld* World, FVector Location, AActor* Except = nullptr, TSubclassOf<ActorT> Class = ActorT::StaticClass())
	{
		ActorT* ClosetActor = nullptr;
		double ClosestDist = 0;

		for (TActorIterator<ActorT> It(World, Class); It; ++It)
		{
			ActorT* Act = *It;
			if (!Act || Act == Except)
				continue;

			auto Dist = FVector::DistSquared2D(Act->GetActorLocation(), Location);
			if(Dist < ClosestDist || ClosetActor == nullptr)
			{
				ClosestDist = Dist;
				ClosetActor = Act;
			}
		}

		return ClosetActor;
	}
	//////////////////////////////////////////////////////////////////////////
	template<typename ElementT, typename AllocT > ElementT GetArrayElementRandom(const TArray<ElementT, AllocT>& InArray)
	{
		if (InArray.Num() > 0)
			return InArray[FMath::RandHelper(InArray.Num())];

		return ElementT();
	}
	//////////////////////////////////////////////////////////////////////////
	template<typename ElementT, typename AllocT > ElementT GetArrayElementRandom(const TArray<ElementT, AllocT>& InArray, FRandomStream& Rnd)
	{
		if (InArray.Num() > 0)
			return InArray[Rnd.RandHelper(InArray.Num())];

		return ElementT();
	}
	//////////////////////////////////////////////////////////////////////////
	template<typename ElementT, typename L> ElementT GetArrayElementRandomWeighted(TArrayView<ElementT>& Items, FRandomStream& Rnd, L Proc)
	{
		if (Items.Num() == 0)
			return ElementT();
		
		int32 Idx = GetArrayIndexRandomWeighted(Items, Rnd, Proc);
		return Items[Idx];
	}
	//////////////////////////////////////////////////////////////////////////
	template<typename ElementT, typename L> int32 GetArrayIndexRandomWeighted(TArrayView<ElementT> Items, FRandomStream& Rnd, L Proc)
	{
		if (Items.Num() == 0)
			return -1;
		if (Items.Num() == 1)
			return 0;

		TArray<TTuple<float, int32>, TInlineAllocator<32>> Sums;

		float Sum = 0;

		for (int i = 0; i < Items.Num(); i++)
		{
			float Weight = Proc(Items[i]);
			if (Weight <= 0)
				continue;

			Sum += Weight;
			Sums.Add(MakeTuple(Sum, i));
		}

		float ChoiceValue = Rnd.FRand() * Sum;
		for (int i = 0; i < Sums.Num(); i++)
		{
			if (Sums[i].Key >= ChoiceValue)
				return Sums[i].Value;
		}

		return 0;
	}
	//////////////////////////////////////////////////////////////////////////
	//more value more chance, 0 == no chance at all
	template<typename ElementT> ElementT GetMapElementRandomWeighted(const TMap<ElementT, float>& Items, float RandAlpha)
	{
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
	//////////////////////////////////////////////////////////////////////////
	template<typename ElementT> ElementT GetMapElementRandomWeighted(const TMap<ElementT, float>& Items) { return GetMapElementRandomWeighted(Items, FMath::FRand()); }
	//////////////////////////////////////////////////////////////////////////
	template<typename ElementT> ElementT GetMapElementRandomWeighted(const TMap<ElementT, float>& Items, FRandomStream& Rnd) { return GetMapElementRandomWeighted(Items, Rnd.FRand()); }
	
	
	//////////////////////////////////////////////////////////////////////////
	void DrawDebugCircle2D(const UWorld* InWorld, FVector Center, float Radius, int32 Segments, const FColor& Color, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0, float Thickness = 0.f);
	//////////////////////////////////////////////////////////////////////////
	inline FVector YawToVector(float YawRadian)
	{
		float SY, CY;
		FMath::SinCos(&SY, &CY, YawRadian);
		return FVector(CY, SY, 0);
	}
	inline FVector2D YawToVector2D(float YawRadian)
	{
		float SY, CY;
		FMath::SinCos(&SY, &CY, YawRadian);
		return FVector2D(CY, SY);
	}
	//////////////////////////////////////////////////////////////////////////
	inline float InterpAngleDegree(float A, float B, float Delta, float InterpSpeed)
	{
		FMath::WindRelativeAnglesDegrees(A, B);
		return FMath::FInterpTo(A, B, Delta, InterpSpeed);
	}
	//////////////////////////////////////////////////////////////////////////
	inline float InterpAngleRadian(float A, float B, float Delta, float InterpSpeed)
	{
		A = FMath::RadiansToDegrees(A);
		B = FMath::RadiansToDegrees(B);
		return FMath::DegreesToRadians(InterpAngleDegree(A, B, Delta, InterpSpeed));
	}

	//
	inline float CircleArea(float R) { return PI * R * R; }
	//
	inline float CircleCircumference(float R) { return PI * 2 * R; }

	//same as FVector.GetClampedToMaxSize but for 2D Vector
	inline FVector2D GetClampedToMaxSize(FVector2D V, float MaxSize)
	{
		if (MaxSize < SMALL_NUMBER)
			return FVector2D::ZeroVector;

		const float VSq = V.SizeSquared();
		if (VSq > FMath::Square(MaxSize))
		{
			const float Scale = MaxSize * FMath::InvSqrt(VSq);
			return FVector2D(V.X * Scale, V.Y * Scale);
		}
		else
		{
			return V;
		}
	}

	inline float CircleFallOff(float Value, float Radius, float FallOff)
	{
		checkSlow(FallOff >= 0 && Radius >= 0 && Value >= 0);

		if(Value >= (Radius + FallOff)) //out of outer radius ?
			return 0;

		if(FallOff == 0 || Value <= Radius) //no falloff or inside inner radius ?
			return 1;

		return 1.0f - ((Value - Radius) / FallOff);
	}
	 
	
	template<bool IsRadian> inline float LerpAngle(float A, float B, float Alpha)
	{
		float D = IsRadian ? FMath::UnwindRadians(B - A) : FMath::UnwindDegrees(B - A);
		return (A + Alpha * D);
	}
	template<bool IsRadian> float BiLerpAngle(float P00, float P10, float P01, float P11, float FracX, float FracY)
	{
		return LerpAngle<IsRadian>(LerpAngle<IsRadian>(P00, P10, FracX), LerpAngle<IsRadian>(P01, P11, FracX), FracY);
	}
	template<typename T> T RandRange(const UE::Math::TVector2<T>& MinMax, FRandomStream& Rnd)
	{
		return (T)Rnd.FRandRange(MinMax.X, MinMax.Y);
	}
	template<typename T> T RandRange(const UE::Math::TVector2<T>& MinMax)
	{
		return (T)FMath::FRandRange(MinMax.X, MinMax.Y);
	}

	inline uint32 PackRGBA8(FLinearColor C)
	{
		uint32 r = ((uint32)(C.R * 255.0f)) << 0;
		uint32 g = ((uint32)(C.G * 255.0f)) << 8;
		uint32 b = ((uint32)(C.B * 255.0f)) << 16;
		uint32 a = ((uint32)(C.A * 255.0f)) << 24;
		return r | g | b | a;
	}
};

