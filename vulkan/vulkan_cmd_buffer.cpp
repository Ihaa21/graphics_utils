
//
// NOTE: Command Helpers
//

inline vk_commands VkCommandsCreate(VkDevice Device, VkCommandPool Pool, platform_block_arena* Arena, u32 FlushAlignment,
                                    u32 StagingTypeId)
{
    vk_commands Result = {};

    VkCommandBufferAllocateInfo CmdBufferAllocateInfo = {};
    CmdBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    CmdBufferAllocateInfo.commandPool = Pool;
    CmdBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    CmdBufferAllocateInfo.commandBufferCount = 1;
    VkCheckResult(vkAllocateCommandBuffers(Device, &CmdBufferAllocateInfo, &Result.Buffer));

    VkFenceCreateInfo FenceCreateInfo = {};
    FenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    FenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VkCheckResult(vkCreateFence(Device, &FenceCreateInfo, 0, &Result.Fence));

    // NOTE: Init Barrier Data
    {
        Result.MemoryBarrierArena = BlockArenaCreate(Arena);
        Result.ImageBarrierArena = BlockArenaCreate(Arena);
        Result.BufferBarrierArena = BlockArenaCreate(Arena);
    }

    // NOTE: Init Transfer Data
    {
        Result.BufferTransferArena = BlockArenaCreate(Arena);
        Result.ImageTransferArena = BlockArenaCreate(Arena);
        Result.FlushAlignment = FlushAlignment;
        Result.StagingArena = VkStagingArenaCreate(Device, MegaBytes(64), FlushAlignment, StagingTypeId);
    }
    
    return Result;
}

inline void VkCommandsBegin(vk_commands* Commands, VkDevice Device)
{
    VkCheckResult(vkWaitForFences(Device, 1, &Commands->Fence, VK_TRUE, 0xFFFFFFFF));
    VkCheckResult(vkResetFences(Device, 1, &Commands->Fence));

    // NOTE: Clear our staging buffer if it was populated before
    ArenaClear(&Commands->StagingArena);
    
    VkCommandBufferBeginInfo BeginInfo = {};
    BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    BeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VkCheckResult(vkBeginCommandBuffer(Commands->Buffer, &BeginInfo));
}

inline void VkCommandsEnd(vk_commands* Commands, VkDevice Device)
{
    VkCommandsBarrierFlush(Commands);
    VkCommandsTransferFlush(Commands, Device);
    VkCheckResult(vkEndCommandBuffer(Commands->Buffer));
}

inline void VkCommandsSubmit(vk_commands* Commands, VkDevice Device, VkQueue Queue)
{
    // NOTE: Flush any remaining barriers/transfers
    VkCommandsBarrierFlush(Commands);
    VkCommandsTransferFlush(Commands, Device);
    
    VkCheckResult(vkEndCommandBuffer(Commands->Buffer));

    VkSubmitInfo SubmitInfo = {};
    SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfo.commandBufferCount = 1;
    SubmitInfo.pCommandBuffers = &Commands->Buffer;
    VkCheckResult(vkQueueSubmit(Queue, 1, &SubmitInfo, Commands->Fence));
}

//
// NOTE: Barriers
//

inline barrier_mask BarrierMask(VkAccessFlags AccessMask, VkPipelineStageFlags StageMask)
{
    barrier_mask Result = {};
    Result.AccessMask = AccessMask;
    Result.StageMask = StageMask;

    return Result;
}

inline void VkBarrierMemoryAdd(vk_commands* Commands, VkAccessFlags InputAccessMask, VkPipelineStageFlags InputStageMask,
                               VkAccessFlags OutputAccessMask, VkPipelineStageFlags OutputStageMask)
{
    VkMemoryBarrier* Barrier = PushStruct(&Commands->MemoryBarrierArena, VkMemoryBarrier);
    Commands->NumMemoryBarriers += 1;
    
    Barrier->sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    Barrier->srcAccessMask = InputAccessMask;
    Barrier->dstAccessMask = OutputAccessMask;

    Commands->SrcStageFlags |= InputStageMask;
    Commands->DstStageFlags |= OutputStageMask;
}

inline void VkBarrierBufferAdd(vk_commands* Commands, VkBuffer Buffer, VkAccessFlags InputAccessMask, VkPipelineStageFlags InputStageMask,
                               VkAccessFlags OutputAccessMask, VkPipelineStageFlags OutputStageMask)
{
    VkBufferMemoryBarrier* Barrier = PushStruct(&Commands->BufferBarrierArena, VkBufferMemoryBarrier);
    Commands->NumBufferBarriers += 1;
    
    Barrier->sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    Barrier->srcAccessMask = InputAccessMask;
    Barrier->dstAccessMask = OutputAccessMask;
    Barrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    Barrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    Barrier->buffer = Buffer;
    Barrier->offset = 0;
    Barrier->size = VK_WHOLE_SIZE;

    Commands->SrcStageFlags |= InputStageMask;
    Commands->DstStageFlags |= OutputStageMask;
}

inline void VkBarrierBufferAdd(vk_commands* Commands, barrier_mask InputMask, barrier_mask OutputMask, VkBuffer Buffer)
{
    VkBarrierBufferAdd(Commands, Buffer, InputMask.AccessMask, InputMask.StageMask, OutputMask.AccessMask, OutputMask.StageMask);
}

inline void VkBarrierImageAdd(vk_commands* Commands, VkImage Image, VkImageAspectFlags AspectFlags,
                              VkAccessFlags InputAccessMask, VkPipelineStageFlags InputStageMask, VkImageLayout InputLayout,
                              VkAccessFlags OutputAccessMask, VkPipelineStageFlags OutputStageMask, VkImageLayout OutputLayout)
{
    VkImageMemoryBarrier* Barrier = PushStruct(&Commands->ImageBarrierArena, VkImageMemoryBarrier);
    Commands->NumImageBarriers += 1;

    Barrier->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    Barrier->srcAccessMask = InputAccessMask;
    Barrier->dstAccessMask = OutputAccessMask;
    Barrier->oldLayout = InputLayout;
    Barrier->newLayout = OutputLayout;
    Barrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    Barrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    Barrier->image = Image;
    Barrier->subresourceRange.aspectMask = AspectFlags;
    Barrier->subresourceRange.baseMipLevel = 0;
    Barrier->subresourceRange.levelCount = 1;
    Barrier->subresourceRange.baseArrayLayer = 0;
    Barrier->subresourceRange.layerCount = 1;

    Commands->SrcStageFlags |= InputStageMask;
    Commands->DstStageFlags |= OutputStageMask;
}

inline void VkBarrierImageAdd(vk_commands* Commands, VkImage Image, VkImageAspectFlags AspectFlags, barrier_mask InputMask,
                              VkImageLayout InputLayout, barrier_mask OutputMask, VkImageLayout OutputLayout)
{
    VkBarrierImageAdd(Commands, Image, AspectFlags, InputMask.AccessMask, InputMask.StageMask, InputLayout, OutputMask.AccessMask,
                      OutputMask.StageMask, OutputLayout);
}

inline void VkCommandsBarrierFlush(vk_commands* Commands)
{
    // NOTE: Since we don't store completely contiguous arrays, we have to potentially do multiple barrier calls
    mm BlockSize = BlockArenaGetBlockSize(&Commands->MemoryBarrierArena);
    block* MemoryBlock = Commands->MemoryBarrierArena.Next;
    block* BufferBlock = Commands->BufferBarrierArena.Next;
    block* ImageBlock = Commands->ImageBarrierArena.Next;
    while (Commands->NumMemoryBarriers != 0 || Commands->NumBufferBarriers != 0 || Commands->NumImageBarriers != 0)
    {
        VkMemoryBarrier* MemoryBarriers = MemoryBlock ? BlockGetData(MemoryBlock, VkMemoryBarrier) : 0;
        VkBufferMemoryBarrier* BufferBarriers = BufferBlock ? BlockGetData(BufferBlock, VkBufferMemoryBarrier) : 0;
        VkImageMemoryBarrier* ImageBarriers = ImageBlock ? BlockGetData(ImageBlock, VkImageMemoryBarrier) : 0;

        // NOTE: Cap number of barriers to the max stored in the block
        u32 NumMemoryBarriers = Min(Commands->NumMemoryBarriers, u32(BlockSize / sizeof(VkMemoryBarrier)));
        u32 NumBufferBarriers = Min(Commands->NumBufferBarriers, u32(BlockSize / sizeof(VkBufferMemoryBarrier)));
        u32 NumImageBarriers = Min(Commands->NumImageBarriers, u32(BlockSize / sizeof(VkImageMemoryBarrier)));
        
        vkCmdPipelineBarrier(Commands->Buffer, Commands->SrcStageFlags, Commands->DstStageFlags, VK_DEPENDENCY_BY_REGION_BIT,
                             NumMemoryBarriers, MemoryBarriers, NumBufferBarriers, BufferBarriers, NumImageBarriers, ImageBarriers);

        // NOTE: Decrement # of barriers we still have stored
        Commands->NumMemoryBarriers = Max(0u, Commands->NumMemoryBarriers - NumMemoryBarriers);
        Commands->NumBufferBarriers = Max(0u, Commands->NumBufferBarriers - NumBufferBarriers);
        Commands->NumImageBarriers = Max(0u, Commands->NumImageBarriers - NumImageBarriers);

        // NOTE: Move to next block if we still have blocks left
        MemoryBlock = MemoryBlock ? MemoryBlock->Next : 0;
        BufferBlock = BufferBlock ? BufferBlock->Next : 0;
        ImageBlock = ImageBlock ? ImageBlock->Next : 0;
    }

    Assert(Commands->NumMemoryBarriers == 0);
    Assert(Commands->NumBufferBarriers == 0);
    Assert(Commands->NumImageBarriers == 0);
    
    Commands->SrcStageFlags = 0;
    Commands->DstStageFlags = 0;

    ArenaClear(&Commands->MemoryBarrierArena);
    ArenaClear(&Commands->BufferBarrierArena);
    ArenaClear(&Commands->ImageBarrierArena);
}

//
// NOTE: GPU Trasnfer
//

#define VkCommandsExpand(x) x

// NOTE: Updates
#define VkCommandsPushWriteStruct6(Commands, Buffer, DstOffset, Type, InputMask, OutputMask) \
    (Type*)VkCommandsPushWrite(Commands, Buffer, DstOffset, sizeof(Type), InputMask, OutputMask)
#define VkCommandsPushWriteArray7(Commands, Buffer, DstOffset, Type, Count, InputMask, OutputMask) \
    (Type*)VkCommandsPushWrite(Commands, Buffer, DstOffset, sizeof(Type)*Count, InputMask, OutputMask)

// NOTE: Full writes
#define VkCommandsPushWriteStruct5(Commands, Buffer, Type, InputMask, OutputMask) \
    (Type*)VkCommandsPushWrite(Commands, Buffer, 0, sizeof(Type), InputMask, OutputMask)
#define VkCommandsPushWriteArray6(Commands, Buffer, Type, Count, InputMask, OutputMask) \
    (Type*)VkCommandsPushWrite(Commands, Buffer, 0, sizeof(Type)*Count, InputMask, OutputMask)

// NOTE: Macro overloading (C++ I hate you)
#define VkCommandsStructGetMacro(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, NAME, ...) NAME
#define VkCommandsPushWriteStruct(...) VkCommandsExpand(VkCommandsStructGetMacro(__VA_ARGS__, VkCommandsPushWriteStruct6, VkCommandsPushWriteStruct5)(__VA_ARGS__))

#define VkCommandsArrayGetMacro(Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, NAME, ...) NAME
#define VkCommandsPushWriteArray(...) VkCommandsExpand(VkCommandsArrayGetMacro(__VA_ARGS__, VkCommandsPushWriteArray7, VkCommandsPushWriteArray6)(__VA_ARGS__))

inline u8* VkCommandsPushWrite(vk_commands* Commands, VkBuffer Buffer, u64 DstOffset, u64 WriteSize, barrier_mask InputMask,
                              barrier_mask OutputMask)
{
    // TODO: Do we need to pass in a alignment or can that just be inferred since this is a buffer? minMemoryMapAlignment
    u8* Result = 0;
    vk_staging_ptr StagingPtr = VkStagingPushSize(&Commands->StagingArena, WriteSize);
    vk_buffer_transfer* Transfer = PushStruct(&Commands->BufferTransferArena, vk_buffer_transfer);

    Result = StagingPtr.Ptr;
    Commands->NumBufferTransfers += 1;
    *Transfer = {};
    Transfer->StagingBuffer = StagingPtr.Buffer;
    Transfer->StagingOffset = StagingPtr.Offset;
    Transfer->Buffer = Buffer;
    Transfer->Size = WriteSize;
    Transfer->DstOffset = DstOffset;
    Transfer->InputMask = InputMask;
    Transfer->OutputMask = OutputMask;

    return Result;
}

inline u8* VkCommandsPushWrite(vk_commands* Commands, VkBuffer Buffer, u64 WriteSize, barrier_mask InputMask, barrier_mask OutputMask)
{
    u8* Result = VkCommandsPushWrite(Commands, Buffer, 0, WriteSize, InputMask, OutputMask);
    return Result;
}

inline u8* VkCommandsPushWriteImage(vk_commands* Commands, VkImage Image, u32 OffsetX, u32 OffsetY, u32 OffsetZ, u32 Width, u32 Height,
                                    u32 Depth, mm TexelSize, VkImageAspectFlagBits AspectMask, VkImageLayout InputLayout,
                                    VkImageLayout OutputLayout, barrier_mask InputMask, barrier_mask OutputMask)
{
    u8* Result = 0;
    // TODO: If we can get the size of a format, we wouldn't need texelsize anymore
    u64 ImageSize = Width * Height * Depth * TexelSize;
    vk_staging_ptr StagingPtr = VkStagingPushSize(&Commands->StagingArena, ImageSize, TexelSize);
    vk_image_transfer* Transfer = PushStruct(&Commands->ImageTransferArena, vk_image_transfer);

    Result = StagingPtr.Ptr;
    Commands->NumImageTransfers += 1;
    Transfer->StagingBuffer = StagingPtr.Buffer;
    Transfer->StagingOffset = StagingPtr.Offset;
    Transfer->Image = Image;
    Transfer->OffsetX = OffsetX;
    Transfer->OffsetY = OffsetY;
    Transfer->OffsetZ = OffsetZ;
    Transfer->Width = Width;
    Transfer->Height = Height;
    Transfer->Depth = Depth;
    Transfer->AspectMask = AspectMask;
    Transfer->InputMask = InputMask;
    Transfer->InputLayout = InputLayout;
    Transfer->OutputMask = OutputMask;
    Transfer->OutputLayout = OutputLayout;
    
    return Result;
}

inline u8* VkCommandsPushWriteImage(vk_commands* Commands, VkImage Image, u32 Width, u32 Height, u32 Depth, mm TexelSize, 
                                   VkImageAspectFlagBits AspectMask, VkImageLayout InputLayout, VkImageLayout OutputLayout,
                                   barrier_mask InputMask, barrier_mask OutputMask)
{
    u8* Result = VkCommandsPushWriteImage(Commands, Image, 0, 0, 0, Width, Height, Depth, TexelSize, AspectMask, InputLayout, OutputLayout,
                                          InputMask, OutputMask);
    return Result;
}

inline u8* VkCommandsPushWriteImage(vk_commands* Commands, VkImage Image, u32 OffsetX, u32 OffsetY, u32 Width, u32 Height,
                                    mm TexelSize, VkImageAspectFlagBits AspectMask, VkImageLayout InputLayout, VkImageLayout OutputLayout,
                                    barrier_mask InputMask, barrier_mask OutputMask)
{
    u8* Result = VkCommandsPushWriteImage(Commands, Image, OffsetX, OffsetY, 0, Width, Height, 1, TexelSize, AspectMask, InputLayout,
                                          OutputLayout, InputMask, OutputMask);
    return Result;
}

inline u8* VkCommandsPushWriteImage(vk_commands* Commands, VkImage Image, u32 Width, u32 Height, mm TexelSize, 
                                    VkImageAspectFlagBits AspectMask, VkImageLayout InputLayout, VkImageLayout OutputLayout,
                                    barrier_mask InputMask, barrier_mask OutputMask)
{
    u8* Result = VkCommandsPushWriteImage(Commands, Image, 0, 0, Width, Height, TexelSize, AspectMask, InputLayout, OutputLayout,
                                          InputMask, OutputMask);
    return Result;
}

inline void VkCommandsTransferFlush(vk_commands* Commands, VkDevice Device)
{
    // NOTE: Flush all staging memory we have written to so far
    if (Commands->StagingArena.Next)
    {
        for (vk_staging_arena_header* CurrHeader = Commands->StagingArena.Next; CurrHeader; CurrHeader = CurrHeader->Next)
        {
            // NOTE: Align the current headers used size to flush alignment so we don't overwrite it with partial writes later
            CurrHeader->Used = Min(CurrHeader->Size, AlignAddress(CurrHeader->Used, u64(Commands->FlushAlignment)));
            
            VkMappedMemoryRange FlushRange = {};
            FlushRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            FlushRange.memory = CurrHeader->GpuMemory;
            FlushRange.offset = 0;
            FlushRange.size = CurrHeader->Used;
            vkFlushMappedMemoryRanges(Device, 1, &FlushRange);
        }
    }

    barrier_mask IntermediateMask = BarrierMask(VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    
    // NOTE: Transfer all buffers
    if (Commands->NumBufferTransfers > 0)
    {
        // NOTE: Apply pre transfer barriers
        {
            block* CurrBlock = Commands->BufferTransferArena.Next;
            for (u32 BufferId = 0; BufferId < Commands->NumBufferTransfers; )
            {
                u32 NumTransfersInBlock = Min(u32(BlockArenaGetBlockSize(&Commands->BufferTransferArena)),
                                              (Commands->NumBufferTransfers - BufferId));
                for (u32 SubBufferId = 0; SubBufferId < NumTransfersInBlock; ++SubBufferId)
                {
                    vk_buffer_transfer* BufferTransfer = BlockGetData(CurrBlock, vk_buffer_transfer) + SubBufferId;            
                    VkBarrierBufferAdd(Commands, BufferTransfer->InputMask, IntermediateMask, BufferTransfer->Buffer);
                }

                BufferId += NumTransfersInBlock;
                CurrBlock = CurrBlock->Next;
            }

            VkCommandsBarrierFlush(Commands);
        }

        // NOTE: Apply transfers
        {
            block* CurrBlock = Commands->BufferTransferArena.Next;
            for (u32 BufferId = 0; BufferId < Commands->NumBufferTransfers; )
            {
                u32 NumTransfersInBlock = Min(u32(BlockArenaGetBlockSize(&Commands->BufferTransferArena)),
                                              (Commands->NumBufferTransfers - BufferId));
                for (u32 SubBufferId = 0; SubBufferId < NumTransfersInBlock; ++SubBufferId)
                {
                    vk_buffer_transfer* BufferTransfer = BlockGetData(CurrBlock, vk_buffer_transfer) + SubBufferId;

                    VkBufferCopy BufferCopy = {};
                    BufferCopy.srcOffset = BufferTransfer->StagingOffset;
                    BufferCopy.dstOffset = BufferTransfer->DstOffset;
                    BufferCopy.size = BufferTransfer->Size;
                    vkCmdCopyBuffer(Commands->Buffer, BufferTransfer->StagingBuffer, BufferTransfer->Buffer, 1, &BufferCopy);
                }

                BufferId += NumTransfersInBlock;
                CurrBlock = CurrBlock->Next;
            }
        }

        // NOTE: Apply post transfer barriers
        {
            block* CurrBlock = Commands->BufferTransferArena.Next;
            for (u32 BufferId = 0; BufferId < Commands->NumBufferTransfers; )
            {
                u32 NumTransfersInBlock = Min(u32(BlockArenaGetBlockSize(&Commands->BufferTransferArena)),
                                              (Commands->NumBufferTransfers - BufferId));
                for (u32 SubBufferId = 0; SubBufferId < NumTransfersInBlock; ++SubBufferId)
                {
                    vk_buffer_transfer* BufferTransfer = BlockGetData(CurrBlock, vk_buffer_transfer) + SubBufferId;
                    VkBarrierBufferAdd(Commands, IntermediateMask, BufferTransfer->OutputMask, BufferTransfer->Buffer);
                }

                BufferId += NumTransfersInBlock;
                CurrBlock = CurrBlock->Next;
            }

            VkCommandsBarrierFlush(Commands);
        }

        ArenaClear(&Commands->BufferTransferArena);
        Commands->NumBufferTransfers = 0;
    }
    
    // NOTE: Transfer all images
    if (Commands->NumImageTransfers > 0)
    {
        // NOTE: Apply pre transfer barriers
        {
            block* CurrBlock = Commands->ImageTransferArena.Next;
            for (u32 ImageId = 0; ImageId < Commands->NumImageTransfers; )
            {
                u32 NumTransfersInBlock = Min(u32(BlockArenaGetBlockSize(&Commands->ImageTransferArena)),
                                              (Commands->NumImageTransfers - ImageId));
                for (u32 SubImageId = 0; SubImageId < NumTransfersInBlock; ++SubImageId)
                {
                    vk_image_transfer* ImageTransfer = BlockGetData(CurrBlock, vk_image_transfer) + SubImageId;
                    VkBarrierImageAdd(Commands, ImageTransfer->Image, ImageTransfer->AspectMask, ImageTransfer->InputMask,
                                      ImageTransfer->InputLayout, IntermediateMask, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
                }
                
                ImageId += NumTransfersInBlock;
                CurrBlock = CurrBlock->Next;
            }

            VkCommandsBarrierFlush(Commands);
        }

        // NOTE: Apply transfers
        {
            block* CurrBlock = Commands->ImageTransferArena.Next;
            for (u32 ImageId = 0; ImageId < Commands->NumImageTransfers; )
            {
                u32 NumTransfersInBlock = Min(u32(BlockArenaGetBlockSize(&Commands->ImageTransferArena)),
                                              (Commands->NumImageTransfers - ImageId));
                for (u32 SubImageId = 0; SubImageId < NumTransfersInBlock; ++SubImageId)
                {
                    vk_image_transfer* ImageTransfer = BlockGetData(CurrBlock, vk_image_transfer) + SubImageId;
                    
                    VkBufferImageCopy ImageCopy = {};
                    ImageCopy.bufferOffset = ImageTransfer->StagingOffset;
                    ImageCopy.bufferRowLength = 0;
                    ImageCopy.bufferImageHeight = 0;
                    ImageCopy.imageSubresource.aspectMask = ImageTransfer->AspectMask;
                    ImageCopy.imageSubresource.mipLevel = 0;
                    ImageCopy.imageSubresource.baseArrayLayer = 0;
                    ImageCopy.imageSubresource.layerCount = 1;
                    ImageCopy.imageOffset.x = ImageTransfer->OffsetX;
                    ImageCopy.imageOffset.y = ImageTransfer->OffsetY;
                    ImageCopy.imageOffset.z = ImageTransfer->OffsetZ;
                    ImageCopy.imageExtent.width = ImageTransfer->Width;
                    ImageCopy.imageExtent.height = ImageTransfer->Height;
                    ImageCopy.imageExtent.depth = ImageTransfer->Depth;
                    vkCmdCopyBufferToImage(Commands->Buffer, ImageTransfer->StagingBuffer, ImageTransfer->Image,
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &ImageCopy);
                }
                
                ImageId += NumTransfersInBlock;
                CurrBlock = CurrBlock->Next;
            }
        }
        
        // NOTE: Apply post transfer barriers
        {
            block* CurrBlock = Commands->ImageTransferArena.Next;
            for (u32 ImageId = 0; ImageId < Commands->NumImageTransfers; )
            {
                u32 NumTransfersInBlock = Min(u32(BlockArenaGetBlockSize(&Commands->ImageTransferArena)),
                                              (Commands->NumImageTransfers - ImageId));
                for (u32 SubImageId = 0; SubImageId < NumTransfersInBlock; ++SubImageId)
                {
                    vk_image_transfer* ImageTransfer = BlockGetData(CurrBlock, vk_image_transfer) + SubImageId;
                    VkBarrierImageAdd(Commands, ImageTransfer->Image, ImageTransfer->AspectMask, IntermediateMask,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, ImageTransfer->OutputMask, ImageTransfer->OutputLayout);
                }
                
                ImageId += NumTransfersInBlock;
                CurrBlock = CurrBlock->Next;
            }
            
            VkCommandsBarrierFlush(Commands);
        }

        ArenaClear(&Commands->ImageTransferArena);
        Commands->NumImageTransfers = 0;
    }
}
