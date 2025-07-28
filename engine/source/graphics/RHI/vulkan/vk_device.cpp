#include "vk_RHI.hpp"
#include "vk_tool.hpp"
#include "../vulkan.hpp"

namespace cannele::inline graphics::rhi
{
    auto create_device(VulkanDeviceCreateInfo* info) -> RefCountPtr<IVulkanDevice>
    {
        // return make_ref_count<vk::VulkanDevice>(info);
        return std::make_shared<vk::VulkanDevice>(info);
    }
}

namespace cannele::inline graphics::rhi::vk
{
    inline namespace
    {
        static auto debug_utils_level = 3;
        static auto exist_debug_utils_error = false;
        static auto enable_message_id_print = false;

        VKAPI_ATTR auto VKAPI_CALL debug_utils_callback(
            VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
            VkDebugUtilsMessageTypeFlagsEXT message_type,
            VkDebugUtilsMessengerCallbackDataEXT const* in_callback_data,
            void* in_user_data
        ) -> VkBool32 {
            auto verbose = (debug_utils_level >= 4) && (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT);
            auto info    = (debug_utils_level >= 3) && (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT);
            auto warning = (debug_utils_level >= 2) && (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT);
            auto error   = (debug_utils_level >= 1) && (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT);

            if (message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
                auto message = std::string{in_callback_data->pMessage};
                if (enable_message_id_print) {
                    message += std::format(" Message ID: {:#x}", in_callback_data->messageIdNumber);
                }
                // std::replace(message.begin(), message.end(), '|', '\n'); // Replace | with newline for better readability.
                exist_debug_utils_error = false;
                using Color = Style::EColor;
                using Format = Style::EFormat;

                if (verbose) {
                    LogSystem::instance()->get_logger("Cannele")->log_stream(
                        Style{Color::white, Color::not_set, Format::bold | Format::underline},
                        "[Validation Verbose]:\n",
                        Style{Color::magenta},
                        message
                    );
                }
                else if (info) {
                    LogSystem::instance()->get_logger("Cannele")->log_stream(
                        Style{Color::green, Color::not_set, Format::bold | Format::underline},
                        "[Validation Info]:\n",
                        Style{Color::magenta},
                        message
                    );
                }
                else if (warning) {
                    LogSystem::instance()->get_logger("Cannele")->log_stream(
                        Style{Color::yellow, Color::not_set, Format::bold | Format::underline},
                        "[Validation Warning]:\n",
                        Style{Color::magenta},
                        message
                    );
                }
                else if (error) {
                    LogSystem::instance()->get_logger("Cannele")->log_stream(
                        Style{Color::red, Color::not_set, Format::bold | Format::underline},
                        "[Validation Error]:\n",
                        Style{Color::magenta},
                        message
                    );
                    exist_debug_utils_error = true;
                }
            }

            if (exist_debug_utils_error) {
                return VK_TRUE;
            }

            return VK_FALSE;
        }
    }

    VulkanDevice::VulkanDevice(VulkanDeviceCreateInfo* info)
        : device_info(*info)
        , allocation_callbacks(info->allocation_callbacks)
    {
        // Volk initialize.
        auto result_volk_init = volkInitialize();
        CNE_ASSERT_WITH(result_volk_init == VK_SUCCESS, std::format("Failed to initialize volk. ERROR: {0}", vk_error_to_string(result_volk_init)));
        auto SDK_version = volkGetInstanceVersion();
        CNE_INFO("Volk initialized with vulkan SDK version {0}.{1}.{2}", VK_VERSION_MAJOR(SDK_version), VK_VERSION_MINOR(SDK_version), VK_VERSION_PATCH(SDK_version));

        auto find_and_enable_if_exist = [](bool* in_enable, std::string_view name, auto member_ptr, auto* availables, std::vector<char const*>* out_enables) {
            auto enable = in_enable ? *in_enable : true;
            if (!enable) return;

            for (auto& available : *availables) {
                if (name == available.*member_ptr) {
                    enable = true;
                    out_enables->emplace_back(name.data());
                }
            }
            if (!enable) {
                CNE_ERROR("'{}' is not found and the feature using it will be disabled.", name);
            }
            if (in_enable) {
                *in_enable = enable;
            }
        };

        auto start_time = std::chrono::high_resolution_clock::time_point{};
        auto duration = int64_t(0);
        auto total_time = int64_t(0);

        CNE_INFO("Creating Vulkan instance...");
        start_time = std::chrono::high_resolution_clock::now();
        {
            VkApplicationInfo app_info = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
            app_info.pApplicationName   = "Simple Application";
            app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
            app_info.pEngineName        = "Cannele";
            app_info.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
            app_info.apiVersion         = SDK_version;

            auto enabled_layers = std::vector<char const*>{};
            {
                uint32_t num_extension_layers;
                vkEnumerateInstanceLayerProperties(&num_extension_layers, nullptr);
                auto available_layers = std::vector<VkLayerProperties>(num_extension_layers);
                vkEnumerateInstanceLayerProperties(&num_extension_layers, available_layers.data());

                find_and_enable_if_exist(&device_info.enable_validation, "VK_LAYER_KHRONOS_validation", &VkLayerProperties::layerName, &available_layers, &enabled_layers);
            }

            auto enabled_extensions = std::vector<char const*>{};
            {
                uint32_t num_extensions_props;
                vkEnumerateInstanceExtensionProperties(nullptr, &num_extensions_props, nullptr);
                auto available_extensions = std::vector<VkExtensionProperties>(num_extensions_props);
                vkEnumerateInstanceExtensionProperties(nullptr, &num_extensions_props, available_extensions.data());

                find_and_enable_if_exist(&device_info.enable_validation, "VK_EXT_debug_utils", &VkExtensionProperties::extensionName, &available_extensions, &enabled_extensions);
                find_and_enable_if_exist(nullptr, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, &VkExtensionProperties::extensionName, &available_extensions, &enabled_extensions);
                find_and_enable_if_exist(nullptr, VK_KHR_SURFACE_EXTENSION_NAME, &VkExtensionProperties::extensionName, &available_extensions, &enabled_extensions);
                find_and_enable_if_exist(nullptr, VK_KHR_WIN32_SURFACE_EXTENSION_NAME, &VkExtensionProperties::extensionName, &available_extensions, &enabled_extensions);

                for (auto& extension : device_info.instance_extensions) {
                    find_and_enable_if_exist(nullptr, extension, &VkExtensionProperties::extensionName, &available_extensions, &enabled_extensions);
                }
            }

            auto instance_info = VkInstanceCreateInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};

            instance_info.pApplicationInfo        = &app_info;
            instance_info.enabledLayerCount       = (uint32_t) enabled_layers.size();
            instance_info.ppEnabledLayerNames     = enabled_layers.data();
            instance_info.enabledExtensionCount   = (uint32_t) enabled_extensions.size();
            instance_info.ppEnabledExtensionNames = enabled_extensions.data();

            auto enabled_validation_layers = std::vector<VkValidationFeatureEnableEXT>{};
            auto enabled_validation_layers_setting = std::vector<VkLayerSettingEXT>{};

            if (device_info.enable_validation && device_info.enable_debug_utils) {
                enabled_validation_layers.emplace_back(VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT);

                // https://vulkan.lunarg.com/doc/sdk/1.4.313.2/windows/khronos_validation_layer.html
                static auto setting_debug_action = std::vector<char const*>{"info", "warn", "error"};
                enabled_validation_layers_setting.emplace_back(
                    "VK_LAYER_KHRONOS_validation",
                    "report_flags",
                    VK_LAYER_SETTING_TYPE_STRING_EXT,
                    setting_debug_action.size(),
                    setting_debug_action.data()
                );

                static auto enable_message_limit = (VkBool32) false;
                enabled_validation_layers_setting.emplace_back(
                    "VK_LAYER_KHRONOS_validation",
                    "enable_message_limit",
                    VK_LAYER_SETTING_TYPE_BOOL32_EXT,
                    1,
                    &enable_message_limit
                );

                static auto max_duplicate_message = 10u;
                enabled_validation_layers_setting.emplace_back(
                    "VK_LAYER_KHRONOS_validation",
                    "duplicate_message_limit",
                    VK_LAYER_SETTING_TYPE_UINT32_EXT,
                    1,
                    &max_duplicate_message
                );
            }

            auto layer_setting_create_info = VkLayerSettingsCreateInfoEXT{VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT};
            layer_setting_create_info.settingCount = (uint32_t) enabled_validation_layers_setting.size();
            layer_setting_create_info.pSettings = enabled_validation_layers_setting.data();
            connect_to_next(&instance_info, &layer_setting_create_info);

            auto validation_features = VkValidationFeaturesEXT{VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
            validation_features.enabledValidationFeatureCount = (uint32_t) enabled_validation_layers.size();
            validation_features.pEnabledValidationFeatures = enabled_validation_layers.data();
            connect_to_next(&instance_info, &validation_features);

            auto debug_utils_create_info = VkDebugUtilsMessengerCreateInfoEXT{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
            if (device_info.enable_debug_utils) {
                debug_utils_create_info.messageType = 0;
                debug_utils_create_info.messageSeverity = 0;
                debug_utils_create_info.pfnUserCallback = debug_utils_callback;

                if (debug_utils_level >= 4) {
                    debug_utils_create_info.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
                }
                if (debug_utils_level >= 3) {
                    debug_utils_create_info.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
                }
                if (debug_utils_level >= 2) {
                    debug_utils_create_info.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
                }
                if (debug_utils_level >= 1) {
                    debug_utils_create_info.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
                }

                if (device_info.enable_validation) {
                    debug_utils_create_info.messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
                }

                connect_to_next(&instance_info, &debug_utils_create_info);
            }
            auto result_instance_create = vkCreateInstance(&instance_info, nullptr, &instance);
            CNE_ASSERT_WITH(result_instance_create == VK_SUCCESS, std::format("Failed to create Vulkan instance. ERROR: {0}", vk_error_to_string(result_instance_create)));

            // Load instance.
            volkLoadInstance(instance);

            if (device_info.enable_debug_utils) {
                auto result_debug_utils_create = vkCreateDebugUtilsMessengerEXT(instance, &debug_utils_create_info, nullptr, &debug_utils_messenger);
                CNE_ASSERT_WITH(result_debug_utils_create == VK_SUCCESS, std::format("Failed to create Vulkan debug utils. ERROR: {0}", vk_error_to_string(result_debug_utils_create)));
            }
        }
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_time).count();
        total_time += duration;
        CNE_INFO("Vulkan instance created in {} ms.", duration);

        CNE_INFO("Picking Vulkan Physical Device...");
        start_time = std::chrono::high_resolution_clock::now();
        {
            uint32_t num_physical_device;
            vkEnumeratePhysicalDevices(instance, &num_physical_device, nullptr);
            std::vector<VkPhysicalDevice> physical_devices(num_physical_device);
            vkEnumeratePhysicalDevices(instance, &num_physical_device, physical_devices.data());
            CNE_ASSERT_WITH(num_physical_device > 0, "No physical device found.");

            VkPhysicalDevice usable_device = VK_NULL_HANDLE;
            for (auto physical_device : physical_devices) {
                VkPhysicalDeviceProperties physical_props;
                vkGetPhysicalDeviceProperties(physical_device, &physical_props);
                if (physical_props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                    usable_device = physical_device;
                }
            }
            CNE_ASSERT_WITH(usable_device != VK_NULL_HANDLE, "No usable physical device found.");
            physical_device = usable_device;

            // Physical Device Properties.
            vkGetPhysicalDeviceMemoryProperties(physical_device, &physical_device_properties.memoryProperties);
            vkGetPhysicalDeviceProperties2(physical_device, &physical_device_properties.properties2);
            CNE_INFO("GPU: {}", physical_device_properties.properties2.properties.deviceName);

            // Physical Device Features.
            vkGetPhysicalDeviceFeatures2(physical_device, &physical_device_features.vk_features2);

            // Queue Infos.
            {
                auto num_graphics_queue = 0u;
                auto num_compute_queue = 0u;
                auto num_transfer_queue = 0u;

                auto num_queue_family_properties = 0u;
                vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &num_queue_family_properties, nullptr);
                std::vector<VkQueueFamilyProperties> queue_family_properties(num_queue_family_properties);
                vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &num_queue_family_properties, queue_family_properties.data());

                auto queue_type_indices = std::vector<uint32_t>(num_queue_family_properties);
                for (auto i = 0u; i < num_queue_family_properties; i++) {
                    queue_type_indices[i] = i;
                }

                // TODO: Sort queue families by priority.
                auto unique_family_indices = std::unordered_set<uint32_t>{};
                auto get_queue_family = [&](std::string_view name, auto queue_flag, auto& num_queues) -> uint32_t {
                    for (auto i = 0u; i < queue_family_properties.size(); i++) {
                        if (auto queue_family = &queue_family_properties[i]; !unique_family_indices.contains(i) && (queue_family->queueFlags & queue_flag)) {
                            num_queues = queue_family->queueCount;
                            CNE_INFO("Found {} family: {} with {} queues", name, i, num_queues);
                            unique_family_indices.emplace(i);
                            return i;
                        }
                    }
                    CNE_ASSERT_WITH(false, std::format("Could not find {} family", name));
                };

                queue_info.graphics_family = get_queue_family("graphics", VK_QUEUE_GRAPHICS_BIT, num_graphics_queue);
                queue_info.compute_family  = get_queue_family("compute", VK_QUEUE_COMPUTE_BIT, num_compute_queue);
                queue_info.transfer_family = get_queue_family("transfer", VK_QUEUE_TRANSFER_BIT, num_transfer_queue);

                queue_info.graphics_queues.resize(num_graphics_queue);
                queue_info.compute_queues.resize(num_compute_queue);
                queue_info.transfer_queues.resize(num_transfer_queue);
            }
        }
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_time).count();
        total_time += duration;
        CNE_INFO("Vulkan Physical Device picked in {} ms.", duration);

        CNE_INFO("Creating Vulkan Logical Device...");
        start_time = std::chrono::high_resolution_clock::now();
        {
            auto enabled_device_extensions = std::vector<char const*>{};
            {
                auto num_device_extension_names = 0u;
                vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &num_device_extension_names, nullptr);
                std::vector<VkExtensionProperties> available_device_extensions(num_device_extension_names);
                vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &num_device_extension_names, available_device_extensions.data());

                auto extension_names = &VkExtensionProperties::extensionName;
                find_and_enable_if_exist(nullptr, VK_KHR_MAINTENANCE1_EXTENSION_NAME, extension_names, &available_device_extensions, &enabled_device_extensions);
                find_and_enable_if_exist(nullptr, VK_KHR_MAINTENANCE2_EXTENSION_NAME, extension_names, &available_device_extensions, &enabled_device_extensions);
                find_and_enable_if_exist(nullptr, VK_KHR_MAINTENANCE3_EXTENSION_NAME, extension_names, &available_device_extensions, &enabled_device_extensions);
                find_and_enable_if_exist(nullptr, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME, extension_names, &available_device_extensions, &enabled_device_extensions);
                find_and_enable_if_exist(nullptr, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME, extension_names, &available_device_extensions, &enabled_device_extensions);
                find_and_enable_if_exist(nullptr, VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME, extension_names, &available_device_extensions, &enabled_device_extensions);
                find_and_enable_if_exist(nullptr, VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME, extension_names, &available_device_extensions, &enabled_device_extensions);
                find_and_enable_if_exist(nullptr, VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME, extension_names, &available_device_extensions, &enabled_device_extensions);
                find_and_enable_if_exist(nullptr, VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME, extension_names, &available_device_extensions, &enabled_device_extensions);
                find_and_enable_if_exist(nullptr, VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME, extension_names, &available_device_extensions, &enabled_device_extensions);
                find_and_enable_if_exist(nullptr, VK_KHR_SPIRV_1_4_EXTENSION_NAME, extension_names, &available_device_extensions, &enabled_device_extensions);
                find_and_enable_if_exist(nullptr, VK_EXT_MESH_SHADER_EXTENSION_NAME, extension_names, &available_device_extensions, &enabled_device_extensions);
                find_and_enable_if_exist(nullptr, VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME, extension_names, &available_device_extensions, &enabled_device_extensions);
                find_and_enable_if_exist(nullptr, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME, extension_names, &available_device_extensions, &enabled_device_extensions);
                find_and_enable_if_exist(nullptr, VK_KHR_SWAPCHAIN_EXTENSION_NAME, extension_names, &available_device_extensions, &enabled_device_extensions);
                find_and_enable_if_exist(nullptr, VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME, extension_names, &available_device_extensions, &enabled_device_extensions);

                find_and_enable_if_exist(&device_info.enable_hdr, VK_EXT_HDR_METADATA_EXTENSION_NAME, extension_names, &available_device_extensions, &enabled_device_extensions);

                find_and_enable_if_exist(&device_info.enable_ray_tracing, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, extension_names, &available_device_extensions, &enabled_device_extensions);
                find_and_enable_if_exist(&device_info.enable_ray_tracing, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, extension_names, &available_device_extensions, &enabled_device_extensions);
                find_and_enable_if_exist(&device_info.enable_ray_tracing, VK_KHR_RAY_QUERY_EXTENSION_NAME, extension_names, &available_device_extensions, &enabled_device_extensions);
            }

            auto available_features = &physical_device_features;
            auto enabled_features = &enabled_physical_device_features;
            #define CHECK_AND_ENABLE(FEATURE) CNE_ASSERT_WITH(available_features->FEATURE == VK_TRUE, "Required feature not available"); enabled_features->FEATURE = VK_TRUE; static_assert(true)
            // Enable all core 1.0 features.
            enabled_features->vk_features2.features = available_features->vk_features2.features;
            // Enable core 1.1 features.
            CHECK_AND_ENABLE(vk11_features.shaderDrawParameters);
            CHECK_AND_ENABLE(vk11_features.uniformAndStorageBuffer16BitAccess);
            CHECK_AND_ENABLE(vk11_features.storageBuffer16BitAccess);
            // Enable core 1.2 features.
            CHECK_AND_ENABLE(vk12_features.drawIndirectCount);
            CHECK_AND_ENABLE(vk12_features.imagelessFramebuffer);
            CHECK_AND_ENABLE(vk12_features.separateDepthStencilLayouts);
            CHECK_AND_ENABLE(vk12_features.descriptorIndexing);
            CHECK_AND_ENABLE(vk12_features.runtimeDescriptorArray);
            CHECK_AND_ENABLE(vk12_features.descriptorBindingPartiallyBound);
            CHECK_AND_ENABLE(vk12_features.descriptorBindingVariableDescriptorCount);
            CHECK_AND_ENABLE(vk12_features.shaderSampledImageArrayNonUniformIndexing);
            CHECK_AND_ENABLE(vk12_features.descriptorBindingUpdateUnusedWhilePending);
            CHECK_AND_ENABLE(vk12_features.descriptorBindingSampledImageUpdateAfterBind);
            CHECK_AND_ENABLE(vk12_features.descriptorBindingStorageBufferUpdateAfterBind);
            CHECK_AND_ENABLE(vk12_features.shaderStorageBufferArrayNonUniformIndexing);
            CHECK_AND_ENABLE(vk12_features.shaderUniformBufferArrayNonUniformIndexing);
            CHECK_AND_ENABLE(vk12_features.descriptorBindingUniformBufferUpdateAfterBind);
            CHECK_AND_ENABLE(vk12_features.descriptorBindingStorageImageUpdateAfterBind);
            CHECK_AND_ENABLE(vk12_features.vulkanMemoryModel);
            CHECK_AND_ENABLE(vk12_features.vulkanMemoryModelDeviceScope);
            CHECK_AND_ENABLE(vk12_features.timelineSemaphore);
            CHECK_AND_ENABLE(vk12_features.bufferDeviceAddress);
            CHECK_AND_ENABLE(vk12_features.shaderFloat16);
            CHECK_AND_ENABLE(vk12_features.storagePushConstant8);
            CHECK_AND_ENABLE(vk12_features.hostQueryReset);
            CHECK_AND_ENABLE(vk12_features.storageBuffer8BitAccess);
            CHECK_AND_ENABLE(vk12_features.uniformAndStorageBuffer8BitAccess);
            // Enable core 1.3 features.
            CHECK_AND_ENABLE(vk13_features.dynamicRendering);
            CHECK_AND_ENABLE(vk13_features.synchronization2);
            CHECK_AND_ENABLE(vk13_features.maintenance4);
            CHECK_AND_ENABLE(vk13_features.shaderDemoteToHelperInvocation);
            // Enable dynamic state features.
            CHECK_AND_ENABLE(extended_dynamic_state2_features.extendedDynamicState2);
            CHECK_AND_ENABLE(extended_dynamic_state2_features.extendedDynamicState2LogicOp);
            CHECK_AND_ENABLE(extended_dynamic_state3_features.extendedDynamicState3DepthClampEnable);
            CHECK_AND_ENABLE(extended_dynamic_state3_features.extendedDynamicState3PolygonMode);
            CHECK_AND_ENABLE(extended_dynamic_state3_features.extendedDynamicState3RasterizationSamples);
            CHECK_AND_ENABLE(extended_dynamic_state3_features.extendedDynamicState3ColorBlendEnable);
            CHECK_AND_ENABLE(extended_dynamic_state3_features.extendedDynamicState3ColorBlendEquation);
            CHECK_AND_ENABLE(extended_dynamic_state3_features.extendedDynamicState3ColorWriteMask);
            CHECK_AND_ENABLE(extended_dynamic_state3_features.extendedDynamicState3LogicOpEnable);
            // Enable mesh shader features.
            CHECK_AND_ENABLE(mesh_shader_features.taskShader);
            CHECK_AND_ENABLE(mesh_shader_features.meshShader);
            CHECK_AND_ENABLE(vertex_input_dynamic_state_features.vertexInputDynamicState);
            #undef CHECK_AND_ENABLE
            // Enable ray tracing features
            if (device_info.enable_ray_tracing) {
                enabled_features->ray_tracing_pipeline_features.rayTracingPipelineShaderGroupHandleCaptureReplay = VK_TRUE;
                enabled_features->ray_tracing_pipeline_features.rayTracingPipelineTraceRaysIndirect = VK_TRUE;
                enabled_features->ray_query_features.rayQuery = VK_TRUE;
            }

            auto queue_cis = std::vector<VkDeviceQueueCreateInfo>{};
            auto graphics_queue_priority = std::vector<float>{};
            auto compute_queue_priority = std::vector<float>{};
            auto transfer_queue_priority = std::vector<float>{};
            {
                queue_info.graphics_queues[0].priority = 1.0f;
                queue_info.compute_queues[0].priority = 0.9f;

                auto fill_queue_priority = [&](auto* queue_priority, auto* queue_info) {
                    queue_priority->reserve(queue_info->size());
                    for (auto& queue : *queue_info) {
                        queue_priority->emplace_back(queue.priority);
                    }
                };
                fill_queue_priority(&graphics_queue_priority, &queue_info.graphics_queues);
                fill_queue_priority(&compute_queue_priority, &queue_info.compute_queues);
                fill_queue_priority(&transfer_queue_priority, &queue_info.transfer_queues);

                auto fill_queue_create_info = [&](auto* queues, auto family, auto* queue_priority) {
                    queue_cis.emplace_back(
                        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                        nullptr,
                        0,
                        family,
                        (uint32_t) queues->size(),
                        queue_priority->data()
                    );
                };
                fill_queue_create_info(&queue_info.graphics_queues, queue_info.graphics_family, &graphics_queue_priority);
                fill_queue_create_info(&queue_info.compute_queues, queue_info.compute_family, &compute_queue_priority);
                fill_queue_create_info(&queue_info.transfer_queues, queue_info.transfer_family, &transfer_queue_priority);
            }

            auto device_ci = VkDeviceCreateInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
            connect_to_next(&device_ci, &enabled_features->vk_features2);
            CNE_ASSERT(enabled_features->vk_features2.pNext == &enabled_features->vk11_features);
            CNE_ASSERT(enabled_features->vk11_features.pNext == &enabled_features->vk12_features);
            CNE_ASSERT(enabled_features->vk12_features.pNext == &enabled_features->vk13_features);
            device_ci.queueCreateInfoCount    = (uint32_t) queue_cis.size();
            device_ci.pQueueCreateInfos       = queue_cis.data();
            device_ci.enabledExtensionCount   = (uint32_t) enabled_device_extensions.size();
            device_ci.ppEnabledExtensionNames = enabled_device_extensions.data();

            auto result_device_create = vkCreateDevice(physical_device, &device_ci, nullptr, &device);
            CNE_ASSERT_WITH(result_device_create == VK_SUCCESS, std::format("Failed to create vulkan device. ERROR: {0}", vk_error_to_string(result_device_create)));

            volkLoadDevice(device);

            // Get Queue
            auto get_queue = [&](auto* queues, auto family, std::string_view name) {
                for (auto i = 0u; i < queues->size(); i++) {
                    vkGetDeviceQueue(device, family, i, &queues->at(i).queue);
                    set_resource_name(device, VK_OBJECT_TYPE_QUEUE, reinterpret_cast<uint64_t>(queues->at(i).queue), std::format("{}_{}", name, i));
                }
            };
            get_queue(&queue_info.graphics_queues, queue_info.graphics_family, "graphics");
            get_queue(&queue_info.compute_queues, queue_info.compute_family, "compute");
            get_queue(&queue_info.transfer_queues, queue_info.transfer_family, "transfer");

            // Pipeline Cache
            auto pipeline_cache_ci = VkPipelineCacheCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
            pipeline_cache_ci.initialDataSize = 0;
            pipeline_cache_ci.pInitialData = nullptr;
            auto result_create_pipeline_cache = vkCreatePipelineCache(device, &pipeline_cache_ci, nullptr, &pipeline_cache);
            CNE_ASSERT_WITH(result_create_pipeline_cache == VK_SUCCESS, std::format("Failed to create vulkan pipeline cache. ERROR: {0}", vk_error_to_string(result_create_pipeline_cache)));

            // Allocator
            {
                auto vma_vulkan_functions = VmaVulkanFunctions{
                    .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
                    .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
                };

                auto allocator_ci = VmaAllocatorCreateInfo{};
                allocator_ci.flags            = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT | VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
                allocator_ci.physicalDevice   = physical_device;
                allocator_ci.device           = device;
                allocator_ci.instance         = instance;
                allocator_ci.vulkanApiVersion = SDK_version;
                allocator_ci.pVulkanFunctions = &vma_vulkan_functions;
                auto result_create_allocator = vmaCreateAllocator(&allocator_ci, &allocator);
                CNE_ASSERT_WITH(result_create_allocator == VK_SUCCESS, std::format("Failed to create vulkan allocator. ERROR: {0}", vk_error_to_string(result_create_allocator)));
            }

        }
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_time).count();
        total_time += duration;
        CNE_INFO("Vulkan Logical Device created in {0} ms.", duration);

        graphics_queue = std::make_unique<VulkanQueue>(this, EQueueType::graphics, queue_info.graphics_family, queue_info.graphics_queues[0].queue);
        async_transfer_queue = std::make_unique<VulkanQueue>(this, EQueueType::transfer, queue_info.transfer_family, queue_info.transfer_queues[0].queue);
        async_compute_queue = std::make_unique<VulkanQueue>(this, EQueueType::compute, queue_info.compute_family, queue_info.compute_queues[0].queue);

        layout_manager = std::make_unique<VulkanLayoutManager>(this);
        pipeline_manager = std::make_unique<VulkanPipelineManager>(this);
        // TODO: Set this by user.
        buffer_pool = std::make_unique<ResourcePool<VulkanBuffer>>(3);
        texture_pool = std::make_unique<ResourcePool<VulkanTexture>>(3);
        bindless_manager = std::make_unique<VulkanBindlessManager>(this);

        shader_factory = std::make_unique<ShaderFactory>(this);

        CNE_INFO("Vulkan RHI initializing is completed. Using {0} ms.", total_time);
    }

    PhysicalDeviceFeatures::PhysicalDeviceFeatures()
    {
        empty_vk_structure(vk_features2, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
        empty_vk_structure(vk11_features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES);
        empty_vk_structure(vk12_features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES);
        empty_vk_structure(vk13_features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES);

        empty_vk_structure(acceleration_structure_features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR);
        empty_vk_structure(ray_tracing_pipeline_features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR);
        empty_vk_structure(ray_query_features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR);

        empty_vk_structure(extended_dynamic_state2_features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT);
        empty_vk_structure(extended_dynamic_state3_features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT);
        empty_vk_structure(mesh_shader_features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT);
        empty_vk_structure(vertex_input_dynamic_state_features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT);

        connect(&vk11_features);
        connect(&vk12_features);
        connect(&vk13_features);

        connect(&acceleration_structure_features);
        connect(&ray_tracing_pipeline_features);
        connect(&ray_query_features);

        connect(&extended_dynamic_state2_features);
        connect(&extended_dynamic_state3_features);
        connect(&mesh_shader_features);
        connect(&vertex_input_dynamic_state_features);
    }

    auto PhysicalDeviceFeatures::connect(auto* next) -> void
    {
        connect_to_next(&vk_features2, next);
    }

    PhysicalDeviceProperties::PhysicalDeviceProperties()
    {
        empty_vk_structure(properties2, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);
        empty_vk_structure(subgroup_properties, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES);
        empty_vk_structure(descriptor_indexing_properties, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES);
        empty_vk_structure(acceleration_structure_properties, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR);

        connect_to_next(&properties2, &subgroup_properties);
        connect_to_next(&properties2, &descriptor_indexing_properties);
        connect_to_next(&properties2, &acceleration_structure_properties);
    }

    VulkanDevice::~VulkanDevice()
    {
        auto result_device_wait_idle = vkDeviceWaitIdle(device);
        CNE_ASSERT_WITH(result_device_wait_idle == VK_SUCCESS, std::format("Device error: {}", vk_error_to_string(result_device_wait_idle)));

        shader_factory.reset();

        bindless_manager.reset();
        samplers.clear();
        texture_pool.reset();
        buffer_pool.reset();
        pipeline_manager.reset();
        layout_manager.reset();

        async_transfer_queue.reset();
        async_compute_queue.reset();
        graphics_queue.reset();

        vmaDestroyAllocator(allocator);

        vkDestroyPipelineCache(device, pipeline_cache, nullptr);

        vkDestroyDevice(device, nullptr);

        if (device_info.enable_debug_utils) {
            vkDestroyDebugUtilsMessengerEXT(instance, debug_utils_messenger, nullptr);
        }

        vkDestroyInstance(instance, nullptr);
    }

    auto VulkanDevice::new_frame(uint32_t frame_count) -> void
    {
        buffer_pool->new_frame(frame_count);
        texture_pool->new_frame(frame_count);
        graphics_queue->refresh_command_buffers();
        async_transfer_queue->refresh_command_buffers();
        async_compute_queue->refresh_command_buffers();
    }

    auto VulkanDevice::wait_idle() -> void
    {
        vkDeviceWaitIdle(device);
    }

    auto VulkanDevice::queue(EQueueType type) -> VulkanQueue*
    {
        switch (type) {
            case EQueueType::graphics: return graphics_queue.get();
            case EQueueType::compute:  return async_compute_queue.get();
            case EQueueType::transfer: return async_transfer_queue.get();
            default: return nullptr;
        }
    }

    auto VulkanDevice::queue_family(EQueueType type) -> uint32_t
    {
        switch (type) {
            case EQueueType::graphics: return queue_info.graphics_family;
            case EQueueType::compute:  return queue_info.compute_family;
            case EQueueType::transfer: return queue_info.transfer_family;
            case EQueueType::ignore:   return VK_QUEUE_FAMILY_IGNORED;
            default: return VK_QUEUE_FAMILY_IGNORED;
        }
    }
}
