#ifndef HEADER_GE_VULKAN_SCALED_FBO_HPP
#define HEADER_GE_VULKAN_SCALED_FBO_HPP

#include "ge_vulkan_fbo_texture.hpp"

namespace GE
{
class GEVulkanScaledFBO : public GEVulkanFBOTexture
{
private:
    VkDescriptorSetLayout m_descriptor_set_layout;

    VkDescriptorPool m_descriptor_pool;

    VkDescriptorSet m_descriptor_set;
public:
    // ------------------------------------------------------------------------
    GEVulkanScaledFBO(GEVulkanDriver* vk, const core::dimension2d<u32>& size);
    // ------------------------------------------------------------------------
    virtual ~GEVulkanScaledFBO();
    // ------------------------------------------------------------------------
    virtual VkRenderPass createRenderPass(VkFormat final_format);
    // ------------------------------------------------------------------------
    virtual VkFramebuffer createFramebuffer(VkImageView main);
    // ------------------------------------------------------------------------
    virtual unsigned getColorAttachmentCount() const              { return 1; }
    // ------------------------------------------------------------------------
    virtual unsigned getFinalSubpassIndex() const                 { return 1; }
    // ------------------------------------------------------------------------
    virtual VkDescriptorSetLayout getDescriptorSetLayout() const
                                            { return m_descriptor_set_layout; }
    // ------------------------------------------------------------------------
    virtual VkDescriptorSet* getDescriptorSet()
                                                  { return &m_descriptor_set; }
};   // GEVulkanScaledFBO

}

#endif
