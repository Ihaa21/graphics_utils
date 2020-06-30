#pragma once

// TODO: Figure out names of everything here
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
// NOTE: Barrier Batcher
//

struct vk_barrier_batcher
{
    u32 MaxNumImgBarriers;
    u32 NumImgBarriers;
    VkImageMemoryBarrier* ImgBarrierArray;

    u32 MaxNumBufferBarriers;
    u32 NumBufferBarriers;
    VkBufferMemoryBarrier* BufferBarrierArray;
};

//
// NOTE: Desc Updater
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
    VkAccessFlags DstAccessMask;
};

// TODO: Currently unused
struct vk_buffer_update
{
    VkBuffer Buffer;
    u32 Size;
    u64 StagingOffset;
    u32 DstOffset;
    VkAccessFlags DstAccessMask;
};

struct vk_image_transfer
{
    u64 StagingOffset;
    
    VkImage Image;
    u32 Width;
    u32 Height;
    VkImageLayout Layout;
    VkImageAspectFlagBits AspectMask;
};

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

    // NOTE: Buffer update data
    u32 MaxNumBufferUpdates;
    u32 NumBufferUpdates;
    vk_buffer_update* BufferUpdateArray;
};
