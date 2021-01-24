
//
// NOTE: Memory Functions
//

inline i32 VkGetMemoryType(VkPhysicalDeviceMemoryProperties* MemoryProperties, u32 RequiredType, VkMemoryPropertyFlags RequiredProperties)
{
    i32 Result = -1;
    
    u32 MemoryCount = MemoryProperties->memoryTypeCount;
    for (u32 MemoryIndex = 0; MemoryIndex < MemoryCount; ++MemoryIndex)
    {
        // TODO: Do we need this type bit thing here or can we remove?
        u32 MemoryTypeBits = 1 << MemoryIndex;
        VkMemoryPropertyFlags Properties = MemoryProperties->memoryTypes[MemoryIndex].propertyFlags;

        if ((RequiredType & MemoryTypeBits) && (Properties & RequiredProperties) == RequiredProperties)
        {
            Result = MemoryIndex;
            break;
        }
    }

    return Result;
}

inline VkDeviceMemory VkMemoryAllocate(VkDevice Device, u32 Type, u64 Size)
{
    VkDeviceMemory Result = {};
    
    VkMemoryAllocateInfo MemoryAllocateInfo = {};
    MemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    MemoryAllocateInfo.allocationSize = Size;
    MemoryAllocateInfo.memoryTypeIndex = Type;
    VkCheckResult(vkAllocateMemory(Device, &MemoryAllocateInfo, 0, &Result));

    return Result;
}

inline u64 VkIncrementPointer(u64 BaseAddress, VkMemoryRequirements Requirements)
{
    u64 Result = AlignAddress(BaseAddress, Requirements.alignment) + Requirements.size;
    return Result;
}

//
// NOTE: Linear Arena
//

inline vk_linear_arena VkLinearArenaCreate(VkDevice Device, u32 MemoryTypeId, u64 Size)
{
    vk_linear_arena Result = {};
    Result.Size = Size;
    Result.Memory = VkMemoryAllocate(Device, MemoryTypeId, Size);

    return Result;
}

inline void VkLinearArenaDestroy(VkDevice Device, vk_linear_arena* Arena)
{
    vkFreeMemory(Device, Arena->Memory, 0);
    *Arena = {};
}

inline vk_ptr VkPushSize(vk_linear_arena* Arena, u64 Size, u64 Alignment)
{
    u64 AlignedAddress = AlignAddress(Arena->Used, Alignment);
    Assert(AlignedAddress + Size <= Arena->Size);
    
    vk_ptr Result = {};
    Result.Memory = Arena->Memory;
    Result.Offset = AlignedAddress;

    Arena->Used = AlignedAddress + Size;

    return Result;
}

inline void VkArenaClear(vk_linear_arena* Arena)
{
    Arena->Used = 0;
}

inline vk_temp_mem VkBeginTempMem(vk_linear_arena* Arena)
{
    // NOTE: This function lets us take all memory allocated past this point and later
    // free it
    vk_temp_mem TempMem = {};
    TempMem.Arena = Arena;
    TempMem.Used = Arena->Used;

    return TempMem;
}

inline void VkEndTempMem(vk_temp_mem TempMem)
{
    TempMem.Arena->Used = TempMem.Used;
}

//
// NOTE: Dynamic Arena
//

inline vk_dynamic_arena VkDynamicArenaCreate(u32 GpuMemoryType, u64 GpuBlockSize)
{
    vk_dynamic_arena Result = {};
    Result.CpuArena = DynamicArenaCreate(KiloBytes(4));
    Result.GpuBlockSize = GpuBlockSize;
    Result.GpuMemoryType = GpuMemoryType;

    return Result;
}

inline vk_ptr VkPushSize(VkDevice Device, vk_dynamic_arena* Arena, u64 Size, u64 Alignment)
{
    vk_ptr Result = {};

    vk_dynamic_arena_header* Header = Arena->CurrHeader;
    u64 AlignedOffset = Header ? AlignAddress(Header->Used, Alignment) : 0;
    if (!Header || (AlignedOffset + Size) > Header->Size)
    {
        // NOTE: Allocate new block
        vk_dynamic_arena_header* NewHeader = PushStruct(&Arena->CpuArena, vk_dynamic_arena_header);
        *NewHeader = {};
        
        NewHeader->GpuMemory = VkMemoryAllocate(Device, Arena->GpuMemoryType, Max(Size, Arena->GpuBlockSize));
        Arena->CurrHeader = NewHeader;
        Header = NewHeader;
        AlignedOffset = AlignAddress(Header->Used, Alignment);
    }

    // NOTE: Suballocate
    Result.Memory = Header->GpuMemory;
    Result.Offset = Header->Used + AlignedOffset;
    Header->Used = AlignedOffset + Size;

    return Result;
}

inline void VkArenaClear(VkDevice Device, vk_dynamic_arena* Arena)
{
    for (dynamic_arena_header* Header = Arena->CpuArena.Next;
         Header;
         Header = Header->Next)
    {
        u32 ByteId = 0;
        for (vk_dynamic_arena_header* VkHeader = (vk_dynamic_arena_header*)DynamicArenaHeaderGetData(Header);
             ByteId < DynamicArenaHeaderGetSize(Header);
             ByteId += sizeof(*VkHeader), VkHeader += 1)
        {
            vkFreeMemory(Device, VkHeader->GpuMemory, 0);
        }
    }

    ArenaClear(&Arena->CpuArena);
}

inline vk_dynamic_temp_mem VkBeginTempMem(vk_dynamic_arena* Arena)
{
    vk_dynamic_temp_mem Result = {};
    Result.Arena = Arena;
    Result.GpuHeader = Arena->CurrHeader;
    Result.GpuUsed = Result.GpuHeader->Used;

    return Result;
}

inline void VkEndTempMem(VkDevice Device, vk_dynamic_temp_mem TempMem)
{
    for (dynamic_arena_header* Header = TempMem.Arena->CpuArena.Prev;
         Header;
         Header = Header->Prev)
    {
        vk_dynamic_arena_header* FirstGpuHeader = (vk_dynamic_arena_header*)DynamicArenaHeaderGetData(Header);
        u32 NumGpuHeaders = u32(DynamicArenaHeaderGetSize(Header) / sizeof(*FirstGpuHeader));

        for (u32 GpuHeaderId = NumGpuHeaders - 1; GpuHeaderId != 0; --GpuHeaderId)
        {
            vk_dynamic_arena_header* CurrGpuHeader = FirstGpuHeader + GpuHeaderId;

            if (CurrGpuHeader == TempMem.GpuHeader)
            {
                CurrGpuHeader->Used = TempMem.GpuUsed;
            }
            else
            {
                vkFreeMemory(Device, CurrGpuHeader->GpuMemory, 0);
                Header->Used -= sizeof(vk_dynamic_arena_header);
            }
        }
    }
}
