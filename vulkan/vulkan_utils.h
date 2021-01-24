#pragma once

#include "math\math.h"
#include "vulkan_memory.h"
#include "vulkan_pipeline.h"

//
// NOTE: Descriptor Layout Builder
//

// TODO: Dynamic Arena
struct vk_descriptor_layout_builder
{
    u32 CurrNumBindings;
    VkDescriptorSetLayoutBinding Bindings[100];
    VkDescriptorSetLayout* Layout;
};

//
// NOTE: Render Pass Builder
//

struct vk_render_pass_builder
{
    temp_mem TempMem;
    linear_arena* Arena;
    
    // NOTE: Attachment Data
    u32 MaxNumAttachments;
    u32 NumAttachments;
    VkAttachmentDescription* Attachments;

    // NOTE: Dependencies
    u32 MaxNumDependencies;
    u32 NumDependencies;
    VkSubpassDependency* Dependencies;
    
    u32 MaxNumInputAttachmentRefs;
    u32 NumInputAttachmentRefs;
    VkAttachmentReference* InputAttachmentRefs;

    u32 MaxNumColorAttachmentRefs;
    u32 NumColorAttachmentRefs;
    VkAttachmentReference* ColorAttachmentRefs;

    u32 MaxNumResolveAttachmentRefs;
    u32 NumResolveAttachmentRefs;
    VkAttachmentReference* ResolveAttachmentRefs;

    u32 MaxNumDepthAttachmentRefs;
    u32 NumDepthAttachmentRefs;
    VkAttachmentReference* DepthAttachmentRefs;
    
    u32 MaxNumSubPasses;
    u32 NumSubPasses;
    VkSubpassDescription* SubPasses;
    
};

//
// NOTE: Barrier Manager
//

struct barrier_mask
{
    VkAccessFlags AccessMask;
    VkPipelineStageFlags StageMask;
};

struct vk_barrier_manager
{
    u32 MaxNumMemoryBarriers;
    u32 NumMemoryBarriers;
    VkMemoryBarrier* MemoryBarrierArray;

    u32 MaxNumImageBarriers;
    u32 NumImageBarriers;
    VkImageMemoryBarrier* ImageBarrierArray;

    u32 MaxNumBufferBarriers;
    u32 NumBufferBarriers;
    VkBufferMemoryBarrier* BufferBarrierArray;

    VkPipelineStageFlags SrcStageFlags;
    VkPipelineStageFlags DstStageFlags;
};

//
// NOTE: Descriptor Updater
//

struct vk_descriptor_manager
{
    linear_arena Arena;
    
    u32 MaxNumWrites;
    u32 NumWrites;
    VkWriteDescriptorSet* WriteArray;
};

//
// NOTE: Transfer updater
//

struct vk_buffer_transfer
{
    VkBuffer Buffer;
    u64 DstOffset;
    u64 Size;
    u64 StagingOffset;

    barrier_mask InputMask;
    barrier_mask OutputMask;
};

struct vk_image_transfer
{
    u64 StagingOffset;

    VkImageAspectFlags AspectMask;
    VkImage Image;
    u32 Width;
    u32 Height;
    
    barrier_mask InputMask;
    VkImageLayout InputLayout;

    barrier_mask OutputMask;
    VkImageLayout OutputLayout;
};

// TODO: Add resource reading 
struct vk_transfer_manager
{
    linear_arena Arena;

    // NOTE: Vk Constants
    u64 FlushAlignment;
    
    // NOTE: Staging data
    u64 StagingSize;
    u64 StagingOffset;
    u8* StagingPtr;
    VkDeviceMemory StagingMem;
    VkBuffer StagingBuffer;

    // NOTE: Buffer data
    u32 MaxNumBufferTransfers;
    u32 NumBufferTransfers;
    vk_buffer_transfer* BufferTransferArray;
    
    // NOTE: Image data
    u32 MaxNumImageTransfers;
    u32 NumImageTransfers;
    vk_image_transfer* ImageTransferArray;
};

//
// NOTE: Helper structs
//

struct vk_commands
{
    VkCommandBuffer Buffer;
    VkFence Fence;
};

struct vk_image
{
    VkImage Image;
    VkImageView View;
};

internal void VkCheckResult(VkResult Result);

#include "vulkan_memory.cpp"
#include "vulkan_pipeline.cpp"
#include "vulkan_utils.cpp"
