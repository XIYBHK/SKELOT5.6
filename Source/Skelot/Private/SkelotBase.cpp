// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotBase.h"


FMatrix3x4Half::FMatrix3x4Half(const FMatrix3x4& From)
{
	const FLinearColor* RESTRICT Src = (FLinearColor*)&From;
	FFloat16Color* RESTRICT Dst = (FFloat16Color*)this;
	Dst[0] = Src[0];
	Dst[1] = Src[1];
	Dst[2] = Src[2];
}

void FMatrix3x4Half::SetMatrix(const FMatrix44f& Mat)
{
	const float* RESTRICT Src = &(Mat.M[0][0]);
	FFloat16* RESTRICT Dest = &(M[0][0]);

	Dest[0] = (float)Src[0];   // [0][0]
	Dest[1] = (float)Src[1];   // [0][1]
	Dest[2] = (float)Src[2];   // [0][2]
	Dest[3] = (float)Src[3];   // [0][3]

	Dest[4] = (float)Src[4];   // [1][0]
	Dest[5] = (float)Src[5];   // [1][1]
	Dest[6] = (float)Src[6];   // [1][2]
	Dest[7] = (float)Src[7];   // [1][3]

	Dest[8] = (float)Src[8];   // [2][0]
	Dest[9] = (float)Src[9];   // [2][1]
	Dest[10] = (float)Src[10]; // [2][2]
	Dest[11] = (float)Src[11]; // [2][3]
}

void FMatrix3x4Half::SetMatrixTranspose(const FMatrix44f& Mat)
{
	const float* RESTRICT Src = &(Mat.M[0][0]);
	FFloat16* RESTRICT Dest = &(M[0][0]);

	Dest[0] = (float)Src[0];   // [0][0]
	Dest[1] = (float)Src[4];   // [1][0]
	Dest[2] = (float)Src[8];   // [2][0]
	Dest[3] = (float)Src[12];  // [3][0]

	Dest[4] = (float)Src[1];   // [0][1]
	Dest[5] = (float)Src[5];   // [1][1]
	Dest[6] = (float)Src[9];   // [2][1]
	Dest[7] = (float)Src[13];  // [3][1]

	Dest[8] = (float)Src[2];   // [0][2]
	Dest[9] = (float)Src[6];   // [1][2]
	Dest[10] = (float)Src[10]; // [2][2]
	Dest[11] = (float)Src[14]; // [3][2]
}

void UE::Math::TransformCenteredBoxEx(const TBoxCenterExtent<float>& In, const FMatrix44f& M, TBoxCenterExtent<float>& OutTransformedBound, TBoxMinMax<float>& IncreasingBound)
{
	
	const auto m0 = VectorLoadAligned(M.M[0]);
	const auto m1 = VectorLoadAligned(M.M[1]);
	const auto m2 = VectorLoadAligned(M.M[2]);
	const auto m3 = VectorLoadAligned(M.M[3]);

	const auto CurOrigin = VectorLoadFloat3(&In.Center);
	const auto CurExtent = VectorLoadFloat3(&In.Extent);

	auto NewOrigin = VectorMultiply(VectorReplicate(CurOrigin, 0), m0);
	NewOrigin = VectorMultiplyAdd(VectorReplicate(CurOrigin, 1), m1, NewOrigin);
	NewOrigin = VectorMultiplyAdd(VectorReplicate(CurOrigin, 2), m2, NewOrigin);
	NewOrigin = VectorAdd(NewOrigin, m3);

	auto NewExtent = VectorAbs(VectorMultiply(VectorReplicate(CurExtent, 0), m0));
	NewExtent = VectorAdd(NewExtent, VectorAbs(VectorMultiply(VectorReplicate(CurExtent, 1), m1)));
	NewExtent = VectorAdd(NewExtent, VectorAbs(VectorMultiply(VectorReplicate(CurExtent, 2), m2)));

	IncreasingBound.Min = VectorMin(IncreasingBound.Min, VectorSubtract(NewOrigin, NewExtent));
	IncreasingBound.Max = VectorMax(IncreasingBound.Max, VectorAdd(NewOrigin, NewExtent));

	VectorStoreFloat3(NewOrigin, &OutTransformedBound.Center);
	VectorStoreFloat3(NewExtent, &OutTransformedBound.Extent);
}

