#include "vk_RHI.hpp"
#include "vk_tool.hpp"

#include <algorithm>

namespace cannele::inline graphics::rhi::vk
{
    inline namespace
    {
        static uint32_t num_backbuffers = 3;
    }

    auto VulkanDevice::create_swapchain(SwapchainCreateInfo* info) -> SwapchainHandle
    {
        return std::make_shared<VulkanSwapchain>(this, info);
    }

    VulkanSwapchain::VulkanSwapchain(VulkanDevice* device, SwapchainCreateInfo* info)
        : VulkanDeviceChild<VulkanSwapchain>(device)
    {
#ifdef VK_USE_PLATFORM_WIN32_KHR
            auto surface_ci = VkWin32SurfaceCreateInfoKHR{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
            surface_ci.hinstance = GetModuleHandle(nullptr);
            surface_ci.hwnd = static_cast<HWND>(info->window_handle);
            auto result_surface_create = vkCreateWin32SurfaceKHR(parent->instance, &surface_ci, parent->allocation_callbacks, &surface);
            CNE_ASSERT_WITH(result_surface_create == VK_SUCCESS, std::format("Failed to create vulkan surface. ERROR: {0}", vk_error_to_string(result_surface_create)));
#else
            CNE_ASSERT_WITH(false, "Surface of Platform is not supported now.");
#endif

        auto physical_device = device->physical_device;

        auto num_available_presents = (uint32_t) 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &num_available_presents, nullptr);
        auto available_present_modes = &support_details.present_modes;
        available_present_modes->resize(num_available_presents);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &num_available_presents, available_present_modes->data());
        auto num_available_formats = (uint32_t) 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &num_available_formats, nullptr);
        auto available_formats = &support_details.surface_formats;
        available_formats->resize(num_available_formats);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &num_available_formats, available_formats->data());

        this->present_mode   = convert_to_vk_present_mode(info->present_mode);
        this->surface_format = convert_to_vk_format(info->surface_format);
        this->color_space    = convert_to_vk_color_space(info->color_space);
        this->extent.width   = info->width;
        this->extent.height  = info->height;

        create_swapchain();

        // Create fence and semaphore, if backbuffers size changed, we need to recreate them.
        auto semaphore_ci = VkSemaphoreCreateInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        auto fence_info = VkFenceCreateInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT};
        for (auto i = 0u; i < num_backbuffers; ++i) {
            auto semaphore1 = &backbuffer_ready_semaphores.emplace_back();
            auto semaphore2 = &render_finished_semaphores.emplace_back();

            auto result1 = vkCreateSemaphore(device->device, &semaphore_ci, parent->allocation_callbacks, semaphore1);
            auto result2 = vkCreateSemaphore(device->device, &semaphore_ci, parent->allocation_callbacks, semaphore2);
            CNE_ASSERT_WITH(result1 == VK_SUCCESS, std::format("Failed to create swapchain semaphore: {}", vk_error_to_string(result1)));
            CNE_ASSERT_WITH(result2 == VK_SUCCESS, std::format("Failed to create swapchain semaphore: {}", vk_error_to_string(result2)));
        }
        last_submition_times.resize(num_backbuffers, 0);
    }

    VulkanSwapchain::~VulkanSwapchain()
    {
        for (auto& semaphore : backbuffer_ready_semaphores) {
            vkDestroySemaphore(parent->device, semaphore, parent->allocation_callbacks);
        }
        for (auto& semaphore : render_finished_semaphores) {
            vkDestroySemaphore(parent->device, semaphore, parent->allocation_callbacks);
        }

        for (auto& texture : backbuffers) {
            texture.reset();
        }

        if (swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(parent->device, swapchain, parent->allocation_callbacks);
        }

        vkDestroySurfaceKHR(parent->instance, surface, parent->allocation_callbacks);
    }

    auto VulkanSwapchain::acquire_next_backbuffer() -> TextureHandle
    {
        if (last_submition_times[frame_index] != 0) {
            parent->queue(EQueueType::graphics)->wait_command_list(last_submition_times[frame_index], UINT64_MAX);
        }
        auto result = vkAcquireNextImageKHR(parent->device, swapchain, UINT64_MAX, backbuffer_ready_semaphores[frame_index], VK_NULL_HANDLE, &image_index);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            create_swapchain();
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            CNE_ASSERT_WITH(false, std::format("Failed to acquire next backbuffer: {}", vk_error_to_string(result)));
        }

        return backbuffers[image_index];
    }

    auto VulkanSwapchain::present(uint64_t submission_time) -> void
    {
        auto present_info = VkPresentInfoKHR{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores    = &render_finished_semaphores[frame_index];
        present_info.swapchainCount     = 1;
        present_info.pSwapchains        = &swapchain;
        present_info.pImageIndices      = &image_index;
        present_info.pResults           = nullptr;

        auto present_queue = parent->queue(EQueueType::graphics)->queue;
        auto result = vkQueuePresentKHR(present_queue, &present_info);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            create_swapchain();
        }
        else if (result != VK_SUCCESS) {
            CNE_ASSERT_WITH(false, std::format("Failed to present: {}", vk_error_to_string(result)));
        }

        last_submition_times[frame_index] = submission_time;

        frame_index = (frame_index + 1) % backbuffers.size();
    }

    auto VulkanSwapchain::enqueue_backbuffer_ready_wait_semaphore() -> void
    {
        parent->queue(EQueueType::graphics)->add_wait_semaphore(backbuffer_ready_semaphores[frame_index], 0);
    }

    auto VulkanSwapchain::enqueue_render_finish_signal_semaphore() -> void
    {
        parent->queue(EQueueType::graphics)->add_signal_semaphore(render_finished_semaphores[frame_index], 0);
    }

    auto VulkanSwapchain::create_swapchain() -> void
    {
        // Chose present mode.
        auto target_mode = VkPresentModeKHR{VK_PRESENT_MODE_MAX_ENUM_KHR};
        for (auto mode : support_details.present_modes) {
            if (mode == present_mode) {
                target_mode = mode;
                break;
            }
        }
        if (target_mode == VK_PRESENT_MODE_MAX_ENUM_KHR) {
            present_mode = support_details.present_modes[0];
            CNE_WARN("Present mode is not available. Using the first available present mode.");
        }

        // Chose surface format.
        auto target_format = VkFormat{VK_FORMAT_UNDEFINED};
        for (auto available_format : support_details.surface_formats) {
            if (available_format.format == surface_format && available_format.colorSpace == color_space) {
                target_format = available_format.format;
                break;
            }
        }
        if (target_format == VK_FORMAT_UNDEFINED) {
            surface_format = support_details.surface_formats[0].format;
            CNE_WARN("Surface format is not available. Using the first available surface format.");
        }

        auto surface_capabilities = VkSurfaceCapabilitiesKHR{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(parent->physical_device, surface, &surface_capabilities);

        auto extent = surface_capabilities.currentExtent;
        if (extent.width == 0xFFFFFFFF || extent.height == 0xFFFFFFFF) {
            extent.width = std::clamp(extent.width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
            extent.height = std::clamp(extent.height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);
        }

        num_backbuffers = std::clamp(num_backbuffers, surface_capabilities.minImageCount, surface_capabilities.maxImageCount);

        auto old_swapchain = swapchain;
        VkSwapchainCreateInfoKHR swapchain_ci{};
        swapchain_ci.sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchain_ci.surface               = surface;
        swapchain_ci.minImageCount         = num_backbuffers;
        swapchain_ci.imageFormat           = surface_format;
        swapchain_ci.imageColorSpace       = color_space;
        swapchain_ci.imageExtent           = extent;
        swapchain_ci.imageArrayLayers      = 1;
        swapchain_ci.imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        swapchain_ci.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
        swapchain_ci.queueFamilyIndexCount = 0;
        swapchain_ci.pQueueFamilyIndices   = nullptr;
        swapchain_ci.preTransform          = surface_capabilities.currentTransform;
        swapchain_ci.compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchain_ci.presentMode           = present_mode;
        swapchain_ci.clipped               = VK_TRUE;
        swapchain_ci.oldSwapchain          = old_swapchain;

        auto result = vkCreateSwapchainKHR(parent->device, &swapchain_ci, parent->allocation_callbacks, &swapchain);
        CNE_ASSERT_WITH(result == VK_SUCCESS, std::format("Failed to create swapchain: {}", vk_error_to_string(result)));

        if (old_swapchain) {
            vkDestroySwapchainKHR(parent->device, old_swapchain, parent->allocation_callbacks);
        }

        auto images = std::vector<VkImage>{num_backbuffers};
        vkGetSwapchainImagesKHR(parent->device, swapchain, &num_backbuffers, images.data());
        backbuffers.clear();
        for (auto i = 0u; i < num_backbuffers; i++) {
            auto texture_info = TextureCreateInfo{
                .dimension = ETextureDimension::tex_2d,
                .format = convert_to_format(surface_format),
                .usage = ETextureUsage::color_attachment | ETextureUsage::transfer_src | ETextureUsage::transfer_dst,
                .extent = math::uint2{extent.width, extent.height},
                .initial_state = EResourceStates::present,
                .keep_initial_state = true
            };
            auto texture = std::make_shared<VulkanTexture>(parent, &texture_info, images[i]);
            texture->name = "Swapchain backbuffer" + std::to_string(i);
            backbuffers.emplace_back(texture);
        }
    }
}
