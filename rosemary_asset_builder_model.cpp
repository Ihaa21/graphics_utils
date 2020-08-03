/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Ihor Szlachtycz $
   $Notice: (C) Copyright 2014 by Dream.Inc, Inc. All Rights Reserved. $
   ======================================================================== */

/*
    NOTE: For rendering, we want multiple materials per model. We can either have the material id
          per vertex, or we can break the model up into prim sets, each with one material.

          Decision: Break the model up. I don't think for regular models that we care about blending
                    materials in the art pipeline. This may change, but I don't think it will.

          Then, we have the following data:

          Model: Has a array of meshes, each with a corresponding material. Each mesh stores a array
                 of vertices, each material has BRDF data in it.

          Obj files are annoying because they account for everything. They are not compact in their
          storage of model data.

          We can have multiple objects + object groups in a obj file. For this loader, we will ignore
          these completely.

          The file can declare materials during the face part, and it can declare the same material
          twice in different places. The loader has to aggregate everything then. 
 */

//
// NOTE: Animation loading functions
//

inline m4 AssimpLoadM4(aiMatrix4x4t<f32> Mat)
{
    m4 Result = {};
    
    // NOTE: Copy over matrix (converting row major to column major)
    Result.e[0] = Mat.a1;
    Result.e[1] = Mat.b1;
    Result.e[2] = Mat.c1;
    Result.e[3] = Mat.d1;

    Result.e[4] = Mat.a2;
    Result.e[5] = Mat.b2;
    Result.e[6] = Mat.c2;
    Result.e[7] = Mat.d2;

    Result.e[8] = Mat.a3;
    Result.e[9] = Mat.b3;
    Result.e[10] = Mat.c3;
    Result.e[11] = Mat.d3;

    Result.e[12] = Mat.a4;
    Result.e[13] = Mat.b4;
    Result.e[14] = Mat.c4;
    Result.e[15] = Mat.d4;

    return Result;
}

inline i32 AssimpGetLoadedBoneId(loader_model* Model, string BoneName)
{
    i32 Result = -1;
    for (i32 StoredBoneId = 0; StoredBoneId < (i32)Model->NumBones; ++StoredBoneId)
    {
        if (StringsEqual(BoneName, Model->BoneArray[StoredBoneId].Name))
        {
            Result = StoredBoneId;
            break;
        }
    }

    return Result;
}

inline void AssimpLoadBoneTransforms(loader_model* Model, m4* DstBoneTransforms, u32 FrameId,
                                     aiAnimation* AiAnimation, aiNode* AiNode, m4 ParentTransform,
                                     u32* NumBonesWritten)
{
    string NodeName = String(AiNode->mName.data);
    i32 DstBoneId = AssimpGetLoadedBoneId(Model, NodeName);
    
    m4 NodeTransform = {};
    if (DstBoneId == -1)
    {
        // NOTE: This node is just a transform
        NodeTransform = AssimpLoadM4(AiNode->mTransformation);
    }
    else
    {
        // NOTE: This node is a bone, find our transform for this frame
        aiNodeAnim* Channel = 0;
        for (u32 NodeAnimId = 0; NodeAnimId < AiAnimation->mNumChannels; NodeAnimId++)
        {
            aiNodeAnim* AiNodeAnim = AiAnimation->mChannels[NodeAnimId];
            if (StringsEqual(NodeName, String(AiNodeAnim->mNodeName.data)))
            {
                Channel = AiNodeAnim;
            }
        }
        Assert(Channel != 0);

#define ASSIMP_LOAD_ATTRIBUTE(AttribName)                               \
        if (Channel->mNum##AttribName##Keys == 1)                       \
        {                                                               \
            Ai##AttribName##Key = Channel->m##AttribName##Keys[0];      \
        }                                                               \
        else                                                            \
        {                                                               \
            for (u32 AttribName##FrameId = 0;                           \
                 AttribName##FrameId < Channel->mNum##AttribName##Keys; \
                 AttribName##FrameId++)                                 \
            {                                                           \
                if ((f32)FrameId <= (f32)Channel->m##AttribName##Keys[AttribName##FrameId].mTime) \
                {                                                       \
                    Ai##AttribName##Key = Channel->m##AttribName##Keys[AttribName##FrameId]; \
                        break;                                          \
                }                                                       \
            }                                                           \
        }

        aiVectorKey AiPositionKey = {};
        ASSIMP_LOAD_ATTRIBUTE(Position);

        aiVectorKey AiScalingKey = {};
        ASSIMP_LOAD_ATTRIBUTE(Scaling);

        aiQuatKey AiRotationKey = {};
        ASSIMP_LOAD_ATTRIBUTE(Rotation);
        
#undef ASSIMP_LOAD_ATTRIBUTE
                    
        v3 Pos = V3(AiPositionKey.mValue.x, AiPositionKey.mValue.y, AiPositionKey.mValue.z) / Model->MaxPosAxis;
        v3 Scale = V3(AiScalingKey.mValue.x, AiScalingKey.mValue.y, AiScalingKey.mValue.z);
        q4 Rotation = Q4(AiRotationKey.mValue.x, AiRotationKey.mValue.y, AiRotationKey.mValue.z, AiRotationKey.mValue.w);

        NodeTransform = M4Pos(Pos)*Q4ToM4(Rotation)*M4Scale(Scale);
        
        *NumBonesWritten += 1;
    }
    
    m4 GlobalTransform = ParentTransform * NodeTransform;
    if (DstBoneId != -1)
    {
        loader_bone* Bone = Model->BoneArray + DstBoneId;
        m4 FinalTransform = Model->GlobalInverseTransform * GlobalTransform * Bone->ModelToBone;
        DstBoneTransforms[DstBoneId] = FinalTransform;
    }

    for (u32 ChildId = 0; ChildId < AiNode->mNumChildren; ChildId++)
    {
        AssimpLoadBoneTransforms(Model, DstBoneTransforms, FrameId, AiAnimation, AiNode->mChildren[ChildId],
                                 GlobalTransform, NumBonesWritten);
    }
}

internal loader_model AssimpLoadModel(u32 ModelId, char* ModelPath, u32* MaterialIndex, b32 SwapYZ,
                                      b32 HasAnimations, animation_id* AnimationMappings,
                                      u32 NumAnimationMappings)
{
    // NOTE: https://learnopengl.com/Model-Loading/Model
    loader_model Result = {};
    
    Assimp::Importer Importer;
    const aiScene* Scene = Importer.ReadFile(ModelPath, aiProcess_Triangulate | aiProcess_OptimizeMeshes | aiProcess_GenSmoothNormals);
    if(!Scene || Scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !Scene->mRootNode) 
    {
        const char* Error = Importer.GetErrorString();
        InvalidCodePath;
    }

    // NOTE: Update our material offset
    u32 SavedMaterialIndex = *MaterialIndex;
    *MaterialIndex += Scene->mNumMaterials;

    // NOTE: Count number of vertices + populate some mesh meta data
    u32 TotalNumVertices = 0;
    {
        Result.NumMeshes = Scene->mNumMeshes;
        Result.MeshArray = StatePushArray(file_mesh, Result.NumMeshes, &Result.MeshArrayOffset);

        for (u32 MeshId = 0; MeshId < Scene->mNumMeshes; MeshId++)
        {
            aiMesh* SrcMesh = Scene->mMeshes[MeshId];
            file_mesh* DstMesh = Result.MeshArray + MeshId;
            *DstMesh = {};

            DstMesh->NumVertices = SrcMesh->mNumVertices;
            TotalNumVertices += SrcMesh->mNumVertices;
            
            DstMesh->MaterialId = SavedMaterialIndex + SrcMesh->mMaterialIndex;
        }
    }
    
    // NOTE: Load mesh data
    v4u* BoneIdArray = 0;
    v4* BoneWeightArray = 0;
    {
        // NOTE: Allocate geometry
        Result.GeometryOffset = State.CurrentDataOffset;
        v3* PosArray = StatePushArray(v3, TotalNumVertices, 0);
        v3* NormalArray = StatePushArray(v3, TotalNumVertices, 0);
        v2* TexCoordArray = StatePushArray(v2, TotalNumVertices, 0);
        if (HasAnimations)
        {
            BoneIdArray = StatePushArray(v4u, TotalNumVertices, 0);
            BoneWeightArray = StatePushArray(v4, TotalNumVertices, 0);
            
            // NOTE: Set default values on the bone arrays
            ZeroMem(BoneIdArray, sizeof(v4u)*TotalNumVertices);
            ZeroMem(BoneWeightArray, sizeof(v4)*TotalNumVertices);
        }
        Result.GeometrySize = u32(State.CurrentDataOffset - Result.GeometryOffset);

        // NOTE: Load vertices
        u32 CurrVertId = 0;
        for (u32 MeshId = 0; MeshId < Scene->mNumMeshes; MeshId++)
        {
            aiMesh* SrcMesh = Scene->mMeshes[MeshId];
            file_mesh* DstMesh = Result.MeshArray + MeshId;

            // NOTE: Process vertices
            Assert(SrcMesh->mNumUVComponents[1] == 0);
            for (u32 VertId = 0; VertId < SrcMesh->mNumVertices; ++VertId, ++CurrVertId)
            {
                v3 WritePos = {};
                if (SwapYZ)
                {
                    WritePos = V3(SrcMesh->mVertices[CurrVertId].x,
                                  SrcMesh->mVertices[CurrVertId].z,
                                  SrcMesh->mVertices[CurrVertId].y);
                    PosArray[CurrVertId] = WritePos;
                }
                else
                {
                    WritePos = V3(SrcMesh->mVertices[CurrVertId].x,
                                  SrcMesh->mVertices[CurrVertId].y,
                                  SrcMesh->mVertices[CurrVertId].z);
                    PosArray[CurrVertId] = WritePos;
                }

                // NOTE: We mul by 2.0f to map to -0.5, 0.5 range, not -1, 1 range
                Result.MaxPosAxis = Max(Result.MaxPosAxis, 2.0f*Abs(WritePos.x));
                Result.MaxPosAxis = Max(Result.MaxPosAxis, 2.0f*Abs(WritePos.y));
                Result.MaxPosAxis = Max(Result.MaxPosAxis, 2.0f*Abs(WritePos.z));
                
                NormalArray[CurrVertId] = V3(SrcMesh->mNormals[VertId].x,
                                             SrcMesh->mNormals[VertId].y,
                                             SrcMesh->mNormals[VertId].z);

                if (SrcMesh->mTextureCoords[0])
                {
                    TexCoordArray[CurrVertId] = V2(SrcMesh->mTextureCoords[0][VertId].x,
                                                   SrcMesh->mTextureCoords[0][VertId].y);
                }
                else
                {
                    // NOTE: Some meshes have no tex coords for some reason
                    TexCoordArray[CurrVertId] = V2(0, 0);
                }
            }
        }

        // NOTE: Normalize all positions of the model
        Assert(Result.MaxPosAxis != 0.0f);
        for (u32 PosId = 0; PosId < TotalNumVertices; ++PosId)
        {
            // NOTE: This maps the model to be in -0.5, 0.5 space
            PosArray[PosId] = PosArray[PosId] / (Result.MaxPosAxis);
        }
        
        // NOTE: Allocate + Load indicies
        Result.IndexOffset = State.CurrentDataOffset;
        for (u32 MeshId = 0; MeshId < Scene->mNumMeshes; MeshId++)
        {
            aiMesh* SrcMesh = Scene->mMeshes[MeshId];
            file_mesh* DstMesh = Result.MeshArray + MeshId;

            // NOTE: Process indicies
            for (u32 FaceId = 0; FaceId < SrcMesh->mNumFaces; FaceId++)
            {
                aiFace Face = SrcMesh->mFaces[FaceId];

                DstMesh->NumIndices += Face.mNumIndices;
                u32* CurrIndex = StatePushArray(u32, Face.mNumIndices, 0);
                for (u32 IndexId = 0; IndexId < Face.mNumIndices; IndexId++)
                {
                    CurrIndex[IndexId] = Face.mIndices[IndexId];
                }
            }
        }
        Result.IndexSize = u32(State.CurrentDataOffset - Result.IndexOffset);
    }
    
    if (HasAnimations)
    {
        // TODO: Fix this guy
        // NOTE: Find the root ai node for bones
        aiNode* AiBoneRootNode = 0;
        for (u32 ChildId = 0; ChildId < Scene->mRootNode->mNumChildren; ++ChildId)
        {
            aiNode* Child = Scene->mRootNode->mChildren[ChildId];
            if (StringsEqual(String("Armature"), String(Child->mName.data)))
            {
                AiBoneRootNode = Child;
                break;
            }
        }
    
        // NOTE: Load bones metadata
        Result.BoneArray = (loader_bone*)(State.TempArena.Mem + State.TempArena.Used);
        {
            // IMPORTANT: This is hacky but to prevent having a separate loop to count how many bones we have, we write into memory and
            // then reserve the allocation after
            for (u32 MeshId = 0; MeshId < Scene->mNumMeshes; ++MeshId)
            {
                aiMesh* SrcMesh = Scene->mMeshes[MeshId];
                for (u32 BoneId = 0; BoneId < SrcMesh->mNumBones; ++BoneId)
                {
                    aiBone* AiBone = SrcMesh->mBones[BoneId];
                    string BoneName = String(AiBone->mName.data);

                    // NOTE: Check if we already added this bone
                    i32 CurrGlobalBoneId = AssimpGetLoadedBoneId(&Result, BoneName);
                    if (CurrGlobalBoneId == -1)
                    {
                        // NOTE: This is a new bone so append it
                        CurrGlobalBoneId = Result.NumBones++;
                        loader_bone* DstBone = Result.BoneArray + CurrGlobalBoneId;

                        // NOTE: Load the bones metadata
                        DstBone->Name = BoneName;
                        DstBone->ModelToBone = AssimpLoadM4(AiBone->mOffsetMatrix);

                        // NOTE: Scale the ModelToBones pos relative to our normalization const
                        DstBone->ModelToBone.v[3].xyz /= Result.MaxPosAxis;
                    }            
                }
            }

            Assert(Result.NumBones > 0);
            Result.BoneArray = PushArray(&State.TempArena, loader_bone, Result.NumBones);
        }
        
        // NOTE: Load per vertex bone data
        u32 CurrVertId = 0;
        for (u32 MeshId = 0; MeshId < Scene->mNumMeshes; ++MeshId)
        {
            aiMesh* SrcMesh = Scene->mMeshes[MeshId];

            for (u32 BoneId = 0; BoneId < SrcMesh->mNumBones; ++BoneId)
            {
                aiBone* AiBone = SrcMesh->mBones[BoneId];

                string BoneName = String(AiBone->mName.data);
                i32 CurrGlobalBoneId = AssimpGetLoadedBoneId(&Result, BoneName);
                Assert(CurrGlobalBoneId != -1);

                // NOTE: Populate per vertex data
                for (u32 WeightId = 0; WeightId < AiBone->mNumWeights; WeightId++)
                {
                    u32 NewVertexId = AiBone->mWeights[WeightId].mVertexId + CurrVertId;
                    f32 NewWeight = AiBone->mWeights[WeightId].mWeight;

                    for (u32 StoreId = 0; StoreId < 4; ++StoreId)
                    {
                        u32* StoredBoneId = BoneIdArray[NewVertexId].e + StoreId;
                        f32* StoredWeight = BoneWeightArray[NewVertexId].e + StoreId;

                        if (NewWeight > *StoredWeight)
                        {
                            // NOTE: Shift all other bone ids and weights down by 1
                            u32 OldBoneId = *StoredBoneId;
                            f32 OldWeight = *StoredWeight;
                            for (u32 I = StoreId + 1; I < 4; ++I)
                            {
                                u32 TempBoneId = BoneIdArray[NewVertexId].e[I];
                                f32 TempWeight = BoneWeightArray[NewVertexId].e[I];

                                BoneIdArray[NewVertexId].e[I] = OldBoneId;
                                BoneWeightArray[NewVertexId].e[I] = OldWeight;

                                OldBoneId = TempBoneId;
                                OldWeight = TempWeight;
                            }
                            
                            // NOTE: Put in our new value
                            *StoredBoneId = (u32)CurrGlobalBoneId;
                            *StoredWeight = NewWeight;

                            break;
                        }
                    }
                }
            }
            
            CurrVertId += SrcMesh->mNumVertices;
        }
        
        // NOTE: Pass through all our vertices and make sure that the chosen bones weigh up to 1
        for (u32 VertexId = 0; VertexId < TotalNumVertices; ++VertexId)
        {
            v4 BoneWeights = BoneWeightArray[VertexId];
            f32 TotalWeight = BoneWeights.e[0] + BoneWeights.e[1] + BoneWeights.e[2] + BoneWeights.e[3];
            BoneWeightArray[VertexId] = BoneWeights / TotalWeight;
        }

        // NOTE: Load animations (we store frames and transforms in one segment for each animation)
        Result.GlobalInverseTransform = Inverse(AssimpLoadM4(AiBoneRootNode->mTransformation));
        Result.NumAnimations = Scene->mNumAnimations;
        file_animation* AnimationArray = StatePushArray(file_animation, Result.NumAnimations, &Result.AnimationOffset);
        
        Assert(Result.NumAnimations == NumAnimationMappings);
        
        aiAnimation** SrcAnimation_ = Scene->mAnimations;
        file_animation* DstAnimation = AnimationArray;
        u32 SavedNumAnimations = Result.NumAnimations;
        for (u32 AnimationId = 0; AnimationId < SavedNumAnimations; ++AnimationId, ++SrcAnimation_, ++DstAnimation)
        {
            if (AnimationMappings[AnimationId] == AnimationId_None)
            {
                Result.NumAnimations -= 1;
                DstAnimation -= 1;
                continue;
            }

            aiAnimation* SrcAnimation = *SrcAnimation_;
            
            DstAnimation->AnimationId = AnimationMappings[AnimationId];
            DstAnimation->TotalTime = (f32)SrcAnimation->mDuration/(f32)SrcAnimation->mTicksPerSecond;
            DstAnimation->NumFrames = (u32)SrcAnimation->mDuration;
            
            // NOTE: Load animation frames
            file_animation_frame* FrameArray = StatePushArray(file_animation_frame, DstAnimation->NumFrames, &DstAnimation->FrameOffset);
            for (u32 FrameId = 0; FrameId < DstAnimation->NumFrames; ++FrameId)
            {
                file_animation_frame* Frame = FrameArray + FrameId;
                Frame->TimeOffset = (f32)FrameId / (f32)SrcAnimation->mTicksPerSecond;
            }

            // NOTE: Load transforms
            Result.AnimationTransformSize += sizeof(m4)*Result.NumBones*DstAnimation->NumFrames;
            DstAnimation->TransformOffset = State.CurrentDataOffset;
            for (u32 FrameId = 0; FrameId < DstAnimation->NumFrames; ++FrameId)
            {
                m4* BoneTransforms = StatePushArray(m4, Result.NumBones, 0);
            
                u32 NumBonesWritten = 0;
                AssimpLoadBoneTransforms(&Result, BoneTransforms, FrameId, SrcAnimation, AiBoneRootNode, M4Identity(), &NumBonesWritten);
                Assert(NumBonesWritten == Result.NumBones);
            }
        }
    }
    
    return Result;
}

inline void LoadStaticModel(u32 ModelId, char* ModelPath, u32* MaterialIndex, b32 SwapYZ, file_static_model* OutModel)
{
    loader_model LoaderModel = AssimpLoadModel(ModelId, ModelPath, MaterialIndex, SwapYZ, false, 0, 0);
    
    *OutModel = {};
    OutModel->AssetTypeId = ModelId;
    
    OutModel->NumMeshes = LoaderModel.NumMeshes;
    OutModel->MeshArrayOffset = LoaderModel.MeshArrayOffset;

    OutModel->GeometrySize = LoaderModel.GeometrySize;
    OutModel->GeometryOffset = LoaderModel.GeometryOffset;
    OutModel->IndexSize = LoaderModel.IndexSize;
    OutModel->IndexOffset = LoaderModel.IndexOffset;
}

inline void LoadAnimatedModel(u32 ModelId, char* ModelPath, b32 SwapYZ, file_animated_model* OutModel, u32* MaterialIndex,
                              animation_id* AnimationMappings, u32 NumAnimationMappings)
{
    loader_model LoaderModel = AssimpLoadModel(ModelId, ModelPath, MaterialIndex, SwapYZ, true,
                                               AnimationMappings, NumAnimationMappings);

    *OutModel = {};
    OutModel->AssetTypeId = ModelId;
    
    OutModel->NumMeshes = LoaderModel.NumMeshes;
    OutModel->MeshArrayOffset = LoaderModel.MeshArrayOffset;

    OutModel->GeometrySize = LoaderModel.GeometrySize;
    OutModel->GeometryOffset = LoaderModel.GeometryOffset;
    OutModel->IndexSize = LoaderModel.IndexSize;
    OutModel->IndexOffset = LoaderModel.IndexOffset;

    OutModel->NumBones = LoaderModel.NumBones;
    OutModel->NumAnimations = LoaderModel.NumAnimations;
    OutModel->AnimationOffset = LoaderModel.AnimationOffset;
    OutModel->AnimationTransformSize = LoaderModel.AnimationTransformSize;
}

inline void ModelCountNumAssets(char* ModelPath, char* ParentFolder, char* DefaultTextureName, u32* OutNumMaterials,
                                u32* OutNumUnIndexedTextures)
{
    Assimp::Importer Importer;
    const aiScene* Scene = Importer.ReadFile(ModelPath, aiProcess_Triangulate | aiProcess_OptimizeMeshes | aiProcess_GenSmoothNormals);
    if(!Scene || Scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !Scene->mRootNode) 
    {
        const char* Error = Importer.GetErrorString();
        InvalidCodePath;
    }
    
    // TODO: Make sure we aren't loading the same textures more than once
    // TODO: Sometimes some materials are unused. Delete those
    // NOTE: Load materials
    // TODO: When we add a texture job, make sure its unique
    asset_texture_id DiffuseTextureId = AssetTextureId(true, Texture_White);
    if (DefaultTextureName)
    {
        DiffuseTextureId = AddUnIndexedTextureAsset(ParentFolder, DefaultTextureName);
    }
    
    for (u32 MaterialId = 0; MaterialId < Scene->mNumMaterials; ++MaterialId)
    {
        aiMaterial* AiMaterial = Scene->mMaterials[MaterialId];

        b32 IsIndexed = true;
        u32 NumDiffuseTextures = AiMaterial->GetTextureCount(aiTextureType_DIFFUSE);
        if (NumDiffuseTextures > 0)
        {
            aiString DiffuseStr;
            AiMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &DiffuseStr);

            IsIndexed = false;
            char* DiffuseStrCopy = (char*)malloc(sizeof(char)*(strlen(DiffuseStr.C_Str()) + 1));
            strcpy(DiffuseStrCopy, DiffuseStr.C_Str());
            DiffuseTextureId = AddUnIndexedTextureAsset(ParentFolder, DiffuseStrCopy);
        }

        AddUnIndexedMaterialAsset(DiffuseTextureId);
    }

    *OutNumMaterials = Scene->mNumMaterials;
}

#if 0

inline b32 MtlIsNameChar(char C)
{
    b32 Result = IsLetter(C) || C == '_' || C == '.';
    return Result;
}

inline u32 MtlNumNameChars(string Body)
{
    u32 Result = 0;

    // NOTE: First char cannot be a numeric
    if (MtlIsNameChar(Body.Chars[0]))
    {
        while ((MtlIsNameChar(Body.Chars[Result]) || IsNumeric(Body.Chars[Result])) &&
               Result < Body.NumChars)
        {
            Result += 1;
        }
    }
    
    return Result;
}

inline string MtlReadNameAndAdvance(string* Body)
{
    string Result = {};
    
    u32 NumCharsInName = MtlNumNameChars(*Body);
    Result = String(Body->Chars, NumCharsInName);
    AdvanceString(Body, NumCharsInName);

    return Result;
}

internal load_request LoadObj(linear_arena* Arena, char* MtlLibDir, u32 ModelId, u32 Size, char* Data,
                              file_model* Model)
{
#if 0
    loader_model LoaderModel = {};
    string ObjFileStr = String(Data, Size);

    // NOTE: Load all material libs
    // IMPORTANT: We assume its never more than 10
    u32 NumMaterialLibs = 0;
    string MaterialLibBodies[10] = {};
    {
        string MtlLibStr = String("mtllib ");
        string NewMtlStr = String("newmtl ");
        string CurrChar = ObjFileStr;
        string MaterialLibNames[10] = {};

        // NOTE: Get all material lib names
        while (CurrChar.NumChars > 0)
        {
            if (StringContained(MtlLibStr, CurrChar))
            {
                AdvanceString(&CurrChar, MtlLibStr.NumChars);
                AdvancePastSpaces(&CurrChar);
                MaterialLibNames[NumMaterialLibs] = MtlReadNameAndAdvance(&CurrChar);
                NumMaterialLibs += 1;
            }
            else
            {
                AdvanceString(&CurrChar, 1);
            }
        }
        Assert(NumMaterialLibs > 0);

        // NOTE: Open material lib and check number of materials inside
        for (u32 MaterialLibId = 0; MaterialLibId < NumMaterialLibs; ++MaterialLibId)
        {
            // TODO: Hard coded the path here
            // NOTE: Open mtl lib file
            char* MtlFullPath = CombineStrings(MtlLibDir, MaterialLibNames[MaterialLibId]);
            loaded_file MtlFile = LoadFile(MtlFullPath);
            free(MtlFullPath);

            MaterialLibBodies[MaterialLibId] = String((char*)MtlFile.Data, (u32)MtlFile.Size);

            // NOTE: Scan and see how many mtls there are
            {
                string MtlCurrChar = MaterialLibBodies[MaterialLibId];
                while(MtlCurrChar.NumChars > 0)
                {
                    if (StringContained(NewMtlStr, MtlCurrChar))
                    {
                        AdvanceString(&MtlCurrChar, NewMtlStr.NumChars);
                        LoaderModel.NumMeshes += 1;
                    }
                    else
                    {
                        AdvanceString(&MtlCurrChar, 1);
                    }
                }
            }
        }

        // NOTE: Allocate space for materials and meshes
        LoaderModel.MeshArray = PushArray(Arena, loader_mesh, LoaderModel.NumMeshes);
        LoaderModel.MaterialArray = PushArray(Arena, loader_material, LoaderModel.NumMeshes);
        // IMPORTANT: For now, we just assume that 100k vertices is more than we will ever load
        for (u32 MeshId = 0; MeshId < LoaderModel.NumMeshes; ++MeshId)
        {
            loader_mesh* Mesh = LoaderModel.MeshArray + MeshId;
            *Mesh = {};
            
            Mesh->PosArray = PushArray(Arena, v3, 100000);
            Mesh->NormalArray = PushArray(Arena, v3, 100000);
            Mesh->TexCoordArray = PushArray(Arena, v2, 100000);

            loader_material* Material = LoaderModel.MaterialArray + MeshId;
            *Material = {};
        }
    }
    
    // NOTE: Load all our materials
    {
        string NewMtlStr = String("newmtl ");
        string AmbientColorStr = String("Ka ");
        string DirectionColorStr = String("Kd ");
        string SpecularColorStr = String("Ks ");

        loader_material* CurrMaterial = LoaderModel.MaterialArray - 1;
        for (u32 MaterialLibId = 0; MaterialLibId < NumMaterialLibs; ++MaterialLibId)
        {
            string CurrChar = MaterialLibBodies[MaterialLibId];
            while(CurrChar.NumChars > 0)
            {
                if (StringContained(NewMtlStr, CurrChar))
                {
                    CurrMaterial += 1;

                    AdvanceString(&CurrChar, NewMtlStr.NumChars);
                    AdvancePastSpaces(&CurrChar);
                    CurrMaterial->Name = MtlReadNameAndAdvance(&CurrChar);
                }
                else if (StringContained(AmbientColorStr, CurrChar))
                {
                    sscanf(CurrChar.Chars, "Ka %f %f %f", &CurrMaterial->AmbientColor.x,
                           &CurrMaterial->AmbientColor.y, &CurrMaterial->AmbientColor.z);
                    AdvanceString(&CurrChar, AmbientColorStr.NumChars);
                }
                else if (StringContained(DirectionColorStr, CurrChar))
                {
                    sscanf(CurrChar.Chars, "Kd %f %f %f", &CurrMaterial->DiffuseColor.x,
                           &CurrMaterial->DiffuseColor.y, &CurrMaterial->DiffuseColor.z);
                    AdvanceString(&CurrChar, DirectionColorStr.NumChars);
                }
                else if (StringContained(SpecularColorStr, CurrChar))
                {
                    sscanf(CurrChar.Chars, "Ks %f %f %f", &CurrMaterial->SpecularColor.x,
                           &CurrMaterial->SpecularColor.y, &CurrMaterial->SpecularColor.z);
                    AdvanceString(&CurrChar, SpecularColorStr.NumChars);
                }
                else
                {
                    AdvanceString(&CurrChar, 1);
                }
            }
        }
    }
    
    // NOTE: Load in our vertex data
    {
        string UseMtlStr = String("usemtl ");
        string VnStr = String("vn ");
        string VtStr = String("vt ");
        string VStr = String("v ");
        string FStr = String("f ");
        string NewMeshStr = String("o ");

        // IMPORTANT: We assume we won't have more than 100k vertices
        v3* GlobalPositionArray = PushArray(Arena, v3, 100000);
        v3* GlobalNormalArray = PushArray(Arena, v3, 100000);
        v2* GlobalTexCoordArray = PushArray(Arena, v2, 100000);

        string CurrChar = ObjFileStr;

        i32 CurrMeshId = -1;
        u32 NumPositions = 0;
        v3* CurrPosition = GlobalPositionArray;
        u32 NumNormals = 0;
        v3* CurrNormal = GlobalNormalArray;
        u32 NumTexCoords = 0;
        v2* CurrTexCoord = GlobalTexCoordArray;
        while (CurrChar.NumChars > 0)
        {
            if (StringContained(VnStr, CurrChar))
            {
                sscanf(CurrChar.Chars, "vn %f %f %f", &CurrNormal->x, &CurrNormal->y, &CurrNormal->z);
                ++CurrNormal;
                ++NumNormals;

                AdvanceString(&CurrChar, VnStr.NumChars);
            }
            else if (StringContained(VtStr, CurrChar))
            {
                sscanf(CurrChar.Chars, "vt %f %f", &CurrTexCoord->x, &CurrTexCoord->y);
                ++CurrTexCoord;
                ++NumTexCoords;

                AdvanceString(&CurrChar, VtStr.NumChars);
            }
            else if (StringContained(VStr, CurrChar))
            {
                sscanf(CurrChar.Chars, "v %f %f %f", &CurrPosition->x, &CurrPosition->y, &CurrPosition->z);
                ++CurrPosition;
                ++NumPositions;

                AdvanceString(&CurrChar, VStr.NumChars);
            }
            else if (StringContained(FStr, CurrChar))
            {
                // NOTE: Write out the vertices into the correct mesh
                Assert(CurrMeshId >= 0);
                loader_mesh* CurrMesh = LoaderModel.MeshArray + CurrMeshId;

                // NOTE: Walk the string and count number of / until a new line
                u32 NumSlashes = 0;
                for (u32 CharId = 0; CharId < CurrChar.NumChars; ++CharId)
                {
                    if (CurrChar.Chars[CharId] == '\n')
                    {
                        break;
                    }

                    if (CurrChar.Chars[CharId] == '/')
                    {
                        NumSlashes += 1;
                    }
                }
                
                u32 NumVertices = NumSlashes / 2; // NOTE: There are 2 slashes per vertex
                if (NumVertices == 3)
                {
                    // NOTE: Defines a triangle
                    u32 Pos0Id, Pos1Id, Pos2Id;
                    u32 Normal0Id, Normal1Id, Normal2Id;
                    u32 TexCoord0Id, TexCoord1Id, TexCoord2Id;
                    sscanf(CurrChar.Chars, "f %d/%d/%d %d/%d/%d %d/%d/%d",
                           &Pos0Id, &TexCoord0Id, &Normal0Id,
                           &Pos1Id, &TexCoord1Id, &Normal1Id,
                           &Pos2Id, &TexCoord2Id, &Normal2Id);

                    Pos0Id -= 1; Pos1Id -= 1; Pos2Id -= 1;
                    Normal0Id -= 1; Normal1Id -= 1; Normal2Id -= 1;
                    TexCoord0Id -= 1; TexCoord1Id -= 1; TexCoord2Id -= 1;
                
                    CurrMesh->PosArray[CurrMesh->NumVertices+0] = GlobalPositionArray[Pos0Id];
                    CurrMesh->PosArray[CurrMesh->NumVertices+1] = GlobalPositionArray[Pos1Id];
                    CurrMesh->PosArray[CurrMesh->NumVertices+2] = GlobalPositionArray[Pos2Id];

                    CurrMesh->NormalArray[CurrMesh->NumVertices+0] = GlobalNormalArray[Normal0Id];
                    CurrMesh->NormalArray[CurrMesh->NumVertices+1] = GlobalNormalArray[Normal1Id];
                    CurrMesh->NormalArray[CurrMesh->NumVertices+2] = GlobalNormalArray[Normal2Id];

                    CurrMesh->TexCoordArray[CurrMesh->NumVertices+0] = GlobalTexCoordArray[TexCoord0Id];
                    CurrMesh->TexCoordArray[CurrMesh->NumVertices+1] = GlobalTexCoordArray[TexCoord1Id];
                    CurrMesh->TexCoordArray[CurrMesh->NumVertices+2] = GlobalTexCoordArray[TexCoord2Id];
                
                    CurrMesh->NumVertices += 3;
                }
                else if (NumVertices == 4)
                {
                    // NOTE: Defines a quad
                    u32 Pos0Id, Pos1Id, Pos2Id, Pos3Id;
                    u32 Normal0Id, Normal1Id, Normal2Id, Normal3Id;
                    u32 TexCoord0Id, TexCoord1Id, TexCoord2Id, TexCoord3Id;
                    sscanf(CurrChar.Chars, "f %d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d",
                           &Pos0Id, &TexCoord0Id, &Normal0Id,
                           &Pos1Id, &TexCoord1Id, &Normal1Id,
                           &Pos2Id, &TexCoord2Id, &Normal2Id,
                           &Pos3Id, &TexCoord3Id, &Normal3Id);

                    Pos0Id -= 1; Pos1Id -= 1; Pos2Id -= 1; Pos3Id -= 1;
                    Normal0Id -= 1; Normal1Id -= 1; Normal2Id -= 1; Normal3Id -= 1;
                    TexCoord0Id -= 1; TexCoord1Id -= 1; TexCoord2Id -= 1; TexCoord3Id -= 1;
                
                    CurrMesh->PosArray[CurrMesh->NumVertices+0] = GlobalPositionArray[Pos0Id];
                    CurrMesh->PosArray[CurrMesh->NumVertices+1] = GlobalPositionArray[Pos1Id];
                    CurrMesh->PosArray[CurrMesh->NumVertices+2] = GlobalPositionArray[Pos2Id];
                    CurrMesh->PosArray[CurrMesh->NumVertices+3] = GlobalPositionArray[Pos0Id];
                    CurrMesh->PosArray[CurrMesh->NumVertices+4] = GlobalPositionArray[Pos2Id];
                    CurrMesh->PosArray[CurrMesh->NumVertices+5] = GlobalPositionArray[Pos3Id];

                    CurrMesh->NormalArray[CurrMesh->NumVertices+0] = GlobalNormalArray[Normal0Id];
                    CurrMesh->NormalArray[CurrMesh->NumVertices+1] = GlobalNormalArray[Normal1Id];
                    CurrMesh->NormalArray[CurrMesh->NumVertices+2] = GlobalNormalArray[Normal2Id];
                    CurrMesh->NormalArray[CurrMesh->NumVertices+3] = GlobalNormalArray[Normal0Id];
                    CurrMesh->NormalArray[CurrMesh->NumVertices+4] = GlobalNormalArray[Normal2Id];
                    CurrMesh->NormalArray[CurrMesh->NumVertices+5] = GlobalNormalArray[Normal3Id];

                    CurrMesh->TexCoordArray[CurrMesh->NumVertices+0] = GlobalTexCoordArray[TexCoord0Id];
                    CurrMesh->TexCoordArray[CurrMesh->NumVertices+1] = GlobalTexCoordArray[TexCoord1Id];
                    CurrMesh->TexCoordArray[CurrMesh->NumVertices+2] = GlobalTexCoordArray[TexCoord2Id];
                    CurrMesh->TexCoordArray[CurrMesh->NumVertices+3] = GlobalTexCoordArray[TexCoord0Id];
                    CurrMesh->TexCoordArray[CurrMesh->NumVertices+4] = GlobalTexCoordArray[TexCoord2Id];
                    CurrMesh->TexCoordArray[CurrMesh->NumVertices+5] = GlobalTexCoordArray[TexCoord3Id];
                
                    CurrMesh->NumVertices += 6;
                }
                else
                {
                    InvalidCodePath;
                }
                
                AdvanceString(&CurrChar, FStr.NumChars);
            }
            else if (StringContained(UseMtlStr, CurrChar))
            {
                AdvanceString(&CurrChar, UseMtlStr.NumChars);
                AdvancePastSpaces(&CurrChar);
                string MaterialName = MtlReadNameAndAdvance(&CurrChar);

                // NOTE: Check which mesh this corresponds too
                b32 Found = false;
                for (u32 MaterialId = 0; MaterialId < LoaderModel.NumMeshes; ++MaterialId)
                {
                    if (StringsEqual(MaterialName, LoaderModel.MaterialArray[MaterialId].Name))
                    {
                        CurrMeshId = MaterialId;
                        Found = true;
                        break;
                    }
                }

                Assert(Found);
            }
            else if (StringContained(NewMeshStr, CurrChar))
            {
                AdvanceString(&CurrChar, NewMeshStr.NumChars);
                AdvancePastSpaces(&CurrChar);
            }
            else
            {
                AdvanceString(&CurrChar, 1);
            }
        }
    }
#endif
    
    // NOTE: Create a load request. We pack the mesh array first, and then we pack each meshes data
    // sequentially in memory
    load_request Result = {};

#if 0
    {
        Result.Size = sizeof(file_mesh)*LoaderModel.NumMeshes;
        for (u32 MeshId = 0; MeshId < LoaderModel.NumMeshes; ++MeshId)
        {
            Result.Size += (2*sizeof(v3) + sizeof(v2))*LoaderModel.MeshArray[MeshId].NumVertices;
        }
        Result.Data = malloc(Result.Size);

        linear_arena LoadRequestArena = InitArena(Result.Data, Result.Size);

        // NOTE: Write out the file model
        u32 FileMeshArraySize = sizeof(file_mesh)*LoaderModel.NumMeshes;
        file_mesh* FileMeshArray = PushArray(&LoadRequestArena, file_mesh, LoaderModel.NumMeshes);
        
        Model->AssetTypeId = ModelId;
        Model->NumMeshes = LoaderModel.NumMeshes;
        Model->MeshArrayOffset = State.CurrentDataOffset + (u32)((u64)FileMeshArray - (u64)Result.Data);
        Model->GeometryOffset = Model->MeshArrayOffset + FileMeshArraySize;
        Model->GeometrySize = (u32)(Result.Size - FileMeshArraySize);
        
        file_mesh* CurrFileMesh = FileMeshArray;
        for (u32 MeshId = 0; MeshId < LoaderModel.NumMeshes; ++MeshId, ++CurrFileMesh)
        {
            loader_material* CurrMaterial = LoaderModel.MaterialArray + MeshId;
            loader_mesh* CurrMesh = LoaderModel.MeshArray + MeshId;
            
            // NOTE: Material data
            CurrFileMesh->AmbientColor = CurrMaterial->AmbientColor;
            CurrFileMesh->DiffuseColor = CurrMaterial->DiffuseColor;
            CurrFileMesh->SpecularColor = CurrMaterial->SpecularColor;

            // NOTE: Store geometry data
            v3* WritePos = PushArray(&LoadRequestArena, v3, CurrMesh->NumVertices);
            Copy(CurrMesh->PosArray, WritePos, sizeof(v3)*CurrMesh->NumVertices);
            
            v3* WriteNormal = PushArray(&LoadRequestArena, v3, CurrMesh->NumVertices);
            Copy(CurrMesh->NormalArray, WriteNormal, sizeof(v3)*CurrMesh->NumVertices);

            v2* WriteTexCoord = PushArray(&LoadRequestArena, v2, CurrMesh->NumVertices);
            Copy(CurrMesh->TexCoordArray, WriteTexCoord, sizeof(v2)*CurrMesh->NumVertices);
            
            // NOTE: Geometry data
            CurrFileMesh->NumVertices = CurrMesh->NumVertices;
        }

        Assert(LoadRequestArena.Used == Result.Size);
    }
#endif
    
    return Result;
}

#endif
