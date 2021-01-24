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

