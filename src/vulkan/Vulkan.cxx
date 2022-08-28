/*
 * Vulkan.cxx
 * Initializes and manages all the engine's Vulkan instances.
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

Vulkan::Vulkan(GraphicsInput graphicsInput) {
    // store as class attribute for modules to access
    this->graphicsInput = graphicsInput;

    // initialize modules using smart pointers and store as class properties
    spdlog::debug("Initializing Vulkan ...");
    this->m_vulkanInstance = std::make_unique<VulkanInstance>(this);
    this->m_window = std::make_unique<Window>(this);
    this->m_physicalDevice = std::make_unique<PhysicalDevice>(this);
    this->m_logicalDevice = std::make_unique<LogicalDevice>(this);
    this->m_VMA = std::make_unique<VulkanMemoryAllocator>(this);
    this->m_swapChain = std::make_unique<SwapChain>(this);
    this->m_imageViews = std::make_unique<ImageViews>(this);
    this->m_renderPass = std::make_unique<RenderPass>(this);
    this->m_graphicsPipeline = std::make_unique<GraphicsPipeline>(this);
    this->m_frameBuffers = std::make_unique<FrameBuffers>(this);

    CommandBuffer::createCommandPool(&this->graphicsCommandPool, (VkCommandPoolCreateFlags) 0,
                                     this->m_logicalDevice->logicalDevice,
                                     this->m_physicalDevice->queueFamilies.graphicsFamily.value());
    CommandBuffer::createCommandPool(&this->transferCommandPool, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
                                     this->m_logicalDevice->logicalDevice,
                                     this->m_physicalDevice->queueFamilies.transferFamily.value());
    Buffers::createVertexBuffer(&this->vertexBuffer, this->m_VMA->memoryAllocator,
                                this->m_physicalDevice->queueFamilies,
                                graphicsInput.vertices, this->m_logicalDevice->logicalDevice,
                                this->transferCommandPool, this->m_logicalDevice->transferQueue);
    Buffers::createIndexBuffer(&this->indexBuffer, this->m_VMA->memoryAllocator,
                               this->m_physicalDevice->queueFamilies,
                               graphicsInput.indices, this->m_logicalDevice->logicalDevice,
                               this->transferCommandPool, this->m_logicalDevice->transferQueue);
    CommandBuffer::createCommandBuffer(&this->graphicsCommandBuffers, this->MAX_FRAMES_IN_FLIGHT,
                                       this->m_logicalDevice->logicalDevice, this->graphicsCommandPool);
    Synchronization::createSyncObjects(&this->imageAvailableSemaphores, &this->renderFinishedSemaphores,
                                       &this->inFlightFences, this->m_logicalDevice->logicalDevice,
                                       this->MAX_FRAMES_IN_FLIGHT);
    spdlog::debug("Running Vulkan renderer ...");

    while(!glfwWindowShouldClose(this->m_window->window)) {
        glfwPollEvents(); // Respond to window events (exit, resize, etc.)
        renderFrame();
    }
}

void Vulkan::renderFrame() {
    uint32_t imageIndex;
    this->waitForPreviousFrame();
    this->getNextSwapChainImage(&imageIndex); // <-- swap chain recreation called here
    this->resetGraphicsCmdBuffer(imageIndex);
    this->submitGraphicsCmdBuffer();
    this->presentImageBuffer(&imageIndex);
    this->frameIndex = (this->frameIndex + 1) % this->MAX_FRAMES_IN_FLIGHT;
}

// Synchronization / Command Buffer wrappers

void Vulkan::waitForPreviousFrame() {
    vkWaitForFences(this->m_logicalDevice->logicalDevice, 1,
                    &this->inFlightFences[this->frameIndex], VK_TRUE, UINT64_MAX);
}

void Vulkan::getNextSwapChainImage(uint32_t *imageIndex) {

    // acquire next image view, also get swap chain status
    VkResult result = vkAcquireNextImageKHR(this->m_logicalDevice->logicalDevice, this->m_swapChain->swapChain,
                                            UINT64_MAX, this->imageAvailableSemaphores[frameIndex],
                                            VK_NULL_HANDLE, imageIndex);

    /* check if vkAcquireNextImageKHR returned an out of date framebuffer flag
     * Note: this is not a feature on all Vulkan compatible drivers! also checking via GLFW resize callback!
     */
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        this->recreateSwapChain();
        return;

    // TODO: Handle VK_SUBOPTIMAL_KHR status code
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        spdlog::error("An error occurred when acquiring the next swap chain image view; Exiting.");
        throw std::runtime_error("Failed to acquire swap chain image!");
    }
    // reset fence only if we know we're submitting work
    vkResetFences(this->m_logicalDevice->logicalDevice, 1, &this->inFlightFences[frameIndex]);
}

void Vulkan::resetGraphicsCmdBuffer(uint32_t imageIndex) {
    vkResetCommandBuffer(this->graphicsCommandBuffers[frameIndex], 0);
    CommandBuffer::recordGraphicsCommands(this->graphicsCommandBuffers[frameIndex], imageIndex,
                                       this->m_graphicsPipeline->graphicsPipeline, this->m_renderPass->renderPass,
                                       this->m_frameBuffers->swapChainFrameBuffers, this->vertexBuffer,
                                       this->indexBuffer, this->graphicsInput, this->m_swapChain->swapChainExtent);
}

void Vulkan::submitGraphicsCmdBuffer() {
    CommandBuffer::submitCommandBuffer(&this->graphicsCommandBuffers[this->frameIndex],
                                       this->m_logicalDevice->graphicsQueue, this->inFlightFences[this->frameIndex],
                                       this->imageAvailableSemaphores[this->frameIndex],
                                       this->renderFinishedSemaphores[this->frameIndex],
                                       this->waitSemaphores, this->signalSemaphores);
}

void Vulkan::presentImageBuffer(uint32_t *imageIndex) {
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = this->signalSemaphores;

    VkSwapchainKHR swapChains[] = {this->m_swapChain->swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = imageIndex;
    presentInfo.pResults = nullptr; // optional

    VkResult result = vkQueuePresentKHR(this->m_logicalDevice->presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || this->framebufferResized) {
        this->framebufferResized = false; // reset GLFW triggered framebuffer resized flag
        this->recreateSwapChain();

    } else if (result != VK_SUCCESS) {
        spdlog::error("An error occurred while submitting a swap chain image for presentation.");
        throw std::runtime_error("Failed to present the current swap chain image!");
    }
}

// Swap chain recreation (m_swapChain, m_imageViews, m_frameBuffers)
void Vulkan::recreateSwapChain() {
    this->m_window->waitForWindowFocus();
    this->m_logicalDevice->waitForDeviceIdle();
    // destroy the previous swap chain / dependent modules
    this->m_swapChain.reset();
    this->m_imageViews.reset();
    this->m_frameBuffers.reset();
    this->m_swapChain = std::make_unique<SwapChain>(this);
    this->m_imageViews = std::make_unique<ImageViews>(this);
    this->m_frameBuffers = std::make_unique<FrameBuffers>(this);
}

Vulkan::~Vulkan() {
    this->m_logicalDevice->waitForDeviceIdle();
    // Clean up synchronization objects
    for (int i = 0; i < this->MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(this->m_logicalDevice->logicalDevice, this->imageAvailableSemaphores.at(i), nullptr);
        vkDestroySemaphore(this->m_logicalDevice->logicalDevice, this->renderFinishedSemaphores.at(i), nullptr);
        vkDestroyFence(this->m_logicalDevice->logicalDevice, this->inFlightFences.at(i), nullptr);
    }
    // Clean up Command Buffers, Logical & Physical devices
    vkDestroyCommandPool(this->m_logicalDevice->logicalDevice, this->transferCommandPool, nullptr);
    vkDestroyCommandPool(this->m_logicalDevice->logicalDevice, this->graphicsCommandPool, nullptr);
}

// Module base class constructor
VkModuleBase::VkModuleBase(Vulkan *m_vulkan) {
    this->m_vulkan = m_vulkan; // store pointer to core Vulkan class instance in every module
}