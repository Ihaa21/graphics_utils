
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

inline VkClearValue VkClearColorCreate(f32 R, f32 G, f32 B, f32 A)
{
    VkClearValue Result = {};
    Result.color.float32[0] = R;
    Result.color.float32[1] = G;
    Result.color.float32[2] = B;
    Result.color.float32[3] = A;

    return Result;
}

inline VkClearValue VkClearDepthStencilCreate(f32 Depth, u32 Stencil)
{
    VkClearValue Result = {};
    Result.depthStencil.depth = Depth;
    Result.depthStencil.stencil = Stencil;

    return Result;
}

//
// NOTE: Coordinate System Functions
//

/*
   NOTE: Vulkan coordinate system:

     - Camera space: x points left, y points up, z points away
     - Screen space: x[-1:1] points left, y[-1:1] points down, z[1:0] points away

     The games coordinate systems are as follows:

       - World space: x points left, y points up the screen, z points up and down
       - Screen space: x points left [0:1], y points up [0:1]
     
 */

inline m4 VkOrthoProjM4(f32 Left, f32 Right, f32 Top, f32 Bottom, f32 Near, f32 Far)
{
    m4 Result = {};
    Result.v[0].x = 2.0f / (Right - Left);
    Result.v[1].y = 2.0f / (Bottom - Top);
    Result.v[2].z = 1.0f / (Near - Far);
    Result.v[3].x = -(Left + Right) / (Right - Left);
    Result.v[3].y = -(Bottom + Top) / (Bottom - Top);
    Result.v[3].z = (-Far) / (Near - Far);
    Result.v[3].w = 1.0f;

    return Result;
}

inline m4 VkPerspProjM4(f32 AspectRatio, f32 Fov, f32 Near, f32 Far)
{
    // NOTE: Reverse Z
    m4 Result = {};
    Result.v[0].x = 1.0f / (AspectRatio*Tan(Fov*0.5f));
    Result.v[1].y = -1.0f / (Tan(Fov*0.5f));
    Result.v[2].z = (-Near) / (Far - Near);
    Result.v[2].w = 1.0f;
    Result.v[3].z = (Near*Far) / (Far - Near);
    
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
// NOTE: Buffer Helpers
//

inline VkBuffer VkBufferHandleCreate(VkDevice Device, VkBufferUsageFlags Usage, u64 BufferSize)
{
    VkBuffer Result = {};
    
    VkBufferCreateInfo BufferCreateInfo = {};
    BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    BufferCreateInfo.size = BufferSize;
    BufferCreateInfo.usage = Usage;
    BufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    BufferCreateInfo.queueFamilyIndexCount = 0;
    BufferCreateInfo.pQueueFamilyIndices = 0;
    VkCheckResult(vkCreateBuffer(Device, &BufferCreateInfo, 0, &Result));

    return Result;
}

inline VkMemoryRequirements VkBufferGetMemoryRequirements(VkDevice Device, VkBuffer Buffer)
{
    VkMemoryRequirements Result;
    vkGetBufferMemoryRequirements(Device, Buffer, &Result);
    return Result;
}

inline vk_ptr VkBufferBindMemory(VkDevice Device, vk_linear_arena* Arena, VkBuffer Buffer, VkMemoryRequirements Requirements)
{
    vk_ptr Result = {};
    Result = VkPushSize(Arena, Requirements.size, Requirements.alignment);
    VkCheckResult(vkBindBufferMemory(Device, Buffer, Result.Memory, Result.Offset));

    return Result;
}

inline void VkBufferCreate(VkDevice Device, VkDeviceMemory Memory, VkBufferUsageFlags Usage,
                           u64 BufferSize, VkBuffer* OutBuffer)
{
    *OutBuffer = VkBufferHandleCreate(Device, Usage, BufferSize);
    VkMemoryRequirements MemoryRequirements = VkBufferGetMemoryRequirements(Device, *OutBuffer);
    VkCheckResult(vkBindBufferMemory(Device, *OutBuffer, Memory, 0));
}

inline void VkBufferCreate(VkDevice Device, vk_linear_arena* Arena, VkBufferUsageFlags Usage,
                           u64 BufferSize, VkBuffer* OutBuffer, vk_ptr* OutGpuPtr)
{
    *OutBuffer = VkBufferHandleCreate(Device, Usage, BufferSize);
    VkMemoryRequirements MemoryRequirements = VkBufferGetMemoryRequirements(Device, *OutBuffer);
    *OutGpuPtr = VkBufferBindMemory(Device, Arena, *OutBuffer, MemoryRequirements);
}

inline VkBuffer VkBufferCreate(VkDevice Device, VkDeviceMemory Memory, VkBufferUsageFlags Usage, u64 BufferSize)
{
    VkBuffer Result = {};
    VkBufferCreate(Device, Memory, Usage, BufferSize, &Result);
    return Result;
}

inline VkBuffer VkBufferCreate(VkDevice Device, vk_linear_arena* Arena, VkBufferUsageFlags Usage,
                               u64 BufferSize)
{
    VkBuffer Result = {};
    vk_ptr Ptr = {};
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
// NOTE: Image View Helpers
//

inline VkImageView VkImageViewCreate(VkDevice Device, VkImage Image, VkImageViewType ViewType, VkFormat Format,
                                     VkImageAspectFlags AspectMask, u32 MipLevel, u32 LayerCount)
{
    VkImageView Result = {};
    
    VkImageViewCreateInfo CreateInfo = {};
    CreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    CreateInfo.image = Image;
    CreateInfo.viewType = ViewType;
    CreateInfo.format = Format;
    CreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    CreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    CreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    CreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    CreateInfo.subresourceRange.aspectMask = AspectMask;
    CreateInfo.subresourceRange.baseMipLevel = MipLevel;
    CreateInfo.subresourceRange.levelCount = 1;
    CreateInfo.subresourceRange.baseArrayLayer = 0;
    CreateInfo.subresourceRange.layerCount = LayerCount;
    VkCheckResult(vkCreateImageView(Device, &CreateInfo, 0, &Result));

    return Result;
}

//
// NOTE: Image Helpers
//

inline VkImage VkImageHandleCreate(VkDevice Device, u32 Width, u32 Height, VkFormat Format, VkImageUsageFlags Usage,
                                   VkSampleCountFlagBits SampleCount = VK_SAMPLE_COUNT_1_BIT)
{
    VkImage Result = {};
    
    VkImageCreateInfo ImageCreateInfo = {};
    ImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    ImageCreateInfo.format = Format;
    ImageCreateInfo.extent.width = Width;
    ImageCreateInfo.extent.height = Height;
    ImageCreateInfo.extent.depth = 1;
    ImageCreateInfo.mipLevels = 1;
    ImageCreateInfo.arrayLayers = 1;
    ImageCreateInfo.samples = SampleCount;
    ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    ImageCreateInfo.usage = Usage;
    ImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkCheckResult(vkCreateImage(Device, &ImageCreateInfo, 0, &Result));

    return Result;
}

inline void VkImageHandleCreate(VkDevice Device, u32 Width, u32 Height, VkFormat Format, VkImageUsageFlags Usage,
                                VkImageAspectFlags AspectMask, VkSampleCountFlagBits SampleCount, VkImage* OutImage, VkImageView* OutImageView)
{
    *OutImage = VkImageHandleCreate(Device, Width, Height, Format, Usage, SampleCount);
    *OutImageView = VkImageViewCreate(Device, *OutImage, VK_IMAGE_VIEW_TYPE_2D, Format, AspectMask, 0, 1);
}

inline vk_image VkImageHandleCreate(VkDevice Device, u32 Width, u32 Height, VkFormat Format, VkImageUsageFlags Usage,
                                    VkImageAspectFlags AspectMask, VkSampleCountFlagBits SampleCount = VK_SAMPLE_COUNT_1_BIT)
{
    vk_image Result = {};
    VkImageHandleCreate(Device, Width, Height, Format, Usage, AspectMask, SampleCount, &Result.Image, &Result.View);

    return Result;
}

inline VkMemoryRequirements VkImageGetMemoryRequirements(VkDevice Device, VkImage Image)
{
    VkMemoryRequirements Result;
    vkGetImageMemoryRequirements(Device, Image, &Result);
    return Result;
}    

inline vk_ptr VkImageBindMemory(VkDevice Device, vk_linear_arena* Arena, VkImage Image, VkMemoryRequirements Requirements)
{
    vk_ptr Result = VkPushSize(Arena, Requirements.size, Requirements.alignment);
    VkCheckResult(vkBindImageMemory(Device, Image, Result.Memory, Result.Offset));

    return Result;
}

inline VkImage VkImageCreate(VkDevice Device, vk_linear_arena* Arena, u32 Width, u32 Height, VkFormat Format, VkImageUsageFlags Usage,
                             VkSampleCountFlagBits SampleCount = VK_SAMPLE_COUNT_1_BIT)
{
    VkImage Result = VkImageHandleCreate(Device, Width, Height, Format, Usage, SampleCount);

    VkMemoryRequirements MemoryRequirements = VkImageGetMemoryRequirements(Device, Result);
    VkImageBindMemory(Device, Arena, Result, MemoryRequirements);

    return Result;
}

inline void VkImageCreate(VkDevice Device, vk_linear_arena* Arena, u32 Width, u32 Height, VkFormat Format, VkImageUsageFlags Usage,
                          VkImageAspectFlags AspectMask, VkImage* OutImage, VkImageView* OutImageView)
{
    *OutImage = VkImageCreate(Device, Arena, Width, Height, Format, Usage);
    *OutImageView = VkImageViewCreate(Device, *OutImage, VK_IMAGE_VIEW_TYPE_2D, Format, AspectMask, 0, 1);
}

inline void VkImageCreate(VkDevice Device, vk_linear_arena* Arena, u32 Width, u32 Height, VkFormat Format, VkImageUsageFlags Usage,
                          VkImageAspectFlags AspectMask, VkSampleCountFlagBits SampleCount, VkImage* OutImage, VkImageView* OutImageView)
{
    *OutImage = VkImageCreate(Device, Arena, Width, Height, Format, Usage, SampleCount);
    *OutImageView = VkImageViewCreate(Device, *OutImage, VK_IMAGE_VIEW_TYPE_2D, Format, AspectMask, 0, 1);
}

inline vk_image VkImageCreate(VkDevice Device, vk_linear_arena* Arena, u32 Width, u32 Height, VkFormat Format, VkImageUsageFlags Usage,
                              VkImageAspectFlags AspectMask, VkSampleCountFlagBits SampleCount = VK_SAMPLE_COUNT_1_BIT)
{
    vk_image Result = {};
    VkImageCreate(Device, Arena, Width, Height, Format, Usage, AspectMask, SampleCount, &Result.Image, &Result.View);

    return Result;
}

inline VkImage VkCubeMapCreate(VkDevice Device, vk_linear_arena* Arena, u32 Width, u32 Height, VkFormat Format, VkImageUsageFlags Usage,
                               u32 MipLevels)
{
    VkImage Result = {};
    
    VkImageCreateInfo ImageCreateInfo = {};
    ImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    ImageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    ImageCreateInfo.format = Format;
    ImageCreateInfo.extent.width = Width;
    ImageCreateInfo.extent.height = Height;
    ImageCreateInfo.extent.depth = 1;
    ImageCreateInfo.mipLevels = MipLevels;
    ImageCreateInfo.arrayLayers = 6;
    ImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    ImageCreateInfo.usage = Usage;
    ImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkCheckResult(vkCreateImage(Device, &ImageCreateInfo, 0, &Result));

    VkMemoryRequirements ImageMemRequirements;
    vkGetImageMemoryRequirements(Device, Result, &ImageMemRequirements);
    vk_ptr MemPtr = VkPushSize(Arena, ImageMemRequirements.size, ImageMemRequirements.alignment);
    VkCheckResult(vkBindImageMemory(Device, Result, MemPtr.Memory, MemPtr.Offset));

    return Result;
}

inline vk_image VkCubeMapCreate(VkDevice Device, vk_linear_arena* Arena, u32 Width, u32 Height, VkFormat Format,
                                VkImageUsageFlags Usage, VkImageAspectFlags AspectMask, u32 MipLevels)
{
    vk_image Result = {};
    Result.Image = VkCubeMapCreate(Device, Arena, Width, Height, Format, Usage, MipLevels);
    Result.View = VkImageViewCreate(Device, Result.Image, VK_IMAGE_VIEW_TYPE_CUBE, Format, AspectMask, 0, 6);

    return Result;
}

inline void VkImageDestroy(VkDevice Device, vk_image Image)
{
    vkDestroyImageView(Device, Image.View, 0);
    vkDestroyImage(Device, Image.Image, 0);
}

//
// NOTE: Sampler Helpers
//

inline VkSampler VkSamplerCreate(VkDevice Device, VkFilter Filter, VkSamplerAddressMode AddressMode, VkBorderColor BorderColor,
                                 f32 Ansitropy)
{
    VkSamplerCreateInfo CreateInfo = {};
    CreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    CreateInfo.magFilter = Filter;
    CreateInfo.minFilter = Filter;
    CreateInfo.addressModeU = AddressMode;
    CreateInfo.addressModeV = AddressMode;
    CreateInfo.addressModeW = AddressMode;
    CreateInfo.anisotropyEnable = Ansitropy > 0.0f ? VK_TRUE : VK_FALSE;
    CreateInfo.maxAnisotropy = Ansitropy;
    CreateInfo.compareEnable = VK_FALSE;
    CreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    CreateInfo.mipLodBias = 0;
    CreateInfo.minLod = 0;
    CreateInfo.maxLod = 0;
    CreateInfo.borderColor = BorderColor;
    CreateInfo.unnormalizedCoordinates = VK_FALSE;

    VkSampler Result = {};
    VkCheckResult(vkCreateSampler(Device, &CreateInfo, 0, &Result));
    
    return Result;
}

inline VkSampler VkSamplerMipMapCreate(VkDevice Device, VkFilter Filter, VkSamplerAddressMode AddressMode, f32 Ansitropy,
                                       VkSamplerMipmapMode MipMapMode, f32 MipLodBias, f32 MinLod, f32 MaxLod)
{
    VkSamplerCreateInfo CreateInfo = {};
    CreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    CreateInfo.magFilter = Filter;
    CreateInfo.minFilter = Filter;
    CreateInfo.addressModeU = AddressMode;
    CreateInfo.addressModeV = AddressMode;
    CreateInfo.addressModeW = AddressMode;
    CreateInfo.anisotropyEnable = Ansitropy > 0.0f ? VK_TRUE : VK_FALSE;
    CreateInfo.maxAnisotropy = Ansitropy;
    CreateInfo.compareEnable = VK_FALSE;
    CreateInfo.mipmapMode = MipMapMode;
    CreateInfo.mipLodBias = MipLodBias;
    CreateInfo.minLod = MinLod;
    CreateInfo.maxLod = MaxLod;
    CreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    CreateInfo.unnormalizedCoordinates = VK_FALSE;

    VkSampler Result = {};
    VkCheckResult(vkCreateSampler(Device, &CreateInfo, 0, &Result));
    
    return Result;
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
    // TODO: Use a resizable array!
    vk_descriptor_manager Result = {};

    u32 ArenaSize = (sizeof(VkWriteDescriptorSet)*MaxNumWrites +
                     Max((u32)sizeof(VkDescriptorImageInfo), (u32)sizeof(VkDescriptorBufferInfo))*MaxNumWrites);
    Result.Arena = LinearSubArena(Arena, ArenaSize);
    Result.MaxNumWrites = MaxNumWrites;
    Result.WriteArray = PushArray(&Result.Arena, VkWriteDescriptorSet, MaxNumWrites);

    return Result;
}

inline void VkDescriptorBufferWrite(vk_descriptor_manager* Manager, VkDescriptorSet Set, u32 Binding,
                                    VkDescriptorType DescType, VkBuffer Buffer, u64 Offset = 0)
{
    VkDescriptorBufferInfo* BufferInfo = PushStruct(&Manager->Arena, VkDescriptorBufferInfo);
    BufferInfo->buffer = Buffer;
    BufferInfo->offset = Offset;
    BufferInfo->range = VK_WHOLE_SIZE;

    Assert(Manager->NumWrites < Manager->MaxNumWrites);
    VkWriteDescriptorSet* DsWrite = Manager->WriteArray + Manager->NumWrites++;
    *DsWrite = {};
    DsWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    DsWrite->dstSet = Set;
    DsWrite->dstBinding = Binding;
    DsWrite->descriptorCount = 1;
    DsWrite->descriptorType = DescType;
    DsWrite->pBufferInfo = BufferInfo;
}

inline void VkDescriptorTexelBufferWrite(vk_descriptor_manager* Manager, VkDescriptorSet Set, u32 Binding,
                                         VkDescriptorType DescType, VkBufferView* BufferView)
{
    Assert(Manager->NumWrites < Manager->MaxNumWrites);
    VkWriteDescriptorSet* DsWrite = Manager->WriteArray + Manager->NumWrites++;
    *DsWrite = {};
    DsWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    DsWrite->dstSet = Set;
    DsWrite->dstBinding = Binding;
    DsWrite->descriptorCount = 1;
    DsWrite->descriptorType = DescType;
    DsWrite->pTexelBufferView = BufferView;
}

inline void VkDescriptorImageWrite(vk_descriptor_manager* Manager, VkDescriptorSet Set, u32 Binding, u32 ArrayElementId,
                                   VkDescriptorType DescType, VkImageView ImageView, VkSampler Sampler,
                                   VkImageLayout ImageLayout)
{
    VkDescriptorImageInfo* ImageInfo = PushStruct(&Manager->Arena, VkDescriptorImageInfo);
    ImageInfo->sampler = Sampler;
    ImageInfo->imageView = ImageView;
    ImageInfo->imageLayout = ImageLayout;

    Assert(Manager->NumWrites < Manager->MaxNumWrites);
    VkWriteDescriptorSet* DsWrite = Manager->WriteArray + Manager->NumWrites++;
    *DsWrite = {};
    DsWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    DsWrite->dstSet = Set;
    DsWrite->dstBinding = Binding;
    DsWrite->dstArrayElement = ArrayElementId;
    DsWrite->descriptorCount = 1;
    DsWrite->descriptorType = DescType;
    DsWrite->pImageInfo = ImageInfo;
}

inline void VkDescriptorImageWrite(vk_descriptor_manager* Manager, VkDescriptorSet Set, u32 Binding,
                                   VkDescriptorType DescType, VkImageView ImageView, VkSampler Sampler,
                                   VkImageLayout ImageLayout)
{
    VkDescriptorImageWrite(Manager, Set, Binding, 0, DescType, ImageView, Sampler, ImageLayout);
}

inline void VkDescriptorManagerFlush(VkDevice Device, vk_descriptor_manager* Manager)
{
    vkUpdateDescriptorSets(Device, Manager->NumWrites, Manager->WriteArray, 0, 0);

    Manager->NumWrites = 0;
    Manager->Arena.Used = sizeof(VkWriteDescriptorSet)*Manager->MaxNumWrites;
}

//
// NOTE: Render Pass Helpers
//

inline vk_render_pass_builder VkRenderPassBuilderBegin(linear_arena* Arena)
{
    vk_render_pass_builder Result = {};
    Result.Arena = Arena;
    Result.TempMem = BeginTempMem(Arena);

    // IMPORTANT: These arrays should be larger if these sizes aren't enough
    Result.MaxNumAttachments = 10;
    Result.Attachments = PushArray(Arena, VkAttachmentDescription, Result.MaxNumAttachments);

    Result.MaxNumDependencies = 10;
    Result.Dependencies = PushArray(Arena, VkSubpassDependency, Result.MaxNumDependencies);

    Result.MaxNumInputAttachmentRefs = 100;
    Result.InputAttachmentRefs = PushArray(Arena, VkAttachmentReference, Result.MaxNumInputAttachmentRefs);

    Result.MaxNumColorAttachmentRefs = 100;
    Result.ColorAttachmentRefs = PushArray(Arena, VkAttachmentReference, Result.MaxNumColorAttachmentRefs);

    Result.MaxNumResolveAttachmentRefs = 100;
    Result.ResolveAttachmentRefs = PushArray(Arena, VkAttachmentReference, Result.MaxNumResolveAttachmentRefs);

    Result.MaxNumDepthAttachmentRefs = 10;
    Result.DepthAttachmentRefs = PushArray(Arena, VkAttachmentReference, Result.MaxNumDepthAttachmentRefs);

    Result.MaxNumSubPasses = 10;
    Result.SubPasses = PushArray(Arena, VkSubpassDescription, Result.MaxNumSubPasses);
    
    return Result;
}

inline u32 VkRenderPassAttachmentAdd(vk_render_pass_builder* Builder, VkFormat Format, VkSampleCountFlagBits SampleCount,
                                     VkAttachmentLoadOp LoadOp, VkAttachmentStoreOp StoreOp, VkImageLayout InitialLayout,
                                     VkImageLayout FinalLayout)
{
    Assert(Builder->NumAttachments < Builder->MaxNumAttachments);

    u32 Id = Builder->NumAttachments++;
    VkAttachmentDescription* Color = Builder->Attachments + Id;
    Color->flags = 0;
    Color->format = Format;
    Color->samples = SampleCount;
    Color->loadOp = LoadOp;
    Color->storeOp = StoreOp;
    Color->stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    Color->stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    Color->initialLayout = InitialLayout;
    Color->finalLayout = FinalLayout;

    return Id;
}

inline u32 VkRenderPassAttachmentAdd(vk_render_pass_builder* Builder, VkFormat Format, VkAttachmentLoadOp LoadOp,
                                     VkAttachmentStoreOp StoreOp, VkImageLayout InitialLayout, VkImageLayout FinalLayout)
{
    u32 Result = VkRenderPassAttachmentAdd(Builder, Format, VK_SAMPLE_COUNT_1_BIT, LoadOp, StoreOp, InitialLayout, FinalLayout);
    return Result;
}

inline void VkRenderPassSubPassBegin(vk_render_pass_builder* Builder, VkPipelineBindPoint BindPoint)
{
    Assert(Builder->NumSubPasses < Builder->MaxNumSubPasses);
    
    VkSubpassDescription* SubPass = Builder->SubPasses + Builder->NumSubPasses;
    *SubPass = {};
    SubPass->pipelineBindPoint = BindPoint;
    SubPass->pColorAttachments = Builder->ColorAttachmentRefs + Builder->NumColorAttachmentRefs;
    SubPass->pInputAttachments = Builder->InputAttachmentRefs + Builder->NumInputAttachmentRefs;
}

inline void VkRenderPassInputRefAdd(vk_render_pass_builder* Builder, u32 AttachmentId, VkImageLayout Layout)
{
    Assert(Builder->NumInputAttachmentRefs < Builder->MaxNumInputAttachmentRefs);
    Assert(AttachmentId < Builder->NumAttachments);

    VkAttachmentReference* Reference = Builder->InputAttachmentRefs + Builder->NumInputAttachmentRefs++;
    *Reference = {};
    Reference->attachment = AttachmentId;
    Reference->layout = Layout;
    
    VkSubpassDescription* SubPass = Builder->SubPasses + Builder->NumSubPasses;
    SubPass->inputAttachmentCount += 1;
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

inline void VkRenderPassColorResolveRefAdd(vk_render_pass_builder* Builder, u32 ColorId, u32 ResolveId, VkImageLayout Layout)
{
    Assert(Builder->NumColorAttachmentRefs < Builder->MaxNumColorAttachmentRefs);
    Assert(ColorId < Builder->NumAttachments);
    Assert(ResolveId < Builder->NumAttachments);

    VkAttachmentReference* ColorReference = Builder->ColorAttachmentRefs + Builder->NumColorAttachmentRefs++;
    *ColorReference = {};
    ColorReference->attachment = ColorId;
    ColorReference->layout = Layout;

    VkAttachmentReference* ResolveReference = Builder->ResolveAttachmentRefs + Builder->NumResolveAttachmentRefs++;
    *ResolveReference = {};
    ResolveReference->attachment = ResolveId;
    ResolveReference->layout = Layout;

    // NOTE: If first resolve attachment, add it to our subpass
    VkSubpassDescription* SubPass = Builder->SubPasses + Builder->NumSubPasses;
    if (!SubPass->pResolveAttachments)
    {
        SubPass->pResolveAttachments = Builder->ResolveAttachmentRefs + Builder->NumResolveAttachmentRefs - 1;
    }
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

inline void VkRenderPassDependency(vk_render_pass_builder* Builder, VkPipelineStageFlags SrcStageFlags,
                                   VkPipelineStageFlags DstStageFlags, VkAccessFlags SrcAccessMask, VkAccessFlags DstAccessMask,
                                   VkDependencyFlags DependencyFlags)
{
    Assert(Builder->NumDependencies < Builder->MaxNumDependencies);

    VkSubpassDependency* Dependency = Builder->Dependencies + Builder->NumDependencies++;
    if (Builder->NumSubPasses - 1 < 0)
    {
        Dependency->srcSubpass = VK_SUBPASS_EXTERNAL;
    }
    else
    {
        Dependency->srcSubpass = Builder->NumSubPasses - 1;
    }

    Dependency->dstSubpass = Builder->NumSubPasses;
    
    Dependency->srcStageMask = SrcStageFlags;
    Dependency->dstStageMask = DstStageFlags;
    Dependency->srcAccessMask = SrcAccessMask;
    Dependency->dstAccessMask = DstAccessMask;
    Dependency->dependencyFlags = DependencyFlags;
}

inline VkRenderPass VkRenderPassBuilderEnd(vk_render_pass_builder* Builder, VkDevice Device)
{
    VkRenderPass Result = {};

    // NOTE: Check if we have to update some dependencies to external for dst
    for (u32 DependencyId = 0; DependencyId < Builder->NumDependencies; ++DependencyId)
    {
        if (Builder->Dependencies[DependencyId].dstSubpass == Builder->NumSubPasses)
        {
            Builder->Dependencies[DependencyId].dstSubpass = VK_SUBPASS_EXTERNAL;
        }
    }
    
    VkRenderPassCreateInfo RenderPassCreateInfo = {};
    RenderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    RenderPassCreateInfo.attachmentCount = Builder->NumAttachments;
    RenderPassCreateInfo.pAttachments = Builder->Attachments;
    RenderPassCreateInfo.subpassCount = Builder->NumSubPasses;
    RenderPassCreateInfo.pSubpasses = Builder->SubPasses;
    RenderPassCreateInfo.dependencyCount = Builder->NumDependencies;
    RenderPassCreateInfo.pDependencies = Builder->Dependencies;
    VkCheckResult(vkCreateRenderPass(Device, &RenderPassCreateInfo, 0, &Result));

    EndTempMem(Builder->TempMem);
    
    return Result;
}

//
// NOTE: Compute Shader Helpers
//

inline void VkComputeDispatch(vk_commands Commands, vk_pipeline* Pipeline, VkDescriptorSet* DescriptorSets, u32 NumDescriptorSets, u32 DispatchX,
                              u32 DispatchY, u32 DispatchZ)
{
    vkCmdBindPipeline(Commands.Buffer, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline->Handle);
    vkCmdBindDescriptorSets(Commands.Buffer, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline->Layout, 0,
                            NumDescriptorSets, DescriptorSets, 0, 0);
    vkCmdDispatch(Commands.Buffer, DispatchX, DispatchY, DispatchZ);
}
