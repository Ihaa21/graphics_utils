//
// NOTE: Coordinate System Functions
//

/*
   NOTE: Vulkan coordinate system:

     - Camera space: x points left, y points up, z points away
     - Screen space: x[-1:1] points left, y[-1:1] points down, z[0:1] points away

     The games coordinate systems are as follows:

       - World space: x points left, y points up the screen, z points up and down
       - Screen space: x points left [0:1], y points up [0:1]
     
 */

RENDER_WORLD_TO_SCREEN(VkWorldToScreen)
{
    v3 Result = {};

    m4 ProjMat = CameraGetVP(Camera);
    v4 ProjPoint = ProjMat*V4(WorldPos, 1.0f);
    Result = ProjPoint.xyz / ProjPoint.w;

    // NOTE: Convert vulkan NDC to games screen coordinates
    Result.xy = 0.5f*(Result.xy + 1.0f);
    Result.y = 1.0f - Result.y;
    
    return Result;
}

RENDER_SCREEN_TO_WORLD(VkScreenToWorld)
{
    m4 ProjMat = CameraGetVP(Camera);
    m4 InverseMat = Inverse(ProjMat);
    
    // NOTE: Convert the games [0:1] coordinates to vulkans screen coordinates
    v2 NDC = 2.0f*ScreenPos - V2(1, 1);
    NDC.y *= -1.0f;
    
    v4 World = InverseMat*V4(NDC.x, NDC.y, Camera->Near, 1.0f);
    World.xyz /= World.w;

    return World.xyz;
}

RENDER_ORTHO_PROJ_M4(VkOrthoProjM4)
{
    m4 Result = {};
    Result.v[0].x = 2.0f / (Right - Left);
    Result.v[1].y = -2.0f / (Top - Bottom);
    Result.v[2].z = 1.0f / (Far - Near);
    Result.v[3].x = (-Right + Left) / (Right - Left);
    Result.v[3].y = (Top - Bottom) / (Top - Bottom);
    Result.v[3].z = (-Near) / (Far - Near);
    Result.v[3].w = 1.0f;

    return Result;
}

RENDER_PERSP_PROJ_M4(VkPerspProjM4)
{
    m4 Result = {};
    Result.v[0].x = 1.0f / (AspectRatio*Tan(Fov*0.5f));
    Result.v[1].y = -1.0f / (Tan(Fov*0.5f));
    Result.v[2].z = (-Far) / (Near - Far);
    Result.v[2].w = 1.0f;
    Result.v[3].z = (Near*Far) / (Near - Far);
    
    return Result;
}

inline aabb2i VkClipConversion(render_state* RenderState, aabb2i ClipBounds)
{
    aabb2i Result = ClipBounds;
    Result.Min.y = RenderState->Settings.Height - ClipBounds.Max.y;
    Result.Max.y = RenderState->Settings.Height - ClipBounds.Min.y;

    return Result;
}

//
// NOTE: Memory Allocators
//

inline u64 VkAlignAddress(u64 Address, u64 Alignment)
{
    u64 Result = (Address + (Alignment-1)) & ~(Alignment-1);
    return Result;
}

inline vk_gpu_linear_arena VkInitGpuLinearArena(vk_gpu_ptr MemPtr, u64 Size)
{
    vk_gpu_linear_arena Result = {};
    Result.Size = Size;
    Result.MemPtr = MemPtr;

    return Result;
}

inline vk_gpu_ptr VkPushSize(vk_gpu_linear_arena* Arena, u64 Size, u64 Alignment)
{
    Assert(Alignment != 0);
    
    // TODO: Can we assume that our memory is aligned to our resources max requirement?
    u64 AlignedOffset = VkAlignAddress(Arena->Used, Alignment);
    Assert(AlignedOffset + Size <= Arena->Size);
    
    vk_gpu_ptr Result = {};
    Result.Memory = Arena->MemPtr.Memory;
    Result.Offset = AlignedOffset;

    Arena->Used = AlignedOffset + Size;

    return Result;
}

inline vk_gpu_temp_mem VkBeginTempMem(vk_gpu_linear_arena* Arena)
{
    // NOTE: This function lets us take all memory allocated past this point and later
    // free it
    vk_gpu_temp_mem TempMem = {};
    TempMem.Arena = Arena;
    TempMem.Used = Arena->Used;

    return TempMem;
}

inline void VkEndTempMem(vk_gpu_temp_mem TempMem)
{
    TempMem.Arena->Used = TempMem.Used;
}

inline VkDeviceMemory VkMemoryAllocate(vulkan_state* VulkanState, u32 Type, u64 Size)
{
    VkDeviceMemory Result = {};
    
    VkMemoryAllocateInfo MemoryAllocateInfo = {};
    MemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    MemoryAllocateInfo.allocationSize = Size;
    MemoryAllocateInfo.memoryTypeIndex = Type;
    VkCheckResult(vkAllocateMemory(VulkanState->Device, &MemoryAllocateInfo, 0, &Result));

    return Result;
}

//
// NOTE: General Helpers
//

// TODO: Is it better to just pass temp mem to some of these structures so that it gets reused and
// doesn't need to be preallocated and wasted?

inline vk_commands VkCreateCommands(VkDevice Device, VkCommandPool Pool)
{
    vk_commands Result = {};

    VkCommandBufferAllocateInfo CmdBufferAllocateInfo = {};
    CmdBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    CmdBufferAllocateInfo.commandPool = Pool;
    CmdBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    CmdBufferAllocateInfo.commandBufferCount = 1;
    VkCheckResult(vkAllocateCommandBuffers(Device, &CmdBufferAllocateInfo, &Result.Buffer));

    VkSemaphoreCreateInfo SemaphoreCreateInfo = {};
    SemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkCheckResult(vkCreateSemaphore(Device, &SemaphoreCreateInfo, 0, &Result.FinishSemaphore));

    VkFenceCreateInfo FenceCreateInfo = {};
    FenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    FenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VkCheckResult(vkCreateFence(Device, &FenceCreateInfo, 0, &Result.Fence));

    return Result;
}

// TODO: We don't use the gpu ptr anywhere as far as I can tell, remove?
inline void VkCreateBuffer(VkDevice Device, vk_gpu_linear_arena* Arena, VkBufferUsageFlags Usage,
                           u64 BufferSize, VkBuffer* OutBuffer, vk_gpu_ptr* OutGpuPtr)
{
    VkBufferCreateInfo BufferCreateInfo = {};
    BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    BufferCreateInfo.size = BufferSize;
    BufferCreateInfo.usage = Usage;
    BufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    BufferCreateInfo.queueFamilyIndexCount = 0;
    BufferCreateInfo.pQueueFamilyIndices = 0;
    VkCheckResult(vkCreateBuffer(Device, &BufferCreateInfo, 0, OutBuffer));

    VkMemoryRequirements BufferMemRequirements;
    vkGetBufferMemoryRequirements(Device, *OutBuffer, &BufferMemRequirements);
    *OutGpuPtr = VkPushSize(Arena, BufferMemRequirements.size, BufferMemRequirements.alignment);
    VkCheckResult(vkBindBufferMemory(Device, *OutBuffer, *OutGpuPtr->Memory, OutGpuPtr->Offset));
}

inline void VkCreateBuffer(VkDevice Device, vk_gpu_linear_arena* Arena, VkBufferUsageFlags Usage,
                           u64 BufferSize, vk_buffer* Buffer)
{
    VkCreateBuffer(Device, Arena, Usage, BufferSize, &Buffer->Buffer, &Buffer->Ptr);
}

inline void VkCreateComputePipeline(VkDevice Device, VkShaderModule Shader,
                                    VkDescriptorSetLayout* Layouts, u32 NumLayouts,
                                    VkPipeline* OutPipeline, VkPipelineLayout* OutPipelineLayout)
{
    VkPipelineShaderStageCreateInfo ShaderStageCreateInfo = {};
    ShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    ShaderStageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    ShaderStageCreateInfo.module = Shader;
    ShaderStageCreateInfo.pName = "main";
    ShaderStageCreateInfo.pSpecializationInfo = 0;
            
    VkPipelineLayoutCreateInfo LayoutCreateInfo = {};
    LayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    LayoutCreateInfo.setLayoutCount = NumLayouts;
    LayoutCreateInfo.pSetLayouts = Layouts;
    VkCheckResult(vkCreatePipelineLayout(Device, &LayoutCreateInfo, 0, OutPipelineLayout));

    VkComputePipelineCreateInfo PipelineCreateInfo = {};
    PipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    PipelineCreateInfo.stage = ShaderStageCreateInfo;
    PipelineCreateInfo.layout = *OutPipelineLayout;
    PipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    PipelineCreateInfo.basePipelineIndex = -1;
    VkCheckResult(vkCreateComputePipelines(Device, VK_NULL_HANDLE, 1, &PipelineCreateInfo, 0,
                                           OutPipeline));
}

inline vk_image VkCreateImage(VkDevice Device, vk_gpu_linear_arena* Arena, u32 Width, u32 Height,
                              VkFormat Format, i32 Usage, VkImageAspectFlags AspectMask)
{
    vk_image Result = {};
    
    VkImageCreateInfo ImageCreateInfo = {};
    ImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    ImageCreateInfo.format = Format;
    ImageCreateInfo.extent.width = Width;
    ImageCreateInfo.extent.height = Height;
    ImageCreateInfo.extent.depth = 1;
    ImageCreateInfo.mipLevels = 1;
    ImageCreateInfo.arrayLayers = 1;
    ImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    ImageCreateInfo.usage = Usage;
    ImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ImageCreateInfo.queueFamilyIndexCount = 0;
    ImageCreateInfo.pQueueFamilyIndices = 0;
    ImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkCheckResult(vkCreateImage(Device, &ImageCreateInfo, 0, &Result.Image));

    VkMemoryRequirements ImageMemRequirements;
    vkGetImageMemoryRequirements(Device, Result.Image, &ImageMemRequirements);
    vk_gpu_ptr MemPtr = VkPushSize(Arena, ImageMemRequirements.size, ImageMemRequirements.alignment);
    VkCheckResult(vkBindImageMemory(Device, Result.Image, *MemPtr.Memory, MemPtr.Offset));

    // NOTE: Create image view
    VkImageViewCreateInfo ImgViewCreateInfo = {};
    ImgViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ImgViewCreateInfo.image = Result.Image;
    ImgViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ImgViewCreateInfo.format = Format;
    ImgViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    ImgViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    ImgViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    ImgViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    ImgViewCreateInfo.subresourceRange.aspectMask = AspectMask;
    ImgViewCreateInfo.subresourceRange.baseMipLevel = 0;
    ImgViewCreateInfo.subresourceRange.levelCount = 1;
    ImgViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    ImgViewCreateInfo.subresourceRange.layerCount = 1;
    VkCheckResult(vkCreateImageView(Device, &ImgViewCreateInfo, 0, &Result.View));

    return Result;
}

inline VkFramebuffer VkCreateFBO(vulkan_state* VulkanState, VkRenderPass Rp, VkImageView* Views, u32 NumViews,
                                 u32 Width, u32 Height)
{
    VkFramebuffer Result = {};
    
    VkFramebufferCreateInfo FrameBufferCreateInfo = {};
    FrameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    FrameBufferCreateInfo.renderPass = Rp;
    FrameBufferCreateInfo.attachmentCount = NumViews;
    FrameBufferCreateInfo.pAttachments = Views;
    FrameBufferCreateInfo.width = Width;
    FrameBufferCreateInfo.height = Height;
    FrameBufferCreateInfo.layers = 1;
    VkCheckResult(vkCreateFramebuffer(VulkanState->Device, &FrameBufferCreateInfo, 0, &Result));

    return Result;
}

inline void VkReCreateFBO(vulkan_state* VulkanState, VkRenderPass Rp, VkImageView* Views, u32 NumViews,
                          VkFramebuffer* Fbo, u32 Width, u32 Height)
{
    if (*Fbo != VK_NULL_HANDLE)
    {
        vkDestroyFramebuffer(VulkanState->Device, *Fbo, 0);
    }

    *Fbo = VkCreateFBO(VulkanState, Rp, Views, NumViews, Width, Height);
}

//
// NOTE: Descriptor Set Helpers
//

inline VkDescriptorSet VkAllocateDescriptorSet(vulkan_state* VulkanState, VkDescriptorSetLayout Layout)
{
    VkDescriptorSet Result = {};
    
    VkDescriptorSetAllocateInfo DSAllocateInfo = {};
    DSAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    DSAllocateInfo.descriptorPool = VulkanState->DPool;
    DSAllocateInfo.descriptorSetCount = 1;
    DSAllocateInfo.pSetLayouts = &Layout;
    VkCheckResult(vkAllocateDescriptorSets(VulkanState->Device, &DSAllocateInfo, &Result));

    return Result;
}

struct vk_descriptor_layout_builder
{
    u32 CurrNumBindings;
    VkDescriptorSetLayoutBinding Bindings[10];
    VkDescriptorSetLayout* Layout;
};

inline vk_descriptor_layout_builder VkDescriptorLayoutBegin(VkDescriptorSetLayout* Layout)
{
    vk_descriptor_layout_builder Result = {};
    Result.Layout = Layout;
    
    return Result;
}

inline void VkDescriptorLayoutAdd(vk_descriptor_layout_builder* Builder, VkDescriptorType Type, u32 DescriptorCount,
                                  VkShaderStageFlags StageFlags)
{
    Assert(Builder->CurrNumBindings < ArrayCount(Builder->Bindings));
    u32 Id = Builder->CurrNumBindings++;
    Builder->Bindings[Id] = {};
    Builder->Bindings[Id].binding = Id;
    Builder->Bindings[Id].descriptorType = Type;
    Builder->Bindings[Id].descriptorCount = DescriptorCount;
    Builder->Bindings[Id].stageFlags = StageFlags;
}

inline void VkDescriptorLayoutEnd(vulkan_state* VulkanState, vk_descriptor_layout_builder* Builder)
{
    VkDescriptorSetLayoutCreateInfo DSLayoutCreateInfo = {};
    DSLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    DSLayoutCreateInfo.bindingCount = Builder->CurrNumBindings;
    DSLayoutCreateInfo.pBindings = Builder->Bindings;
    VkCheckResult(vkCreateDescriptorSetLayout(VulkanState->Device, &DSLayoutCreateInfo, 0, Builder->Layout));
}

//
// NOTE: Render Pass Helpers
//

inline VkAttachmentDescription VkRenderPassAttachment(VkFormat Format, VkAttachmentLoadOp LoadOp, VkAttachmentStoreOp StoreOp,
                                                      VkImageLayout InitialLayout, VkImageLayout FinalLayout)
{
    VkAttachmentDescription Result = {};
    Result.flags = 0;
    Result.format = Format;
    Result.samples = VK_SAMPLE_COUNT_1_BIT;
    Result.loadOp = LoadOp;
    Result.storeOp = StoreOp;
    Result.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    Result.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    Result.initialLayout = InitialLayout;
    Result.finalLayout = FinalLayout;

    return Result;
}

inline VkRenderPass VkRenderPassColorDepth(vulkan_state* VulkanState, VkAttachmentDescription AttachmentDescriptions[2])
{
    VkRenderPass Result = {};

    // IMPORTANT: We assume first attachment is color, second is depth
    VkAttachmentReference ColorReference = {};
    ColorReference.attachment = 0;
    ColorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference DepthReference = {};
    DepthReference.attachment = 1;
    DepthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription SubpassDescriptions[1] = {};
    SubpassDescriptions[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    SubpassDescriptions[0].colorAttachmentCount = 1;
    SubpassDescriptions[0].pColorAttachments = &ColorReference;
    SubpassDescriptions[0].pDepthStencilAttachment = &DepthReference;
        
    VkRenderPassCreateInfo RenderPassCreateInfo = {};
    RenderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    RenderPassCreateInfo.attachmentCount = 2;
    RenderPassCreateInfo.pAttachments = AttachmentDescriptions;
    RenderPassCreateInfo.subpassCount = 1;
    RenderPassCreateInfo.pSubpasses = SubpassDescriptions;

    VkCheckResult(vkCreateRenderPass(VulkanState->Device, &RenderPassCreateInfo, 0, &Result));

    return Result;
}

//
// NOTE: Img Layout Updater
//

inline vk_barrier_batcher VkInitBarrierBatcher(linear_arena* Arena, u32 MaxNumBarriers)
{
    vk_barrier_batcher Result = {};

    Result.MaxNumImgBarriers = MaxNumBarriers;
    Result.ImgBarrierArray = PushArray(Arena, VkImageMemoryBarrier, MaxNumBarriers);

    Result.MaxNumBufferBarriers = MaxNumBarriers;
    Result.BufferBarrierArray = PushArray(Arena, VkBufferMemoryBarrier, MaxNumBarriers);

    return Result;
}

inline VkImageMemoryBarrier* VkGetImgBarriers(vk_barrier_batcher* Batcher, u32 NumBarriers)
{
    Assert(Batcher->NumImgBarriers + NumBarriers <= Batcher->MaxNumImgBarriers);
    VkImageMemoryBarrier* Result = Batcher->ImgBarrierArray + Batcher->NumImgBarriers;
    Batcher->NumImgBarriers += NumBarriers;

    return Result;
}

inline VkBufferMemoryBarrier* VkGetBufferBarriers(vk_barrier_batcher* Batcher, u32 NumBarriers)
{
    Assert(Batcher->NumBufferBarriers + NumBarriers <= Batcher->MaxNumBufferBarriers);
    VkBufferMemoryBarrier* Result = Batcher->BufferBarrierArray + Batcher->NumBufferBarriers;
    Batcher->NumBufferBarriers += NumBarriers;

    return Result;
}

inline void VkFlushBarrierBatcher(VkCommandBuffer CmdBuffer, vk_barrier_batcher* Batcher,
                                  VkPipelineStageFlags SrcStage, VkPipelineStageFlags DstStage,
                                  VkDependencyFlags Dependecy)
{
    vkCmdPipelineBarrier(CmdBuffer, SrcStage, DstStage, Dependecy, 0, 0, Batcher->NumBufferBarriers,
                         Batcher->BufferBarrierArray, Batcher->NumImgBarriers, Batcher->ImgBarrierArray);

    Batcher->NumBufferBarriers = 0;
    Batcher->NumImgBarriers = 0;
}

//
// NOTE: Descriptor Updater
//

inline vk_desc_updater VkInitDescUpdater(linear_arena* Arena, u32 MaxNumWrites)
{
    vk_desc_updater Result = {};

    u32 ArenaSize = (sizeof(VkWriteDescriptorSet)*MaxNumWrites +
                     Max((u32)sizeof(VkDescriptorImageInfo), (u32)sizeof(VkDescriptorBufferInfo))*MaxNumWrites);
    Result.Arena = LinearSubArena(Arena, ArenaSize);
    Result.MaxNumWrites = MaxNumWrites;
    Result.WriteArray = PushArray(&Result.Arena, VkWriteDescriptorSet, MaxNumWrites);

    return Result;
}

inline void VkBufferWriteDesc(vk_desc_updater* Updater, VkDescriptorSet Set, u32 Binding,
                              VkDescriptorType DescType, VkBuffer Buffer)
{
    VkDescriptorBufferInfo* BufferInfo = PushStruct(&Updater->Arena, VkDescriptorBufferInfo);
    BufferInfo->buffer = Buffer;
    BufferInfo->offset = 0;
    BufferInfo->range = VK_WHOLE_SIZE;

    Assert(Updater->NumWrites < Updater->MaxNumWrites);
    VkWriteDescriptorSet* DsWrite = Updater->WriteArray + Updater->NumWrites++;
    *DsWrite = {};
    DsWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    DsWrite->dstSet = Set;
    DsWrite->dstBinding = Binding;
    DsWrite->descriptorCount = 1;
    DsWrite->descriptorType = DescType;
    DsWrite->pBufferInfo = BufferInfo;
}

inline void VkImageWriteDesc(vk_desc_updater* Updater, VkDescriptorSet Set, u32 Binding,
                             VkDescriptorType DescType, VkImageView ImageView, VkSampler Sampler,
                             VkImageLayout ImageLayout)
{
    VkDescriptorImageInfo* ImageInfo = PushStruct(&Updater->Arena, VkDescriptorImageInfo);
    ImageInfo->sampler = Sampler;
    ImageInfo->imageView = ImageView;
    ImageInfo->imageLayout = ImageLayout;

    Assert(Updater->NumWrites < Updater->MaxNumWrites);
    VkWriteDescriptorSet* DsWrite = Updater->WriteArray + Updater->NumWrites++;
    *DsWrite = {};
    DsWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    DsWrite->dstSet = Set;
    DsWrite->dstBinding = Binding;
    DsWrite->descriptorCount = 1;
    DsWrite->descriptorType = DescType;
    DsWrite->pImageInfo = ImageInfo;
}

inline void VkFlushDescUpdater(vulkan_state* VulkanState, vk_desc_updater* Updater)
{
    vkUpdateDescriptorSets(VulkanState->Device, Updater->NumWrites, Updater->WriteArray, 0, 0);

    Updater->NumWrites = 0;
    Updater->Arena.Used = sizeof(VkWriteDescriptorSet)*Updater->MaxNumWrites;
}

//
// NOTE: vk_transfer_updater
//

inline vk_transfer_updater VkInitTransferUpdater(vulkan_state* VulkanState, linear_arena* CpuArena, vk_gpu_linear_arena* GpuArena,
                                                 u32 FlushAlignment, u64 StagingSize, u32 MaxNumBufferTransfers,
                                                 u32 MaxNumImageTransfers, u32 MaxNumBufferUpdates)
{
    vk_transfer_updater Result = {};

    // TODO: Fix this
    u32 ArenaSize = (500*MaxNumBufferTransfers);
    Result.Arena = LinearSubArena(CpuArena, ArenaSize);
    Result.FlushAlignment = FlushAlignment;

    StagingSize = VkAlignAddress(StagingSize, FlushAlignment);
    
    // NOTE: Staging data
    {
        VkBufferCreateInfo BufferCreateInfo = {};
        BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        BufferCreateInfo.size = StagingSize;
        BufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        BufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VkCheckResult(vkCreateBuffer(VulkanState->Device, &BufferCreateInfo, 0, &Result.StagingBuffer));

        VkMemoryRequirements BufferMemRequirements;
        vkGetBufferMemoryRequirements(VulkanState->Device, Result.StagingBuffer, &BufferMemRequirements);

        Result.StagingMem = VkMemoryAllocate(VulkanState, VulkanState->StagingTypeId, BufferMemRequirements.size);
        VkCheckResult(vkBindBufferMemory(VulkanState->Device, Result.StagingBuffer, Result.StagingMem, 0));
        VkCheckResult(vkMapMemory(VulkanState->Device, Result.StagingMem, 0, BufferMemRequirements.size,
                                  0, (void**)&Result.StagingPtr));
        Result.StagingSize = BufferMemRequirements.size;
    }
    
    // NOTE: Buffer data
    Result.MaxNumBufferTransfers = MaxNumBufferTransfers;
    Result.BufferTransferArray = PushArray(CpuArena, vk_buffer_transfer, MaxNumBufferTransfers);

    // NOTE: Image data
    Result.MaxNumImageTransfers = MaxNumImageTransfers;
    Result.ImageTransferArray = PushArray(CpuArena, vk_image_transfer, MaxNumImageTransfers);

    // NOTE: Buffer update data
    Result.MaxNumBufferUpdates = MaxNumBufferUpdates;
    Result.BufferUpdateArray = PushArray(CpuArena, vk_buffer_update, MaxNumBufferUpdates);

    return Result;
}

inline u8* VkPushBufferUpdateToGpu(vk_transfer_updater* Updater, VkBuffer Buffer, u32 BufferSize,
                                   u32 BufferOffset, mm Alignment)
{
    Updater->StagingOffset = AlignAddress(Updater->StagingOffset, Alignment);
    Assert((Updater->StagingOffset + BufferSize) <= Updater->StagingSize);
    u8* Result = Updater->StagingPtr + Updater->StagingOffset;
 
    vk_buffer_update* Update = Updater->BufferUpdateArray + Updater->NumBufferUpdates++;
    Update->Buffer = Buffer;
    Update->Size = BufferSize;
    Update->StagingOffset = Updater->StagingOffset;
    Update->DstOffset = BufferOffset;

    Updater->StagingOffset += u64(BufferSize);
    return Result;
}

#define VkPushStructBufferToGpu(Updater, Buffer, Type, Alignment, DstAccessMask) (Type*)VkPushBufferToGpu(Updater, Buffer, sizeof(Type), Alignment, DstAccessMask)
#define VkPushArrayBufferToGpu(Updater, Buffer, Type, Count, Alignment, DstAccessMask) (Type*)VkPushBufferToGpu(Updater, Buffer, sizeof(Type)*Count, Alignment, DstAccessMask)
inline u8* VkPushBufferToGpu(vk_transfer_updater* Updater, VkBuffer Buffer, u32 BufferSize, mm Alignment,
                             VkAccessFlags DstAccessMask)
{
    Updater->StagingOffset = AlignAddress(Updater->StagingOffset, Alignment);
    Assert((Updater->StagingOffset + BufferSize) <= Updater->StagingSize);
    u8* Result = Updater->StagingPtr + Updater->StagingOffset;

    vk_buffer_transfer* Transfer = Updater->BufferTransferArray + Updater->NumBufferTransfers++;
    Transfer->Buffer = Buffer;
    Transfer->Size = BufferSize;
    Transfer->StagingOffset = Updater->StagingOffset;
    Transfer->DstAccessMask = DstAccessMask;
    
    Updater->StagingOffset += BufferSize;
    return Result;
}

inline void VkPushBufferToGpu(vk_transfer_updater* Updater, VkBuffer Buffer, u32 BufferSize, mm Alignment,
                              VkAccessFlags DstAccessMask, void* BufferData)
{
    u8* StagingPtr = VkPushBufferToGpu(Updater, Buffer, BufferSize, Alignment, DstAccessMask);
    Copy(BufferData, StagingPtr, BufferSize);
}

inline void VkPushImageToGpu(vk_transfer_updater* Updater, VkImage Image, file_texture* Texture)
{
    Assert(Updater->StagingOffset + Texture->Size <= Updater->StagingSize);
    PlatformApi.ReadAssetFile(Texture->FileId, Texture->PixelOffset, Texture->Size,
                              Updater->StagingPtr + Updater->StagingOffset);

    vk_image_transfer* Transfer = Updater->ImageTransferArray + Updater->NumImageTransfers++;
    Transfer->StagingOffset = Updater->StagingOffset;
    Transfer->Image = Image;
    Transfer->Width = Texture->Width;
    Transfer->Height = Texture->Height;
    Transfer->Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    Transfer->AspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    
    Updater->StagingOffset += Texture->Size;
}

inline u8* VkPushImageToGpu(vk_transfer_updater* Updater, VkImage Image, u32 Width, u32 Height,
                            u32 ImageSize, VkImageLayout Layout, VkImageAspectFlagBits AspectMask)
{
    Assert(Updater->StagingOffset + ImageSize <= Updater->StagingSize);
    u8* Result = Updater->StagingPtr + Updater->StagingOffset;

    vk_image_transfer* Transfer = Updater->ImageTransferArray + Updater->NumImageTransfers++;
    Transfer->StagingOffset = Updater->StagingOffset;
    Transfer->Image = Image;
    Transfer->Width = Width;
    Transfer->Height = Height;
    Transfer->Layout = Layout;
    Transfer->AspectMask = AspectMask;
    
    Updater->StagingOffset += ImageSize;
    return Result;
}

inline void VkFlushTransferUpdater(VkDevice Device, VkCommandBuffer CmdBuffer,
                                   vk_transfer_updater* Updater)
{
    VkMappedMemoryRange FlushRange = {};
    FlushRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    FlushRange.memory = Updater->StagingMem;
    FlushRange.offset = 0;
    FlushRange.size = VkAlignAddress(Updater->StagingOffset, Updater->FlushAlignment);
    vkFlushMappedMemoryRanges(Device, 1, &FlushRange);
    
    // NOTE: Transfer all buffers
    {
        {
            temp_mem TempMem = BeginTempMem(&Updater->Arena);
            for (u32 BufferId = 0; BufferId < Updater->NumBufferTransfers; ++BufferId)
            {
                vk_buffer_transfer BufferTransfer = Updater->BufferTransferArray[BufferId];
        
                VkBufferCopy BufferCopy = {};
                BufferCopy.srcOffset = BufferTransfer.StagingOffset;
                BufferCopy.dstOffset = 0;
                BufferCopy.size = BufferTransfer.Size;    

                vkCmdCopyBuffer(CmdBuffer, Updater->StagingBuffer, BufferTransfer.Buffer, 1, &BufferCopy);
            }

            EndTempMem(TempMem);
        }
        
        {
            temp_mem TempMem = BeginTempMem(&Updater->Arena);
            VkBufferMemoryBarrier* FinalLayoutBarriers = PushArray(&Updater->Arena, VkBufferMemoryBarrier, Updater->NumBufferTransfers);
            for (u32 BufferId = 0; BufferId < Updater->NumBufferTransfers; ++BufferId)
            {
                vk_buffer_transfer BufferTransfer = Updater->BufferTransferArray[BufferId];
                
                FinalLayoutBarriers[BufferId] = {};
                FinalLayoutBarriers[BufferId].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                FinalLayoutBarriers[BufferId].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                FinalLayoutBarriers[BufferId].dstAccessMask = BufferTransfer.DstAccessMask;
                FinalLayoutBarriers[BufferId].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                FinalLayoutBarriers[BufferId].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                FinalLayoutBarriers[BufferId].buffer = BufferTransfer.Buffer;
                FinalLayoutBarriers[BufferId].offset = 0;
                FinalLayoutBarriers[BufferId].size = BufferTransfer.Size;
            }

            vkCmdPipelineBarrier(CmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                 0, 0, 0, Updater->NumBufferTransfers, FinalLayoutBarriers, 0, 0);
            EndTempMem(TempMem);
        }

        Updater->NumBufferTransfers = 0;
    }
    
    // NOTE: Transfer all images
    {
        {
            temp_mem TempMem = BeginTempMem(&Updater->Arena);
            VkImageMemoryBarrier* TransferDstBarriers = PushArray(&Updater->Arena, VkImageMemoryBarrier, Updater->NumImageTransfers);
            for (u32 ImageId = 0; ImageId < Updater->NumImageTransfers; ++ImageId)
            {
                vk_image_transfer ImageTransfer = Updater->ImageTransferArray[ImageId];

                TransferDstBarriers[ImageId] = {};
                TransferDstBarriers[ImageId].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                TransferDstBarriers[ImageId].srcAccessMask = 0;
                TransferDstBarriers[ImageId].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                TransferDstBarriers[ImageId].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                TransferDstBarriers[ImageId].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                TransferDstBarriers[ImageId].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                TransferDstBarriers[ImageId].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                TransferDstBarriers[ImageId].image = ImageTransfer.Image;
                TransferDstBarriers[ImageId].subresourceRange.aspectMask = ImageTransfer.AspectMask;
                TransferDstBarriers[ImageId].subresourceRange.baseMipLevel = 0;
                TransferDstBarriers[ImageId].subresourceRange.levelCount = 1;
                TransferDstBarriers[ImageId].subresourceRange.baseArrayLayer = 0;
                TransferDstBarriers[ImageId].subresourceRange.layerCount = 1;
            }

            vkCmdPipelineBarrier(CmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0, 0, 0, 0, 0, Updater->NumImageTransfers, TransferDstBarriers);
            EndTempMem(TempMem);
        }

        {
            for (u32 ImageId = 0; ImageId < Updater->NumImageTransfers; ++ImageId)
            {
                vk_image_transfer ImageTransfer = Updater->ImageTransferArray[ImageId];

                VkBufferImageCopy ImageCopy = {};
                ImageCopy.bufferOffset = ImageTransfer.StagingOffset;
                ImageCopy.bufferRowLength = 0;
                ImageCopy.bufferImageHeight = 0;
                ImageCopy.imageSubresource.aspectMask = ImageTransfer.AspectMask;
                ImageCopy.imageSubresource.mipLevel = 0;
                ImageCopy.imageSubresource.baseArrayLayer = 0;
                ImageCopy.imageSubresource.layerCount = 1;
                ImageCopy.imageOffset.x = 0;
                ImageCopy.imageOffset.y = 0;
                ImageCopy.imageOffset.z = 0;
                ImageCopy.imageExtent.width = ImageTransfer.Width;
                ImageCopy.imageExtent.height = ImageTransfer.Height;
                ImageCopy.imageExtent.depth = 1;

                vkCmdCopyBufferToImage(CmdBuffer, Updater->StagingBuffer, ImageTransfer.Image,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &ImageCopy);
            }
        }

        {
            temp_mem TempMem = BeginTempMem(&Updater->Arena);
            VkImageMemoryBarrier* FinalLayoutBarriers = PushArray(&Updater->Arena, VkImageMemoryBarrier, Updater->NumImageTransfers);
            for (u32 ImageId = 0; ImageId < Updater->NumImageTransfers; ++ImageId)
            {
                vk_image_transfer ImageTransfer = Updater->ImageTransferArray[ImageId];

                FinalLayoutBarriers[ImageId] = {};
                FinalLayoutBarriers[ImageId].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                FinalLayoutBarriers[ImageId].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                FinalLayoutBarriers[ImageId].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                FinalLayoutBarriers[ImageId].newLayout = ImageTransfer.Layout;
                FinalLayoutBarriers[ImageId].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                FinalLayoutBarriers[ImageId].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                FinalLayoutBarriers[ImageId].image = ImageTransfer.Image;
                FinalLayoutBarriers[ImageId].subresourceRange.aspectMask = ImageTransfer.AspectMask;
                FinalLayoutBarriers[ImageId].subresourceRange.baseMipLevel = 0;
                FinalLayoutBarriers[ImageId].subresourceRange.levelCount = 1;
                FinalLayoutBarriers[ImageId].subresourceRange.baseArrayLayer = 0;
                FinalLayoutBarriers[ImageId].subresourceRange.layerCount = 1;
            }

            vkCmdPipelineBarrier(CmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                 0, 0, 0, 0, 0, Updater->NumImageTransfers, FinalLayoutBarriers);
            EndTempMem(TempMem);
        }

        Updater->NumImageTransfers = 0;
    }
    
#if 0
    // NOTE: Update all buffers
    {
        temp_mem TempMem = BeginTempMem(&Updater->Arena);
        for (u32 BufferId = 0; BufferId < Updater->NumBufferUpdates; ++BufferId)
        {
            vk_buffer_update BufferUpdates = Updater->BufferUpdateArray[BufferId];
        
            VkBufferCopy BufferCopy = {};
            BufferCopy.srcOffset = BufferUpdates.StagingOffset;
            BufferCopy.dstOffset = BufferUpdates.DstOffset;
            BufferCopy.size = BufferUpdates.Size;    

            vkCmdCopyBuffer(CmdBuffer, Updater->StagingBuffer, BufferUpdates.Buffer, 1, &BufferCopy);
        }

        Updater->NumBufferUpdates = 0;
        EndTempMem(TempMem);
    }
#endif
    
    Updater->StagingOffset = 0;
}
