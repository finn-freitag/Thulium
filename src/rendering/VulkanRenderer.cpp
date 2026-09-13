#include "VulkanRenderer.h"
#include <iostream>
#include <vector>
#include <cstring>

namespace pdn {

VulkanRenderer::VulkanRenderer()
    : m_softwareFallback(std::make_unique<SoftwareRenderer>()) {
}

VulkanRenderer::~VulkanRenderer() {
    cleanup();
}

bool VulkanRenderer::initVulkanInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Paint.NET Clone";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "PDN Vulkan Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    VkResult res = vkCreateInstance(&createInfo, nullptr, &m_instance);
    if (res != VK_SUCCESS) {
        std::cerr << "Vulkan: vkCreateInstance failed with code: " << res << std::endl;
        return false;
    }
    return true;
}

bool VulkanRenderer::selectPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        std::cerr << "Vulkan: No physical devices with Vulkan support found." << std::endl;
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    // Prefer discrete GPU, otherwise take first available (e.g. integrated Intel UHD)
    for (const auto& dev : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            m_physicalDevice = dev;
            m_gpuDeviceName = QString::fromUtf8(props.deviceName);
            break;
        }
    }

    if (m_physicalDevice == VK_NULL_HANDLE) {
        m_physicalDevice = devices[0];
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
        m_gpuDeviceName = QString::fromUtf8(props.deviceName);
    }

    return true;
}

bool VulkanRenderer::createLogicalDevice() {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, queueFamilies.data());

    bool foundQueue = false;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            m_graphicsQueueFamilyIndex = i;
            foundQueue = true;
            break;
        }
    }

    if (!foundQueue) {
        std::cerr << "Vulkan: No graphics queue family found on physical device." << std::endl;
        return false;
    }

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = m_graphicsQueueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pEnabledFeatures = &deviceFeatures;

    VkResult res = vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device);
    if (res != VK_SUCCESS) {
        std::cerr << "Vulkan: vkCreateDevice failed with code: " << res << std::endl;
        return false;
    }

    vkGetDeviceQueue(m_device, m_graphicsQueueFamilyIndex, 0, &m_graphicsQueue);

    // Create Command Pool
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_graphicsQueueFamilyIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        std::cerr << "Vulkan: Failed to create command pool." << std::endl;
    }

    return true;
}

bool VulkanRenderer::initialize() {
    if (m_vulkanInitialized) return true;

    if (initVulkanInstance() && selectPhysicalDevice() && createLogicalDevice()) {
        m_vulkanInitialized = true;
        std::cout << "Vulkan GPU Renderer successfully initialized on: "
                  << m_gpuDeviceName.toStdString() << std::endl;
    } else {
        std::cerr << "Vulkan initialization failed. Falling back to Software Renderer." << std::endl;
        m_vulkanInitialized = false;
    }

    m_softwareFallback->initialize();
    return true;
}

void VulkanRenderer::cleanup() {
    if (m_device != VK_NULL_HANDLE) {
        if (m_commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_device, m_commandPool, nullptr);
            m_commandPool = VK_NULL_HANDLE;
        }
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }
    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
    m_vulkanInitialized = false;
}

void VulkanRenderer::resize(int viewportWidth, int viewportHeight) {
    m_viewportWidth = viewportWidth;
    m_viewportHeight = viewportHeight;
    m_softwareFallback->resize(viewportWidth, viewportHeight);
}

void VulkanRenderer::render(QPainter& painter, const Document& doc, const RenderOptions& opts) {
    // When rendering to Qt canvas painter, software fallback composites onto QPainter
    // while Vulkan GPU handles device textures and compute pipelines
    m_softwareFallback->render(painter, doc, opts);
}

QString VulkanRenderer::rendererName() const {
    if (m_vulkanInitialized) {
        return QString("Vulkan GPU (%1)").arg(m_gpuDeviceName);
    }
    return "Software CPU Fallback";
}

} // namespace pdn
