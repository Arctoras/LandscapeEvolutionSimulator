#include "app.h"

#include <iostream>
#include <fstream>

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<char const*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif


void App::run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}


void App::initWindow()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}


void App::initVulkan()
{
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createGraphicsPipeline();
    createCommandPool();
    createCommandBuffers();
    createSyncObjects();
}


std::vector<const char*> getRequiredExtensions()
{
    uint32_t glfwExtensionCount = 0;
    auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (enableValidationLayers) {
        extensions.push_back(vk::EXTDebugUtilsExtensionName);
    }

    return extensions;
}


void App::createInstance()
{
    constexpr vk::ApplicationInfo appInfo{
        .pApplicationName   = "Hello Triangle",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName        = "No Engine",
        .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion         = vk::ApiVersion14
    };

    // Get the required layers
    std::vector<char const*> requiredLayers;
    if (enableValidationLayers) {
        requiredLayers.assign(validationLayers.begin(), validationLayers.end());
    }

    // Check if the required layers are supported by the Vulkan implementation.
    auto layerProperties = context.enumerateInstanceLayerProperties();
    if (std::ranges::any_of(requiredLayers, [&layerProperties](auto const& requiredLayer) {
        return std::ranges::none_of(layerProperties,
            [requiredLayer](auto const& layerProperty)
            { return strcmp(layerProperty.layerName, requiredLayer) == 0; });
        }))
    {
        throw std::runtime_error("One or more required layers are not supported!");
    }

    // Get the required extensions.
    auto requiredExtensions = getRequiredExtensions();

    // Check if the required extensions are supported by the Vulkan implementation.
    auto extensionProperties = context.enumerateInstanceExtensionProperties();
    for (auto const& requiredExtension : requiredExtensions)
    {
        if (std::ranges::none_of(extensionProperties,
            [requiredExtension](auto const& extensionProperty)
            { return strcmp(extensionProperty.extensionName, requiredExtension) == 0; }))
        {
            throw std::runtime_error("Required extension not supported: " + std::string(requiredExtension));
        }
    }

    vk::InstanceCreateInfo createInfo{
        .pApplicationInfo        = &appInfo,
        .enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size()),
        .ppEnabledLayerNames     = requiredLayers.data(),
        .enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size()),
        .ppEnabledExtensionNames = requiredExtensions.data()
    };
    instance = vk::raii::Instance(context, createInfo);
}


void App::createSurface()
{
    VkSurfaceKHR _surface;
    if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0)
    {
        throw std::runtime_error("failed to create window surface!");
    }
    surface = vk::raii::SurfaceKHR(instance, _surface);
}


void App::pickPhysicalDevice()
{
    std::vector<vk::raii::PhysicalDevice> devices = instance.enumeratePhysicalDevices();
    const auto devIter = std::ranges::find_if(devices,
        [&](auto const& device) {
            auto queueFamilies = device.getQueueFamilyProperties();
            bool isSuitable = device.getProperties().apiVersion >= VK_API_VERSION_1_3;
            const auto qfpIter = std::ranges::find_if(queueFamilies,
                [](vk::QueueFamilyProperties const& qfp)
                {
                    return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
                });
            isSuitable = isSuitable && (qfpIter != queueFamilies.end());
            auto extensions = device.enumerateDeviceExtensionProperties();
            bool found = true;
            for (auto const& extension : deviceExtensions) {
                auto extensionIter = std::ranges::find_if(extensions, [extension](auto const& ext) {return strcmp(ext.extensionName, extension) == 0; });
                found = found && extensionIter != extensions.end();
            }
            isSuitable = isSuitable && found;
            if (isSuitable) {
                physicalDevice = device;
            }
            return isSuitable;
        });
    if (devIter == devices.end()) {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}


void App::createLogicalDevice()
{
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

    // get the first index into queueFamilyProperties which supports both graphics and present
    for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
    {
        if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
            physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface))
        {
            // found a queue family that supports both graphics and present
            queueIndex = qfpIndex;
            break;
        }
    }
    if (queueIndex == ~0)
    {
        throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
    }

    // query for Vulkan 1.3 features
    vk::StructureChain<
        vk::PhysicalDeviceFeatures2, 
        vk::PhysicalDeviceVulkan11Features,
        vk::PhysicalDeviceVulkan13Features,
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain = {
            {},                                   // vk::PhysicalDeviceFeatures2
            {.shaderDrawParameters = true},       // vk::PhysicalDeviceVulkan11Features
            {.dynamicRendering = true},           // vk::PhysicalDeviceVulkan13Features
            {.extendedDynamicState = true}        // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
    };

    // create a Device
    float                     queuePriority = 0.5f;
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo{ 
        .queueFamilyIndex = queueIndex,
        .queueCount       = 1,
        .pQueuePriorities = &queuePriority
    };
    vk::DeviceCreateInfo      deviceCreateInfo{
        .pNext                   = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
        .queueCreateInfoCount    = 1,
        .pQueueCreateInfos       = &deviceQueueCreateInfo,
        .enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data()
    };

    device = vk::raii::Device(physicalDevice, deviceCreateInfo);
    queue = vk::raii::Queue(device, queueIndex, 0);
}


uint32_t App::chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const& surfaceCapabilities)
{
    auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
    if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount))
    {
        minImageCount = surfaceCapabilities.maxImageCount;
    }
    return minImageCount;
}


vk::SurfaceFormatKHR App::chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const& availableFormats)
{
    assert(!availableFormats.empty());
    const auto formatIt = std::ranges::find_if(
        availableFormats,
        [](const auto& format) { return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear; });
    return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
}


vk::PresentModeKHR App::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
{
    assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) { return presentMode == vk::PresentModeKHR::eFifo; }));
    return std::ranges::any_of(availablePresentModes,
        [](const vk::PresentModeKHR value) { return vk::PresentModeKHR::eMailbox == value; }) ?
        vk::PresentModeKHR::eMailbox :
        vk::PresentModeKHR::eFifo;
}


vk::Extent2D App::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    return {
        std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
    };
}


void App::createSwapChain()
{
    auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
    swapChainExtent = chooseSwapExtent(surfaceCapabilities);
    swapChainSurfaceFormat = chooseSwapSurfaceFormat(physicalDevice.getSurfaceFormatsKHR(*surface));
    vk::SwapchainCreateInfoKHR swapChainCreateInfo{ 
        .surface = *surface,
        .minImageCount = chooseSwapMinImageCount(surfaceCapabilities),
        .imageFormat = swapChainSurfaceFormat.format,
        .imageColorSpace = swapChainSurfaceFormat.colorSpace,
        .imageExtent = swapChainExtent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .preTransform = surfaceCapabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = chooseSwapPresentMode(physicalDevice.getSurfacePresentModesKHR(*surface)),
        .clipped = true
    };

    swapChain = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
    swapChainImages = swapChain.getImages();
}


void App::createImageViews()
{
    assert(swapChainImageViews.empty());

    vk::ImageViewCreateInfo imageViewCreateInfo{ .viewType = vk::ImageViewType::e2D, .format = swapChainSurfaceFormat.format, .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1} };
    for (auto image : swapChainImages)
    {
        imageViewCreateInfo.image = image;
        swapChainImageViews.emplace_back(device, imageViewCreateInfo);
    }
}


vk::raii::ShaderModule App::createShaderModule(const std::vector<char>& code) const
{
    vk::ShaderModuleCreateInfo createInfo{ .codeSize = code.size() * sizeof(char), .pCode = reinterpret_cast<const uint32_t*>(code.data()) };
    vk::raii::ShaderModule     shaderModule{ device, createInfo };

    return shaderModule;
}


static std::vector<char> readFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file!");
    }
    std::vector<char> buffer(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    file.close();
    return buffer;
}


void App::createGraphicsPipeline()
{
    vk::raii::ShaderModule shaderModule = createShaderModule(readFile("shaders/base.spv"));

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo{ .stage = vk::ShaderStageFlagBits::eVertex, .module = shaderModule, .pName = "vertMain" };
    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{ .stage = vk::ShaderStageFlagBits::eFragment, .module = shaderModule, .pName = "fragMain" };
    vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    vk::PipelineVertexInputStateCreateInfo   vertexInputInfo;
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{ .topology = vk::PrimitiveTopology::eTriangleList };
    vk::PipelineViewportStateCreateInfo      viewportState{ .viewportCount = 1, .scissorCount = 1 };

    vk::PipelineRasterizationStateCreateInfo rasterizer{ .depthClampEnable = vk::False, .rasterizerDiscardEnable = vk::False, .polygonMode = vk::PolygonMode::eFill, .cullMode = vk::CullModeFlagBits::eBack, .frontFace = vk::FrontFace::eClockwise, .depthBiasEnable = vk::False, .depthBiasSlopeFactor = 1.0f, .lineWidth = 1.0f };

    vk::PipelineMultisampleStateCreateInfo multisampling{ .rasterizationSamples = vk::SampleCountFlagBits::e1, .sampleShadingEnable = vk::False };

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{ .blendEnable = vk::False,
                                                               .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA };

    vk::PipelineColorBlendStateCreateInfo colorBlending{ .logicOpEnable = vk::False, .logicOp = vk::LogicOp::eCopy, .attachmentCount = 1, .pAttachments = &colorBlendAttachment };

    std::vector dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor };
    vk::PipelineDynamicStateCreateInfo dynamicState{ .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()), .pDynamicStates = dynamicStates.data() };

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{ .setLayoutCount = 0, .pushConstantRangeCount = 0 };

    pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
        {.stageCount = 2,
         .pStages = shaderStages,
         .pVertexInputState = &vertexInputInfo,
         .pInputAssemblyState = &inputAssembly,
         .pViewportState = &viewportState,
         .pRasterizationState = &rasterizer,
         .pMultisampleState = &multisampling,
         .pColorBlendState = &colorBlending,
         .pDynamicState = &dynamicState,
         .layout = pipelineLayout,
         .renderPass = nullptr},
        {.colorAttachmentCount = 1, .pColorAttachmentFormats = &swapChainSurfaceFormat.format} };

    graphicsPipeline = vk::raii::Pipeline(device, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
}


void App::createCommandPool()
{
    vk::CommandPoolCreateInfo poolInfo{ .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer, .queueFamilyIndex = queueIndex };
    commandPool = vk::raii::CommandPool(device, poolInfo);
}


void App::createCommandBuffers()
{
    commandBuffers.clear();

    vk::CommandBufferAllocateInfo allocInfo{ 
        .commandPool        = commandPool,
        .level              = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = MAX_FRAMES_IN_FLIGHT
    };

    commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
}


void App::recordCommandBuffer(uint32_t imageIndex)
{
    commandBuffers[currentFrame].begin({});

    // Before starting rendering, transition the swapchain image to COLOR_ATTACHMENT_OPTIMAL
    transitionImageLayout(
        imageIndex,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        {},                                                         // srcAccessMask (no need to wait for previous operations)
        vk::AccessFlagBits2::eColorAttachmentWrite,                 // dstAccessMask
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,         // srcStage
        vk::PipelineStageFlagBits2::eColorAttachmentOutput          // dstStage
    );

    vk::ClearValue              clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
    vk::RenderingAttachmentInfo attachmentInfo = {
        .imageView = swapChainImageViews[imageIndex],
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = clearColor
    };
    vk::RenderingInfo renderingInfo = {
        .renderArea = {.offset = {0, 0}, .extent = swapChainExtent},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachmentInfo
    };

    commandBuffers[currentFrame].beginRendering(renderingInfo);
    commandBuffers[currentFrame].bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
    commandBuffers[currentFrame].setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
    commandBuffers[currentFrame].setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));
    commandBuffers[currentFrame].draw(3, 1, 0, 0);
    commandBuffers[currentFrame].endRendering();

    // After rendering, transition the swapchain image to PRESENT_SRC
    transitionImageLayout(
        imageIndex,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits2::eColorAttachmentWrite,                // srcAccessMask
        {},                                                        // dstAccessMask
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // srcStage
        vk::PipelineStageFlagBits2::eBottomOfPipe                  // dstStage
    );

    commandBuffers[currentFrame].end();
}


void App::transitionImageLayout(
    uint32_t imageIndex,
    vk::ImageLayout oldLayout,
    vk::ImageLayout newLayout,
    vk::AccessFlags2 srcAccessMask,
    vk::AccessFlags2 dstAccessMask,
    vk::PipelineStageFlags2 srcStageMask,
    vk::PipelineStageFlags2 dstStageMask
) {
    vk::ImageMemoryBarrier2 barrier = {
        .srcStageMask = srcStageMask,
        .srcAccessMask = srcAccessMask,
        .dstStageMask = dstStageMask,
        .dstAccessMask = dstAccessMask,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = swapChainImages[imageIndex],
        .subresourceRange = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    vk::DependencyInfo dependencyInfo = {
        .dependencyFlags = {},
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier
    };
    commandBuffers[currentFrame].pipelineBarrier2(dependencyInfo);
}

    
void App::createSyncObjects()
{
    presentCompleteSemaphores.clear();
    renderFinishedSemaphores.clear();
    inFlightFences.clear();

    for (size_t i = 0; i < swapChainImages.size(); i++)
    {
        presentCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
        renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        inFlightFences.emplace_back(device, vk::FenceCreateInfo{ .flags = vk::FenceCreateFlagBits::eSignaled });
    }
}


void App::drawFrame()
{
    while (vk::Result::eTimeout == device.waitForFences(*inFlightFences[currentFrame], vk::True, UINT64_MAX))
        ;
    auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphores[semaphoreIndex], nullptr);

    if (result == vk::Result::eErrorOutOfDateKHR)
    {
        recreateSwapChain();
        return;
    }
    if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
    {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    device.resetFences(*inFlightFences[currentFrame]);
    commandBuffers[currentFrame].reset();
    recordCommandBuffer(imageIndex);

    vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    const vk::SubmitInfo   submitInfo{ .waitSemaphoreCount = 1, .pWaitSemaphores = &*presentCompleteSemaphores[semaphoreIndex], .pWaitDstStageMask = &waitDestinationStageMask, .commandBufferCount = 1, .pCommandBuffers = &*commandBuffers[currentFrame], .signalSemaphoreCount = 1, .pSignalSemaphores = &*renderFinishedSemaphores[imageIndex] };
    queue.submit(submitInfo, *inFlightFences[currentFrame]);

    try
    {
        const vk::PresentInfoKHR presentInfoKHR{ .waitSemaphoreCount = 1, .pWaitSemaphores = &*renderFinishedSemaphores[imageIndex], .swapchainCount = 1, .pSwapchains = &*swapChain, .pImageIndices = &imageIndex };
        result = queue.presentKHR(presentInfoKHR);
        if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || framebufferResized)
        {
            framebufferResized = false;
            recreateSwapChain();
        }
        else if (result != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to present swap chain image!");
        }
    }
    catch (const vk::SystemError& e)
    {
        if (e.code().value() == static_cast<int>(vk::Result::eErrorOutOfDateKHR))
        {
            recreateSwapChain();
            return;
        }
        else
        {
            throw;
        }
    }

    semaphoreIndex = (semaphoreIndex + 1) % presentCompleteSemaphores.size();
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}


void App::cleanupSwapChain()
{
    swapChainImageViews.clear();
    swapChain = nullptr;
}


void App::recreateSwapChain()
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    device.waitIdle();

    cleanupSwapChain();
    createSwapChain();
    createImageViews();
}


void App::framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    auto app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
    app->framebufferResized = true;
}


void App::mainLoop()
{
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        drawFrame();
    }

    device.waitIdle();
}


void App::cleanup()
{
    glfwDestroyWindow(window);

    glfwTerminate();
}


void App::setupDebugMessenger() 
{
    if (!enableValidationLayers) return;

    vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    vk::DebugUtilsMessageTypeFlagsEXT     messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
    vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{
        .messageSeverity = severityFlags,
        .messageType = messageTypeFlags,
        .pfnUserCallback = &debugCallback
    };

    debugMessenger = instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL App::debugCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
    vk::DebugUtilsMessageTypeFlagsEXT type,
    const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void*)
{
    std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;

    return vk::False;
}
