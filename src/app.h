#pragma once

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec4 states;

    static vk::VertexInputBindingDescription getBindingDescription()
    {
        return { 0, sizeof( Vertex ), vk::VertexInputRate::eVertex };
    }

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
        return {
            vk::VertexInputAttributeDescription( 0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos) ),
            vk::VertexInputAttributeDescription( 1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal) )
        };
    }
};

constexpr int seed = 0;
constexpr float noiseFrequency = 0.001f;
constexpr float heightScale = 250;
constexpr uint32_t gridWidth = 2000;
constexpr uint32_t gridHeight = 2000;

static_assert(gridWidth <= UINT32_MAX / gridHeight, "Too many vertices in grid");

constexpr uint32_t numVertices = gridWidth * gridHeight;
constexpr uint64_t numIndices = 6 * (gridWidth - 1) * (gridHeight - 1);

static_assert(numVertices % 64 == 0, "Number of vertices is not divisible by 64");

class App
{
public:
    void run();

private:
    void initWindow();
    void mainLoop();
    void cleanup();

    void generateMesh();

    void processInputs();

    void initVulkan();
    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();

    [[nodiscard]] vk::raii::ImageView createImageView( vk::raii::Image &image, vk::Format format, vk::ImageAspectFlags aspectFlags );
    void createImageViews();

    static uint32_t             chooseSwapMinImageCount( vk::SurfaceCapabilitiesKHR const &surfaceCapabilities );
    static vk::SurfaceFormatKHR chooseSwapSurfaceFormat( const std::vector<vk::SurfaceFormatKHR> &availableFormats );
    static vk::PresentModeKHR   chooseSwapPresentMode( const std::vector<vk::PresentModeKHR> &availablePresentModes );
    vk::Extent2D                chooseSwapExtent( const vk::SurfaceCapabilitiesKHR &capabilities );
    void createSwapChain();

    void createDescriptorSetLayout();
    void createComputeDescriptorSetLayout();
    [[nodiscard]] vk::raii::ShaderModule createShaderModule( const std::vector<char> &code ) const;
    void createGraphicsPipeline();
    void createComputePipeline();

    void createBuffer( vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Buffer &buffer, vk::raii::DeviceMemory &bufferMemory );
    void copyBuffer( vk::raii::Buffer &srcBuffer, vk::raii::Buffer &dstBuffer, vk::DeviceSize size );
    [[nodiscard]] uint32_t findMemoryType( uint32_t typeFilter, vk::MemoryPropertyFlags properties );


    void createStorageBuffers();
    void createIndexBuffer();
    void createUniformBuffers();

    void createDescriptorPool();
    void createDescriptorSets();
    void createComputeDescriptorSets();

    void createImage( uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Image &image, vk::raii::DeviceMemory &imageMemory );
    void createDepthResources();

    [[nodiscard]] vk::Format findSupportedFormat( const std::vector<vk::Format> &candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features );
    [[nodiscard]] vk::Format findDepthFormat();

    void createCommandPool();
    void createCommandBuffers();
    void createComputeCommandBuffers();

    void recordCommandBuffer( uint32_t imageIndex );
    void recordComputeCommandBuffer();
    void transitionImageLayout( vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask, vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask, vk::ImageAspectFlags imageAspectFlags );

    void createSyncObjects();

    void updateUniformBuffer( uint32_t currentImage );
    void drawFrame();

    void cleanupSwapChain();
    void recreateSwapChain();

    static void framebufferResizeCallback( GLFWwindow *window, int width, int height );

    void setupDebugMessenger();
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
        vk::DebugUtilsMessageTypeFlagsEXT type,
        const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
        void * );

private:
    GLFWwindow *window = nullptr;

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

    vk::raii::DescriptorSetLayout        descriptorSetLayout = nullptr;
    vk::raii::DescriptorSetLayout        computeDescriptorSetLayout = nullptr;
    vk::raii::PipelineLayout             pipelineLayout = nullptr;
    vk::raii::PipelineLayout             computePipelineLayout = nullptr;
    vk::raii::Pipeline		             graphicsPipeline = nullptr;
    vk::raii::Pipeline                   computePipeline = nullptr;

    vk::raii::Image                      depthImage = nullptr;
    vk::raii::DeviceMemory               depthImageMemory = nullptr;
    vk::raii::ImageView                  depthImageView = nullptr;

    vk::raii::Buffer                     indexBuffer = nullptr;
    vk::raii::DeviceMemory               indexBufferMemory = nullptr;

    vk::raii::Buffer                     dimensionsBuffer = nullptr;
    vk::raii::DeviceMemory               dimensionsBufferMemory = nullptr;

    std::vector<vk::raii::Buffer>        uniformBuffers;
    std::vector<vk::raii::DeviceMemory>  uniformBuffersMemory;
    std::vector<void *>                  uniformBuffersMapped;

    std::vector<vk::raii::Buffer>        shaderStorageBuffers;
    std::vector<vk::raii::DeviceMemory>  shaderStorageBuffersMemory;

    vk::raii::DescriptorPool             descriptorPool = nullptr;
    std::vector<vk::raii::DescriptorSet> descriptorSets;
    std::vector<vk::raii::DescriptorSet> computeDescriptorSets;

    vk::raii::CommandPool                commandPool = nullptr;
    std::vector<vk::raii::CommandBuffer> commandBuffers;
    std::vector<vk::raii::CommandBuffer> computeCommandBuffers;

    vk::raii::Semaphore                  semaphore = nullptr;
    uint64_t						     timelineValue = 0;
    std::vector<vk::raii::Fence>         inFlightFences;
    uint32_t                             currentFrame = 0;

    bool framebufferResized = false;

    std::vector<const char *> requiredDeviceExtensions = {
        vk::KHRSwapchainExtensionName,
        vk::KHRSpirv14ExtensionName,
        vk::KHRSynchronization2ExtensionName,
        vk::KHRCreateRenderpass2ExtensionName
    };

    Vertex   *vertices = nullptr;
    uint32_t *indices = nullptr;

public:
    glm::vec3 cameraPosition = {0, 0, 40};
    glm::vec3 cameraDirection = {-0.5f, -0.5f, -0.5f};
    float speed = 5;
    float mouseSensitivity = 1;

    float prevTime = 0;
    float time = 0;
    float deltaTime = 0;

    glm::vec2 prevCursorPos = { 0, 0 };
    glm::vec2 cursorPos = { 0, 0 };
    glm::vec2 cursorPosDelta = { 0, 0 };
};

