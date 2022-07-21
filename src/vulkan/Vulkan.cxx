/*
 * Vulkan.cxx
 * Initializes and manages all the engine's Vulkan instances.
 *
 * Copyright 2022 Max Rodriguez

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 */

#include "Vulkan.hxx"

#include <spdlog/spdlog.h>

void Vulkan::initialize(const char* engineName, GLFWwindow *engineWindow) {

    spdlog::debug("Initializing Vulkan ...");
    VulkanInstance::createInstance(&this->vulkanInstance, engineName,
                                   this->enableValidationLayers, this->validationLayers);
    WindowSurface::createSurfaceKHR(&this->surface, this->vulkanInstance, engineWindow);
    PhysicalDevice::selectPhysicalDevice(&this->physicalDevice, this->vulkanInstance,
                                         this->surface, this->requiredExtensions);
    LogicalDevice::createLogicalDevice(&this->logicalDevice, &this->graphicsQueue, &this->presentQueue,
                                       this->physicalDevice, this->surface, this->requiredExtensions,
                                       this->enableValidationLayers, this->validationLayers);
    SwapChain::createSwapChain(&this->swapChain, &this->swapChainImages, &this->swapChainImageFormat,
                               &this->swapChainExtent, this->logicalDevice, this->physicalDevice,
                               this->surface, engineWindow);
    ImageViews::createImageViews(&this->swapChainImageViews, this->logicalDevice,
                                 this->swapChainImages, this->swapChainImageFormat);
    RenderPass::createRenderPass(&this->renderPass, this->logicalDevice, this->swapChainImageFormat);
    GraphicsPipeline::createGraphicsPipeline(&this->graphicsPipeline, &this->pipelineLayout, this->renderPass,
                                             this->logicalDevice, this->swapChainExtent);
    FrameBuffers::createFrameBuffers(&this->swapChainFrameBuffers, this->swapChainImageViews,
                                     this->logicalDevice, this->renderPass, this->swapChainExtent);
    CommandBuffer::createCommandPool(&this->commandPool, this->logicalDevice, this->physicalDevice, this->surface);
    CommandBuffer::createCommandBuffer(&this->commandBuffers, this->MAX_FRAMES_IN_FLIGHT,
                                       this->logicalDevice, this->commandPool);
    Synchronization::createSyncObjects(&this->imageAvailableSemaphores, &this->renderFinishedSemaphores,
                                       &this->inFlightFences, this->logicalDevice, this->MAX_FRAMES_IN_FLIGHT);
    spdlog::debug("Initialized Vulkan instances.");
}

// Synchronization / Command Buffer wrappers
void Vulkan::waitForDeviceIdle() {
    vkDeviceWaitIdle(this->logicalDevice);
}
void Vulkan::waitForPreviousFrame(uint32_t frameIndex) {
    vkWaitForFences(this->logicalDevice, 1, &this->inFlightFences[frameIndex], VK_TRUE, UINT64_MAX);
    vkResetFences(this->logicalDevice, 1, &this->inFlightFences[frameIndex]);
}
void Vulkan::getNextSwapChainImage(uint32_t *imageIndex, uint32_t frameIndex) {
    vkAcquireNextImageKHR(this->logicalDevice, this->swapChain, UINT64_MAX,
                          this->imageAvailableSemaphores[frameIndex], VK_NULL_HANDLE, imageIndex);
}
void Vulkan::resetCommandBuffer(uint32_t imageIndex, uint32_t frameIndex) {
    vkResetCommandBuffer(this->commandBuffers[frameIndex], 0);
    CommandBuffer::recordCommandBuffer(this->commandBuffers[frameIndex], imageIndex, this->graphicsPipeline,
                                       this->renderPass, this->swapChainFrameBuffers, this->swapChainExtent);
}
void Vulkan::submitCommandBuffer(uint32_t frameIndex) {
    CommandBuffer::submitCommandBuffer(&this->commandBuffers[frameIndex], this->graphicsQueue,
                                       this->inFlightFences[frameIndex],
                                       this->imageAvailableSemaphores[frameIndex],
                                       this->renderFinishedSemaphores[frameIndex],
                                       this->waitSemaphores, this->signalSemaphores);
}
void Vulkan::presentImageBuffer(uint32_t *imageIndex) {
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = this->signalSemaphores;

    VkSwapchainKHR swapChains[] = {this->swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = imageIndex;
    presentInfo.pResults = nullptr; // optional

    vkQueuePresentKHR(this->presentQueue, &presentInfo);
}

void Vulkan::shutdown() {
    // Clean up synchronization objects
    for (int i = 0; i < this->MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(this->logicalDevice, this->imageAvailableSemaphores.at(i), nullptr);
        vkDestroySemaphore(this->logicalDevice, this->renderFinishedSemaphores.at(i), nullptr);
        vkDestroyFence(this->logicalDevice, this->inFlightFences.at(i), nullptr);
    }
    // Clean up Command Buffers
    vkDestroyCommandPool(this->logicalDevice, this->commandPool, nullptr);
    // Clean up Frame Buffers
    for (auto framebuffer : swapChainFrameBuffers) {
        vkDestroyFramebuffer(this->logicalDevice, framebuffer, nullptr);
    }
    // Clean up Pipeline instances
    vkDestroyPipeline(this->logicalDevice, this->graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(this->logicalDevice, this->pipelineLayout, nullptr);
    vkDestroyRenderPass(this->logicalDevice, this->renderPass, nullptr);
    // Clean up swap chainb Image Views
    for (auto imageView : this->swapChainImageViews) {
        vkDestroyImageView(this->logicalDevice, imageView, nullptr);
    }
    // Clean up Swap Chain, Vulkan logical & physical devices
    vkDestroySwapchainKHR(this->logicalDevice, this->swapChain, nullptr);
    vkDestroyDevice(this->logicalDevice, nullptr);
    vkDestroySurfaceKHR(this->vulkanInstance, this->surface, nullptr);
    vkDestroyInstance(this->vulkanInstance, nullptr);
}