/*
 * ImageViews.cxx
 * Creates the Vulkan image view instances for the swap chain buffers.
 *
 * VULKRAY ENGINE SOFTWARE
 * Copyright (c) 2022, Max Rodriguez. All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license. You should have received a copy of this license along
 * with this source code in a file named "COPYING."
 */

#include "Vulkan.h"

#include <spdlog/spdlog.h>

ImageViews::ImageViews(Vulkan *m_vulkan): VkModuleBase(m_vulkan) {

    // Resize image views vector to available swap chain images
    this->swapChainImageViews.resize(this->m_vulkan->m_swapChain->swapChainImages.size());

    for (size_t i = 0; i < this->m_vulkan->m_swapChain->swapChainImages.size(); i++) {
        // Create image view for every swap image
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = this->m_vulkan->m_swapChain->swapChainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = this->m_vulkan->m_swapChain->swapChainImageFormat;
        // color mapping on images (set default, no special engine feature needs this)
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        // no mip-mapping levels or multiple layers used. (not needed)
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        // Create the image view instance
        VkResult result = vkCreateImageView(this->m_vulkan->m_logicalDevice->logicalDevice,
                                            &createInfo, nullptr, &this->swapChainImageViews.at(i));
        if (result != VK_SUCCESS) {
            spdlog::error("Failed to create Vulkan swap chain image view instance!");
            throw std::runtime_error("Failed to create Vulkan image views!");
        }
    }
}

ImageViews::~ImageViews() {
    for (size_t i = 0; i < this->swapChainImageViews.size(); i++) {
        vkDestroyImageView(this->m_vulkan->m_logicalDevice->logicalDevice,
                           this->swapChainImageViews[i], nullptr);
        this->swapChainImageViews[i] = VK_NULL_HANDLE; // less validation layer errors on clean up
    }
}