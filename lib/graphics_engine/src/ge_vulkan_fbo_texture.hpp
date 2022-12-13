#ifndef HEADER_GE_VULKAN_FBO_TEXTURE_HPP
#define HEADER_GE_VULKAN_FBO_TEXTURE_HPP

#include "ge_vulkan_texture.hpp"

namespace GE
{
class GEVulkanDepthTexture;
class GEVulkanFBOTexture : public GEVulkanTexture
{
protected:
    GEVulkanDepthTexture* m_depth_texture;

private:
    VkRenderPass m_rtt_render_pass;

    VkFramebuffer m_rtt_frame_buffer;
public:
    // ------------------------------------------------------------------------
    GEVulkanFBOTexture(GEVulkanDriver* vk, const core::dimension2d<u32>& size,
                       bool create_depth);
    // ------------------------------------------------------------------------
    virtual ~GEVulkanFBOTexture();
    // ------------------------------------------------------------------------
    virtual void* lock(video::E_TEXTURE_LOCK_MODE mode =
                       video::ETLM_READ_WRITE, u32 mipmap_level = 0)
                                                               { return NULL; }
    // ------------------------------------------------------------------------
    virtual void unlock()                                                    {}
    // ------------------------------------------------------------------------
    virtual const core::dimension2d<u32>& getOriginalSize() const
                                                        { return m_orig_size; }
    // ------------------------------------------------------------------------
    virtual const core::dimension2d<u32>& getSize() const    { return m_size; }
    // ------------------------------------------------------------------------
    virtual bool hasMipMaps() const                           { return false; }
    // ------------------------------------------------------------------------
    virtual void regenerateMipMapLevels(void* mipmap_data = NULL)            {}
    // ------------------------------------------------------------------------
    virtual u64 getTextureHandler() const
                                  { return (u64)(m_image_view.get()->load()); }
    // ------------------------------------------------------------------------
    virtual void reload()                                                    {}
    // ------------------------------------------------------------------------
    virtual void updateTexture(void* data, irr::video::ECOLOR_FORMAT format,
                               u32 w, u32 h, u32 x, u32 y)                   {}
    // ------------------------------------------------------------------------
    virtual std::shared_ptr<std::atomic<VkImageView> > getImageView() const
                                                       { return m_image_view; }
    // ------------------------------------------------------------------------
    void createRTT();
    // ------------------------------------------------------------------------
    VkRenderPass getRTTRenderPass() const         { return m_rtt_render_pass; }
    // ------------------------------------------------------------------------
    VkFramebuffer getRTTFramebuffer() const      { return m_rtt_frame_buffer; }
    // ------------------------------------------------------------------------
    GEVulkanDepthTexture* getDepthTexture() const   { return m_depth_texture; }
    // ------------------------------------------------------------------------
    virtual VkRenderPass createRenderPass(VkFormat final_format)
                                                     { return VK_NULL_HANDLE; }
    // ------------------------------------------------------------------------
    virtual VkFramebuffer createFramebuffer(VkImageView main)
                                                     { return VK_NULL_HANDLE; }
    // ------------------------------------------------------------------------
    virtual unsigned getColorAttachmentCount() const              { return 0; }
    // ------------------------------------------------------------------------
    virtual unsigned getFinalSubpassIndex() const                 { return 0; }
    // ------------------------------------------------------------------------
    virtual VkDescriptorSetLayout getDescriptorSetLayout() const
                                                     { return VK_NULL_HANDLE; }
    // ------------------------------------------------------------------------
    virtual VkDescriptorSet* getDescriptorSet()                { return NULL; }
};   // GEVulkanFBOTexture

}

#endif
