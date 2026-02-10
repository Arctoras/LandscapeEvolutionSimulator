#pragma once

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>


class App
{
public:
    void run();
private:
    void initWindow();

    void initVulkan();
    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createImageViews();

    static uint32_t             chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const& surfaceCapabilities);
    static vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
    static vk::PresentModeKHR   chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);
    vk::Extent2D                chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);
    void createSwapChain();

    [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const;
    void createGraphicsPipeline();

    void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Buffer& buffer, vk::raii::DeviceMemory& bufferMemory);
    void copyBuffer(vk::raii::Buffer& srcBuffer, vk::raii::Buffer& dstBuffer, vk::DeviceSize size);
    [[nodiscard]] uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);

    void createVertexBuffer();
    void createIndexBuffer();
    void createCommandPool();
    void createCommandBuffers();

    void recordCommandBuffer(uint32_t imageIndex);
    void transitionImageLayout(uint32_t imageIndex, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask, vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask);

    void createSyncObjects();

    void drawFrame();

    void cleanupSwapChain();
    void recreateSwapChain();

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    void mainLoop();
    void cleanup();


    void setupDebugMessenger();
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
        vk::DebugUtilsMessageTypeFlagsEXT type,
        const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void*);

private:
    GLFWwindow* window = nullptr;

    vk::raii::Context                    context;
    vk::raii::Instance                   instance = nullptr;
    vk::raii::DebugUtilsMessengerEXT     debugMessenger = nullptr;

    vk::raii::SurfaceKHR                 surface = nullptr;

    vk::raii::PhysicalDevice             physicalDevice = nullptr;
    vk::raii::Device                     device = nullptr;

    uint32_t                             queueIndex = ~0;
    vk::raii::Queue                      queue = nullptr;
    vk::raii::SwapchainKHR               swapChain = nullptr;
    std::vector<vk::Image>               swapChainImages;
    vk::SurfaceFormatKHR                 swapChainSurfaceFormat;
    vk::Extent2D                         swapChainExtent;
    std::vector<vk::raii::ImageView>     swapChainImageViews;

    vk::raii::PipelineLayout             pipelineLayout = nullptr;
    vk::raii::Pipeline		             graphicsPipeline = nullptr;

    vk::raii::Buffer       vertexBuffer = nullptr;
    vk::raii::DeviceMemory vertexBufferMemory = nullptr;
    vk::raii::Buffer       indexBuffer = nullptr;
    vk::raii::DeviceMemory indexBufferMemory = nullptr;

    vk::raii::CommandPool                commandPool = nullptr;
    std::vector<vk::raii::CommandBuffer> commandBuffers;
    
    std::vector<vk::raii::Semaphore>     presentCompleteSemaphores;
    std::vector<vk::raii::Semaphore>     renderFinishedSemaphores;
    std::vector<vk::raii::Fence>         inFlightFences;

    uint32_t                             semaphoreIndex = 0;
    uint32_t                             currentFrame = 0;

    bool framebufferResized = false;

    std::vector<const char*> requiredDeviceExtensions = {
        vk::KHRSwapchainExtensionName,
        vk::KHRSpirv14ExtensionName,
        vk::KHRSynchronization2ExtensionName,
        vk::KHRCreateRenderpass2ExtensionName
    };
};

