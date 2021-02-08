#pragma once

//
// NOTE: Pipeline Manager
//

struct vk_pipeline
{
    VkPipeline Handle;
    VkPipelineLayout Layout;
};

struct vk_shader_ref
{
    char* FileName;
    char* MainName;
    FILETIME ModifiedTime;
    VkShaderStageFlagBits Stage;
};

enum vk_pipeline_entry_type
{
    VkPipelineEntry_None,
    
    VkPipelineEntry_Graphics,
    VkPipelineEntry_Compute,
};

struct vk_pipeline_graphics_entry
{
    VkVertexInputBindingDescription* VertBindings;
    VkVertexInputAttributeDescription* VertAttributes;
    VkPipelineVertexInputStateCreateInfo VertexInputState;
    
    VkPipelineInputAssemblyStateCreateInfo InputAssemblyState;
    VkPipelineTessellationStateCreateInfo TessellationState;

    VkViewport* ViewPorts;
    VkRect2D* Scissors;
    VkPipelineViewportStateCreateInfo ViewportState;
    
    VkPipelineRasterizationStateCreateInfo RasterizationState;
    VkPipelineRasterizationConservativeStateCreateInfoEXT ConservativeState;
    VkPipelineMultisampleStateCreateInfo MultisampleState;    
    VkPipelineDepthStencilStateCreateInfo DepthStencilState;

    VkPipelineColorBlendAttachmentState* Attachments;
    VkPipelineColorBlendStateCreateInfo ColorBlendState;

    VkDynamicState* DynamicStates;
    VkPipelineDynamicStateCreateInfo DynamicStateCreateInfo;
    
    VkGraphicsPipelineCreateInfo PipelineCreateInfo;
};

struct vk_pipeline_compute_entry
{
    VkComputePipelineCreateInfo PipelineCreateInfo;
};

// TODO: Verify we can only have up to 5 shaders for graphics pipelines
#define VK_MAX_PIPELINE_STAGES 5
struct vk_pipeline_entry
{
    vk_pipeline_entry_type Type;
    union
    {
        vk_pipeline_graphics_entry GraphicsEntry;
        vk_pipeline_compute_entry ComputeEntry;
    };

    u32 NumShaders;
    vk_shader_ref ShaderRefs[VK_MAX_PIPELINE_STAGES];

    vk_pipeline Pipeline;
};

struct vk_pipeline_manager
{
    linear_arena Arena;
    
    u32 MaxNumPipelines;
    u32 NumPipelines;
    vk_pipeline_entry* PipelineArray;
};

//
// NOTE: Pipline Builder
//

enum vk_pipeline_builder_flags
{
    VkPipelineFlag_HasDepthStencil = 1 << 0,
};

struct vk_pipeline_builder_shader
{
    char* FileName;
    char* MainName;
    VkShaderStageFlagBits Stage;
};

struct vk_pipeline_builder
{
    linear_arena* Arena;
    temp_mem TempMem;

    u32 Flags;

    // NOTE: Shader data
    u32 NumShaders;
    vk_pipeline_builder_shader Shaders[VK_MAX_PIPELINE_STAGES];
    
    // NOTE: Vertex data
    u32 CurrVertexBindingSize;
    u32 CurrVertexLocation;

    u32 MaxNumVertexBindings;
    u32 NumVertexBindings;
    VkVertexInputBindingDescription* VertexBindings;

    u32 MaxNumVertexAttributes;
    u32 NumVertexAttributes;
    VkVertexInputAttributeDescription* VertexAttributes;

    // NOTE: Input Assembly Data
    VkPipelineInputAssemblyStateCreateInfo InputAssembly;
    
    // NOTE: Depth Stencil Data
    VkPipelineDepthStencilStateCreateInfo DepthStencil;

    // NOTE: Color Attachment Data
    u32 MaxNumColorAttachments;
    u32 NumColorAttachments;
    VkPipelineColorBlendAttachmentState* ColorAttachments;

    VkPipelineRasterizationStateCreateInfo RasterizationState;
    VkPipelineRasterizationConservativeStateCreateInfoEXT ConservativeState;
    VkPipelineMultisampleStateCreateInfo MultiSampleState;

    VkGraphicsPipelineCreateInfo PipelineCreateInfo;
};

inline void VkPipelineInputAssemblyAdd(vk_pipeline_builder* Builder, VkPrimitiveTopology Topology, VkBool32 PrimRestart);

