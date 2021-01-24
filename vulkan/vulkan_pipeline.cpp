
//
// NOTE: Pipeline Manager
//

inline vk_pipeline_manager VkPipelineManagerCreate(linear_arena* Arena)
{
    vk_pipeline_manager Result = {};
    Result.Arena = LinearSubArena(Arena, MegaBytes(5));
    Result.MaxNumPipelines = 100; // TODO: This is hardcoded for now
    Result.PipelineArray = PushArray(&Result.Arena, vk_pipeline_entry, Result.MaxNumPipelines);

    return Result;
}

inline VkPipelineShaderStageCreateInfo VkPipelineShaderStage(VkShaderStageFlagBits Stage, VkShaderModule Module, char* MainName)
{
    VkPipelineShaderStageCreateInfo Result = {};
    Result.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    Result.stage = Stage;
    Result.module = Module;
    Result.pName = MainName;
    
    return Result;
}

inline void VkPipelineAddShaderRef(vk_pipeline_manager* Manager, vk_pipeline_entry* Entry, char* FileName, char* MainName,
                                   VkShaderStageFlagBits Stage)
{
    Assert(Entry->NumShaders < VK_MAX_PIPELINE_STAGES);
    vk_shader_ref* ShaderRef = Entry->ShaderRefs + Entry->NumShaders++;
    
    // NOTE: Copy strings since our DLL might get swapped and create all shaders
    ShaderRef->FileName = PushString(&Manager->Arena, FileName);
    ShaderRef->MainName = PushString(&Manager->Arena, MainName);
    ShaderRef->Stage = Stage;
}

inline void VkPipelineAddShaderRef(vk_pipeline_manager* Manager, vk_pipeline_entry* Entry, vk_pipeline_builder_shader BuilderShader)
{
    VkPipelineAddShaderRef(Manager, Entry, BuilderShader.FileName, BuilderShader.MainName, BuilderShader.Stage);
}

inline VkShaderModule VkPipelineGetShaderModule(VkDevice Device, HANDLE File, linear_arena* TempArena, vk_shader_ref* ShaderRef)
{
    VkShaderModule Result = {};
    
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

    if (!GetFileTime(File, 0, 0, &ShaderRef->ModifiedTime))
    {
        DWORD Error = GetLastError();
        InvalidCodePath;
    }
    
    VkShaderModuleCreateInfo ShaderModuleCreateInfo = {};
    ShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ShaderModuleCreateInfo.codeSize = CodeSize.LowPart;
    ShaderModuleCreateInfo.pCode = Code;
    VkCheckResult(vkCreateShaderModule(Device, &ShaderModuleCreateInfo, 0, &Result));

    EndTempMem(TempMem);

    return Result;
}

inline VkShaderModule VkPipelineGetShaderModule(VkDevice Device, linear_arena* TempArena, vk_shader_ref* ShaderRef)
{
    HANDLE File = CreateFileA(ShaderRef->FileName, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (File == INVALID_HANDLE_VALUE)
    {
        DWORD Error = GetLastError();
        InvalidCodePath;
    }

    VkShaderModule Result = VkPipelineGetShaderModule(Device, File, TempArena, ShaderRef);
    CloseHandle(File);

    return Result;
}

inline vk_pipeline* VkPipelineComputeCreate(VkDevice Device, vk_pipeline_manager* Manager, linear_arena* TempArena, char* FileName,
                                            char* MainName, VkDescriptorSetLayout* Layouts, u32 NumLayouts, u32 PushConstantSize = 0)
{
    Assert(Manager->NumPipelines < Manager->MaxNumPipelines);
    vk_pipeline_entry* Entry = Manager->PipelineArray + Manager->NumPipelines++;
    *Entry = {};
    Entry->Type = VkPipelineEntry_Compute;
    VkPipelineAddShaderRef(Manager, Entry, FileName, MainName, VK_SHADER_STAGE_COMPUTE_BIT);
    
    // NOTE: Setup pipeline create infos and create pipeline
    {
        vk_pipeline_compute_entry* ComputeEntry = &Entry->ComputeEntry;
        VkShaderModule CsShader = VkPipelineGetShaderModule(Device, TempArena, Entry->ShaderRefs + 0);
        VkPipelineShaderStageCreateInfo ShaderStageCreateInfo = VkPipelineShaderStage(VK_SHADER_STAGE_COMPUTE_BIT, CsShader, MainName);

        VkPushConstantRange PushConstantRange = {};
        PushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        PushConstantRange.offset = 0;
        PushConstantRange.size = PushConstantSize;
        
        VkPipelineLayoutCreateInfo LayoutCreateInfo = {};
        LayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        LayoutCreateInfo.setLayoutCount = NumLayouts;
        LayoutCreateInfo.pSetLayouts = Layouts;
        if (PushConstantSize > 0)
        {
            LayoutCreateInfo.pushConstantRangeCount = 1;
            LayoutCreateInfo.pPushConstantRanges = &PushConstantRange;
        }
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

inline vk_pipeline* VkPipelineGraphicsCreate(VkDevice Device, vk_pipeline_manager* Manager, linear_arena* TempArena,
                                             vk_pipeline_builder_shader* Shaders, u32 NumShaders,
                                             VkPipelineLayoutCreateInfo* LayoutCreateInfo, VkGraphicsPipelineCreateInfo* PipelineCreateInfo)
{
    Assert(Manager->NumPipelines < Manager->MaxNumPipelines);
    vk_pipeline_entry* Entry = Manager->PipelineArray + Manager->NumPipelines++;
    *Entry = {};
    Entry->Type = VkPipelineEntry_Graphics;

    // NOTE: Setup pipeline create infos and create pipeline
    {
        vk_pipeline_graphics_entry* GraphicsEntry = &Entry->GraphicsEntry;

        // NOTE: Copy all pipeline create infos into managers arena
        {
            GraphicsEntry->VertexInputState = *PipelineCreateInfo->pVertexInputState;
            {
                if (GraphicsEntry->VertexInputState.vertexBindingDescriptionCount > 0)
                {
                    GraphicsEntry->VertBindings = PushArray(&Manager->Arena, VkVertexInputBindingDescription, GraphicsEntry->VertexInputState.vertexBindingDescriptionCount);
                    Copy(GraphicsEntry->VertexInputState.pVertexBindingDescriptions, GraphicsEntry->VertBindings, sizeof(VkVertexInputBindingDescription)*GraphicsEntry->VertexInputState.vertexBindingDescriptionCount);
                    GraphicsEntry->VertexInputState.pVertexBindingDescriptions = GraphicsEntry->VertBindings;
                }

                if (GraphicsEntry->VertexInputState.vertexAttributeDescriptionCount > 0)
                {
                    GraphicsEntry->VertAttributes = PushArray(&Manager->Arena, VkVertexInputAttributeDescription, GraphicsEntry->VertexInputState.vertexAttributeDescriptionCount);
                    Copy(GraphicsEntry->VertexInputState.pVertexAttributeDescriptions, GraphicsEntry->VertAttributes, sizeof(VkVertexInputAttributeDescription)*GraphicsEntry->VertexInputState.vertexAttributeDescriptionCount);
                    GraphicsEntry->VertexInputState.pVertexAttributeDescriptions = GraphicsEntry->VertAttributes;
                }
            }
            
            GraphicsEntry->InputAssemblyState = *PipelineCreateInfo->pInputAssemblyState;

            GraphicsEntry->ViewportState = *PipelineCreateInfo->pViewportState;
            {
                if (GraphicsEntry->ViewportState.pViewports)
                {
                    GraphicsEntry->ViewPorts = PushArray(&Manager->Arena, VkViewport, GraphicsEntry->ViewportState.viewportCount);
                    Copy(GraphicsEntry->ViewportState.pViewports, GraphicsEntry->ViewPorts, sizeof(VkViewport)*GraphicsEntry->ViewportState.viewportCount);
                    GraphicsEntry->ViewportState.pViewports = GraphicsEntry->ViewPorts;
                }

                if (GraphicsEntry->ViewportState.pScissors)
                {
                    GraphicsEntry->Scissors = PushArray(&Manager->Arena, VkRect2D, GraphicsEntry->ViewportState.scissorCount);
                    Copy(GraphicsEntry->ViewportState.pScissors, GraphicsEntry->Scissors, sizeof(VkRect2D)*GraphicsEntry->ViewportState.scissorCount);
                    GraphicsEntry->ViewportState.pScissors = GraphicsEntry->Scissors;
                }
            }
            
            GraphicsEntry->RasterizationState = *PipelineCreateInfo->pRasterizationState;
            GraphicsEntry->MultisampleState = *PipelineCreateInfo->pMultisampleState;
            
            GraphicsEntry->ColorBlendState = *PipelineCreateInfo->pColorBlendState;
            if (GraphicsEntry->ColorBlendState.attachmentCount > 0)
            {
                GraphicsEntry->Attachments = PushArray(&Manager->Arena, VkPipelineColorBlendAttachmentState, GraphicsEntry->ColorBlendState.attachmentCount);
                Copy(GraphicsEntry->ColorBlendState.pAttachments, GraphicsEntry->Attachments, sizeof(VkPipelineColorBlendAttachmentState)*GraphicsEntry->ColorBlendState.attachmentCount);
                GraphicsEntry->ColorBlendState.pAttachments = GraphicsEntry->Attachments;
            }
            
            GraphicsEntry->DynamicStateCreateInfo = *PipelineCreateInfo->pDynamicState;
            if (GraphicsEntry->DynamicStateCreateInfo.dynamicStateCount > 0)
            {
                GraphicsEntry->DynamicStates = PushArray(&Manager->Arena, VkDynamicState, GraphicsEntry->DynamicStateCreateInfo.dynamicStateCount);
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

        // NOTE: Store references to our shaders in managers arena
        VkShaderModule ShaderModules[VK_MAX_PIPELINE_STAGES] = {};
        VkPipelineShaderStageCreateInfo ShaderStages[VK_MAX_PIPELINE_STAGES] = {};
        for (u32 ShaderId = 0; ShaderId < NumShaders; ++ShaderId)
        {
            VkPipelineAddShaderRef(Manager, Entry, Shaders[ShaderId]);
            ShaderModules[ShaderId] = VkPipelineGetShaderModule(Device, TempArena, Entry->ShaderRefs + ShaderId);
            ShaderStages[ShaderId] = VkPipelineShaderStage(Shaders[ShaderId].Stage, ShaderModules[ShaderId], Shaders[ShaderId].MainName);
        }
        VkCheckResult(vkCreatePipelineLayout(Device, LayoutCreateInfo, 0, &Entry->Pipeline.Layout));

        // NOTE: Create pipeline (patch up some values in the create info)
        GraphicsEntry->PipelineCreateInfo.pStages = ShaderStages;
        GraphicsEntry->PipelineCreateInfo.stageCount = NumShaders;
        GraphicsEntry->PipelineCreateInfo.layout = Entry->Pipeline.Layout;
        VkCheckResult(vkCreateGraphicsPipelines(Device, VK_NULL_HANDLE, 1, &GraphicsEntry->PipelineCreateInfo, 0, &Entry->Pipeline.Handle));

        for (u32 ShaderId = 0; ShaderId < NumShaders; ++ShaderId)
        {
            vkDestroyShaderModule(Device, ShaderModules[ShaderId], 0);
        }
    }
    
    return &Entry->Pipeline;
}

inline void VkPipelineUpdateShaders(VkDevice Device, linear_arena* TempArena, vk_pipeline_manager* Manager)
{
    for (u32 PipelineId = 0; PipelineId < Manager->NumPipelines; ++PipelineId)
    {
        vk_pipeline_entry* Entry = Manager->PipelineArray + PipelineId;

        b32 ReCreatePSO = false;

        HANDLE FileHandles[VK_MAX_PIPELINE_STAGES] = {};
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

                    // NOTE: Generate shader create infos
                    VkShaderModule ShaderModules[VK_MAX_PIPELINE_STAGES] = {};
                    VkPipelineShaderStageCreateInfo ShaderStages[VK_MAX_PIPELINE_STAGES] = {};
                    for (u32 ShaderId = 0; ShaderId < Entry->NumShaders; ++ShaderId)
                    {
                        vk_shader_ref* ShaderRef = Entry->ShaderRefs + ShaderId;
                        ShaderModules[ShaderId] = VkPipelineGetShaderModule(Device, FileHandles[ShaderId], TempArena, ShaderRef);
                        ShaderStages[ShaderId] = VkPipelineShaderStage(ShaderRef->Stage, ShaderModules[ShaderId], ShaderRef->MainName);
                    }

                    VkGraphicsPipelineCreateInfo PipelineCreateInfo = GraphicsEntry->PipelineCreateInfo;
                    PipelineCreateInfo.stageCount = Entry->NumShaders;
                    PipelineCreateInfo.pStages = ShaderStages;
                    VkCheckResult(vkCreateGraphicsPipelines(Device, VK_NULL_HANDLE, 1, &PipelineCreateInfo, 0, &Entry->Pipeline.Handle));

                    for (u32 ShaderId = 0; ShaderId < Entry->NumShaders; ++ShaderId)
                    {
                        vkDestroyShaderModule(Device, ShaderModules[ShaderId], 0);
                    }
                } break;

                case VkPipelineEntry_Compute:
                {
                    vk_pipeline_compute_entry* ComputeEntry = &Entry->ComputeEntry;

                    vk_shader_ref* ShaderRef = Entry->ShaderRefs + 0;
                    VkShaderModule ShaderModule = VkPipelineGetShaderModule(Device, FileHandles[0], TempArena, ShaderRef);
                    VkPipelineShaderStageCreateInfo ShaderStageCreateInfo = VkPipelineShaderStage(ShaderRef->Stage, ShaderModule, ShaderRef->MainName);

                    VkComputePipelineCreateInfo PipelineCreateInfo = ComputeEntry->PipelineCreateInfo;
                    PipelineCreateInfo.stage = ShaderStageCreateInfo;
                    VkCheckResult(vkCreateComputePipelines(Device, VK_NULL_HANDLE, 1, &PipelineCreateInfo, 0, &Entry->Pipeline.Handle));

                    vkDestroyShaderModule(Device, ShaderModule, 0);
                } break;

                default:
                {
                    InvalidCodePath;
                } break;
            }
        }

        for (u32 ShaderId = 0; ShaderId < Entry->NumShaders; ++ShaderId)
        {
            CloseHandle(FileHandles[ShaderId]);
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

    // TODO: This should be specified but make more pipelines and see how to break it up
    // NOTE: Specify rasterization flags
    Result.RasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    Result.RasterizationState.depthClampEnable = VK_FALSE;
    Result.RasterizationState.rasterizerDiscardEnable = VK_FALSE;
    Result.RasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    Result.RasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    Result.RasterizationState.depthBiasEnable = VK_FALSE;
    Result.RasterizationState.depthBiasConstantFactor = 0.0f;
    Result.RasterizationState.depthBiasClamp = 0.0f;
    Result.RasterizationState.depthBiasSlopeFactor = 0.0f;
    Result.RasterizationState.lineWidth = 1.0f;
    
    // NOTE: Set the multi sampling state
    Result.MultiSampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    Result.MultiSampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    Result.MultiSampleState.sampleShadingEnable = VK_FALSE;
    Result.MultiSampleState.minSampleShading = 1.0f;
    Result.MultiSampleState.pSampleMask = 0;
    Result.MultiSampleState.alphaToCoverageEnable = VK_FALSE;
    Result.MultiSampleState.alphaToOneEnable = VK_FALSE;

    // NOTE: Set some default values
    VkPipelineInputAssemblyAdd(&Result, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);
    
    return Result;
}

inline void VkPipelineShaderAdd(vk_pipeline_builder* Builder, char* FileName, char* MainName, VkShaderStageFlagBits Stage)
{
    Assert(Builder->NumShaders < ArrayCount(Builder->Shaders));
    vk_pipeline_builder_shader* Shader = Builder->Shaders + Builder->NumShaders++;
    Shader->FileName = FileName;
    Shader->MainName = MainName;
    Shader->Stage = Stage;
}

inline void VkPipelineVertexBindingBegin(vk_pipeline_builder* Builder)
{
    Assert(Builder->NumVertexBindings < Builder->MaxNumVertexBindings);
    
    VkVertexInputBindingDescription* VertexBinding = Builder->VertexBindings + Builder->NumVertexBindings;
    VertexBinding->binding = Builder->NumVertexBindings++;
    VertexBinding->inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    Builder->CurrVertexBindingSize = 0;
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

inline void VkPipelineVertexAttributeAddOffset(vk_pipeline_builder* Builder, u32 Offset)
{
    Builder->CurrVertexBindingSize += Offset;
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

inline void VkPipelineDepthOffsetAdd(vk_pipeline_builder* Builder, f32 ConstantFactor, f32 Clamp, f32 SlopeFactor)
{
    Assert((Builder->Flags & VkPipelineFlag_HasDepthStencil) != 0);
    Builder->RasterizationState.depthBiasEnable = VK_TRUE;
    Builder->RasterizationState.depthBiasConstantFactor = ConstantFactor;
    Builder->RasterizationState.depthBiasClamp = Clamp;
    Builder->RasterizationState.depthBiasSlopeFactor = SlopeFactor;
}

inline void VkPipelineStencilStateAdd(vk_pipeline_builder* Builder, VkStencilOpState Front, VkStencilOpState Back)
{
    Builder->Flags |= VkPipelineFlag_HasDepthStencil;

    Builder->DepthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    Builder->DepthStencil.stencilTestEnable = VK_TRUE;
    Builder->DepthStencil.front = Front;
    Builder->DepthStencil.back = Back;
}

inline void VkPipelineColorAttachmentAdd(vk_pipeline_builder* Builder, VkBlendOp ColorBlend, VkBlendFactor SrcColor, VkBlendFactor DstColor,
                                         VkBlendOp AlphaBlend, VkBlendFactor SrcAlpha, VkBlendFactor DstAlpha,
                                         VkColorComponentFlags WriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)
{
    Assert(Builder->NumColorAttachments < Builder->MaxNumColorAttachments);

    VkPipelineColorBlendAttachmentState* ColorAttachment = Builder->ColorAttachments + Builder->NumColorAttachments++;
    ColorAttachment->blendEnable = VK_TRUE;
    ColorAttachment->srcColorBlendFactor = SrcColor;
    ColorAttachment->dstColorBlendFactor = DstColor;
    ColorAttachment->colorBlendOp = ColorBlend;
    ColorAttachment->srcAlphaBlendFactor = SrcAlpha;
    ColorAttachment->dstAlphaBlendFactor = DstAlpha;
    ColorAttachment->alphaBlendOp = AlphaBlend;
    ColorAttachment->colorWriteMask = WriteMask;
}

inline void VkPipelineRasterizationStateSet(vk_pipeline_builder* Builder, VkBool32 RasterizerDiscardEnable, VkPolygonMode PolygonMode,
                                            VkCullModeFlags CullMode, VkFrontFace FrontFace)
{
    Builder->RasterizationState.rasterizerDiscardEnable = RasterizerDiscardEnable;
    Builder->RasterizationState.polygonMode = PolygonMode;
    Builder->RasterizationState.cullMode = CullMode;
    Builder->RasterizationState.frontFace = FrontFace;
}

inline void VkPipelineMsaaStateSet(vk_pipeline_builder* Builder, VkSampleCountFlagBits SampleCount, VkBool32 SampleShadingEnabled)
{
    // NOTE: Set the multi sampling state
    Builder->MultiSampleState.rasterizationSamples = SampleCount;
    Builder->MultiSampleState.sampleShadingEnable = SampleShadingEnabled;
    Builder->MultiSampleState.minSampleShading = 1.0f;
    Builder->MultiSampleState.pSampleMask = 0;
    Builder->MultiSampleState.alphaToCoverageEnable = VK_FALSE;
    Builder->MultiSampleState.alphaToOneEnable = VK_FALSE;
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
    PipelineCreateInfo.pRasterizationState = &Builder->RasterizationState;
    if (Builder->Flags & VkPipelineFlag_HasDepthStencil)
    {
        PipelineCreateInfo.pDepthStencilState = &Builder->DepthStencil;
    }
    PipelineCreateInfo.pMultisampleState = &Builder->MultiSampleState;
    PipelineCreateInfo.pColorBlendState = &ColorBlendStateCreateInfo;
    PipelineCreateInfo.pDynamicState = &DynamicStateCreateInfo;
    PipelineCreateInfo.renderPass = RenderPass;
    PipelineCreateInfo.subpass = SubPassId;
    PipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    PipelineCreateInfo.basePipelineIndex = -1;

    Result = VkPipelineGraphicsCreate(Device, Manager, Builder->Arena, Builder->Shaders, Builder->NumShaders, &LayoutCreateInfo,
                                      &PipelineCreateInfo);
    
    EndTempMem(Builder->TempMem);

    return Result;
}
