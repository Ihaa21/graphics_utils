
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
// NOTE: Shader Helpers
//

inline VkShaderModule VkCreateShaderModule(VkDevice Device, linear_arena* TempArena, char* FileName)
{
    temp_mem TempMem = BeginTempMem(TempArena);
    
    FILE* File = fopen(FileName, "rb");
    if (!File)
    {
        InvalidCodePath;
    }

    fseek(File, 0, SEEK_END);
    u32 CodeSize = ftell(File);
    fseek(File, 0, SEEK_SET);
    u32* Code = (u32*)PushSize(TempArena, CodeSize);
    fread(Code, CodeSize, 1, File);
    fclose(File);
    
    VkShaderModuleCreateInfo ShaderModuleCreateInfo =
        {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            0,
            0,
            CodeSize,
            Code,
        };

    VkShaderModule ShaderModule;
    VkCheckResult(vkCreateShaderModule(Device, &ShaderModuleCreateInfo, 0, &ShaderModule));

    EndTempMem(TempMem);
    
    return ShaderModule;
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
                            VkFormat Format, i32 Usage, VkImageAspectFlags AspectMask, VkImage* OutImage,
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
// NOTE: Pipeline Helpers
//

inline void VkComputePipelineCreate(VkDevice Device, VkShaderModule Shader,
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

inline VkRenderPass VkRenderPassColorDepth(VkDevice Device, VkAttachmentDescription AttachmentDescriptions[2])
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

    VkCheckResult(vkCreateRenderPass(Device, &RenderPassCreateInfo, 0, &Result));

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
