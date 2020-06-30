#pragma once

#include "math\math.h"

//
// NOTE: Memory
//

struct vk_gpu_ptr
{
    VkDeviceMemory* Memory;
    u64 Offset;
};

struct vk_gpu_linear_arena
{
    u64 Size;
    u64 Used;
    vk_gpu_ptr MemPtr;
};

struct vk_gpu_temp_mem
{
    vk_gpu_linear_arena* Arena;
    u64 Used;
};

//
// NOTE: Descriptor Layout Builder
//

struct vk_descriptor_layout_builder
{
    u32 CurrNumBindings;
    VkDescriptorSetLayoutBinding Bindings[10];
    VkDescriptorSetLayout* Layout;
};

//
// NOTE: Barrier Batcher
//

struct barrier_mask
{
    VkAccessFlagBits AccessMask;
    VkPipelineStageFlags StageMask;
};

struct vk_barrier_batcher
{
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

struct vk_desc_updater
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
    u32 Size;
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
// TODO: Make this be a thing that goes into diff modes depending on the game state
// TODO: Free the extra memory when we set this into game play mode
struct vk_transfer_updater
{
    linear_arena Arena;

    // NOTE: Vk Constants
    u32 FlushAlignment;
    
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
    VkSemaphore FinishSemaphore;
    VkFence Fence;
};

#include "vulkan_utils.cpp"
