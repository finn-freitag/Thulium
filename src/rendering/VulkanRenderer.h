#pragma once

#include "IRenderer.h"
#include "SoftwareRenderer.h"
#include <vulkan/vulkan.h>
#include <memory>

namespace pdn {

class VulkanRenderer : public IRenderer {
public:
    VulkanRenderer();
    ~VulkanRenderer() override;

    bool initialize() override;
    void cleanup() override;
    void resize(int viewportWidth, int viewportHeight) override;

    void render(QPainter& painter, const Document& doc, const RenderOptions& opts) override;

    bool isGpuAccelerated() const override { return m_vulkanInitialized; }
    QString rendererName() const override;

private:
    bool initVulkanInstance();
    bool selectPhysicalDevice();
    bool createLogicalDevice();

    VkInstance m_instance = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    uint32_t m_graphicsQueueFamilyIndex = 0;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;

    bool m_vulkanInitialized = false;
    QString m_gpuDeviceName;
    int m_viewportWidth = 0;
    int m_viewportHeight = 0;

    // Fallback software renderer for blitting to QPainter and graceful fallback
    std::unique_ptr<SoftwareRenderer> m_softwareFallback;
};

} // namespace pdn
