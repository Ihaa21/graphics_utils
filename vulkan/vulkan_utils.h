#pragma once

#include "math\math.h"
#include "vulkan_memory.h"
#include "vulkan_pipeline.h"
#include "vulkan_cmd_buffer.h"

//
// NOTE: Descriptor Layout Builder
//

// TODO: Dynamic Arena
struct vk_descriptor_layout_builder
{
    u32 CurrNumBindings;
    VkDescriptorSetLayoutBinding Bindings[100];
    VkDescriptorSetLayout* Layout;
};

//
// NOTE: Render Pass Builder
//

struct vk_render_pass_builder
{
    temp_mem TempMem;
    linear_arena* Arena;
    
    // NOTE: Attachment Data
    u32 MaxNumAttachments;
    u32 NumAttachments;
    VkAttachmentDescription* Attachments;

    // NOTE: Dependencies
    u32 MaxNumDependencies;
    u32 NumDependencies;
    VkSubpassDependency* Dependencies;
    
    u32 MaxNumInputAttachmentRefs;
    u32 NumInputAttachmentRefs;
    VkAttachmentReference* InputAttachmentRefs;

    u32 MaxNumColorAttachmentRefs;
    u32 NumColorAttachmentRefs;
    VkAttachmentReference* ColorAttachmentRefs;

    u32 MaxNumResolveAttachmentRefs;
    u32 NumResolveAttachmentRefs;
    VkAttachmentReference* ResolveAttachmentRefs;

    u32 MaxNumDepthAttachmentRefs;
    u32 NumDepthAttachmentRefs;
    VkAttachmentReference* DepthAttachmentRefs;
    
    u32 MaxNumSubPasses;
    u32 NumSubPasses;
    VkSubpassDescription* SubPasses;
    
};

//
// NOTE: Descriptor Updater
//

struct vk_descriptor_manager
{
    linear_arena Arena;
    
    u32 MaxNumWrites;
    u32 NumWrites;
    VkWriteDescriptorSet* WriteArray;
};

//
// NOTE: Helper structs
//

struct vk_image
{
    VkImage Image;
    VkImageView View;
};

internal void VkCheckResult(VkResult Result);
inline VkBuffer VkBufferHandleCreate(VkDevice Device, VkBufferUsageFlags Usage, u64 BufferSize);
inline VkMemoryRequirements VkBufferGetMemoryRequirements(VkDevice Device, VkBuffer Buffer);

#include "vulkan_memory.cpp"
#include "vulkan_pipeline.cpp"
#include "vulkan_cmd_buffer.cpp"
#include "vulkan_utils.cpp"
