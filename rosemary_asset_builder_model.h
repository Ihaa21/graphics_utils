#pragma once

/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Ihor Szlachtycz $
   $Notice: (C) Copyright 2014 by Dream.Inc, Inc. All Rights Reserved. $
   ======================================================================== */

// TODO: This is a hack rn, do this better
#undef internal
#undef global
#undef local_global

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#define internal static
#define global static
#define local_global static

struct loader_mesh
{
    // NOTE: Geometry    
    u32 NumVertices;
    u32 NumIndices;

    // NOTE: Material
    u32 MaterialId;
};

struct loader_bone
{
    string Name;
    m4 ModelToBone;
};

struct loader_animation_frame
{
    f32 TimeOffset;
    m4* BoneTransforms;
};

struct loader_animation
{
    animation_id AnimationId;
    f32 TotalTime;
    u32 NumFrames;
    loader_animation_frame* FrameArray;
};

struct loader_model
{
    u32 NumMeshes;
    u64 MeshArrayOffset;
    file_mesh* MeshArray;

    // NOTE: Animation matrices
    m4 GlobalInverseTransform;

    f32 MaxPosAxis; // NOTE: Used for normalizing pos to be in [-1, 1] on each axis
    u32 GeometrySize;
    u64 GeometryOffset;

    u32 IndexSize;
    u64 IndexOffset;
    
    u32 NumBones;
    loader_bone* BoneArray;
    u32 NumAnimations;
    u32 AnimationTransformSize;
    u64 AnimationOffset;
};
