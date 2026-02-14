#include "app.h"

#include <chrono>
#include <iostream>
#include <fstream>
#include <ranges>

#include <glm/gtc/matrix_transform.hpp>


constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<char const *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif


struct UniformBufferObject
{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;

    glm::vec3 lightDir;
};


static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
	{
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
	}
}


static void cursorPositionCallback(GLFWwindow *window, double xPos, double yPos)
{
    App *app = static_cast<App *>(glfwGetWindowUserPointer(window));
	app->cursorPos = { xPos, yPos };
}


static void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
    if (glfwGetWindowAttrib(window, GLFW_HOVERED))
    {
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            if (glfwRawMouseMotionSupported())
                glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        }
    }
}

static void scrollCallback(GLFWwindow* window, double xOffset, double yOffset)
{
    App* app = static_cast<App*>(glfwGetWindowUserPointer(window));

    if( yOffset < 0 )
    {
		app->speed *= 0.9f;
	} 
	else if(yOffset > 0)
	{
		app->speed *= 1.1f;
	}
}


void App::run()
{
    generateMesh();

    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}


void App::initWindow()
{
    glfwInit();

    glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
    glfwWindowHint( GLFW_RESIZABLE, GLFW_TRUE );

    window = glfwCreateWindow( WIDTH, HEIGHT, "Vulkan", nullptr, nullptr );
    glfwSetWindowUserPointer( window, this );
    glfwSetFramebufferSizeCallback( window, framebufferResizeCallback );

    glfwSetCursorPosCallback(window, cursorPositionCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetScrollCallback(window, scrollCallback);

    cameraDirection = glm::normalize(cameraDirection);
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
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createCommandPool();
    createDepthResources();
    createTextureImage();
    createTextureImageView();
    createTextureSampler();
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
    createSyncObjects();
}


std::vector<const char *> getRequiredExtensions()
{
    uint32_t glfwExtensionCount = 0;
    auto glfwExtensions = glfwGetRequiredInstanceExtensions( &glfwExtensionCount );

    std::vector extensions( glfwExtensions, glfwExtensions + glfwExtensionCount );
    if( enableValidationLayers )
    {
        extensions.push_back( vk::EXTDebugUtilsExtensionName );
    }

    return extensions;
}


void App::createInstance()
{
    constexpr vk::ApplicationInfo appInfo{
        .pApplicationName   = "Hello Triangle",
        .applicationVersion = VK_MAKE_VERSION( 1, 0, 0 ),
        .pEngineName        = "No Engine",
        .engineVersion      = VK_MAKE_VERSION( 1, 0, 0 ),
        .apiVersion         = vk::ApiVersion14
    };

    // Get the required layers
    std::vector<char const *> requiredLayers;
    if( enableValidationLayers )
    {
        requiredLayers.assign( validationLayers.begin(), validationLayers.end() );
    }

    // Check if the required layers are supported by the Vulkan implementation.
    auto layerProperties = context.enumerateInstanceLayerProperties();
    for( auto const &requiredLayer : requiredLayers )
    {
        if( std::ranges::none_of( layerProperties,
                                  [requiredLayer]( auto const &layerProperty )
                                  {
                                      return strcmp( layerProperty.layerName, requiredLayer ) == 0;
                                  } ) )
        {
            throw std::runtime_error( "Required layer not supported: " + std::string( requiredLayer ) );
        }
    }

    // Get the required extensions.
    auto requiredExtensions = getRequiredExtensions();

    // Check if the required extensions are supported by the Vulkan implementation.
    auto extensionProperties = context.enumerateInstanceExtensionProperties();
    for( auto const &requiredExtension : requiredExtensions )
    {
        if( std::ranges::none_of( extensionProperties,
                                  [requiredExtension]( auto const &extensionProperty )
                                  {
                                      return strcmp( extensionProperty.extensionName, requiredExtension ) == 0;
                                  } ) )
        {
            throw std::runtime_error( "Required extension not supported: " + std::string( requiredExtension ) );
        }
    }

    vk::InstanceCreateInfo createInfo{
        .pApplicationInfo        = &appInfo,
        .enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size()),
        .ppEnabledLayerNames     = requiredLayers.data(),
        .enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size()),
        .ppEnabledExtensionNames = requiredExtensions.data()
    };
    instance = vk::raii::Instance( context, createInfo );
}


void App::createSurface()
{
    VkSurfaceKHR _surface;
    if( glfwCreateWindowSurface( *instance, window, nullptr, &_surface ) != 0 )
    {
        throw std::runtime_error( "failed to create window surface!" );
    }
    surface = vk::raii::SurfaceKHR( instance, _surface );
}


void App::pickPhysicalDevice()
{
    std::vector<vk::raii::PhysicalDevice> devices = instance.enumeratePhysicalDevices();
    const auto                            devIter = std::ranges::find_if(
        devices,
        [&]( auto const &device )
        {
// Check if the device supports the Vulkan 1.3 API version
            bool supportsVulkan1_3 = device.getProperties().apiVersion >= VK_API_VERSION_1_3;

            // Check if any of the queue families support graphics operations
            auto queueFamilies = device.getQueueFamilyProperties();
            bool supportsGraphics =
                std::ranges::any_of( queueFamilies, []( auto const &qfp )
                                     {
                                         return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics);
                                     } );

// Check if all required device extensions are available
            auto availableDeviceExtensions = device.enumerateDeviceExtensionProperties();
            bool supportsAllRequiredExtensions =
                std::ranges::all_of( requiredDeviceExtensions,
                                     [&availableDeviceExtensions]( auto const &requiredDeviceExtension )
                                     {
                                         return std::ranges::any_of( availableDeviceExtensions,
                                                                     [requiredDeviceExtension]( auto const &availableDeviceExtension )
                                                                     {
                                                                         return strcmp( availableDeviceExtension.extensionName, requiredDeviceExtension ) == 0;
                                                                     } );
                                     } );

            auto features = device.template getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
            bool supportsRequiredFeatures =
                features.template get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy &&
                features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
                features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

            return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
        } );
    if( devIter != devices.end() )
    {
        physicalDevice = *devIter;
    } else
    {
        throw std::runtime_error( "failed to find a suitable GPU!" );
    }
}


void App::createLogicalDevice()
{
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

    // get the first index into queueFamilyProperties which supports both graphics and present
    for( uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++ )
    {
        if( (queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
            physicalDevice.getSurfaceSupportKHR( qfpIndex, *surface ) )
        {
            // found a queue family that supports both graphics and present
            queueIndex = qfpIndex;
            break;
        }
    }
    if( queueIndex == ~0 )
    {
        throw std::runtime_error( "Could not find a queue for graphics and present -> terminating" );
    }

    // query for Vulkan 1.3 features
    vk::StructureChain<
        vk::PhysicalDeviceFeatures2,
        vk::PhysicalDeviceVulkan11Features,
        vk::PhysicalDeviceVulkan13Features,
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain = {
            {.features = {.samplerAnisotropy = true }},           // vk::PhysicalDeviceFeatures2
            {.shaderDrawParameters = true},                       // vk::PhysicalDeviceVulkan11Features
            {.synchronization2 = true, .dynamicRendering = true}, // vk::PhysicalDeviceVulkan13Features
            {.extendedDynamicState = true}                        // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
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
        .enabledExtensionCount   = static_cast<uint32_t>(requiredDeviceExtensions.size()),
        .ppEnabledExtensionNames = requiredDeviceExtensions.data()
    };

    device = vk::raii::Device( physicalDevice, deviceCreateInfo );
    queue = vk::raii::Queue( device, queueIndex, 0 );
}


uint32_t App::chooseSwapMinImageCount( vk::SurfaceCapabilitiesKHR const &surfaceCapabilities )
{
    auto minImageCount = std::max( 3u, surfaceCapabilities.minImageCount );
    if( (0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount) )
    {
        minImageCount = surfaceCapabilities.maxImageCount;
    }
    return minImageCount;
}


vk::SurfaceFormatKHR App::chooseSwapSurfaceFormat( std::vector<vk::SurfaceFormatKHR> const &availableFormats )
{
    assert( !availableFormats.empty() );
    const auto formatIt = std::ranges::find_if(
        availableFormats,
        []( const auto &format )
        {
            return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
        } );
    return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
}


vk::PresentModeKHR App::chooseSwapPresentMode( const std::vector<vk::PresentModeKHR> &availablePresentModes )
{
    assert( std::ranges::any_of( availablePresentModes, []( auto presentMode )
                                 {
                                     return presentMode == vk::PresentModeKHR::eFifo;
                                 } ) );
    return std::ranges::any_of( availablePresentModes,
                                []( const vk::PresentModeKHR value )
                                {
                                    return vk::PresentModeKHR::eMailbox == value;
                                } ) ?
        vk::PresentModeKHR::eMailbox :
                                    vk::PresentModeKHR::eFifo;
}


vk::Extent2D App::chooseSwapExtent( const vk::SurfaceCapabilitiesKHR &capabilities )
{
    if( capabilities.currentExtent.width != 0xFFFFFFFF )
    {
        return capabilities.currentExtent;
    }
    int width, height;
    glfwGetFramebufferSize( window, &width, &height );

    return {
        std::clamp<uint32_t>( width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width ),
        std::clamp<uint32_t>( height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height )
    };
}


void App::createSwapChain()
{
    auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR( *surface );
    swapChainExtent = chooseSwapExtent( surfaceCapabilities );
    swapChainSurfaceFormat = chooseSwapSurfaceFormat( physicalDevice.getSurfaceFormatsKHR( *surface ) );
    vk::SwapchainCreateInfoKHR swapChainCreateInfo{
        .surface          = *surface,
        .minImageCount    = chooseSwapMinImageCount( surfaceCapabilities ),
        .imageFormat      = swapChainSurfaceFormat.format,
        .imageColorSpace  = swapChainSurfaceFormat.colorSpace,
        .imageExtent      = swapChainExtent,
        .imageArrayLayers = 1,
        .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .preTransform     = surfaceCapabilities.currentTransform,
        .compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode      = chooseSwapPresentMode( physicalDevice.getSurfacePresentModesKHR( *surface ) ),
        .clipped          = true
    };

    swapChain = vk::raii::SwapchainKHR( device, swapChainCreateInfo );
    swapChainImages = swapChain.getImages();
}


vk::raii::ImageView App::createImageView( vk::raii::Image &image, vk::Format format, vk::ImageAspectFlags aspectFlags )
{
    vk::ImageViewCreateInfo imageViewCreateInfo{
        .image            = image,
        .viewType         = vk::ImageViewType::e2D,
        .format           = format,
        .subresourceRange = {aspectFlags, 0, 1, 0, 1}
    };
    return vk::raii::ImageView( device, imageViewCreateInfo );
}


void App::createImageViews()
{
    assert( swapChainImageViews.empty() );

    vk::ImageViewCreateInfo imageViewCreateInfo{ .viewType = vk::ImageViewType::e2D, .format = swapChainSurfaceFormat.format, .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1} };
    for( auto image : swapChainImages )
    {
        imageViewCreateInfo.image = image;
        swapChainImageViews.emplace_back( device, imageViewCreateInfo );
    }
}


void App::createDescriptorSetLayout()
{
    std::array bindings = {
        vk::DescriptorSetLayoutBinding( 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, nullptr ),
        vk::DescriptorSetLayoutBinding( 1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr )
    };

    vk::DescriptorSetLayoutCreateInfo layoutInfo{ .bindingCount = static_cast<uint32_t>(bindings.size()), .pBindings = bindings.data() };
    descriptorSetLayout = vk::raii::DescriptorSetLayout( device, layoutInfo );
}


vk::raii::ShaderModule App::createShaderModule( const std::vector<char> &code ) const
{
    vk::ShaderModuleCreateInfo createInfo{ .codeSize = code.size() * sizeof( char ), .pCode = reinterpret_cast<const uint32_t *>(code.data()) };
    vk::raii::ShaderModule     shaderModule{ device, createInfo };

    return shaderModule;
}


static std::vector<char> readFile( const std::string &filename )
{
    std::ifstream file( filename, std::ios::ate | std::ios::binary );
    if( !file.is_open() )
    {
        throw std::runtime_error( "failed to open file!" );
    }
    std::vector<char> buffer( file.tellg() );
    file.seekg( 0, std::ios::beg );
    file.read( buffer.data(), static_cast<std::streamsize>(buffer.size()) );
    file.close();
    return buffer;
}


void App::createGraphicsPipeline()
{
    vk::raii::ShaderModule shaderModule = createShaderModule( readFile( "shaders/base.spv" ) );

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
       .stage  = vk::ShaderStageFlagBits::eVertex,
       .module = shaderModule,
       .pName  = "vertMain"
    };
    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
        .stage  = vk::ShaderStageFlagBits::eFragment,
        .module = shaderModule,
        .pName  = "fragMain"
    };
    vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    auto bindingDescription    = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
        .vertexBindingDescriptionCount   = 1,
        .pVertexBindingDescriptions      = &bindingDescription,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
        .pVertexAttributeDescriptions    = attributeDescriptions.data()
    };
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
        .topology = vk::PrimitiveTopology::eTriangleList
    };
    vk::PipelineViewportStateCreateInfo      viewportState{
        .viewportCount = 1,
        .scissorCount  = 1
    };

    vk::PipelineRasterizationStateCreateInfo rasterizer{
        .depthClampEnable        = vk::False,
        .rasterizerDiscardEnable = vk::False,
        .polygonMode             = vk::PolygonMode::eFill,
        .cullMode                = vk::CullModeFlagBits::eBack,
        .frontFace               = vk::FrontFace::eCounterClockwise,
        .depthBiasEnable         = vk::False,
        .depthBiasSlopeFactor    = 1.0f,
        .lineWidth               = 1.0f
    };

    vk::PipelineMultisampleStateCreateInfo multisampling{
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable  = vk::False
    };

    vk::PipelineDepthStencilStateCreateInfo depthStencil{
        .depthTestEnable       = vk::True,
        .depthWriteEnable      = vk::True,
        .depthCompareOp        = vk::CompareOp::eLess,
        .depthBoundsTestEnable = vk::False,
        .stencilTestEnable     = vk::False
    };

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable    = vk::False,
        .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
    };

    vk::PipelineColorBlendStateCreateInfo colorBlending{
        .logicOpEnable   = vk::False,
        .logicOp         = vk::LogicOp::eCopy,
        .attachmentCount = 1,
        .pAttachments    = &colorBlendAttachment
    };

    std::vector dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor };
    vk::PipelineDynamicStateCreateInfo dynamicState{
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates    = dynamicStates.data()
    };

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
        .setLayoutCount         = 1,
        .pSetLayouts            = &*descriptorSetLayout,
        .pushConstantRangeCount = 0
    };

    pipelineLayout = vk::raii::PipelineLayout( device, pipelineLayoutInfo );

    vk::Format depthFormat = findDepthFormat();

    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
        {
            .stageCount          = 2,
            .pStages             = shaderStages,
            .pVertexInputState   = &vertexInputInfo,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState      = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState   = &multisampling,
            .pDepthStencilState  = &depthStencil,
            .pColorBlendState    = &colorBlending,
            .pDynamicState       = &dynamicState,
            .layout              = pipelineLayout,
            .renderPass          = nullptr
        },
        {
            .colorAttachmentCount    = 1,
            .pColorAttachmentFormats = &swapChainSurfaceFormat.format,
            .depthAttachmentFormat   = depthFormat
        } 
    };

    graphicsPipeline = vk::raii::Pipeline( device, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>() );
}


void App::createDescriptorPool()
{
    std::array poolSize{
        vk::DescriptorPoolSize( vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT ),
        vk::DescriptorPoolSize( vk::DescriptorType::eCombinedImageSampler, MAX_FRAMES_IN_FLIGHT )
    };
    vk::DescriptorPoolCreateInfo poolInfo{
        .flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets       = MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount = static_cast<uint32_t>(poolSize.size()),
        .pPoolSizes    = poolSize.data()
    };

    descriptorPool = vk::raii::DescriptorPool( device, poolInfo );
}


void App::createDescriptorSets()
{
    std::vector<vk::DescriptorSetLayout> layouts( MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout );
    vk::DescriptorSetAllocateInfo allocInfo{
        .descriptorPool     = descriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
        .pSetLayouts        = layouts.data()
    };

    descriptorSets.clear();
    descriptorSets = device.allocateDescriptorSets( allocInfo );

    for( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        vk::DescriptorBufferInfo bufferInfo{
            .buffer = uniformBuffers[i],
            .offset = 0,
            .range  = sizeof( UniformBufferObject )
        };
        vk::DescriptorImageInfo imageInfo{
            .sampler     = textureSampler,
            .imageView   = textureImageView,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
        };
        std::array descriptorWrites{
            vk::WriteDescriptorSet{
                .dstSet          = descriptorSets[i],
                .dstBinding      = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType  = vk::DescriptorType::eUniformBuffer,
                .pBufferInfo     = &bufferInfo
            },
            vk::WriteDescriptorSet{
                .dstSet          = descriptorSets[i],
                .dstBinding      = 1,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType  = vk::DescriptorType::eCombinedImageSampler,
                .pImageInfo      = &imageInfo
            }
        };
        device.updateDescriptorSets( descriptorWrites, {} );
    }
}


static bool hasStencilComponent( vk::Format format )
{
    return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
}


void App::createDepthResources()
{
    vk::Format depthFormat = findDepthFormat();

    createImage( swapChainExtent.width, swapChainExtent.height, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal, depthImage, depthImageMemory );
    depthImageView = createImageView( depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth );
}


vk::Format App::findSupportedFormat( const std::vector<vk::Format> &candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features )
{
    auto formatIt = std::ranges::find_if( candidates, [&]( auto const format )
                                          {
                                              vk::FormatProperties props = physicalDevice.getFormatProperties( format );
                                              return (((tiling == vk::ImageTiling::eLinear) && ((props.linearTilingFeatures & features) == features)) ||
                                                       ((tiling == vk::ImageTiling::eOptimal) && ((props.optimalTilingFeatures & features) == features)));
                                          } );
    if( formatIt == candidates.end() )
    {
        throw std::runtime_error( "failed to find supported format!" );
    }
    return *formatIt;
}


vk::Format App::findDepthFormat()
{
    return findSupportedFormat(
        { vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment );
}


void App::createCommandPool()
{
    vk::CommandPoolCreateInfo poolInfo{
        .flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = queueIndex
    };
    commandPool = vk::raii::CommandPool( device, poolInfo );
}


void App::createTextureImage()
{
    int texWidth = 512, texHeight = 512, texChannels = 4;
    vk::DeviceSize imageSize = texWidth * texHeight * texChannels;
    uint32_t *pixels = static_cast<uint32_t *>(calloc( texWidth * texHeight, sizeof( uint32_t ) ));

    if( !pixels )
    {
        throw std::runtime_error( "failed to load texture image!" );
    }

    // Create texture
    for( int y = 0; y < texHeight; y++ )
    {
        for( int x = 0; x < texWidth; x++ )
        {
            uint8_t r, g, b = 255, a = 255;

            r = static_cast<uint8_t>( (glm::sin( (2 * y * glm::two_pi<float>()) / texHeight ) + 1) * 122.5f );
            g = static_cast<uint8_t>( (glm::cos( (2 * x * glm::two_pi<float>()) / texWidth ) + 1) * 122.5f );

            pixels[y * texWidth + x] = r +
                (static_cast<uint32_t>(g) << 8) +
                (static_cast<uint32_t>(b) << 16) +
                (static_cast<uint32_t>(a) << 24);
        }
    }

    vk::raii::Buffer       stagingBuffer( {} );
    vk::raii::DeviceMemory stagingBufferMemory( {} );
    createBuffer( imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory );

    void *data = stagingBufferMemory.mapMemory( 0, imageSize );
    memcpy( data, pixels, imageSize );
    stagingBufferMemory.unmapMemory();

    free( pixels );

    createImage( texWidth, texHeight, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal, textureImage, textureImageMemory );

    transitionImageLayout( textureImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal );
    copyBufferToImage( stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight) );
    transitionImageLayout( textureImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal );
}


void App::createImage( uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Image &image, vk::raii::DeviceMemory &imageMemory )
{
    vk::ImageCreateInfo imageInfo{
        .imageType   = vk::ImageType::e2D,
        .format      = format,
        .extent      = {width, height, 1},
        .mipLevels   = 1,
        .arrayLayers = 1,
        .samples     = vk::SampleCountFlagBits::e1,
        .tiling      = tiling,
        .usage       = usage,
        .sharingMode = vk::SharingMode::eExclusive
    };

    image = vk::raii::Image( device, imageInfo );

    vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo{ .allocationSize = memRequirements.size,
                                     .memoryTypeIndex = findMemoryType( memRequirements.memoryTypeBits, properties ) };
    imageMemory = vk::raii::DeviceMemory( device, allocInfo );
    image.bindMemory( imageMemory, 0 );
}


void App::transitionImageLayout( const vk::raii::Image &image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout )
{
    auto commandBuffer = beginSingleTimeCommands();

    vk::ImageMemoryBarrier barrier{ .oldLayout = oldLayout, .newLayout = newLayout, .image = image, .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1} };

    vk::PipelineStageFlags sourceStage;
    vk::PipelineStageFlags destinationStage;

    if( oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal )
    {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    } else if( oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal )
    {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    } else
    {
        throw std::invalid_argument( "unsupported layout transition!" );
    }
    commandBuffer->pipelineBarrier( sourceStage, destinationStage, {}, {}, nullptr, barrier );
    endSingleTimeCommands( *commandBuffer );
}


void App::copyBufferToImage( const vk::raii::Buffer &buffer, vk::raii::Image &image, uint32_t width, uint32_t height )
{
    std::unique_ptr<vk::raii::CommandBuffer> commandBuffer = beginSingleTimeCommands();
    vk::BufferImageCopy region{
        .bufferOffset      = 0,
        .bufferRowLength   = 0,
        .bufferImageHeight = 0,
        .imageSubresource  = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
        .imageOffset       = {0, 0, 0},
        .imageExtent       = {width, height, 1}
    };
    commandBuffer->copyBufferToImage( buffer, image, vk::ImageLayout::eTransferDstOptimal, { region } );
    endSingleTimeCommands( *commandBuffer );
}

std::unique_ptr<vk::raii::CommandBuffer> App::beginSingleTimeCommands()
{
    vk::CommandBufferAllocateInfo            allocInfo{ .commandPool = commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1 };
    std::unique_ptr<vk::raii::CommandBuffer> commandBuffer = std::make_unique<vk::raii::CommandBuffer>( std::move( vk::raii::CommandBuffers( device, allocInfo ).front() ) );

    vk::CommandBufferBeginInfo beginInfo{ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
    commandBuffer->begin( beginInfo );

    return commandBuffer;
}

void App::endSingleTimeCommands( vk::raii::CommandBuffer &commandBuffer )
{
    commandBuffer.end();

    vk::SubmitInfo submitInfo{ .commandBufferCount = 1, .pCommandBuffers = &*commandBuffer };
    queue.submit( submitInfo, nullptr );
    queue.waitIdle();
}


void App::createTextureImageView()
{
    textureImageView = createImageView( textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor );
}


void App::createTextureSampler()
{
    vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();
    vk::SamplerCreateInfo        samplerInfo{
        .magFilter        = vk::Filter::eLinear,
        .minFilter        = vk::Filter::eLinear,
        .mipmapMode       = vk::SamplerMipmapMode::eLinear,
        .addressModeU     = vk::SamplerAddressMode::eRepeat,
        .addressModeV     = vk::SamplerAddressMode::eRepeat,
        .addressModeW     = vk::SamplerAddressMode::eRepeat,
        .mipLodBias       = 0.0f,
        .anisotropyEnable = vk::True,
        .maxAnisotropy    = properties.limits.maxSamplerAnisotropy,
        .compareEnable    = vk::False,
        .compareOp        = vk::CompareOp::eAlways
    };
    textureSampler = vk::raii::Sampler( device, samplerInfo );
}


void App::createVertexBuffer()
{
    vk::DeviceSize         bufferSize = sizeof( vertices[0] ) * vertices.size();
    vk::raii::Buffer       stagingBuffer( {} );
    vk::raii::DeviceMemory stagingBufferMemory( {} );
    createBuffer( bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory );

    void *dataStaging = stagingBufferMemory.mapMemory( 0, bufferSize );
    memcpy( dataStaging, vertices.data(), bufferSize );
    stagingBufferMemory.unmapMemory();

    createBuffer( bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, vertexBuffer, vertexBufferMemory );

    copyBuffer( stagingBuffer, vertexBuffer, bufferSize );
}

void App::createIndexBuffer()
{
    vk::DeviceSize bufferSize = sizeof( indices[0] ) * indices.size();

    vk::raii::Buffer       stagingBuffer( {} );
    vk::raii::DeviceMemory stagingBufferMemory( {} );
    createBuffer( bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory );

    void *data = stagingBufferMemory.mapMemory( 0, bufferSize );
    memcpy( data, indices.data(), (size_t)bufferSize );
    stagingBufferMemory.unmapMemory();

    createBuffer( bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, indexBuffer, indexBufferMemory );

    copyBuffer( stagingBuffer, indexBuffer, bufferSize );
}


void App::createUniformBuffers()
{
    uniformBuffers.clear();
    uniformBuffersMemory.clear();
    uniformBuffersMapped.clear();

    for( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        vk::DeviceSize bufferSize = sizeof( UniformBufferObject );
        vk::raii::Buffer buffer( {} );
        vk::raii::DeviceMemory bufferMem( {} );
        createBuffer( bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, buffer, bufferMem );
        uniformBuffers.emplace_back( std::move( buffer ) );
        uniformBuffersMemory.emplace_back( std::move( bufferMem ) );
        uniformBuffersMapped.emplace_back( uniformBuffersMemory[i].mapMemory( 0, bufferSize ) );
    }
}


void App::createBuffer( vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Buffer &buffer, vk::raii::DeviceMemory &bufferMemory )
{
    vk::BufferCreateInfo bufferInfo{ .size = size, .usage = usage, .sharingMode = vk::SharingMode::eExclusive };
    buffer = vk::raii::Buffer( device, bufferInfo );
    vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo{ .allocationSize = memRequirements.size, .memoryTypeIndex = findMemoryType( memRequirements.memoryTypeBits, properties ) };
    bufferMemory = vk::raii::DeviceMemory( device, allocInfo );
    buffer.bindMemory( bufferMemory, 0 );
}

void App::copyBuffer( vk::raii::Buffer &srcBuffer, vk::raii::Buffer &dstBuffer, vk::DeviceSize size )
{
    vk::CommandBufferAllocateInfo allocInfo{ .commandPool = commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1 };
    vk::raii::CommandBuffer       commandCopyBuffer = std::move( device.allocateCommandBuffers( allocInfo ).front() );
    commandCopyBuffer.begin( vk::CommandBufferBeginInfo{ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit } );
    commandCopyBuffer.copyBuffer( *srcBuffer, *dstBuffer, vk::BufferCopy( 0, 0, size ) );
    commandCopyBuffer.end();
    queue.submit( vk::SubmitInfo{ .commandBufferCount = 1, .pCommandBuffers = &*commandCopyBuffer }, nullptr );
    queue.waitIdle();
}

uint32_t App::findMemoryType( uint32_t typeFilter, vk::MemoryPropertyFlags properties )
{
    vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

    for( uint32_t i = 0; i < memProperties.memoryTypeCount; i++ )
    {
        if( (typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties )
        {
            return i;
        }
    }

    throw std::runtime_error( "failed to find suitable memory type!" );
}



void App::createCommandBuffers()
{
    commandBuffers.clear();

    vk::CommandBufferAllocateInfo allocInfo{
        .commandPool        = commandPool,
        .level              = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = MAX_FRAMES_IN_FLIGHT
    };

    commandBuffers = vk::raii::CommandBuffers( device, allocInfo );
}


void App::recordCommandBuffer( uint32_t imageIndex )
{
    commandBuffers[currentFrame].begin( {} );

    // Before starting rendering, transition the swapchain image to COLOR_ATTACHMENT_OPTIMAL
    transitionImageLayout(
        swapChainImages[imageIndex],
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        {},                                                         // srcAccessMask (no need to wait for previous operations)
        vk::AccessFlagBits2::eColorAttachmentWrite,                 // dstAccessMask
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,         // srcStage
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,         // dstStage
        vk::ImageAspectFlagBits::eColor
    );
    // Transition depth image to depth attachment optimal layout
    transitionImageLayout(
        vk::Image(depthImage),
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eDepthAttachmentOptimal,
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::ImageAspectFlagBits::eDepth
    );

    vk::ClearValue clearColor = vk::ClearColorValue( 0.0f, 0.0f, 0.0f, 1.0f );
    vk::ClearValue clearDepth = vk::ClearDepthStencilValue( 1.0f, 0 );

    vk::RenderingAttachmentInfo colorAttachmentInfo = {
        .imageView   = swapChainImageViews[imageIndex],
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp      = vk::AttachmentLoadOp::eClear,
        .storeOp     = vk::AttachmentStoreOp::eStore,
        .clearValue  = clearColor 
    };

    vk::RenderingAttachmentInfo depthAttachmentInfo = {
        .imageView   = depthImageView,
        .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
        .loadOp      = vk::AttachmentLoadOp::eClear,
        .storeOp     = vk::AttachmentStoreOp::eDontCare,
        .clearValue  = clearDepth 
    };

    vk::RenderingInfo renderingInfo = {
        .renderArea           = {.offset = {0, 0}, .extent = swapChainExtent},
        .layerCount           = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &colorAttachmentInfo,
        .pDepthAttachment     = &depthAttachmentInfo 
    };

    commandBuffers[currentFrame].beginRendering( renderingInfo );
    commandBuffers[currentFrame].bindPipeline( vk::PipelineBindPoint::eGraphics, *graphicsPipeline );
    commandBuffers[currentFrame].setViewport( 0, vk::Viewport( 0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f ) );
    commandBuffers[currentFrame].setScissor( 0, vk::Rect2D( vk::Offset2D( 0, 0 ), swapChainExtent ) );
    commandBuffers[currentFrame].bindVertexBuffers( 0, *vertexBuffer, { 0 } );
    commandBuffers[currentFrame].bindIndexBuffer( *indexBuffer, 0, vk::IndexTypeValue<decltype(indices)::value_type>::value );
    commandBuffers[currentFrame].bindDescriptorSets( vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, *descriptorSets[currentFrame], nullptr );
    commandBuffers[currentFrame].drawIndexed( static_cast<uint32_t>(indices.size()), 1, 0, 0, 0 );
    commandBuffers[currentFrame].endRendering();

    // After rendering, transition the swapchain image to PRESENT_SRC
    transitionImageLayout(
        swapChainImages[imageIndex],
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits2::eColorAttachmentWrite,                // srcAccessMask
        {},                                                        // dstAccessMask
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // srcStage
        vk::PipelineStageFlagBits2::eBottomOfPipe,                 // dstStage
        vk::ImageAspectFlagBits::eColor
    );

    commandBuffers[currentFrame].end();
}


void App::transitionImageLayout(
    vk::Image image,
    vk::ImageLayout oldLayout,
    vk::ImageLayout newLayout,
    vk::AccessFlags2 srcAccessMask,
    vk::AccessFlags2 dstAccessMask,
    vk::PipelineStageFlags2 srcStageMask,
    vk::PipelineStageFlags2 dstStageMask,
    vk::ImageAspectFlags imageAspectFlags )
{
    vk::ImageMemoryBarrier2 barrier = {
        .srcStageMask        = srcStageMask,
        .srcAccessMask       = srcAccessMask,
        .dstStageMask        = dstStageMask,
        .dstAccessMask       = dstAccessMask,
        .oldLayout           = oldLayout,
        .newLayout           = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = image,
        .subresourceRange    = {
               .aspectMask     = imageAspectFlags,
               .baseMipLevel   = 0,
               .levelCount     = 1,
               .baseArrayLayer = 0,
               .layerCount     = 1} };
    vk::DependencyInfo dependency_info = {
        .dependencyFlags         = {},
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers    = &barrier };
    commandBuffers[currentFrame].pipelineBarrier2( dependency_info );
}


void App::createSyncObjects()
{
    presentCompleteSemaphores.clear();
    renderFinishedSemaphores.clear();
    inFlightFences.clear();

    for( size_t i = 0; i < swapChainImages.size(); i++ )
    {
        presentCompleteSemaphores.emplace_back( device, vk::SemaphoreCreateInfo() );
        renderFinishedSemaphores.emplace_back( device, vk::SemaphoreCreateInfo() );
    }

    for( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        inFlightFences.emplace_back( device, vk::FenceCreateInfo{ .flags = vk::FenceCreateFlagBits::eSignaled } );
    }
}


void App::updateUniformBuffer( uint32_t currentImage )
{
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    time = std::chrono::duration<float, std::chrono::seconds::period>( currentTime - startTime ).count();

    deltaTime = time - prevTime;
    prevTime = time;

    UniformBufferObject ubo{};
    ubo.model = glm::mat4( 1.0f );

    ubo.view = lookAt( cameraPosition, cameraPosition + cameraDirection, glm::vec3( 0.0f, 0.0f, 1.0f ) );

    ubo.proj = glm::perspective( glm::radians( 45.0f ), static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height), 0.1f, 1000.0f );
    ubo.proj[1][1] *= -1;
    
    ubo.lightDir = { 0.5f,0.5f,-0.5f };

    memcpy( uniformBuffersMapped[currentImage], &ubo, sizeof( ubo ) );
}


void App::drawFrame()
{
    while( vk::Result::eTimeout == device.waitForFences( *inFlightFences[currentFrame], vk::True, UINT64_MAX ) )
        ;
    auto [result, imageIndex] = swapChain.acquireNextImage( UINT64_MAX, *presentCompleteSemaphores[semaphoreIndex], nullptr );

    if( result == vk::Result::eErrorOutOfDateKHR )
    {
        recreateSwapChain();
        return;
    }
    if( result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR )
    {
        throw std::runtime_error( "failed to acquire swap chain image!" );
    }

    device.resetFences( *inFlightFences[currentFrame] );
    commandBuffers[currentFrame].reset();
    recordCommandBuffer( imageIndex );

    updateUniformBuffer( currentFrame );

    vk::PipelineStageFlags waitDestinationStageMask( vk::PipelineStageFlagBits::eColorAttachmentOutput );
    const vk::SubmitInfo   submitInfo{ .waitSemaphoreCount = 1, .pWaitSemaphores = &*presentCompleteSemaphores[semaphoreIndex], .pWaitDstStageMask = &waitDestinationStageMask, .commandBufferCount = 1, .pCommandBuffers = &*commandBuffers[currentFrame], .signalSemaphoreCount = 1, .pSignalSemaphores = &*renderFinishedSemaphores[imageIndex] };
    queue.submit( submitInfo, *inFlightFences[currentFrame] );

    try
    {
        const vk::PresentInfoKHR presentInfoKHR{ .waitSemaphoreCount = 1, .pWaitSemaphores = &*renderFinishedSemaphores[imageIndex], .swapchainCount = 1, .pSwapchains = &*swapChain, .pImageIndices = &imageIndex };
        result = queue.presentKHR( presentInfoKHR );
        if( result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || framebufferResized )
        {
            framebufferResized = false;
            recreateSwapChain();
        } else if( result != vk::Result::eSuccess )
        {
            throw std::runtime_error( "failed to present swap chain image!" );
        }
    } catch( const vk::SystemError &e )
    {
        if( e.code().value() == static_cast<int>(vk::Result::eErrorOutOfDateKHR) )
        {
            recreateSwapChain();
            return;
        } else
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
    glfwGetFramebufferSize( window, &width, &height );
    while( width == 0 || height == 0 )
    {
        glfwGetFramebufferSize( window, &width, &height );
        glfwWaitEvents();
    }

    device.waitIdle();

    cleanupSwapChain();
    createSwapChain();
    createImageViews();
    createDepthResources();
}


void App::framebufferResizeCallback( GLFWwindow *window, int width, int height )
{
    auto app = reinterpret_cast<App *>(glfwGetWindowUserPointer( window ));
    app->framebufferResized = true;
}


void App::mainLoop()
{
    while( !glfwWindowShouldClose( window ) )
    {
        glfwPollEvents();
        processInputs();

        drawFrame();
    }

    device.waitIdle();
}


void App::processInputs()
{
    glm::vec3 movementDirection = {
        glfwGetKey(window, GLFW_KEY_D) - glfwGetKey(window, GLFW_KEY_A),
        glfwGetKey(window, GLFW_KEY_W) - glfwGetKey(window, GLFW_KEY_S),
        glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) - glfwGetKey(window, GLFW_KEY_SPACE)
    };

	cursorPosDelta = cursorPos - prevCursorPos;
	prevCursorPos = cursorPos;

	float yaw = cursorPosDelta.x * mouseSensitivity * deltaTime;
	float pitch = cursorPosDelta.y * mouseSensitivity * deltaTime;

	float angleUp = glm::acos(glm::dot(cameraDirection, glm::vec3(0, 0, 1)));
	if( pitch != 0)
		std::cout << pitch << " | " << angleUp << std::endl;

	if (pitch < 0 && angleUp + pitch - 0.02f < 0 ||
		pitch > 0 && angleUp + pitch + 0.02f > glm::pi<float>()) {
		pitch = 0;
	}

    cameraDirection = glm::vec4(cameraDirection, 1.0f) * glm::rotate(glm::rotate(glm::mat4(1.0f), yaw, glm::vec3(0, 0, 1)), pitch, glm::normalize(glm::cross(cameraDirection, glm::vec3(0, 0, 1))));

	if (glm::length(movementDirection) > 0)
	{
		glm::vec3 forward = cameraDirection;
		glm::vec3 right = glm::cross(forward, glm::vec3(0, 0, 1));
		glm::vec3 up = glm::cross(forward, right);

		glm::vec3 displacement = glm::normalize(movementDirection) * speed * deltaTime;

		cameraPosition += right * displacement.x + forward * displacement.y + up * displacement.z;
	}
}


void App::cleanup()
{
    glfwDestroyWindow( window );

    glfwTerminate();
}


void App::generateMesh()
{
    uint32_t width = std::min(static_cast<uint32_t>(gridWidth * resolution), static_cast<uint32_t>(UINT16_MAX));
    uint32_t height = std::min(static_cast<uint32_t>(gridHeight * resolution), static_cast<uint32_t>(UINT16_MAX));
    
    uint32_t numVertices = width * height;
    uint64_t numIndices = 6 * (width - 1) * (height - 1);

    vertices.resize( numVertices );

    float left = -static_cast<float>(gridWidth - 1) / 2;
    float bottom = -static_cast<float>(gridHeight - 1) / 2;

    for (uint16_t y = 0; y < height; y++)
    {
        for (uint16_t x = 0; x < width; x++)
        {
            float xPos = left + (x / resolution);
            float yPos = bottom + (y / resolution);

            float xNorm = 2 * xPos / gridWidth;
            float yNorm = 2 * yPos / gridHeight;

            float zPos = 0;
            for (int i = 0; i < 4; i++)
            {
                zPos += (glm::sin(13 * xNorm * glm::exp(i)) + glm::cos(11 * yNorm * glm::exp(i))) / glm::exp(i);
            }

            vertices[y * width + x] = Vertex{
                .pos      = {xPos, yPos, 10 * zPos},
                .color    = {1, 1, 1},
                .texCoord = {y, x}
            };
        }
    }

    indices.resize(numIndices);

    uint64_t id = 0;
    for (uint16_t y = 0; y < height - 1; y++)
    {
        for (uint16_t x = 0; x < width - 1; x++)
        {
            indices[id++] = y * width + x;
            indices[id++] = y * width + x + 1;
            indices[id++] = (y + 1) * width + x + 1;

            indices[id++] = (y + 1) * width + x + 1;
            indices[id++] = (y + 1) * width + x;
            indices[id++] = y * width + x;
        }
    }
}


void App::setupDebugMessenger()
{
    if( !enableValidationLayers ) return;

    vk::DebugUtilsMessageSeverityFlagsEXT severityFlags( vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError );
    vk::DebugUtilsMessageTypeFlagsEXT     messageTypeFlags( vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation );
    vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{
        .messageSeverity = severityFlags,
        .messageType     = messageTypeFlags,
        .pfnUserCallback = &debugCallback
    };

    debugMessenger = instance.createDebugUtilsMessengerEXT( debugUtilsMessengerCreateInfoEXT );
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL App::debugCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
    vk::DebugUtilsMessageTypeFlagsEXT type,
    const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void * )
{
    std::cerr << "validation layer: type " << to_string( type ) << " msg: " << pCallbackData->pMessage << std::endl;

    return vk::False;
}
