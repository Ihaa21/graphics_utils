#pragma once

//
// NOTE: Memory
//

struct vk_ptr
{
    VkDeviceMemory Memory;
    u64 Offset;
};

struct vk_linear_arena
{
    u64 Size;
    u64 Used;
    VkDeviceMemory Memory;
};

struct vk_temp_mem
{
    vk_linear_arena* Arena;
    u64 Used;
};

//
// NOTE: Dynamic Arena
//

struct vk_dynamic_arena_header
{
    VkDeviceMemory GpuMemory;
    mm Used;
    mm Size;
};

struct vk_dynamic_arena
{
    vk_dynamic_arena_header* CurrHeader;
    dynamic_arena CpuArena;
    u64 GpuBlockSize;
    u32 GpuMemoryType;
};

struct vk_dynamic_temp_mem
{
    vk_dynamic_arena* Arena;
    vk_dynamic_arena_header* GpuHeader;
    mm GpuUsed;
};

//
// NOTE: Staging Arena
//

struct vk_staging_arena_header
{
    // NOTE: Stored at the top of pages
    vk_staging_arena_header* Next;
    vk_staging_arena_header* Prev;
    VkDeviceMemory GpuMemory;
    VkBuffer GpuBuffer;
    mm Used;
    mm Size;
};

struct vk_staging_ptr
{
    u8* Ptr;
    VkBuffer Buffer;
    u64 Offset;
};

struct vk_staging_arena
{
    // IMPORTANT: We don't do a sentinel cuz then we can't return by value
    vk_staging_arena_header* Prev;
    vk_staging_arena_header* Next;
    mm MinBlockSize;
    u32 StagingTypeId;

    VkDevice Device;
};
