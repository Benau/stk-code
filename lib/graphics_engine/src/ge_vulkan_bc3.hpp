#ifndef HEADER_GE_VULKAN_BC3_HPP
#define HEADER_GE_VULKAN_BC3_HPP

#include "dimension2d.h"
#include "ge_vma.hpp"
#include "vulkan_wrapper.h"

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace GE
{
class GEVulkanDriver;
class GEVulkanBC3
{
private:
    static VkDescriptorSetLayout m_layout;

    static std::vector<VkDescriptorPool> m_descriptor_pool;

    // 2^12 = 4096 texture size
    static std::vector<std::array<VkDescriptorSet, 12> > m_descriptor_sets;

    static VkPipelineLayout m_pipeline_layout;

    static VkPipeline m_pipeline;

    static GEVulkanDriver* m_vk;

    VkImage m_image;

    VkFormat m_internal_format;

    std::vector<std::pair<irr::core::dimension2du, unsigned> > m_mipmap_sizes;

    std::vector<VkBuffer> m_buffer;

    std::vector<VmaAllocation> m_memory;

    std::vector<VkImageView> m_image_view;
public:
    // ------------------------------------------------------------------------
    static void init(GEVulkanDriver* vk);
    // ------------------------------------------------------------------------
    static void destroy();
    // ------------------------------------------------------------------------
    static bool loaded()                               { return m_vk != NULL; }
    // ------------------------------------------------------------------------
    GEVulkanBC3(VkImage image, VkFormat internal_format,
                const std::vector<std::pair<
                irr::core::dimension2du, unsigned> >& mipmap_sizes);
    // ------------------------------------------------------------------------
    ~GEVulkanBC3();
    // ------------------------------------------------------------------------
    void compress(VkCommandBuffer cmd);
    // ------------------------------------------------------------------------
    VkBuffer getBuffer(unsigned i) const                { return m_buffer[i]; }
};   // GEVulkanBC3

}

#endif
