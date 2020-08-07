
//
// NOTE: API Helpers
//

internal void VkCheckResult(VkResult Result)
{
    if (Result != VK_SUCCESS)
    {
        InvalidCodePath;
    }
}

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

inline m4 VkOrthoProjM4(f32 Left, f32 Right, f32 Top, f32 Bottom, f32 Near, f32 Far)
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

inline m4 VkPerspProjM4(f32 AspectRatio, f32 Fov, f32 Near, f32 Far)
{
    m4 Result = {};
    Result.v[0].x = 1.0f / (AspectRatio*Tan(Fov*0.5f));
    Result.v[1].y = -1.0f / (Tan(Fov*0.5f));
    Result.v[2].z = (-Far) / (Near - Far);
    Result.v[2].w = 1.0f;
    Result.v[3].z = (Near*Far) / (Near - Far);
    
    return Result;
}

inline aabb2i VkClipConversion(u32 RenderHeight, aabb2i ClipBounds)
{
    aabb2i Result = ClipBounds;
    Result.Min.y = RenderHeight - ClipBounds.Max.y;
    Result.Max.y = RenderHeight - ClipBounds.Min.y;

    return Result;
}

//
// NOTE: Memory Arena
//

inline vk_gpu_linear_arena VkGpuLinearArenaCreate(VkDeviceMemory Memory, u64 Size)
{
    vk_gpu_linear_arena Result = {};
    Result.Size = Size;
    Result.Memory = Memory;

    return Result;
}

inline vk_gpu_ptr VkPushSize(vk_gpu_linear_arena* Arena, u64 Size, u64 Alignment)
{
    // TODO: Can we assume that our memory is aligned to our resources max requirement?
    u64 AlignedAddress = AlignAddress(Arena->Used, Alignment);
    Assert(AlignedAddress + Size <= Arena->Size);
    
    vk_gpu_ptr Result = {};
    Result.Memory = &Arena->Memory;
    Result.Offset = AlignedAddress;

    Arena->Used = AlignedAddress + Size;

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

//
// NOTE: Command Helpers
//

inline vk_commands VkCommandsCreate(VkDevice Device, VkCommandPool Pool)
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

    return Result;
}

inline void VkCommandsBegin(VkDevice Device, vk_commands Commands)
{
    VkCheckResult(vkWaitForFences(Device, 1, &Commands.Fence, VK_TRUE, 0xFFFFFFFF));
    VkCheckResult(vkResetFences(Device, 1, &Commands.Fence));
    
    VkCommandBufferBeginInfo BeginInfo = {};
    BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    BeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VkCheckResult(vkBeginCommandBuffer(Commands.Buffer, &BeginInfo));
}

inline void VkCommandsSubmit(VkQueue Queue, vk_commands Commands)
{
    VkCheckResult(vkEndCommandBuffer(Commands.Buffer));

    VkSubmitInfo SubmitInfo = {};
    SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfo.commandBufferCount = 1;
    SubmitInfo.pCommandBuffers = &Commands.Buffer;
    VkCheckResult(vkQueueSubmit(Queue, 1, &SubmitInfo, Commands.Fence));
}

//
// NOTE: Buffer Helpers
//

// TODO: We don't use the gpu ptr anywhere as far as I can tell, remove?
inline void VkBufferCreate(VkDevice Device, vk_gpu_linear_arena* Arena, VkBufferUsageFlags Usage,
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

inline VkBuffer VkBufferCreate(VkDevice Device, vk_gpu_linear_arena* Arena, VkBufferUsageFlags Usage,
                               u64 BufferSize)
{
    VkBuffer Result = {};
    vk_gpu_ptr Ptr = {};
    VkBufferCreate(Device, Arena, Usage, BufferSize, &Result, &Ptr);
    return Result;
}

inline VkBufferView VkBufferViewCreate(VkDevice Device, VkBuffer Buffer, VkFormat Format)
{
    VkBufferView Result = {};

    VkBufferViewCreateInfo CreateInfo = {};
    CreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
    CreateInfo.buffer = Buffer;
    CreateInfo.format = Format;
    CreateInfo.offset = 0;
    CreateInfo.range = VK_WHOLE_SIZE;
    VkCheckResult(vkCreateBufferView(Device, &CreateInfo, 0, &Result));

    return Result;
}

//
// NOTE: Image Helpers
//

inline void VkImage2dCreate(VkDevice Device, vk_gpu_linear_arena* Arena, u32 Width, u32 Height,
                            VkFormat Format, VkImageUsageFlags Usage, VkImageAspectFlags AspectMask, VkImage* OutImage,
                            VkImageView* OutImageView)
{
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
    VkCheckResult(vkCreateImage(Device, &ImageCreateInfo, 0, OutImage));

    VkMemoryRequirements ImageMemRequirements;
    vkGetImageMemoryRequirements(Device, *OutImage, &ImageMemRequirements);
    vk_gpu_ptr MemPtr = VkPushSize(Arena, ImageMemRequirements.size, ImageMemRequirements.alignment);
    VkCheckResult(vkBindImageMemory(Device, *OutImage, *MemPtr.Memory, MemPtr.Offset));

    // NOTE: Create image view
    VkImageViewCreateInfo ImgViewCreateInfo = {};
    ImgViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ImgViewCreateInfo.image = *OutImage;
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
    VkCheckResult(vkCreateImageView(Device, &ImgViewCreateInfo, 0, OutImageView));
}

//
// NOTE: FrameBuffer Helpers
//

inline VkFramebuffer VkFboCreate(VkDevice Device, VkRenderPass Rp, VkImageView* Views, u32 NumViews,
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
    VkCheckResult(vkCreateFramebuffer(Device, &FrameBufferCreateInfo, 0, &Result));

    return Result;
}

inline void VkFboReCreate(VkDevice Device, VkRenderPass Rp, VkImageView* Views, u32 NumViews,
                          VkFramebuffer* Fbo, u32 Width, u32 Height)
{
    if (*Fbo != VK_NULL_HANDLE)
    {
        vkDestroyFramebuffer(Device, *Fbo, 0);
    }

    *Fbo = VkFboCreate(Device, Rp, Views, NumViews, Width, Height);
}

//
// NOTE: Descriptor Set Helpers
//

inline VkDescriptorSet VkDescriptorSetAllocate(VkDevice Device, VkDescriptorPool Pool, VkDescriptorSetLayout Layout)
{
    VkDescriptorSet Result = {};
    
    VkDescriptorSetAllocateInfo DSAllocateInfo = {};
    DSAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    DSAllocateInfo.descriptorPool = Pool;
    DSAllocateInfo.descriptorSetCount = 1;
    DSAllocateInfo.pSetLayouts = &Layout;
    VkCheckResult(vkAllocateDescriptorSets(Device, &DSAllocateInfo, &Result));

    return Result;
}

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

inline void VkDescriptorLayoutEnd(VkDevice Device, vk_descriptor_layout_builder* Builder)
{
    VkDescriptorSetLayoutCreateInfo DSLayoutCreateInfo = {};
    DSLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    DSLayoutCreateInfo.bindingCount = Builder->CurrNumBindings;
    DSLayoutCreateInfo.pBindings = Builder->Bindings;
    VkCheckResult(vkCreateDescriptorSetLayout(Device, &DSLayoutCreateInfo, 0, Builder->Layout));
}

//
// NOTE: Descriptor Manager
//

inline vk_descriptor_manager VkDescriptorManagerCreate(linear_arena* Arena, u32 MaxNumWrites)
{
    vk_descriptor_manager Result = {};

    u32 ArenaSize = (sizeof(VkWriteDescriptorSet)*MaxNumWrites +
                     Max((u32)sizeof(VkDescriptorImageInfo), (u32)sizeof(VkDescriptorBufferInfo))*MaxNumWrites);
    Result.Arena = LinearSubArena(Arena, ArenaSize);
    Result.MaxNumWrites = MaxNumWrites;
    Result.WriteArray = PushArray(&Result.Arena, VkWriteDescriptorSet, MaxNumWrites);

    return Result;
}

inline void VkDescriptorBufferWrite(vk_descriptor_manager* Updater, VkDescriptorSet Set, u32 Binding,
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

inline void VkDescriptorImageWrite(vk_descriptor_manager* Updater, VkDescriptorSet Set, u32 Binding,
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

inline void VkDescriptorManagerFlush(VkDevice Device, vk_descriptor_manager* Updater)
{
    vkUpdateDescriptorSets(Device, Updater->NumWrites, Updater->WriteArray, 0, 0);

    Updater->NumWrites = 0;
    Updater->Arena.Used = sizeof(VkWriteDescriptorSet)*Updater->MaxNumWrites;
}

//
// NOTE: Render Pass Helpers
//

// TODO: Add support for MSAA
inline vk_render_pass_builder VkRenderPassBuilderBegin(linear_arena* Arena)
{
    vk_render_pass_builder Result = {};
    Result.Arena = Arena;
    Result.TempMem = BeginTempMem(Arena);

    // IMPORTANT: These arrays should be larger if these sizes aren't enough
    Result.MaxNumAttachments = 10;
    Result.Attachments = PushArray(Arena, VkAttachmentDescription, Result.MaxNumAttachments);

    Result.MaxNumColorAttachmentRefs = 100;
    Result.ColorAttachmentRefs = PushArray(Arena, VkAttachmentReference, Result.MaxNumColorAttachmentRefs);

    Result.MaxNumDepthAttachmentRefs = 10;
    Result.DepthAttachmentRefs = PushArray(Arena, VkAttachmentReference, Result.MaxNumDepthAttachmentRefs);

    Result.MaxNumSubPasses = 10;
    Result.SubPasses = PushArray(Arena, VkSubpassDescription, Result.MaxNumSubPasses);
    
    return Result;
}

inline u32 VkRenderPassAttachmentAdd(vk_render_pass_builder* Builder, VkFormat Format, VkAttachmentLoadOp LoadOp,
                                     VkAttachmentStoreOp StoreOp, VkImageLayout InitialLayout, VkImageLayout FinalLayout)
{
    Assert(Builder->NumAttachments < Builder->MaxNumAttachments);

    u32 Id = Builder->NumAttachments++;
    VkAttachmentDescription* Color = Builder->Attachments + Id;
    Color->flags = 0;
    Color->format = Format;
    Color->samples = VK_SAMPLE_COUNT_1_BIT;
    Color->loadOp = LoadOp;
    Color->storeOp = StoreOp;
    Color->stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    Color->stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    Color->initialLayout = InitialLayout;
    Color->finalLayout = FinalLayout;

    return Id;
}

inline void VkRenderPassSubPassBegin(vk_render_pass_builder* Builder, VkPipelineBindPoint BindPoint)
{
    Assert(Builder->NumSubPasses < Builder->MaxNumSubPasses);

    VkSubpassDescription* SubPass = Builder->SubPasses + Builder->NumSubPasses;
    *SubPass = {};
    SubPass->pipelineBindPoint = BindPoint;
    SubPass->pColorAttachments = Builder->ColorAttachmentRefs + Builder->NumColorAttachmentRefs;
}

inline void VkRenderPassColorRefAdd(vk_render_pass_builder* Builder, u32 AttachmentId, VkImageLayout Layout)
{
    Assert(Builder->NumColorAttachmentRefs < Builder->MaxNumColorAttachmentRefs);
    Assert(AttachmentId < Builder->NumAttachments);

    VkAttachmentReference* Reference = Builder->ColorAttachmentRefs + Builder->NumColorAttachmentRefs++;
    *Reference = {};
    Reference->attachment = AttachmentId;
    Reference->layout = Layout;
    
    VkSubpassDescription* SubPass = Builder->SubPasses + Builder->NumSubPasses;
    SubPass->colorAttachmentCount += 1;
}

inline void VkRenderPassDepthRefAdd(vk_render_pass_builder* Builder, u32 AttachmentId, VkImageLayout Layout)
{
    Assert(Builder->NumDepthAttachmentRefs < Builder->MaxNumDepthAttachmentRefs);
    Assert(AttachmentId < Builder->NumAttachments);

    VkSubpassDescription* SubPass = Builder->SubPasses + Builder->NumSubPasses;
    SubPass->pDepthStencilAttachment = Builder->DepthAttachmentRefs + Builder->NumDepthAttachmentRefs;

    VkAttachmentReference* Reference = Builder->DepthAttachmentRefs + Builder->NumDepthAttachmentRefs++;
    *Reference = {};
    Reference->attachment = AttachmentId;
    Reference->layout = Layout;
}

inline void VkRenderPassSubPassEnd(vk_render_pass_builder* Builder)
{
    Builder->NumSubPasses++;
}

inline VkRenderPass VkRenderPassBuilderEnd(vk_render_pass_builder* Builder, VkDevice Device)
{
    VkRenderPass Result = {};
    
    VkRenderPassCreateInfo RenderPassCreateInfo = {};
    RenderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    RenderPassCreateInfo.attachmentCount = Builder->NumAttachments;
    RenderPassCreateInfo.pAttachments = Builder->Attachments;
    RenderPassCreateInfo.subpassCount = Builder->NumSubPasses;
    RenderPassCreateInfo.pSubpasses = Builder->SubPasses;
    VkCheckResult(vkCreateRenderPass(Device, &RenderPassCreateInfo, 0, &Result));

    EndTempMem(Builder->TempMem);
    
    return Result;
}

//
// NOTE: Barrier Manager
//

inline barrier_mask BarrierMask(VkAccessFlagBits AccessMask, VkPipelineStageFlags StageMask)
{
    barrier_mask Result = {};
    Result.AccessMask = AccessMask;
    Result.StageMask = StageMask;

    return Result;
}

inline vk_barrier_manager VkBarrierManagerCreate(linear_arena* Arena, u32 MaxNumBarriers)
{
    vk_barrier_manager Result = {};

    Result.MaxNumImageBarriers = MaxNumBarriers;
    Result.ImageBarrierArray = PushArray(Arena, VkImageMemoryBarrier, MaxNumBarriers);

    Result.MaxNumBufferBarriers = MaxNumBarriers;
    Result.BufferBarrierArray = PushArray(Arena, VkBufferMemoryBarrier, MaxNumBarriers);

    return Result;
}

inline void VkBarrierBufferAdd(vk_barrier_manager* Batcher, VkAccessFlagBits InputAccessMask, VkPipelineStageFlags InputStageMask,
                               VkAccessFlagBits OutputAccessMask, VkPipelineStageFlags OutputStageMask, VkBuffer Buffer)
{
    Assert(Batcher->NumBufferBarriers < Batcher->MaxNumBufferBarriers);
    VkBufferMemoryBarrier* Barrier = Batcher->BufferBarrierArray + Batcher->NumBufferBarriers++;
    
    Barrier->sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    Barrier->srcAccessMask = InputAccessMask;
    Barrier->dstAccessMask = OutputAccessMask;
    Barrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    Barrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    Barrier->buffer = Buffer;
    Barrier->offset = 0;
    Barrier->size = VK_WHOLE_SIZE;

    Batcher->SrcStageFlags |= InputStageMask;
    Batcher->DstStageFlags |= OutputStageMask;
}

inline void VkBarrierBufferAdd(vk_barrier_manager* Batcher, barrier_mask InputMask, barrier_mask OutputMask, VkBuffer Buffer)
{
    VkBarrierBufferAdd(Batcher, InputMask.AccessMask, InputMask.StageMask, OutputMask.AccessMask, OutputMask.StageMask, Buffer);
}

inline void VkBarrierImageAdd(vk_barrier_manager* Batcher, VkAccessFlagBits InputAccessMask, VkPipelineStageFlags InputStageMask,
                              VkImageLayout InputLayout, VkAccessFlagBits OutputAccessMask, VkPipelineStageFlags OutputStageMask,
                              VkImageLayout OutputLayout, VkImageAspectFlags AspectFlags, VkImage Image)
{
    Assert(Batcher->NumImageBarriers < Batcher->MaxNumImageBarriers);
    VkImageMemoryBarrier* Barrier = Batcher->ImageBarrierArray + Batcher->NumImageBarriers++;

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

    Batcher->SrcStageFlags |= InputStageMask;
    Batcher->DstStageFlags |= OutputStageMask;
}

inline void VkBarrierImageAdd(vk_barrier_manager* Batcher, barrier_mask InputMask, VkImageLayout InputLayout, barrier_mask OutputMask,
                              VkImageLayout OutputLayout, VkImageAspectFlags AspectFlags,
                              VkImage Image)
{
    VkBarrierImageAdd(Batcher, InputMask.AccessMask, InputMask.StageMask, InputLayout, OutputMask.AccessMask, OutputMask.StageMask,
                      OutputLayout, AspectFlags, Image);
}

inline void VkBarrierManagerFlush(vk_barrier_manager* Batcher, VkCommandBuffer CmdBuffer)
{
    vkCmdPipelineBarrier(CmdBuffer, Batcher->SrcStageFlags, Batcher->DstStageFlags, VK_DEPENDENCY_BY_REGION_BIT, 0, 0,
                         Batcher->NumBufferBarriers, Batcher->BufferBarrierArray, Batcher->NumImageBarriers, Batcher->ImageBarrierArray);

    Batcher->NumBufferBarriers = 0;
    Batcher->NumImageBarriers = 0;
    Batcher->SrcStageFlags = 0;
    Batcher->DstStageFlags = 0;
}

//
// NOTE: Pipeline Manager
//

inline VkPipelineShaderStageCreateInfo VkPipelineShaderStage(VkShaderStageFlagBits Stage, VkShaderModule Module, char* MainName)
{
    VkPipelineShaderStageCreateInfo Result = {};
    Result.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    Result.stage = Stage;
    Result.module = Module;
    Result.pName = MainName;
    
    return Result;
}

inline VkShaderModule VkPipelineAddShaderRef(VkDevice Device, HANDLE File, linear_arena* TempArena, char* FileName, char* MainName,
                                             vk_shader_ref* Ref)
{
    temp_mem TempMem = BeginTempMem(TempArena);
    
    // NOTE: Get File code
    LARGE_INTEGER CodeSize = {};
    if (!GetFileSizeEx(File, &CodeSize))
    {
        DWORD Error = GetLastError();
        InvalidCodePath;
    }
    Assert(CodeSize.HighPart == 0);

    u32* Code = (u32*)PushSize(TempArena, CodeSize.LowPart);
    if (!ReadFile(File, Code, CodeSize.LowPart, 0, 0))
    {
        DWORD Error = GetLastError();
        InvalidCodePath;
    }

    // NOTE: Populate Ref
    Ref->FileName = FileName;
    Ref->MainName = MainName;
    if (!GetFileTime(File, 0, 0, &Ref->ModifiedTime))
    {
        DWORD Error = GetLastError();
        InvalidCodePath;
    }
    
    CloseHandle(File);
    
    VkShaderModuleCreateInfo ShaderModuleCreateInfo = {};
    ShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ShaderModuleCreateInfo.codeSize = CodeSize.LowPart;
    ShaderModuleCreateInfo.pCode = Code;

    VkShaderModule Result;
    VkCheckResult(vkCreateShaderModule(Device, &ShaderModuleCreateInfo, 0, &Result));
    EndTempMem(TempMem);
    
    return Result;
}

inline VkShaderModule VkPipelineAddShaderRef(VkDevice Device, linear_arena* TempArena, char* FileName, char* MainName, vk_shader_ref* Ref)
{
    HANDLE File = CreateFileA(FileName, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (File == INVALID_HANDLE_VALUE)
    {
        DWORD Error = GetLastError();
        InvalidCodePath;
    }

    VkShaderModule Result = VkPipelineAddShaderRef(Device, File, TempArena, FileName, MainName, Ref);
    return Result;
}

inline vk_pipeline_manager VkPipelineManagerCreate(linear_arena* Arena)
{
    vk_pipeline_manager Result = {};
    Result.MaxNumPipelines = 100; // TODO: This is hardcoded for now
    Result.PipelineArray = PushArray(Arena, vk_pipeline_entry, Result.MaxNumPipelines);

    return Result;
}

inline vk_pipeline* VkPipelineCsCreate(VkDevice Device, vk_pipeline_manager* Manager, linear_arena* TempArena, char* FileName,
                                       char* MainName, VkDescriptorSetLayout* Layouts, u32 NumLayouts)
{
    Assert(Manager->NumPipelines < Manager->MaxNumPipelines);
    vk_pipeline_entry* Entry = Manager->PipelineArray + Manager->NumPipelines++;
    *Entry = {};
    Entry->Type = VkPipelineEntry_Compute;

    // NOTE: Setup pipeline create infos and create pipeline
    {
        vk_pipeline_compute_entry* ComputeEntry = &Entry->ComputeEntry;
        VkShaderModule CsShader = VkPipelineAddShaderRef(Device, TempArena, FileName, MainName, Entry->ShaderRefs + Entry->NumShaders++);
        VkPipelineShaderStageCreateInfo ShaderStageCreateInfo = VkPipelineShaderStage(VK_SHADER_STAGE_COMPUTE_BIT, CsShader, MainName);

        VkPipelineLayoutCreateInfo LayoutCreateInfo = {};
        LayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        LayoutCreateInfo.setLayoutCount = NumLayouts;
        LayoutCreateInfo.pSetLayouts = Layouts;
        VkCheckResult(vkCreatePipelineLayout(Device, &LayoutCreateInfo, 0, &Entry->Pipeline.Layout));

        ComputeEntry->PipelineCreateInfo = {};
        ComputeEntry->PipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        ComputeEntry->PipelineCreateInfo.stage = ShaderStageCreateInfo;
        ComputeEntry->PipelineCreateInfo.layout = Entry->Pipeline.Layout;
        ComputeEntry->PipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
        ComputeEntry->PipelineCreateInfo.basePipelineIndex = -1;
        VkCheckResult(vkCreateComputePipelines(Device, VK_NULL_HANDLE, 1, &ComputeEntry->PipelineCreateInfo, 0, &Entry->Pipeline.Handle));

        vkDestroyShaderModule(Device, CsShader, 0);
    }
    
    return &Entry->Pipeline;
}

inline vk_pipeline* VkPipelineVsPsCreate(VkDevice Device, vk_pipeline_manager* Manager, linear_arena* Arena, char* VsFileName,
                                         char* VsMainName, char* PsFileName, char* PsMainName,
                                         VkPipelineLayoutCreateInfo* LayoutCreateInfo, VkGraphicsPipelineCreateInfo* PipelineCreateInfo)
{
    Assert(Manager->NumPipelines < Manager->MaxNumPipelines);
    vk_pipeline_entry* Entry = Manager->PipelineArray + Manager->NumPipelines++;
    *Entry = {};
    Entry->Type = VkPipelineEntry_Graphics;

    // NOTE: Setup pipeline create infos and create pipeline
    {
        vk_pipeline_graphics_entry* GraphicsEntry = &Entry->GraphicsEntry;

        // NOTE: Copy all pipeline create infos
        {
            GraphicsEntry->VertexInputState = *PipelineCreateInfo->pVertexInputState;
            {
                if (GraphicsEntry->VertexInputState.vertexBindingDescriptionCount > 0)
                {
                    GraphicsEntry->VertBindings = PushArray(Arena, VkVertexInputBindingDescription, GraphicsEntry->VertexInputState.vertexBindingDescriptionCount);
                    Copy(GraphicsEntry->VertexInputState.pVertexBindingDescriptions, GraphicsEntry->VertBindings, sizeof(VkVertexInputBindingDescription)*GraphicsEntry->VertexInputState.vertexBindingDescriptionCount);
                    GraphicsEntry->VertexInputState.pVertexBindingDescriptions = GraphicsEntry->VertBindings;
                }

                if (GraphicsEntry->VertexInputState.vertexAttributeDescriptionCount > 0)
                {
                    GraphicsEntry->VertAttributes = PushArray(Arena, VkVertexInputAttributeDescription, GraphicsEntry->VertexInputState.vertexAttributeDescriptionCount);
                    Copy(GraphicsEntry->VertexInputState.pVertexAttributeDescriptions, GraphicsEntry->VertAttributes, sizeof(VkVertexInputAttributeDescription)*GraphicsEntry->VertexInputState.vertexAttributeDescriptionCount);
                    GraphicsEntry->VertexInputState.pVertexAttributeDescriptions = GraphicsEntry->VertAttributes;
                }
            }
            
            GraphicsEntry->InputAssemblyState = *PipelineCreateInfo->pInputAssemblyState;

            GraphicsEntry->ViewportState = *PipelineCreateInfo->pViewportState;
            {
                if (GraphicsEntry->ViewportState.pViewports)
                {
                    GraphicsEntry->ViewPorts = PushArray(Arena, VkViewport, GraphicsEntry->ViewportState.viewportCount);
                    Copy(GraphicsEntry->ViewportState.pViewports, GraphicsEntry->ViewPorts, sizeof(VkViewport)*GraphicsEntry->ViewportState.viewportCount);
                    GraphicsEntry->ViewportState.pViewports = GraphicsEntry->ViewPorts;
                }

                if (GraphicsEntry->ViewportState.pScissors)
                {
                    GraphicsEntry->Scissors = PushArray(Arena, VkRect2D, GraphicsEntry->ViewportState.scissorCount);
                    Copy(GraphicsEntry->ViewportState.pScissors, GraphicsEntry->Scissors, sizeof(VkRect2D)*GraphicsEntry->ViewportState.scissorCount);
                    GraphicsEntry->ViewportState.pScissors = GraphicsEntry->Scissors;
                }
            }
            
            GraphicsEntry->RasterizationState = *PipelineCreateInfo->pRasterizationState;
            GraphicsEntry->MultisampleState = *PipelineCreateInfo->pMultisampleState;
            
            GraphicsEntry->ColorBlendState = *PipelineCreateInfo->pColorBlendState;
            if (GraphicsEntry->ColorBlendState.attachmentCount > 0)
            {
                GraphicsEntry->Attachments = PushArray(Arena, VkPipelineColorBlendAttachmentState, GraphicsEntry->ColorBlendState.attachmentCount);
                Copy(GraphicsEntry->ColorBlendState.pAttachments, GraphicsEntry->Attachments, sizeof(VkPipelineColorBlendAttachmentState)*GraphicsEntry->ColorBlendState.attachmentCount);
                GraphicsEntry->ColorBlendState.pAttachments = GraphicsEntry->Attachments;
            }
            
            GraphicsEntry->DynamicStateCreateInfo = *PipelineCreateInfo->pDynamicState;
            if (GraphicsEntry->DynamicStateCreateInfo.dynamicStateCount > 0)
            {
                GraphicsEntry->DynamicStates = PushArray(Arena, VkDynamicState, GraphicsEntry->DynamicStateCreateInfo.dynamicStateCount);
                Copy(GraphicsEntry->DynamicStateCreateInfo.pDynamicStates, GraphicsEntry->DynamicStates, sizeof(VkDynamicState)*GraphicsEntry->DynamicStateCreateInfo.dynamicStateCount);
                GraphicsEntry->DynamicStateCreateInfo.pDynamicStates = GraphicsEntry->DynamicStates;
            }
            
            GraphicsEntry->PipelineCreateInfo = *PipelineCreateInfo;
            GraphicsEntry->PipelineCreateInfo.pVertexInputState = &GraphicsEntry->VertexInputState;
            GraphicsEntry->PipelineCreateInfo.pInputAssemblyState = &GraphicsEntry->InputAssemblyState;
            GraphicsEntry->PipelineCreateInfo.pViewportState = &GraphicsEntry->ViewportState;
            GraphicsEntry->PipelineCreateInfo.pRasterizationState = &GraphicsEntry->RasterizationState;
            GraphicsEntry->PipelineCreateInfo.pMultisampleState = &GraphicsEntry->MultisampleState;
            GraphicsEntry->PipelineCreateInfo.pColorBlendState = &GraphicsEntry->ColorBlendState;
            GraphicsEntry->PipelineCreateInfo.pDynamicState = &GraphicsEntry->DynamicStateCreateInfo;

            if (PipelineCreateInfo->pTessellationState)
            {
                GraphicsEntry->TessellationState = *PipelineCreateInfo->pTessellationState;
                GraphicsEntry->PipelineCreateInfo.pTessellationState = &GraphicsEntry->TessellationState;
            }
            if (PipelineCreateInfo->pDepthStencilState)
            {
                GraphicsEntry->DepthStencilState = *PipelineCreateInfo->pDepthStencilState;
                GraphicsEntry->PipelineCreateInfo.pDepthStencilState = &GraphicsEntry->DepthStencilState;
            }
        }
        
        // NOTE: Create pipeline
        VkShaderModule VsShader = VkPipelineAddShaderRef(Device, Arena, VsFileName, VsMainName, Entry->ShaderRefs + Entry->NumShaders++);
        VkShaderModule PsShader = VkPipelineAddShaderRef(Device, Arena, PsFileName, PsMainName, Entry->ShaderRefs + Entry->NumShaders++);

        // NOTE: Setup the pipeline create infos
        VkPipelineShaderStageCreateInfo ShaderStages[2] = {};
        ShaderStages[0] = VkPipelineShaderStage(VK_SHADER_STAGE_VERTEX_BIT, VsShader, VsMainName);
        ShaderStages[1] = VkPipelineShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, PsShader, PsMainName);
        
        VkCheckResult(vkCreatePipelineLayout(Device, LayoutCreateInfo, 0, &Entry->Pipeline.Layout));

        // NOTE: Patch up some values in the create info
        GraphicsEntry->PipelineCreateInfo.pStages = ShaderStages;
        GraphicsEntry->PipelineCreateInfo.stageCount = ArrayCount(ShaderStages);
        GraphicsEntry->PipelineCreateInfo.layout = Entry->Pipeline.Layout;
        VkCheckResult(vkCreateGraphicsPipelines(Device, VK_NULL_HANDLE, 1, &GraphicsEntry->PipelineCreateInfo, 0, &Entry->Pipeline.Handle));

        vkDestroyShaderModule(Device, VsShader, 0);
        vkDestroyShaderModule(Device, PsShader, 0);
    }
    
    return &Entry->Pipeline;
}

inline void VkPipelineUpdateShaders(VkDevice Device, linear_arena* TempArena, vk_pipeline_manager* Manager)
{
    for (u32 PipelineId = 0; PipelineId < Manager->NumPipelines; ++PipelineId)
    {
        vk_pipeline_entry* Entry = Manager->PipelineArray + PipelineId;

        b32 ReCreatePSO = false;

        HANDLE FileHandles[VK_MAX_NUM_HANDLES] = {};
        for (u32 ShaderId = 0; ShaderId < Entry->NumShaders; ++ShaderId)
        {
            vk_shader_ref* CurrShaderRef = Entry->ShaderRefs + ShaderId;
            
            FileHandles[ShaderId] = CreateFileA(CurrShaderRef->FileName, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
            if (FileHandles[ShaderId] != INVALID_HANDLE_VALUE)
            {
                FILETIME CurrFileTime = {};
                if (!GetFileTime(FileHandles[ShaderId], 0, 0, &CurrFileTime))
                {
                    DWORD Error = GetLastError();
                    InvalidCodePath;
                }
                ReCreatePSO = ReCreatePSO || CompareFileTime(&CurrShaderRef->ModifiedTime, &CurrFileTime) == -1;
            }
            else
            {
                // NOTE: Only allow this to happen if the file is used by a different process which is error 32
                // NOTE: https://docs.microsoft.com/en-us/windows/win32/debug/system-error-codes--0-499-
                DWORD Error = GetLastError();
                Assert(Error == 32);
            }
        }

        if (ReCreatePSO)
        {
            // NOTE: ReCreate the PSO
            switch (Entry->Type)
            {
                case VkPipelineEntry_Graphics:
                {
                    vk_pipeline_graphics_entry* GraphicsEntry = &Entry->GraphicsEntry;

                    VkShaderModule VsShader = VkPipelineAddShaderRef(Device, FileHandles[0], TempArena, Entry->ShaderRefs[0].FileName,
                                                                     Entry->ShaderRefs[0].MainName, Entry->ShaderRefs + 0);
                    VkShaderModule PsShader = VkPipelineAddShaderRef(Device, FileHandles[1], TempArena, Entry->ShaderRefs[1].FileName,
                                                                     Entry->ShaderRefs[1].MainName, Entry->ShaderRefs + 1);
    
                    VkPipelineShaderStageCreateInfo ShaderStages[2] = {};
                    ShaderStages[0] = VkPipelineShaderStage(VK_SHADER_STAGE_VERTEX_BIT, VsShader, Entry->ShaderRefs[0].MainName);
                    ShaderStages[1] = VkPipelineShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, PsShader, Entry->ShaderRefs[1].MainName);

                    VkGraphicsPipelineCreateInfo PipelineCreateInfo = GraphicsEntry->PipelineCreateInfo;
                    PipelineCreateInfo.stageCount = Entry->NumShaders;
                    PipelineCreateInfo.pStages = ShaderStages;
                    VkCheckResult(vkCreateGraphicsPipelines(Device, VK_NULL_HANDLE, 1, &PipelineCreateInfo, 0, &Entry->Pipeline.Handle));

                    vkDestroyShaderModule(Device, VsShader, 0);
                    vkDestroyShaderModule(Device, PsShader, 0);
                } break;

                case VkPipelineEntry_Compute:
                {
                    vk_pipeline_compute_entry* ComputeEntry = &Entry->ComputeEntry;
                    
                    VkShaderModule CsShader = VkPipelineAddShaderRef(Device, FileHandles[0], TempArena, Entry->ShaderRefs[0].FileName,
                                                                     Entry->ShaderRefs[0].MainName, Entry->ShaderRefs + 0);
                    VkPipelineShaderStageCreateInfo ShaderStageCreateInfo = VkPipelineShaderStage(VK_SHADER_STAGE_COMPUTE_BIT, CsShader,
                                                                                                  Entry->ShaderRefs[0].MainName);

                    VkComputePipelineCreateInfo PipelineCreateInfo = ComputeEntry->PipelineCreateInfo;
                    PipelineCreateInfo.stage = ShaderStageCreateInfo;
                    VkCheckResult(vkCreateComputePipelines(Device, VK_NULL_HANDLE, 1, &PipelineCreateInfo, 0, &Entry->Pipeline.Handle));

                    vkDestroyShaderModule(Device, CsShader, 0);
                } break;

                default:
                {
                    InvalidCodePath;
                } break;
            }
        }
        else
        {
            for (u32 ShaderId = 0; ShaderId < Entry->NumShaders; ++ShaderId)
            {
                CloseHandle(FileHandles[ShaderId]);
            }
        }
    }
}

//
// NOTE: Graphics Pipeline Builder
//

inline vk_pipeline_builder VkPipelineBuilderBegin(linear_arena* Arena)
{
    vk_pipeline_builder Result = {};
    Result.Arena = Arena;
    Result.TempMem = BeginTempMem(Arena);

    // IMPORTANT: We set some max stuff here to make other stuff simpler later
    Result.MaxNumVertexBindings = 100;
    Result.VertexBindings = PushArray(Arena, VkVertexInputBindingDescription, Result.MaxNumVertexBindings);

    Result.MaxNumVertexAttributes = 100;
    Result.VertexAttributes = PushArray(Arena, VkVertexInputAttributeDescription, Result.MaxNumVertexAttributes);

    Result.MaxNumColorAttachments = 10;
    Result.ColorAttachments = PushArray(Arena, VkPipelineColorBlendAttachmentState, Result.MaxNumColorAttachments);

    // NOTE: Set some default values
    VkPipelineInputAssemblyAdd(&Result, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);
    
    return Result;
}

inline void VkPipelineVertexShaderAdd(vk_pipeline_builder* Builder, char* FileName, char* MainName)
{
    Builder->VsFileName = FileName;
    Builder->VsMainName = MainName;
}

inline void VkPipelineFragmentShaderAdd(vk_pipeline_builder* Builder, char* FileName, char* MainName)
{
    Builder->PsFileName = FileName;
    Builder->PsMainName = MainName;
}

inline void VkPipelineVertexBindingBegin(vk_pipeline_builder* Builder)
{
    Assert(Builder->NumVertexBindings < Builder->MaxNumVertexBindings);
    
    VkVertexInputBindingDescription* VertexBinding = Builder->VertexBindings + Builder->NumVertexBindings;
    VertexBinding->binding = Builder->NumVertexBindings++;
    VertexBinding->inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    Builder->CurrVertexBindingSize = 0;
    Builder->CurrVertexLocation = 0;
}

inline void VkPipelineVertexBindingEnd(vk_pipeline_builder* Builder)
{
    u32 CurrVertexBinding = Builder->NumVertexBindings - 1;
    Builder->VertexBindings[CurrVertexBinding].stride = Builder->CurrVertexBindingSize;
}

inline void VkPipelineVertexAttributeAdd(vk_pipeline_builder* Builder, VkFormat Format, u32 VertexAttribSize)
{
    Assert(Builder->NumVertexAttributes < Builder->MaxNumVertexAttributes);
    
    u32 CurrVertexBinding = Builder->NumVertexBindings - 1;
    VkVertexInputAttributeDescription* VertexAttribute = Builder->VertexAttributes + Builder->NumVertexAttributes++;
    VertexAttribute->location = Builder->CurrVertexLocation++;
    VertexAttribute->binding = CurrVertexBinding;
    VertexAttribute->format = Format;
    VertexAttribute->offset = Builder->CurrVertexBindingSize;

    Builder->CurrVertexBindingSize += VertexAttribSize;
}

inline void VkPipelineInputAssemblyAdd(vk_pipeline_builder* Builder, VkPrimitiveTopology Topology, VkBool32 PrimRestart)
{
    Builder->InputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    Builder->InputAssembly.topology = Topology;
    Builder->InputAssembly.primitiveRestartEnable = PrimRestart;
}

inline void VkPipelineDepthStateAdd(vk_pipeline_builder* Builder, VkBool32 TestEnable, VkBool32 WriteEnable, VkCompareOp CompareOp)
{
    Builder->Flags |= VkPipelineFlag_HasDepthStencil;
    
    Builder->DepthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    Builder->DepthStencil.depthTestEnable = TestEnable;
    Builder->DepthStencil.depthWriteEnable = WriteEnable;
    Builder->DepthStencil.depthCompareOp = CompareOp;

    // NOTE: These are the defaults
    Builder->DepthStencil.depthBoundsTestEnable = VK_FALSE;
    Builder->DepthStencil.minDepthBounds = 0;
    Builder->DepthStencil.maxDepthBounds = 1;
}

inline void VkPipelineDepthBoundsAdd(vk_pipeline_builder* Builder, f32 Min, f32 Max)
{
    Assert((Builder->Flags & VkPipelineFlag_HasDepthStencil) != 0);
    
    Builder->DepthStencil.depthBoundsTestEnable = VK_TRUE;
    Builder->DepthStencil.minDepthBounds = Min;
    Builder->DepthStencil.maxDepthBounds = Max;
}

inline void VkPipelineStencilStateAdd(vk_pipeline_builder* Builder, VkStencilOpState Front, VkStencilOpState Back)
{
    Builder->Flags |= VkPipelineFlag_HasDepthStencil;

    Builder->DepthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    Builder->DepthStencil.stencilTestEnable = VK_TRUE;
    Builder->DepthStencil.front = Front;
    Builder->DepthStencil.back = Back;
}

inline void VkPipelineColorAttachmentAdd(vk_pipeline_builder* Builder, VkBool32 ColorEnable, VkBlendOp ColorBlend, VkBlendFactor SrcColor,
                                         VkBlendFactor DstColor, VkBlendOp AlphaBlend, VkBlendFactor SrcAlpha, VkBlendFactor DstAlpha,
                                         VkColorComponentFlags WriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)
{
    Assert(Builder->NumColorAttachments < Builder->MaxNumColorAttachments);

    VkPipelineColorBlendAttachmentState* ColorAttachment = Builder->ColorAttachments + Builder->NumColorAttachments++;
    ColorAttachment->blendEnable = ColorEnable;
    ColorAttachment->srcColorBlendFactor = SrcColor;
    ColorAttachment->dstColorBlendFactor = DstColor;
    ColorAttachment->colorBlendOp = ColorBlend;
    ColorAttachment->srcAlphaBlendFactor = SrcAlpha;
    ColorAttachment->dstAlphaBlendFactor = DstAlpha;
    ColorAttachment->alphaBlendOp = AlphaBlend;
    ColorAttachment->colorWriteMask = WriteMask;
}

inline vk_pipeline* VkPipelineBuilderEnd(vk_pipeline_builder* Builder, VkDevice Device, vk_pipeline_manager* Manager,
                                         VkRenderPass RenderPass, u32 SubPassId, VkDescriptorSetLayout* Layouts, u32 NumLayouts)
{
    vk_pipeline* Result = {};
    
    // NOTE: Vertex attribute info
    VkPipelineVertexInputStateCreateInfo VertexInputStateCreateInfo = {};
    VertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VertexInputStateCreateInfo.vertexBindingDescriptionCount = Builder->NumVertexBindings;
    VertexInputStateCreateInfo.pVertexBindingDescriptions = Builder->VertexBindings;
    VertexInputStateCreateInfo.vertexAttributeDescriptionCount = Builder->NumVertexAttributes;
    VertexInputStateCreateInfo.pVertexAttributeDescriptions = Builder->VertexAttributes;
    
    // NOTE: Specify view port info
    VkPipelineViewportStateCreateInfo ViewPortStateCreateInfo = {};
    ViewPortStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    ViewPortStateCreateInfo.viewportCount = 1;
    ViewPortStateCreateInfo.pViewports = 0;
    ViewPortStateCreateInfo.scissorCount = 1;
    ViewPortStateCreateInfo.pScissors = 0;

    // TODO: This should be specified but make more pipelines and see how to break it up
    // NOTE: Specify rasterization flags
    VkPipelineRasterizationStateCreateInfo RasterizationStateCreateInfo = {};
    RasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    RasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
    RasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    RasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    RasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    RasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
    RasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
    RasterizationStateCreateInfo.depthBiasClamp = 0.0f;
    RasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;
    RasterizationStateCreateInfo.lineWidth = 1.0f;

    // TODO: This should be specified but make more pipeliens and see how to break it up
    // NOTE: Set the multi sampling state
    VkPipelineMultisampleStateCreateInfo MultiSampleStateCreateInfo = {};
    MultiSampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    MultiSampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    MultiSampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
    MultiSampleStateCreateInfo.minSampleShading = 1.0f;
    MultiSampleStateCreateInfo.pSampleMask = 0;
    MultiSampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
    MultiSampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

    // TODO: This might need to be configurable
    // NOTE: Set the blending state
    VkPipelineColorBlendStateCreateInfo ColorBlendStateCreateInfo = {};
    ColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    ColorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
    ColorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
    ColorBlendStateCreateInfo.attachmentCount = Builder->NumColorAttachments;
    ColorBlendStateCreateInfo.pAttachments = Builder->ColorAttachments;
    ColorBlendStateCreateInfo.blendConstants[0] = 0.0f;
    ColorBlendStateCreateInfo.blendConstants[1] = 0.0f;
    ColorBlendStateCreateInfo.blendConstants[2] = 0.0f;
    ColorBlendStateCreateInfo.blendConstants[3] = 0.0f;

    // NOTE: Spec dynamic state
    VkDynamicState DynamicStates[2] = {};
    DynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
    DynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;

    VkPipelineDynamicStateCreateInfo DynamicStateCreateInfo = {};
    DynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    DynamicStateCreateInfo.dynamicStateCount = ArrayCount(DynamicStates);
    DynamicStateCreateInfo.pDynamicStates = DynamicStates;
                
    VkPipelineLayoutCreateInfo LayoutCreateInfo = {};
    LayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    LayoutCreateInfo.setLayoutCount = NumLayouts;
    LayoutCreateInfo.pSetLayouts = Layouts;
            
    VkGraphicsPipelineCreateInfo PipelineCreateInfo = {};
    PipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    PipelineCreateInfo.pVertexInputState = &VertexInputStateCreateInfo;
    PipelineCreateInfo.pInputAssemblyState = &Builder->InputAssembly;
    PipelineCreateInfo.pViewportState = &ViewPortStateCreateInfo;
    PipelineCreateInfo.pRasterizationState = &RasterizationStateCreateInfo;
    if (Builder->Flags & VkPipelineFlag_HasDepthStencil)
    {
        PipelineCreateInfo.pDepthStencilState = &Builder->DepthStencil;
    }
    PipelineCreateInfo.pMultisampleState = &MultiSampleStateCreateInfo;
    PipelineCreateInfo.pColorBlendState = &ColorBlendStateCreateInfo;
    PipelineCreateInfo.pDynamicState = &DynamicStateCreateInfo;
    PipelineCreateInfo.renderPass = RenderPass;
    PipelineCreateInfo.subpass = SubPassId;
    PipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    PipelineCreateInfo.basePipelineIndex = -1;

    Result = VkPipelineVsPsCreate(Device, Manager, Builder->Arena, Builder->VsFileName, Builder->VsMainName, Builder->PsFileName,
                                  Builder->PsMainName, &LayoutCreateInfo, &PipelineCreateInfo);
    
    EndTempMem(Builder->TempMem);

    return Result;
}

//
// NOTE: Transfer Manager
//

inline vk_transfer_manager VkTransferManagerCreate(VkDevice Device, u32 StagingTypeId, linear_arena* CpuArena,
                                                   vk_gpu_linear_arena* GpuArena, u64 FlushAlignment, u64 StagingSize,
                                                   u32 MaxNumBufferTransfers, u32 MaxNumImageTransfers)
{
    vk_transfer_manager Result = {};

    u32 ArenaSize = (MaxNumBufferTransfers*sizeof(vk_buffer_transfer) +
                     MaxNumImageTransfers*sizeof(vk_image_transfer));
    Result.Arena = LinearSubArena(CpuArena, ArenaSize);
    Result.FlushAlignment = FlushAlignment;

    StagingSize = AlignAddress(StagingSize, FlushAlignment);
    
    // NOTE: Staging data
    {
        VkBufferCreateInfo BufferCreateInfo = {};
        BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        BufferCreateInfo.size = StagingSize;
        BufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        BufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VkCheckResult(vkCreateBuffer(Device, &BufferCreateInfo, 0, &Result.StagingBuffer));

        VkMemoryRequirements BufferMemRequirements;
        vkGetBufferMemoryRequirements(Device, Result.StagingBuffer, &BufferMemRequirements);

        Result.StagingMem = VkMemoryAllocate(Device, StagingTypeId, BufferMemRequirements.size);
        VkCheckResult(vkBindBufferMemory(Device, Result.StagingBuffer, Result.StagingMem, 0));
        VkCheckResult(vkMapMemory(Device, Result.StagingMem, 0, BufferMemRequirements.size,
                                  0, (void**)&Result.StagingPtr));
        Result.StagingSize = BufferMemRequirements.size;
    }
    
    // NOTE: Buffer data
    Result.MaxNumBufferTransfers = MaxNumBufferTransfers;
    Result.BufferTransferArray = PushArray(CpuArena, vk_buffer_transfer, MaxNumBufferTransfers);

    // NOTE: Image data
    Result.MaxNumImageTransfers = MaxNumImageTransfers;
    Result.ImageTransferArray = PushArray(CpuArena, vk_image_transfer, MaxNumImageTransfers);

    return Result;
}

#define VkTransferPushBufferWriteStruct(Updater, Buffer, Type, Alignment, InputMask, OutputMask) \
    (Type*)VkTransferPushBufferWrite(Updater, Buffer, sizeof(Type), Alignment, InputMask, OutputMask)
#define VkTransferPushBufferWriteArray(Updater, Buffer, Type, Count, Alignment, InputMask, OutputMask) \
    (Type*)VkTransferPushBufferWrite(Updater, Buffer, sizeof(Type)*Count, Alignment, InputMask, OutputMask)
inline u8* VkTransferPushBufferWrite(vk_transfer_manager* Updater, VkBuffer Buffer, u64 BufferSize, u64 Alignment,
                                     barrier_mask InputMask, barrier_mask OutputMask)
{
    Updater->StagingOffset = AlignAddress(Updater->StagingOffset, Alignment);
    Assert((Updater->StagingOffset + BufferSize) <= Updater->StagingSize);
    u8* Result = Updater->StagingPtr + Updater->StagingOffset;
 
    Assert(Updater->NumBufferTransfers < Updater->MaxNumBufferTransfers);
    vk_buffer_transfer* Transfer = Updater->BufferTransferArray + Updater->NumBufferTransfers++;
    Transfer->Buffer = Buffer;
    Transfer->Size = BufferSize;
    Transfer->StagingOffset = Updater->StagingOffset;
    Transfer->InputMask = InputMask;
    Transfer->OutputMask = OutputMask;

    Updater->StagingOffset += BufferSize;
    return Result;
}

inline u8* VkTransferPushImageWrite(vk_transfer_manager* Updater, VkImage Image, u32 Width, u32 Height, u32 ImageSize,
                                    VkImageAspectFlagBits AspectMask, VkImageLayout InputLayout, VkImageLayout OutputLayout,
                                    barrier_mask InputMask, barrier_mask OutputMask)
{
    // TODO: Do we need alignment?
    // TODO: Handle format sizes here?
    Assert(Updater->StagingOffset + ImageSize <= Updater->StagingSize);
    u8* Result = Updater->StagingPtr + Updater->StagingOffset;

    vk_image_transfer* Transfer = Updater->ImageTransferArray + Updater->NumImageTransfers++;
    Transfer->StagingOffset = Updater->StagingOffset;
    Transfer->Image = Image;
    Transfer->Width = Width;
    Transfer->Height = Height;
    Transfer->AspectMask = AspectMask;
    Transfer->InputMask = InputMask;
    Transfer->InputLayout = InputLayout;
    Transfer->OutputMask = OutputMask;
    Transfer->OutputLayout = OutputLayout;
    
    Updater->StagingOffset += ImageSize;
    return Result;
}

inline void VkTransferManagerFlush(vk_transfer_manager* Updater, VkDevice Device, VkCommandBuffer CmdBuffer, vk_barrier_manager* Batcher)
{
    VkMappedMemoryRange FlushRange = {};
    FlushRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    FlushRange.memory = Updater->StagingMem;
    FlushRange.offset = 0;
    FlushRange.size = AlignAddress(Updater->StagingOffset, Updater->FlushAlignment);
    vkFlushMappedMemoryRanges(Device, 1, &FlushRange);

    barrier_mask IntermediateMask = BarrierMask(VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    
    // NOTE: Transfer all buffers
    if (Updater->NumBufferTransfers > 0)
    {
        for (u32 BufferId = 0; BufferId < Updater->NumBufferTransfers; ++BufferId)
        {
            vk_buffer_transfer* BufferTransfer = Updater->BufferTransferArray + BufferId;            
            VkBarrierBufferAdd(Batcher, BufferTransfer->InputMask, IntermediateMask, BufferTransfer->Buffer);
        }

        VkBarrierManagerFlush(Batcher, CmdBuffer);
        
        for (u32 BufferId = 0; BufferId < Updater->NumBufferTransfers; ++BufferId)
        {
            vk_buffer_transfer* BufferTransfer = Updater->BufferTransferArray + BufferId;
        
            VkBufferCopy BufferCopy = {};
            BufferCopy.srcOffset = BufferTransfer->StagingOffset;
            BufferCopy.dstOffset = 0;
            BufferCopy.size = BufferTransfer->Size;
            vkCmdCopyBuffer(CmdBuffer, Updater->StagingBuffer, BufferTransfer->Buffer, 1, &BufferCopy);
        }
        
        for (u32 BufferId = 0; BufferId < Updater->NumBufferTransfers; ++BufferId)
        {
            vk_buffer_transfer* BufferTransfer = Updater->BufferTransferArray + BufferId;
            VkBarrierBufferAdd(Batcher, IntermediateMask, BufferTransfer->OutputMask, BufferTransfer->Buffer);
        }

        VkBarrierManagerFlush(Batcher, CmdBuffer);
        Updater->NumBufferTransfers = 0;
    }
    
    // NOTE: Transfer all images
    if (Updater->NumImageTransfers > 0)
    {
        for (u32 ImageId = 0; ImageId < Updater->NumImageTransfers; ++ImageId)
        {
            vk_image_transfer* ImageTransfer = Updater->ImageTransferArray + ImageId;
            VkBarrierImageAdd(Batcher, ImageTransfer->InputMask, ImageTransfer->InputLayout, IntermediateMask,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, ImageTransfer->AspectMask, ImageTransfer->Image);
        }

        VkBarrierManagerFlush(Batcher, CmdBuffer);

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

        for (u32 ImageId = 0; ImageId < Updater->NumImageTransfers; ++ImageId)
        {
            vk_image_transfer* ImageTransfer = Updater->ImageTransferArray + ImageId;
            VkBarrierImageAdd(Batcher, IntermediateMask, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, ImageTransfer->OutputMask,
                              ImageTransfer->OutputLayout, ImageTransfer->AspectMask, ImageTransfer->Image);
        }

        VkBarrierManagerFlush(Batcher, CmdBuffer);
        Updater->NumImageTransfers = 0;
    }
        
    Updater->StagingOffset = 0;
}
