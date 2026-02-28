// Copyright 2024 Lazy Marmot Games. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Matrix3x4.h"
#include "Math/Matrix.h"
#include "RenderTransform.h"
#include "Engine/NetSerialization.h"


class UMaterialInterface;
class USkelotComponent;
class USkelotAnimCollection;
class FSkelotSkinWeightVertexBuffer;
class FSkeletalMeshLODRenderData;
class FStaticMeshVertexDataInterface;
class FSkelotProxy;



//same as FMatrix3x4 but uses FFloa16
struct alignas(8) FMatrix3x4Half
{
	FFloat16 M[3][4];

	FMatrix3x4Half(){}

	FMatrix3x4Half(const FMatrix3x4& From);

	void SetMatrix(const FMatrix44f& Mat);
	void SetMatrixTranspose(const FMatrix44f& Mat);
};

inline FArchive& operator <<(FArchive& Ar, FMatrix3x4Half& data)
{
	Ar.Serialize(&data, sizeof(FMatrix3x4Half));
	return Ar;
}


inline void SkelotVectorTranspose4x4(VectorRegister4f& A, VectorRegister4f& B, VectorRegister4f& C, VectorRegister4f& D)
{
#ifdef _MM_TRANSPOSE4_PS
	_MM_TRANSPOSE4_PS(A, B, C, D);
#else
	//#TODO Cross Platform Transpose using VectorShuffle maybe ?
	AlignedFloat4 HA(A);
	AlignedFloat4 HB(B);
	AlignedFloat4 HC(C);
	AlignedFloat4 HD(D);

	A = VectorSet(HA[0], HB[0], HC[0], HD[0]);
	B = VectorSet(HA[1], HB[1], HC[1], HD[1]);
	C = VectorSet(HA[2], HB[2], HC[2], HD[2]);
	D = VectorSet(HA[3], HB[3], HC[3], HD[3]);
#endif
}

inline void SkelotVectorTranspose4x4(VectorRegister4d& A, VectorRegister4d& B, VectorRegister4d& C, VectorRegister4d& D)
{
	//#TODO Cross Platform Transpose using VectorShuffle maybe ?
	AlignedDouble4 HA(A);
	AlignedDouble4 HB(B);
	AlignedDouble4 HC(C);
	AlignedDouble4 HD(D);

	A = VectorSet(HA[0], HB[0], HC[0], HD[0]);
	B = VectorSet(HA[1], HB[1], HC[1], HD[1]);
	C = VectorSet(HA[2], HB[2], HC[2], HD[2]);
	D = VectorSet(HA[3], HB[3], HC[3], HD[3]);
}


namespace UE {
namespace Math {

template<typename T> struct TBoxCenterExtent
{
	TVector<T> Center, Extent;

	TBoxCenterExtent() {}
	TBoxCenterExtent(EForceInit In) : Center(In), Extent(In) {}
	TBoxCenterExtent(TVector<T> C, TVector<T> E) : Center(C), Extent(E) {}
	TBoxCenterExtent(const TBox<T>& Box) { Box.GetCenterAndExtents(Center, Extent); }

	TBox<T> GetFBox() const { return TBox<T>(Center - Extent, Center + Extent); }
	void Shift(const FVector& Offset) { Center += Offset; }

	TBoxCenterExtent TransformBy(const TMatrix<T>& M) const
	{
		const auto m0 = VectorLoadAligned(M.M[0]);
		const auto m1 = VectorLoadAligned(M.M[1]);
		const auto m2 = VectorLoadAligned(M.M[2]);
		const auto m3 = VectorLoadAligned(M.M[3]);

		const auto CurOrigin = VectorLoadFloat3(&Center);
		const auto CurExtent = VectorLoadFloat3(&Extent);

		auto NewOrigin = VectorMultiply(VectorReplicate(CurOrigin, 0), m0);
		NewOrigin = VectorMultiplyAdd(VectorReplicate(CurOrigin, 1), m1, NewOrigin);
		NewOrigin = VectorMultiplyAdd(VectorReplicate(CurOrigin, 2), m2, NewOrigin);
		NewOrigin = VectorAdd(NewOrigin, m3);

		auto NewExtent = VectorAbs(VectorMultiply(VectorReplicate(CurExtent, 0), m0));
		NewExtent = VectorAdd(NewExtent, VectorAbs(VectorMultiply(VectorReplicate(CurExtent, 1), m1)));
		NewExtent = VectorAdd(NewExtent, VectorAbs(VectorMultiply(VectorReplicate(CurExtent, 2), m2)));

		TBoxCenterExtent Ret;
		VectorStoreFloat3(NewOrigin, &Ret.Center);
		VectorStoreFloat3(NewExtent, &Ret.Extent);
		return Ret;
	}
	
};


//like FBox but aligned and without bool bIsValid, used for fast bound generation
template<typename T> struct TBoxMinMax
{
	using VecT = TVector<T>;

	TVectorRegisterType<T> Min, Max;

	TBoxMinMax() {}

	TBoxMinMax(EForceInit In)
	{
		Min = VectorSetFloat1(TNumericLimits<T>::Max());
		Max = VectorSetFloat1(-TNumericLimits<T>::Max());
	}
	TBoxMinMax(const TVector<T>& InMin, const TVector<T>& InMax)
	{
		Min = VectorLoadFloat3(&InMin);
		Max = VectorLoadFloat3(&InMax);
	}
	//Adds to this bounding box to include a new bounding volume.
	void Add(const TBoxCenterExtent<T>& R)
	{
		auto RC = VectorLoadFloat3(&R.Center);
		auto RE = VectorLoadFloat3(&R.Extent);
		Min = VectorMin(Min, VectorSubtract(RC, RE));
		Max = VectorMax(Max, VectorAdd(RC, RE));
	}
	void Add(const TBoxMinMax& In)
	{
		Min = VectorMin(Min, In.Min);
		Max = VectorMax(Max, In.Max);
	}
	//transform @OtherBox and add it to this bound 
	void AddTransformed(const TBoxCenterExtent<T>& OtherBox, const TMatrix<T>& M)
	{
		const auto m0 = VectorLoadAligned(M.M[0]);
		const auto m1 = VectorLoadAligned(M.M[1]);
		const auto m2 = VectorLoadAligned(M.M[2]);
		const auto m3 = VectorLoadAligned(M.M[3]);

		const auto CurOrigin = VectorLoadFloat3(&OtherBox.Center);
		const auto CurExtent = VectorLoadFloat3(&OtherBox.Extent);

		auto NewOrigin = VectorMultiply(VectorReplicate(CurOrigin, 0), m0);
		NewOrigin = VectorMultiplyAdd(VectorReplicate(CurOrigin, 1), m1, NewOrigin);
		NewOrigin = VectorMultiplyAdd(VectorReplicate(CurOrigin, 2), m2, NewOrigin);
		NewOrigin = VectorAdd(NewOrigin, m3);

		auto NewExtent = VectorAbs(VectorMultiply(VectorReplicate(CurExtent, 0), m0));
		NewExtent = VectorAdd(NewExtent, VectorAbs(VectorMultiply(VectorReplicate(CurExtent, 1), m1)));
		NewExtent = VectorAdd(NewExtent, VectorAbs(VectorMultiply(VectorReplicate(CurExtent, 2), m2)));

		Min = VectorMin(Min, VectorSubtract(NewOrigin, NewExtent));
		Max = VectorMax(Max, VectorAdd(NewOrigin, NewExtent));
	}
	void Add(const TVectorRegisterType<T>& InVector)
	{
		Min = VectorMin(Min, InVector);
		Max = VectorMax(Max, InVector);
	}
	void Add(const TVector<T>& InVector)
	{
		Add(VectorLoadFloat3(&InVector));
	}
	void Shift(const TVector<T>& Offset)
	{
		auto VOffset = VectorLoadFloat3(&Offset);
		Min = VectorAdd(Min, VOffset);
		Max = VectorAdd(Max, VOffset);
	}
	bool IsForceInitValue() const
	{
		return VectorMaskBits(VectorBitwiseAnd(VectorCompareEQ(Min, VectorSetFloat1(TNumericLimits<T>::Max())), VectorCompareEQ(Max, VectorSetFloat1(-TNumericLimits<T>::Max())))) == 0xF;
	}
	void Expand(const TVector<T>& Value)
	{
		auto V = VectorLoadFloat3(&Value);
		Min = VectorSubtract(Min, V);
		Max = VectorAdd(Max, V);
	}
	void GetSize(TVector<T>& Out) const
	{
		VectorStoreFloat3(VectorSubtract(Max, Min), &Out);
	}

	TVector<T>& GetMin() { return *reinterpret_cast<TVector<T>*>(&Min); }
	TVector<T>& GetMax() { return *reinterpret_cast<TVector<T>*>(&Max); }

	const TVector<T>& GetMin() const { return *reinterpret_cast<const TVector<T>*>(&Min); }
	const TVector<T>& GetMax() const { return *reinterpret_cast<const TVector<T>*>(&Max); }

	void ToCenterExtentBox(TBoxCenterExtent<T>& Out) const
	{
		auto Extent = VectorMultiply(VectorSubtract(Max, Min), VectorSetFloat1(T(0.5)));
		VectorStoreFloat3(VectorAdd(Min, Extent), &Out.Center);
		VectorStoreFloat3(Extent, &Out.Extent);
	}

	TBox<T> ToBox() const { return TBox<T>(*reinterpret_cast<const TVector<T>*>(&Min), *reinterpret_cast<const TVector<T>*>(&Max)); }
	TBoxSphereBounds<T> ToBoxSphereBounds() const { return TBoxSphereBounds<T>(ToBox()); }

	FRenderBounds ToRenderBounds() const { return FRenderBounds((FVector3f)GetMin(), (FVector3f)GetMax()); }

	bool IsNearlyEqual(const TBoxMinMax& Other, T ErrorTolerance = UE_SMALL_NUMBER) const
	{
		const auto VT = VectorSetFloat1(ErrorTolerance);
		const int Mask = VectorMaskBits(VectorBitwiseAnd(
			VectorCompareLE(VectorAbs(VectorSubtract(Min, Other.Min)), VT), 
			VectorCompareLE(VectorAbs(VectorSubtract(Max, Other.Max)), VT)));

		return (Mask & 0b111) == 0b111;
	}
};

/*
@InBox					box to be transformed
@M						Matrix to transform with
@OutTransformedBound	the transformed box
@IncreasingBound		box that OutTransformedBound is be added to
*/
void TransformCenteredBoxEx(const TBoxCenterExtent<float>& InBox, const FMatrix44f& M, TBoxCenterExtent<float>& OutTransformedBound, TBoxMinMax<float>& IncreasingBound);


};
};

using FBoxCenterExtentFloat = UE::Math::TBoxCenterExtent<float>;
using FBoxCenterExtentDouble = UE::Math::TBoxCenterExtent<double>;

using FBoxMinMaxFloat = UE::Math::TBoxMinMax<float>;
using FBoxMinMaxDouble = UE::Math::TBoxMinMax<double>;


template<typename T> inline FArchive& operator <<(FArchive& Ar, UE::Math::TBoxCenterExtent<T>& Bound)
{
	Ar.Serialize(&Bound.Center, sizeof(T[3]));
	Ar.Serialize(&Bound.Extent, sizeof(T[3]));
	return Ar;
}

template<typename T> inline FArchive& operator <<(FArchive& Ar, UE::Math::TBoxMinMax<T>& Bound)
{
	Ar.Serialize(&Bound.Min, sizeof(T[3]));
	Ar.Serialize(&Bound.Max, sizeof(T[3]));
	return Ar;
}

inline float GetBoxCoveringRadius(const FBox3f& Box)
{
	return FMath::Max(Box.Max.Size(), Box.Min.Size());
}

namespace UE {
namespace Math {
template<typename T> struct alignas(16) TMatrix3x4NonTranspose
{
	T M[12];

	TMatrix3x4NonTranspose()
	{
	}
	TMatrix3x4NonTranspose(const TMatrix<T>& RESTRICT Src)
	{
		M[0] = Src.M[0][0];
		M[1] = Src.M[0][1];
		M[2] = Src.M[0][2];

		M[3] = Src.M[1][0];
		M[4] = Src.M[1][1];
		M[5] = Src.M[1][2];

		M[6] = Src.M[2][0];
		M[7] = Src.M[2][1];
		M[8] = Src.M[2][2];

		M[9]  = Src.M[3][0]; //Origin.X
		M[10] = Src.M[3][1]; //Origin.Y
		M[11] = Src.M[3][2]; //Origin.Z
	}
	inline TMatrix3x4NonTranspose& operator = (const TMatrix<T>& Src) { return *( new (this) TMatrix3x4NonTranspose(Src)); }
	//
	inline TMatrix3x4NonTranspose& operator = (const TMatrix3x4NonTranspose& R)
	{
		VectorStoreAligned(VectorLoadAligned(&R.M[0]), &M[0]);
		VectorStoreAligned(VectorLoadAligned(&R.M[4]), &M[4]);
		VectorStoreAligned(VectorLoadAligned(&R.M[8]), &M[8]);
		return *this;
	}
	TVector<T> GetOrigin() const 
	{
		//FMatrix::GetOrigin() ===> return FVector(M[3][0], M[3][1], M[3][2]);
		return TVector<T>(M[9], M[10], M[11]);
	}
	void SetOrigin(const TVector<T>& O) 
	{
		M[9] = O.X; M[10] = O.Y; M[11] = O.Z;
	}

	TMatrix<T> ToMatrix4x4() const
	{
		TMatrix<T> RetValue = TMatrix<T>::Identity;
		RetValue.SetAxis(0, TVector<T>(M[0], M[1], M[2]));
		RetValue.SetAxis(1, TVector<T>(M[3], M[4], M[5]));
		RetValue.SetAxis(2, TVector<T>(M[6], M[7], M[8]));
		RetValue.SetAxis(3, TVector<T>(M[9], M[10], M[11]));
		return RetValue;
	}
	explicit operator TMatrix<T>() const
	{
		return ToMatrix4x4();
	}
	friend FArchive& operator <<(FArchive& Ar, TMatrix3x4NonTranspose& Matrix)
	{
		Ar.Serialize(Matrix.M, sizeof(Matrix.M));
		return Ar;
	}
};


template<typename T> struct alignas(16) TMatrix3x4Transpose
{
	T M[12];

	TMatrix3x4Transpose()
	{
	}
	TMatrix3x4Transpose(const TMatrix<T>& RESTRICT Mat)
	{
		const T* RESTRICT Src = &(Mat.M[0][0]);
		T* RESTRICT Dst = &(M[0]);
		auto A = VectorLoadAligned(Src + 0);
		auto B = VectorLoadAligned(Src + 4);
		auto C = VectorLoadAligned(Src + 8);
		auto D = VectorLoadAligned(Src + 12);
		SkelotVectorTranspose4x4(A, B, C, D);
		VectorStoreAligned(A, Dst + 0);
		VectorStoreAligned(B, Dst + 4);
		VectorStoreAligned(C, Dst + 8);
	}
	//
	inline TMatrix3x4Transpose& operator = (const TMatrix<T>& Mat) { return *(new (this) TMatrix3x4Transpose(Mat)); }
	//
	inline TMatrix3x4Transpose& operator = (const TMatrix3x4Transpose& R)
	{
		VectorStoreAligned(VectorLoadAligned(&R.M[0]), &M[0]);
		VectorStoreAligned(VectorLoadAligned(&R.M[4]), &M[4]);
		VectorStoreAligned(VectorLoadAligned(&R.M[8]), &M[8]);
		return *this;
	}
	TVector<T> GetOrigin() const 
	{
		//FMatrix::GetOrigin() ===> return FVector(M[3][0], M[3][1], M[3][2]);
		return TVector(M[3], M[7], M[11]); 
	}
	void SetOrigin(const TVector<T>& O) 
	{
		M[3] = O.X; M[7] = O.Y; M[11] = O.Z; 
	}
	TMatrix<T> ToMatrix4x4() const
	{
		auto A = VectorLoadAligned(this->M + 0);
		auto B = VectorLoadAligned(this->M + 4);
		auto C = VectorLoadAligned(this->M + 8);
		auto D = VectorSet(T(0), T(0), T(0), T(1));
		SkelotVectorTranspose4x4(A, B, C, D);
		TMatrix<T> RetValue;
		T* RESTRICT Dst = RetValue.M;
		VectorStoreAligned(A, Dst + 0);
		VectorStoreAligned(B, Dst + 4);
		VectorStoreAligned(C, Dst + 8);
		VectorStoreAligned(D, Dst + 12);
		return RetValue;
	}
	explicit operator TMatrix<T>() const
	{
		return ToMatrix4x4();
	}
	friend FArchive& operator <<(FArchive& Ar, TMatrix3x4Transpose& Matrix)
	{
		Ar.Serialize(Matrix.M, sizeof(Matrix.M));
		return Ar;
	}
};

};

};

typedef UE::Math::TMatrix3x4Transpose<float> SkelotShaderMatrixT;	//this is the matrix we send to shader :|


template<typename T> struct TVectorStructOpsTypeTraits_ZeroCommon : public TStructOpsTypeTraitsBase2<T>
{
	enum
	{
		WithZeroConstructor = true,
		WithIdenticalViaEquality = true,
	};
};