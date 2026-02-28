// Copyright 2024 Lazy Marmot Games. All Rights Reserved.


#pragma once


#include "Engine/SkeletalMesh.h"
#include "VertexFactory.h"
#include "SkelotBase.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "RenderResource.h"
#include "SceneManagement.h"
#include "Skelot.h"

class FSkelotSkinWeightVertexBuffer;
class USkelotAnimCollection;
class FSkeletalMeshLODRenderData;
class FSkelotBoneIndexVertexBuffer;
class FSkelotVertexFactory;

//shared for one aim collection
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FSkelotAnimCollectionUniformParams, SKELOT_API)
SHADER_PARAMETER(uint32, BoneCount)
SHADER_PARAMETER(uint32, CurveCount)
SHADER_PARAMETER(uint32, FrameCountSequences)
SHADER_PARAMETER(uint32, MaxTransitionPose)
SHADER_PARAMETER_SRV(Buffer<float4>, AnimationBuffer)
SHADER_PARAMETER_SRV(Buffer<float>, CurveBuffer)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

typedef TUniformBufferRef<FSkelotAnimCollectionUniformParams> FSkelotAnimCollectionUniformParamsRef;


BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FSkelotMeshVertexFetchParams, SKELOT_API)
SHADER_PARAMETER(uint32, ColorIndexMask)
SHADER_PARAMETER(uint32, NumTexCoords)
SHADER_PARAMETER(uint32, NumBoneInfluence) //buffer's number of bone influence, 4 or 8
SHADER_PARAMETER(uint32, Unused1)
SHADER_PARAMETER_SRV(Buffer<float2>, VF_TexCoordBuffer)
SHADER_PARAMETER_SRV(Buffer<float> , VF_PositionBuffer)
SHADER_PARAMETER_SRV(Buffer<float4>, VF_PackedTangentsBuffer)
SHADER_PARAMETER_SRV(Buffer<float4>, VF_ColorComponentsBuffer)
SHADER_PARAMETER_SRV(Buffer<float4>, VF_BoneWeights)
SHADER_PARAMETER_SRV(Buffer<uint4> , VF_BoneIndices)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

typedef TUniformBufferRef<FSkelotMeshVertexFetchParams> FSkelotMeshVertexFetchParamsRef;

////used when not using GPUScene
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FSkelotVertexFactoryParameters, SKELOT_API)
	SHADER_PARAMETER(uint32, NumCustomDataFloats)
	SHADER_PARAMETER(uint32, InstanceOffset)
	SHADER_PARAMETER_SRV(Buffer<float>, Instance_Transforms)
	SHADER_PARAMETER_SRV(Buffer<uint>, Instance_AnimationFrameIndices)
	SHADER_PARAMETER_SRV(Buffer<float>, Instance_CustomData)
	SHADER_PARAMETER_SRV(Buffer<uint>, Instance_DataIndex)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

typedef TUniformBufferRef<FSkelotVertexFactoryParameters> FSkelotVertexFactoryBufferRef;

/*
* used for rendering 1 instance or many through GPUScene
*/
class FSkelotVertexFactory : public FVertexFactory
{
public:
	typedef FVertexFactory Super;


	struct FDataType : FStaticMeshDataType
	{
		FVertexStreamComponent BoneIndices;
		FVertexStreamComponent BoneWeights;
		FVertexStreamComponent ExtraBoneIndices;
		FVertexStreamComponent ExtraBoneWeights;
		int32 MaxBoneInfluence; //max bone influence of shader 
	};

	FDataType Data;
	FRHIUniformBuffer* VertexFetchUB = nullptr;
	FRHIUniformBuffer* AnimCollectionUB = nullptr;	//pointer back to USkelotAnimCollection.AnimCollectionUB 

	FSkelotVertexFactory(ERHIFeatureLevel::Type InFeatureLevel) : Super(InFeatureLevel) { }

	void InitRHI(FRHICommandListBase& RHICmdList) override;
	void FillData(FRHICommandListBase& RHICmdList, const FSkelotBoneIndexVertexBuffer* BoneIndexBuffer, const FSkeletalMeshLODRenderData* LODData);
};


/*
*/
template<int MBI> class TSkelotVertexFactory : public FSkelotVertexFactory
{
public:
	DECLARE_VERTEX_FACTORY_TYPE(TSkelotVertexFactory);

	typedef FSkelotVertexFactory Super;
	
	TSkelotVertexFactory(ERHIFeatureLevel::Type InFeatureLevel) : Super(InFeatureLevel) 
	{
		Data.MaxBoneInfluence = MBI; 
	}

	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
	static bool SupportsTessellationShaders() { return false; }
	static void ValidateCompiledResult(const FVertexFactoryType* Type, EShaderPlatform Platform, const FShaderParameterMap& ParameterMap, TArray<FString>& OutErrors);

};

class FSkelotBoneIndexVertexBuffer : public FVertexBufferWithSRV
{
public:
	FStaticMeshVertexDataInterface* BoneData = nullptr;
	bool bIs16BitBoneIndex = false;
	int MaxBoneInfluences = 0;
	int NumVertices = 0;

	~FSkelotBoneIndexVertexBuffer()
	{
		if (BoneData)
		{
			delete BoneData;
			BoneData = nullptr;
		}
	}

	void Serialize(FArchive& Ar);

	void InitRHI(FRHICommandListBase& RHICmdList) override;
	void ReleaseRHI() override;
	
	void ResizeBuffer()
	{
		if(!BoneData)
		{
			if(bIs16BitBoneIndex)
				BoneData = new TStaticMeshVertexData<uint16>();
			else
				BoneData = new TStaticMeshVertexData<uint8>();
		}
		check(MaxBoneInfluences == 4 || MaxBoneInfluences == 8);
		BoneData->ResizeBuffer(NumVertices * MaxBoneInfluences);
		FMemory::Memzero(BoneData->GetDataPointer(), BoneData->GetResourceSize());
	}
	void SetBoneIndex(uint32 VertexIdx, uint32 InfluenceIdx, uint32 BoneIdx)
	{
		if(bIs16BitBoneIndex)
		{
			FBoneIndex16* Data = ((FBoneIndex16*)BoneData->GetDataPointer());
			Data[VertexIdx * MaxBoneInfluences + InfluenceIdx] = static_cast<FBoneIndex16>(BoneIdx);
		}
		else
		{
			FBoneIndex8* Data = ((FBoneIndex8*)BoneData->GetDataPointer());
			Data[VertexIdx * MaxBoneInfluences + InfluenceIdx] = static_cast<FBoneIndex8>(BoneIdx);
		}
	}

	uint32 GetBufferSizeInBytes() const
	{
		return NumVertices * MaxBoneInfluences * (bIs16BitBoneIndex ? 2u : 1u);
	}
	
};

//vertex buffer containing bone transforms of all baked animations
class FSkelotAnimationBuffer : public FRenderResource
{
public:
	FStaticMeshVertexDataInterface* Transforms = nullptr;

	FBufferRHIRef Buffer;
	FShaderResourceViewRHIRef ShaderResourceViewRHI;
	FUnorderedAccessViewRHIRef UAV;
	bool bHighPrecision = false;
	
	~FSkelotAnimationBuffer();
	void InitRHI(FRHICommandListBase& RHICmdList) override;
	void ReleaseRHI() override;
	void Serialize(FArchive& Ar);

	void AllocateBuffer();
	void InitBuffer(const TArrayView<FTransform> InTransforms, bool InHightPrecision);
	void InitBuffer(uint32 NumMatrix, bool InHightPrecision, bool bFillIdentity);
	void DestroyBuffer();

	FMatrix3x4* GetDataPointerHP() const;
	FMatrix3x4Half* GetDataPointerLP() const;

};

//buffer containing value of curve animations
class FSkelotCurveBuffer : public FRenderResource
{
public:
	static_assert(sizeof(FFloat16) == 2);

	TResourceArray<FFloat16> Values;
	FBufferRHIRef Buffer;
	FShaderResourceViewRHIRef ShaderResourceViewRHI;
	FUnorderedAccessViewRHIRef UAV;
	int32 NumCurve = 0;

	void InitRHI(FRHICommandListBase& RHICmdList) override;
	void ReleaseRHI() override;

	void InitBuffer(uint32 NumFrame, int32 InNumCurve)
	{
		NumCurve = InNumCurve;
		Values.SetNumZeroed(NumFrame * InNumCurve);
	}
};

/*
*/
class FSkelotMeshDataEx :  public TSharedFromThis<FSkelotMeshDataEx>
{
public:
	static const int MAX_INFLUENCE = 8;

	struct FLODData
	{
		FSkelotBoneIndexVertexBuffer BoneData;
		
		static const int32 NUM_VERTEX_FACTORY = SKELOT_WITH_EXTRA_BONE ? 4 : 2;

		TUniquePtr<FSkelotVertexFactory> VertexFactories[NUM_VERTEX_FACTORY];

		const FSkeletalMeshLODRenderData* SkelLODData = nullptr;
		
		FSkelotMeshVertexFetchParamsRef VertexFetchUB;

		FLODData(ERHIFeatureLevel::Type InFeatureLevel);
		void InitResources(FRHICommandListBase& RHICmdList, const USkelotAnimCollection* AC);
		void ReleaseResources();

		FSkelotVertexFactory* GetVertexFactory(int32 MBI) const;
		void Serialize(FArchive& Ar);
	};

	//accessed by [SkeletalMeshLODIndex - BaseLOD]
	TArray<FLODData, TFixedAllocator<SKELOT_MAX_LOD>> LODs;

	void InitFromMesh(int InBaseLOD, USkeletalMesh* SKMesh, const USkelotAnimCollection* AnimSet);
	void InitResources(FRHICommandListBase& RHICmdList, const USkelotAnimCollection* AC);
	void ReleaseResources();
	void Serialize(FArchive& Ar);

	void InitMeshData(const FSkeletalMeshRenderData* SKRenderData, int InBaseLOD);

	uint32 GetTotalBufferSize() const;

};

typedef TSharedPtr<FSkelotMeshDataEx, ESPMode::ThreadSafe> FSkelotMeshDataExPtr;
