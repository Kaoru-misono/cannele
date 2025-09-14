#include "vk_RHI.hpp"
#include "vk_tool.hpp"
#include "vk_shader.hpp"

#include <ranges>

namespace cannele::inline graphics::rhi::vk
{
    inline namespace
    {
        static auto const dynamic_states = std::vector<VkDynamicState>{
            VK_DYNAMIC_STATE_VERTEX_INPUT_EXT,          // VkPipelineVertexInputStateCreateInfo
            VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT, // VkPipelineMultisampleStateCreateInfo::rasterizationSamples
            VK_DYNAMIC_STATE_DEPTH_CLAMP_ENABLE_EXT,    // VkPipelineRasterizationStateCreateInfo::depthClampEnable
            VK_DYNAMIC_STATE_POLYGON_MODE_EXT,          // VkPipelineRasterizationStateCreateInfo::polygonMode
            VK_DYNAMIC_STATE_CULL_MODE,                 // VkPipelineRasterizationStateCreateInfo::cullMode
            VK_DYNAMIC_STATE_FRONT_FACE,                // VkPipelineRasterizationStateCreateInfo::frontFace
            VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE,         // VkPipelineRasterizationStateCreateInfo::depthBiasEnable
            VK_DYNAMIC_STATE_DEPTH_BIAS,                // VkPipelineRasterizationStateCreateInfo::depthBias
            VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,       // VkPipelineViewportStateCreateInfo::viewportCount, pViewports
            VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,        // VkPipelineViewportStateCreateInfo::scissorCount, pScissors
            VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,         // VkPipelineDepthStencilStateCreateInfo::depthTestEnable
            VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,        // VkPipelineDepthStencilStateCreateInfo::depthWriteEnable
            VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,          // VkPipelineDepthStencilStateCreateInfo::depthCompareOp
            VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE,  // VkPipelineDepthStencilStateCreateInfo::depthBoundsTestEnable
            VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE,       // VkPipelineDepthStencilStateCreateInfo::stencilTestEnable
            VK_DYNAMIC_STATE_STENCIL_OP,                // VkPipelineDepthStencilStateCreateInfo::front, VkPipelineDepthStencilStateCreateInfo::back
            VK_DYNAMIC_STATE_DEPTH_BOUNDS,              // VkPipelineDepthStencilStateCreateInfo::minDepthBounds, VkPipelineDepthStencilStateCreateInfo::maxDepthBounds
            VK_DYNAMIC_STATE_LOGIC_OP_ENABLE_EXT,
            VK_DYNAMIC_STATE_LOGIC_OP_EXT,
            VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT,    // VkPipelineColorBlendStateCreateInfo::logicOpEnable
            VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT,  // VkPipelineColorBlendStateCreateInfo::logicOp
            VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT,      // VkPipelineColorBlendStateCreateInfo::colorWriteMask
            VK_DYNAMIC_STATE_BLEND_CONSTANTS,           // VkPipelineColorBlendStateCreateInfo::blendConstants
        };

        struct PipelineCacheHeader final
        {
            static constexpr auto k_magic_number = 0x4E4B4E46;
            static constexpr auto k_version = 1;

            uint32_t magic_number{};
            uint32_t version{};
            uint32_t binary_count{};
        };

        struct PipelineCacheBinaryHeader final
        {
            uint32_t key_size{};
            uint8_t key[VK_MAX_PIPELINE_BINARY_KEY_SIZE_KHR];
            uint32_t data_size{};
            uint32_t data_offset{};
        };

        auto get_pipeline_cache_key(VulkanDevice* device, void* create_info) -> size_t
        {
            struct Hasher final
            {
                size_t seed{};

                auto update(uint8_t* data, uint32_t size) -> void
                {
                    seed = XXH64(data, size, seed);
                }
            };

            auto hasher = Hasher{};

            {
                auto pipeline_key = VkPipelineBinaryKeyKHR{VK_STRUCTURE_TYPE_PIPELINE_BINARY_KEY_KHR};
                CHECK_VK_RESULT(vkGetPipelineKeyKHR(device->device, nullptr, &pipeline_key));

                hasher.update(pipeline_key.key, pipeline_key.keySize);
            }

            {
                auto pipeline_create_info = VkPipelineCreateInfoKHR{VK_STRUCTURE_TYPE_PIPELINE_CREATE_INFO_KHR};
                pipeline_create_info.pNext = create_info;
                auto pipeline_key = VkPipelineBinaryKeyKHR{VK_STRUCTURE_TYPE_PIPELINE_BINARY_KEY_KHR};
                CHECK_VK_RESULT(vkGetPipelineKeyKHR(device->device, &pipeline_create_info, &pipeline_key));

                hasher.update(pipeline_key.key, pipeline_key.keySize);
            }

            return hasher.seed;
        }

        auto serialize_pipeline_binaries(VulkanDevice* device, VkPipeline pipeline) -> std::vector<std::byte>
        {
            auto binary_create_info = VkPipelineBinaryCreateInfoKHR{VK_STRUCTURE_TYPE_PIPELINE_BINARY_CREATE_INFO_KHR};
            binary_create_info.pipeline = pipeline;
            auto binary_handles_info = VkPipelineBinaryHandlesInfoKHR{VK_STRUCTURE_TYPE_PIPELINE_BINARY_HANDLES_INFO_KHR};
            CHECK_VK_RESULT(vkCreatePipelineBinariesKHR(device->device, &binary_create_info, nullptr, &binary_handles_info));

            auto pipeline_binaries = std::vector<VkPipelineBinaryKHR>{binary_handles_info.pipelineBinaryCount, VK_NULL_HANDLE};
            binary_handles_info.pPipelineBinaries = pipeline_binaries.data();
            CHECK_VK_RESULT(vkCreatePipelineBinariesKHR(device->device, &binary_create_info,nullptr, &binary_handles_info));

            auto data_size = sizeof(PipelineCacheHeader);
            data_size += binary_handles_info.pipelineBinaryCount * sizeof(PipelineCacheBinaryHeader);
            for (auto i = 0u; i < binary_handles_info.pipelineBinaryCount; i++) {
                auto binary_data_info = VkPipelineBinaryDataInfoKHR{VK_STRUCTURE_TYPE_PIPELINE_BINARY_DATA_INFO_KHR};
                binary_data_info.pipelineBinary = pipeline_binaries[i];
                auto binary_key = VkPipelineBinaryKeyKHR{VK_STRUCTURE_TYPE_PIPELINE_BINARY_KEY_KHR};
                auto binary_data_size = 0zu;
                CHECK_VK_RESULT(vkGetPipelineBinaryDataKHR(device->device, &binary_data_info, &binary_key, &binary_data_size, nullptr));
                data_size += binary_data_size;
            }

            auto data = std::vector<std::byte>{};
            data.resize(data_size);

            auto data_pointer = data.data();

            auto header = reinterpret_cast<PipelineCacheHeader*>(data_pointer);
            header->magic_number = PipelineCacheHeader::k_magic_number;
            header->version = PipelineCacheHeader::k_version;
            header->binary_count = binary_handles_info.pipelineBinaryCount;
            data_pointer += sizeof(PipelineCacheHeader);

            auto binary_data_offset = sizeof(PipelineCacheHeader) + binary_handles_info.pipelineBinaryCount * sizeof(PipelineCacheBinaryHeader);
            for (auto i = 0u; i < binary_handles_info.pipelineBinaryCount; i++) {
                auto binary_data_info = VkPipelineBinaryDataInfoKHR{VK_STRUCTURE_TYPE_PIPELINE_BINARY_DATA_INFO_KHR};
                binary_data_info.pipelineBinary = pipeline_binaries[i];
                auto binary_key = VkPipelineBinaryKeyKHR{VK_STRUCTURE_TYPE_PIPELINE_BINARY_KEY_KHR};
                auto binary_data_size = 0zu;
                CHECK_VK_RESULT(vkGetPipelineBinaryDataKHR(device->device, &binary_data_info, &binary_key, &binary_data_size, nullptr));

                CHECK_VK_RESULT(vkGetPipelineBinaryDataKHR(device->device, &binary_data_info, &binary_key, &binary_data_size, data.data() + binary_data_offset));

                auto binary_header = reinterpret_cast<PipelineCacheBinaryHeader*>(data_pointer);
                std::memset(binary_header->key, 0, sizeof(PipelineCacheBinaryHeader::key));
                std::memcpy(binary_header->key, binary_key.key, binary_key.keySize);
                binary_header->key_size = binary_key.keySize;
                binary_header->data_size = binary_data_size;
                binary_header->data_offset = binary_data_offset;

                data_pointer += sizeof(PipelineCacheBinaryHeader);
                binary_data_offset += binary_data_size;

                vkDestroyPipelineBinaryKHR(device->device, pipeline_binaries[i], device->allocation_callbacks);
            }

            return data;
        }

        auto deserialize_pipeline_binaries(VulkanDevice* device, std::span<std::byte const> data) -> std::vector<VkPipelineBinaryKHR>
        {
            auto data_size = data.size();
            auto data_pointer = data.data();
            CNE_ASSERT(data_size > sizeof(PipelineCacheHeader));

            auto header = reinterpret_cast<PipelineCacheHeader const*>(data_pointer);
            CNE_ASSERT(true
                && header->magic_number == PipelineCacheHeader::k_magic_number
                && header->version == PipelineCacheHeader::k_version
                && header->binary_count > 0
            );
            data_pointer += sizeof(PipelineCacheHeader);

            auto binary_keys = std::vector<VkPipelineBinaryKeyKHR>{header->binary_count, {VK_STRUCTURE_TYPE_PIPELINE_BINARY_KEY_KHR}};
            auto pipeline_datas = std::vector<VkPipelineBinaryDataKHR>{header->binary_count, {}};

            for (auto i = 0u; i < header->binary_count; i++) {
                auto binary_header = reinterpret_cast<PipelineCacheBinaryHeader const*>(data_pointer);
                data_pointer += sizeof(PipelineCacheBinaryHeader);

                binary_keys[i].keySize = binary_header->key_size;
                std::memcpy(binary_keys[i].key, binary_header->key, binary_header->key_size);

                pipeline_datas[i].dataSize = binary_header->data_size;
                pipeline_datas[i].pData = (void*) (data.data() + binary_header->data_offset);
            }

            auto binary_keys_and_data = VkPipelineBinaryKeysAndDataKHR{};
            binary_keys_and_data.binaryCount         = header->binary_count;
            binary_keys_and_data.pPipelineBinaryKeys = binary_keys.data();
            binary_keys_and_data.pPipelineBinaryData = pipeline_datas.data();

            auto binary_create_info = VkPipelineBinaryCreateInfoKHR{VK_STRUCTURE_TYPE_PIPELINE_BINARY_CREATE_INFO_KHR};
            binary_create_info.pKeysAndDataInfo = &binary_keys_and_data;

            auto binaries = std::vector<VkPipelineBinaryKHR>{header->binary_count, VK_NULL_HANDLE};

            auto handles_info = VkPipelineBinaryHandlesInfoKHR{VK_STRUCTURE_TYPE_PIPELINE_BINARY_HANDLES_INFO_KHR};
            handles_info.pipelineBinaryCount = binaries.size();
            handles_info.pPipelineBinaries   = binaries.data();

            CHECK_VK_RESULT(vkCreatePipelineBinariesKHR(device->device, &binary_create_info, nullptr, &handles_info));

            return binaries;
        }

        using CacheResultAndSize = std::pair<bool, size_t>;
        template <typename PipelineCreateInfo>
        auto create_pipeline_with_cache(
            VulkanDevice* device,
            PipelineCreateInfo* create_info,
            std::function<auto (VulkanDevice*, PipelineCreateInfo*, VkPipeline*) -> VkResult> create_func,
            VkPipeline* out_pipeline
        ) -> CacheResultAndSize // {cached, cache_size}
        {
            auto write_cache = true;
            auto cached = false;
            auto cache_size = 0u;
            auto pipeline = VkPipeline{VK_NULL_HANDLE};

            auto key = get_pipeline_cache_key(device, create_info);
            auto it = device->pipeline_cache.find(key);
            if (it != device->pipeline_cache.end()) {
                auto data = std::span<std::byte const>{it->second};
                if (auto binaries = deserialize_pipeline_binaries(device, data); !binaries.empty()) {
                    auto binary_info = VkPipelineBinaryInfoKHR{VK_STRUCTURE_TYPE_PIPELINE_BINARY_INFO_KHR};
                    binary_info.binaryCount       = binaries.size();
                    binary_info.pPipelineBinaries = binaries.data();
                    binary_info.pNext             = create_info->pNext;
                    create_info->pNext            = &binary_info;
                    if (create_func(device, create_info, &pipeline) == VK_SUCCESS) {
                        write_cache = false;
                        cached = true;
                        cache_size = data.size();
                    } else {
                        create_info->pNext = binary_info.pNext;
                        pipeline = VK_NULL_HANDLE;
                    }

                    for (auto& binary : binaries) {
                        vkDestroyPipelineBinaryKHR(device->device, binary, device->allocation_callbacks);
                    }
                }
            }

            if (!pipeline) {
                auto create_flag = VkPipelineCreateFlags2CreateInfo{VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR};
                if (write_cache) {
                    // Check createInfo chain for existing VkPipelineCreateFlags2CreateInfoKHR
                    bool create_flags_existed = false;
                    VkBaseInStructure* in_struct = (VkBaseInStructure*)create_info->pNext;
                    while (in_struct) {
                        if (in_struct->sType == VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR) {
                            ((VkPipelineCreateFlags2CreateInfo*) in_struct)->flags |=
                                VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR;
                            create_flags_existed = true;
                            break;
                        }
                        in_struct = (VkBaseInStructure*)in_struct->pNext;
                    }
                    // If not found, append VkPipelineCreateFlags2CreateInfoKHR on stack
                    if (!create_flags_existed)
                    {
                        create_flag.flags = VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR;
                        create_flag.pNext = create_info->pNext;
                        create_info->pNext = &create_flag;
                    }
                }
                CHECK_VK_RESULT(create_func(device, create_info, &pipeline));
            }

            if (write_cache) {
                if (auto pipeline_cache_data = serialize_pipeline_binaries(device, pipeline); !pipeline_cache_data.empty()) {
                    device->pipeline_cache.emplace(key, std::move(pipeline_cache_data));
                    cache_size = pipeline_cache_data.size();
                } else {

                }

                auto release_info = VkReleaseCapturedPipelineDataInfoKHR{VK_STRUCTURE_TYPE_RELEASE_CAPTURED_PIPELINE_DATA_INFO_KHR};
                release_info.pipeline = pipeline;
                CHECK_VK_RESULT(vkReleaseCapturedPipelineDataKHR(device->device, &release_info, nullptr));
            }

            *out_pipeline = pipeline;

            return {cached, cache_size};
        }
    }

    auto VulkanDevice::create_graphics_pipeline(std::string_view name, GraphicsPipelineCreateInfo const* info) -> GraphicsPipelineHandle
    {
        auto time = std::chrono::high_resolution_clock::now();
        auto hash = core::hash(
            info->program->id,
            info->dynamic_blend_states,
            info->dynamic_depth_state,
            info->dynamic_input_state,
            info->dynamic_stencil_state,
            XXH64(&info->depth_stencil, sizeof(DepthStencilAttachmentInfo), 0),
            XXH64(info->colors.data(), info->colors.size() * sizeof(ColorAttachmentInfo), 0)
        );

        if (auto it = graphics_pipelines.find(hash); it != graphics_pipelines.end()) {
            auto pipeline = it->second;
            CNE_ASSERT(pipeline);

            return pipeline;
        }

        std::lock_guard<std::mutex> lock(mutex);

        auto pipeline = std::make_shared<VulkanGraphicsPipeline>(this, info);
        graphics_pipelines.emplace(hash, pipeline);

        return pipeline;
    }

    VulkanGraphicsPipeline::VulkanGraphicsPipeline(VulkanDevice* device, GraphicsPipelineCreateInfo const* in_info)
        : RHIGraphicsPipeline(device)
        , info(*in_info)
    {
        auto program = pointer_cast<VulkanShaderProgram>(info.program);
        CNE_ASSERT(!program->modules.empty());

        // TODO: We encode the primitiveRestartEnable to false, implement it later.
        auto input_assembly_state_ci = VkPipelineInputAssemblyStateCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        input_assembly_state_ci.topology               = to_vk_primitive_topology(info.topology);
        input_assembly_state_ci.primitiveRestartEnable = VK_FALSE;

        auto rasterization_ci = VkPipelineRasterizationStateCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rasterization_ci.depthClampEnable        = VK_FALSE;
        rasterization_ci.rasterizerDiscardEnable = VK_FALSE;
        rasterization_ci.lineWidth               = 1.0f;

        // TODO: Enable samples.
        auto multisample_state_ci = VkPipelineMultisampleStateCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisample_state_ci.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
        multisample_state_ci.sampleShadingEnable   = VK_FALSE;
        multisample_state_ci.minSampleShading      = 0.0f;
        multisample_state_ci.pSampleMask           = nullptr;
        multisample_state_ci.alphaToCoverageEnable = VK_FALSE;
        multisample_state_ci.alphaToOneEnable      = VK_FALSE;

        // TODO: Open dynamic states by info.dynamic_state.
        auto dynamic_state_ci = VkPipelineDynamicStateCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        auto pending_dynamic_states = std::span{dynamic_states};
        if (info.program->has_mesh_shader) {
            pending_dynamic_states = pending_dynamic_states.subspan(1, pending_dynamic_states.size() - 1);
            info.dynamic_input_state = false;
        }
        dynamic_state_ci.dynamicStateCount = (uint32_t) pending_dynamic_states.size();
        dynamic_state_ci.pDynamicStates    = pending_dynamic_states.data();

        auto vk_color_formats = (
            info.colors |
            std::ranges::views::transform([] (auto const& color) {
                return to_vk_format(color.format);
            }) |
            std::ranges::to<std::vector>()
        );
        auto depth_stencil_format = to_vk_format(info.depth_stencil.format);

        auto pipeline_rendering_ci = VkPipelineRenderingCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
        pipeline_rendering_ci.viewMask                = 0;
        pipeline_rendering_ci.colorAttachmentCount    = (uint32_t) vk_color_formats.size();
        pipeline_rendering_ci.pColorAttachmentFormats = vk_color_formats.data();
        pipeline_rendering_ci.depthAttachmentFormat   = depth_stencil_format;
        pipeline_rendering_ci.stencilAttachmentFormat = VK_FORMAT_UNDEFINED; // TODO:

        auto create_flag = VkPipelineCreateFlags2CreateInfo{VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR};
        create_flag.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
        create_flag.pNext = &pipeline_rendering_ci;

        pipeline_layout = program->root_layout->pipeline_layout;
        auto pipeline_ci = VkGraphicsPipelineCreateInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipeline_ci.pNext               = &create_flag;
        pipeline_ci.stageCount          = program->shader_stages.size();
        pipeline_ci.pStages             = program->shader_stages.data();
        pipeline_ci.pInputAssemblyState = &input_assembly_state_ci;
        pipeline_ci.pRasterizationState = &rasterization_ci;
        pipeline_ci.pMultisampleState   = &multisample_state_ci;
        pipeline_ci.pDynamicState       = &dynamic_state_ci;
        pipeline_ci.layout              = pipeline_layout;

        auto parent = get_device<VulkanDevice>();
        // auto [cacahed, cache_size] = create_pipeline_with_cache<VkGraphicsPipelineCreateInfo>(
        //     parent,
        //     &pipeline_ci,
        //     [] (VulkanDevice* device, VkGraphicsPipelineCreateInfo* create_info, VkPipeline* pipeline) -> VkResult {
        //         return vkCreateGraphicsPipelines(device->device, VK_NULL_HANDLE, 1, create_info, device->allocation_callbacks, pipeline);
        //     },
        //     &pipeline
        // );
        CHECK_VK_RESULT(vkCreateGraphicsPipelines(device->device, VK_NULL_HANDLE, 1, &pipeline_ci, device->allocation_callbacks, &pipeline));
        CNE_ASSERT(pipeline != VK_NULL_HANDLE);
        // CNE_TRACE("cached: {}, size: {}", cacahed, cache_size);
    }

    VulkanGraphicsPipeline::~VulkanGraphicsPipeline()
    {
        auto parent = get_device<VulkanDevice>();
        if (pipeline) {
            vkDestroyPipeline(parent->device, pipeline, parent->allocation_callbacks);
        }
    }

    auto VulkanGraphicsPipeline::program() const -> RHIShaderProgram const*
    {
        return info.program.get();
    }

    auto VulkanDevice::create_compute_pipeline(std::string_view name, ComputePipelineCreateInfo const* info) -> ComputePipelineHandle
    {
        auto hash = core::hash(
            info->program->id
        );

        if (auto it = compute_pipelines.find(hash); it != compute_pipelines.end()) {
            auto pipeline = it->second;
            CNE_ASSERT(pipeline);

            return pipeline;
        }

        std::lock_guard<std::mutex> lock(mutex);

        auto pipeline = std::make_shared<VulkanComputePipeline>(this, info);
        compute_pipelines.emplace(hash, pipeline);

        return pipeline;
    }

    VulkanComputePipeline::VulkanComputePipeline(VulkanDevice* device, ComputePipelineCreateInfo const* in_info)
        : RHIComputePipeline(device)
        , info(*in_info)
    {
        auto program = pointer_cast<VulkanShaderProgram>(in_info->program);
        CNE_ASSERT(program->modules.size() == 1);

        pipeline_layout = program->root_layout->pipeline_layout;
        auto pipeline_ci = VkComputePipelineCreateInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pipeline_ci.flags  = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
        pipeline_ci.stage  = program->shader_stages[0];
        pipeline_ci.layout = program->root_layout->pipeline_layout;

        auto parent = get_device<VulkanDevice>();
        auto result = vkCreateComputePipelines(device->device, VK_NULL_HANDLE, 1, &pipeline_ci, parent->allocation_callbacks, &pipeline);
        CNE_ASSERT_WITH(result == VK_SUCCESS, std::format("Failed to create compute pipeline: {}", vk_error_to_string(result)));
    }

    VulkanComputePipeline::~VulkanComputePipeline()
    {
        auto parent = get_device<VulkanDevice>();
        if (pipeline) {
            vkDestroyPipeline(parent->device, pipeline, parent->allocation_callbacks);
        }
    }

    auto VulkanComputePipeline::program() const -> RHIShaderProgram const*
    {
        return info.program.get();
    }
}
