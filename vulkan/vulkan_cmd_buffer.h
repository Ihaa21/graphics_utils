#pragma once

//
// NOTE: Barrier Data
//

// TODO: Mostly used in transfer manager, should we get rid of it?
struct barrier_mask
{
    VkAccessFlags AccessMask;
    VkPipelineStageFlags StageMask;
};

//
// NOTE: Transfer Data
//

struct vk_buffer_transfer
{
    VkBuffer StagingBuffer;
    u64 StagingOffset;
    
    VkBuffer Buffer;
    u64 DstOffset;
    u64 Size;

    barrier_mask InputMask;
    barrier_mask OutputMask;
};

struct vk_image_transfer
{
    VkBuffer StagingBuffer;
    u64 StagingOffset;

    VkImageAspectFlags AspectMask;
    VkImage Image;
    u32 OffsetX;
    u32 OffsetY;
    u32 OffsetZ;
    u32 Width;
    u32 Height;
    u32 Depth;
    
    barrier_mask InputMask;
    VkImageLayout InputLayout;

    barrier_mask OutputMask;
    VkImageLayout OutputLayout;
};

struct vk_commands
{
    VkCommandBuffer Buffer;
    VkFence Fence;

    // NOTE: Barrier Batching
    u32 NumMemoryBarriers;
    block_arena MemoryBarrierArena;

    u32 NumImageBarriers;
    block_arena ImageBarrierArena;

    u32 NumBufferBarriers;
    block_arena BufferBarrierArena;

    VkPipelineStageFlags SrcStageFlags;
    VkPipelineStageFlags DstStageFlags;

    // NOTE: Transfer Data
    u32 FlushAlignment;
    vk_staging_arena StagingArena;

    u32 NumBufferTransfers;
    block_arena BufferTransferArena;
    
    u32 NumImageTransfers;
    block_arena ImageTransferArena;
};

inline void VkCommandsBarrierFlush(vk_commands* Commands);
inline void VkCommandsTransferFlush(vk_commands* Commands, VkDevice Device);
