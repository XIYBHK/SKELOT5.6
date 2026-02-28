// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#if CPP
#include "SkelotBase.h"
#endif

#include "SkelotBaseBP.generated.h"


#if !CPP

// Skelot盒体最小最大值（浮点）
USTRUCT(noexport, BlueprintType)
struct alignas(16) FBoxMinMaxFloat
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skelot|数学", meta = (DisplayName = "最小值"))
	FVector3f Min;
	//
	float Unused;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skelot|数学", meta = (DisplayName = "最大值"))
	FVector3f Max;
	//
	float Unused2;
};

// Skelot盒体最小最大值（双精度）
USTRUCT(noexport, BlueprintType)
struct alignas(16) FBoxMinMaxDouble
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skelot|数学", meta = (DisplayName = "最小值"))
	FVector Min;
	//
	double Unused;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skelot|数学", meta = (DisplayName = "最大值"))
	FVector Max;
	//
	double Unused2;
};

// Skelot盒体中心范围（浮点）
USTRUCT(noexport, BlueprintType)
struct FBoxCenterExtentFloat
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skelot|数学", meta = (DisplayName = "中心"))
	FVector3f Center;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skelot|数学", meta = (DisplayName = "范围"))
	FVector3f Extent;
};

// Skelot盒体中心范围（双精度）
USTRUCT(noexport, BlueprintType)
struct FBoxCenterExtentDouble
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skelot|数学", meta = (DisplayName = "中心"))
	FVector Center;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skelot|数学", meta = (DisplayName = "范围"))
	FVector Extent;
};

#endif