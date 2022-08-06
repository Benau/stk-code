#include "ge_vulkan_features.hpp"

#include "ge_vulkan_driver.hpp"
#include "ge_vulkan_shader_manager.hpp"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "../source/Irrlicht/os.h"

namespace GE
{
namespace GEVulkanFeatures
{
// ============================================================================
bool g_supports_bind_textures_at_once = false;
bool g_supports_rgba8_blit = false;
bool g_supports_r8_blit = false;
// https://chunkstories.xyz/blog/a-note-on-descriptor-indexing
bool g_supports_descriptor_indexing = false;
bool g_supports_non_uniform_indexing = false;
bool g_supports_partially_bound = false;
uint32_t g_max_sampler_supported = 0;
bool g_supports_multi_draw_indirect = false;
bool g_supports_base_vertex_rendering = true;
bool g_supports_compute_in_main_queue = false;
bool g_supports_bc3 = false;
bool g_supports_bc7 = false;
bool g_supports_astc = false;
}   // GEVulkanFeatures

// ============================================================================
void GEVulkanFeatures::init(GEVulkanDriver* vk)
{
    g_supports_bind_textures_at_once = true;
    bool dynamic_indexing = true;
    VkPhysicalDeviceLimits limit = vk->getPhysicalDeviceProperties().limits;
    // https://vulkan.gpuinfo.org/displaydevicelimit.php?name=maxDescriptorSetSamplers&platform=all
    // https://vulkan.gpuinfo.org/displaydevicelimit.php?name=maxDescriptorSetSampledImages&platform=all
    // https://vulkan.gpuinfo.org/displaydevicelimit.php?name=maxPerStageDescriptorSamplers&platform=all
    // https://vulkan.gpuinfo.org/displaydevicelimit.php?name=maxPerStageDescriptorSampledImages&platform=all
    // We decide 256 (GEVulkanShaderManager::getSamplerSize()) based on those infos
    g_max_sampler_supported = std::min(
    {
        limit.maxDescriptorSetSamplers,
        limit.maxDescriptorSetSampledImages,
        limit.maxPerStageDescriptorSamplers,
        limit.maxPerStageDescriptorSampledImages
    });
    const unsigned max_sampler_size = GEVulkanShaderManager::getSamplerSize();
    if (max_sampler_size > g_max_sampler_supported)
        g_supports_bind_textures_at_once = false;
    if (vk->getPhysicalDeviceFeatures().shaderSampledImageArrayDynamicIndexing == VK_FALSE)
    {
        dynamic_indexing = false;
        g_supports_bind_textures_at_once = false;
    }
    g_supports_multi_draw_indirect = vk->getPhysicalDeviceFeatures().multiDrawIndirect &&
         vk->getPhysicalDeviceFeatures().drawIndirectFirstInstance;

    VkFormatProperties format_properties = {};
    vkGetPhysicalDeviceFormatProperties(vk->getPhysicalDevice(),
        VK_FORMAT_R8G8B8A8_UNORM, &format_properties);
    g_supports_rgba8_blit = format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
    format_properties = {};
    vkGetPhysicalDeviceFormatProperties(vk->getPhysicalDevice(),
        VK_FORMAT_R8_UNORM, &format_properties);
    g_supports_r8_blit = format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
    format_properties = {};
    vkGetPhysicalDeviceFormatProperties(vk->getPhysicalDevice(),
        VK_FORMAT_BC3_UNORM_BLOCK, &format_properties);
    g_supports_bc3 = format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    format_properties = {};
    vkGetPhysicalDeviceFormatProperties(vk->getPhysicalDevice(),
        VK_FORMAT_BC7_UNORM_BLOCK, &format_properties);
    g_supports_bc7 = format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    format_properties = {};
    vkGetPhysicalDeviceFormatProperties(vk->getPhysicalDevice(),
        VK_FORMAT_ASTC_4x4_UNORM_BLOCK, &format_properties);
    g_supports_astc = format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;

    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(vk->getPhysicalDevice(), NULL,
        &extension_count, NULL);
    std::vector<VkExtensionProperties> extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(vk->getPhysicalDevice(), NULL,
        &extension_count, &extensions[0]);

    for (VkExtensionProperties& prop : extensions)
    {
        if (std::string(prop.extensionName) == "VK_EXT_descriptor_indexing")
            g_supports_descriptor_indexing = true;
    }

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vk->getPhysicalDevice(),
        &queue_family_count, NULL);
    if (queue_family_count != 0)
    {
        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(vk->getPhysicalDevice(),
            &queue_family_count, &queue_families[0]);
        uint32_t main_family = vk->getGraphicsFamily();
        if (main_family < queue_families.size())
        {
            g_supports_compute_in_main_queue =
                (queue_families[main_family].queueFlags & VK_QUEUE_COMPUTE_BIT)
                != 0;
        }
    }

    VkPhysicalDeviceFeatures2 supported_features = {};
    supported_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    VkPhysicalDeviceDescriptorIndexingFeatures descriptor_indexing_features = {};
    descriptor_indexing_features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    supported_features.pNext = &descriptor_indexing_features;

    if (vk->getPhysicalDeviceProperties().apiVersion < VK_API_VERSION_1_1 ||
        !vkGetPhysicalDeviceFeatures2)
        return;
    vkGetPhysicalDeviceFeatures2(vk->getPhysicalDevice(), &supported_features);
    if (supported_features.sType !=
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2)
        return;

    g_supports_non_uniform_indexing = (descriptor_indexing_features
        .shaderSampledImageArrayNonUniformIndexing == VK_TRUE);
    g_supports_partially_bound = (descriptor_indexing_features
        .descriptorBindingPartiallyBound == VK_TRUE);

    bool missing_vkGetPhysicalDeviceProperties2 =
        !vkGetPhysicalDeviceProperties2;
    if (!missing_vkGetPhysicalDeviceProperties2 &&
        !g_supports_bind_textures_at_once && dynamic_indexing)
    {
        // Required for moltenvk argument buffers
        VkPhysicalDeviceProperties2 props1 = {};
        props1.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        VkPhysicalDeviceDescriptorIndexingProperties props2 = {};
        props2.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;
        props1.pNext = &props2;
        vkGetPhysicalDeviceProperties2(vk->getPhysicalDevice(), &props1);
        if (props2.sType ==
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES)
        {
            g_max_sampler_supported = std::min(
            {
                props2.maxPerStageDescriptorUpdateAfterBindSamplers,
                props2.maxPerStageDescriptorUpdateAfterBindSampledImages,
                props2.maxDescriptorSetUpdateAfterBindSamplers,
                props2.maxDescriptorSetUpdateAfterBindSampledImages
            });
            g_supports_bind_textures_at_once =
                g_max_sampler_supported >= max_sampler_size;
        }
    }

#if defined(__APPLE__)
    MVKPhysicalDeviceMetalFeatures mvk_features = {};
    size_t mvk_features_size = sizeof(MVKPhysicalDeviceMetalFeatures);
    vkGetPhysicalDeviceMetalFeaturesMVK(vk->getPhysicalDevice(), &mvk_features,
        &mvk_features_size);
    g_supports_base_vertex_rendering = mvk_features.baseVertexInstanceDrawing;
    if (!g_supports_base_vertex_rendering)
        g_supports_multi_draw_indirect = false;
#endif
}   // init

// ----------------------------------------------------------------------------
void GEVulkanFeatures::printStats()
{
    os::Printer::log(
        "Vulkan can bind textures at once in shader",
        g_supports_bind_textures_at_once ? "true" : "false");
    os::Printer::log(
        "Vulkan can bind mesh textures at once in shader",
        supportsBindMeshTexturesAtOnce() ? "true" : "false");
    os::Printer::log(
        "Vulkan supports linear blitting for rgba8",
        g_supports_rgba8_blit ? "true" : "false");
    os::Printer::log(
        "Vulkan supports linear blitting for r8",
        g_supports_r8_blit ? "true" : "false");
    os::Printer::log(
        "Vulkan supports VK_EXT_descriptor_indexing",
        g_supports_descriptor_indexing ? "true" : "false");
    os::Printer::log(
        "Vulkan supports multi-draw indirect",
        g_supports_multi_draw_indirect ? "true" : "false");
    os::Printer::log(
        "Vulkan supports base vertex rendering",
        g_supports_base_vertex_rendering ? "true" : "false");
    os::Printer::log(
        "Vulkan supports compute in main queue",
        g_supports_compute_in_main_queue ? "true" : "false");
    os::Printer::log(
        "Vulkan supports BC3 texture compression",
        g_supports_bc3 ? "true" : "false");
    os::Printer::log(
        "Vulkan supports BC7 texture compression",
        g_supports_bc7 ? "true" : "false");
    os::Printer::log(
        "Vulkan supports adaptive scalable texture compression (ASTC)",
        g_supports_astc ? "true" : "false");
    os::Printer::log(
        "Vulkan descriptor indexes can be dynamically non-uniform",
        g_supports_non_uniform_indexing ? "true" : "false");
    os::Printer::log(
        "Vulkan descriptor can be partially bound",
        g_supports_partially_bound ? "true" : "false");
}   // printStats

// ----------------------------------------------------------------------------
bool GEVulkanFeatures::supportsBindTexturesAtOnce()
{
    return g_supports_bind_textures_at_once;
}   // supportsBindTexturesAtOnce

// ----------------------------------------------------------------------------
bool GEVulkanFeatures::supportsRGBA8Blit()
{
    return g_supports_rgba8_blit;
}   // supportsRGBA8Blit

// ----------------------------------------------------------------------------
bool GEVulkanFeatures::supportsR8Blit()
{
    return g_supports_r8_blit;
}   // supportsR8Blit

// ----------------------------------------------------------------------------
bool GEVulkanFeatures::supportsDescriptorIndexing()
{
    return g_supports_descriptor_indexing;
}   // supportsDescriptorIndexing

// ----------------------------------------------------------------------------
bool GEVulkanFeatures::supportsNonUniformIndexing()
{
    return g_supports_non_uniform_indexing;
}   // supportsNonUniformIndexing

// ----------------------------------------------------------------------------
bool GEVulkanFeatures::supportsDifferentTexturePerDraw()
{
    return g_supports_bind_textures_at_once &&
        g_supports_descriptor_indexing && g_supports_non_uniform_indexing;
}   // supportsDifferentTexturePerDraw

// ----------------------------------------------------------------------------
bool GEVulkanFeatures::supportsPartiallyBound()
{
    return g_supports_partially_bound;
}   // supportsPartiallyBound

// ----------------------------------------------------------------------------
bool GEVulkanFeatures::supportsBindMeshTexturesAtOnce()
{
    if (!g_supports_bind_textures_at_once)
        return false;
    const unsigned sampler_count = GEVulkanShaderManager::getSamplerSize() *
        GEVulkanShaderManager::getMeshTextureLayer();
    return g_max_sampler_supported >= sampler_count;
}   // supportsBindMeshTexturesAtOnce

// ----------------------------------------------------------------------------
bool GEVulkanFeatures::supportsMultiDrawIndirect()
{
    return g_supports_multi_draw_indirect;
}   // supportsMultiDrawIndirect

// ----------------------------------------------------------------------------
bool GEVulkanFeatures::supportsBaseVertexRendering()
{
    return g_supports_base_vertex_rendering;
}   // supportsBaseVertexRendering

// ----------------------------------------------------------------------------
bool GEVulkanFeatures::supportsComputeInMainQueue()
{
    return g_supports_compute_in_main_queue;
}   // supportsComputeInMainQueue

// ----------------------------------------------------------------------------
bool GEVulkanFeatures::supportsBC3TextureCompression()
{
    return g_supports_bc3;
}   // supportsBC3TextureCompression

// ----------------------------------------------------------------------------
bool GEVulkanFeatures::supportsBC7TextureCompression()
{
    return g_supports_bc7;
}   // supportsBC7TextureCompression

// ----------------------------------------------------------------------------
bool GEVulkanFeatures::supportsASTC()
{
    return g_supports_astc;
}   // supportsASTC

// ----------------------------------------------------------------------------
bool GEVulkanFeatures::supportsTextureCompression()
{
    return g_supports_compute_in_main_queue &&
        (g_supports_bc3 || g_supports_bc7 || g_supports_astc);
}   // supportsTextureCompression

}
