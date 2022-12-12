#include "ge_vulkan_fbo_texture.hpp"

#include "ge_main.hpp"
#include "ge_vulkan_depth_texture.hpp"
#include "ge_vulkan_driver.hpp"

#include <array>
#include <exception>

namespace GE
{
GEVulkanFBOTexture::GEVulkanFBOTexture(GEVulkanDriver* vk,
                                       const core::dimension2d<u32>& size,
                                       bool create_depth)
                  : GEVulkanTexture()
{
    m_vk = vk;
    m_vulkan_device = m_vk->getDevice();
    m_image = VK_NULL_HANDLE;
    m_vma_allocation = VK_NULL_HANDLE;
    m_has_mipmaps = false;
    m_locked_data = NULL;
    m_size = m_orig_size = m_max_size = size;
    m_internal_format = VK_FORMAT_R8G8B8A8_UNORM;

    if (!createImage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT))
        throw std::runtime_error("createImage failed for fbo texture");

    if (!createImageView(VK_IMAGE_ASPECT_COLOR_BIT))
        throw std::runtime_error("createImageView failed for fbo texture");

    m_depth_texture = NULL;
    m_rtt_render_pass = VK_NULL_HANDLE;
    m_rtt_frame_buffer = VK_NULL_HANDLE;
    if (create_depth)
        m_depth_texture = new GEVulkanDepthTexture(m_vk, size);
}   // GEVulkanFBOTexture

// ----------------------------------------------------------------------------
GEVulkanFBOTexture::~GEVulkanFBOTexture()
{
    delete m_depth_texture;
    if (m_rtt_frame_buffer != VK_NULL_HANDLE)
    {
        clearVulkanData();
        vkDestroyFramebuffer(m_vk->getDevice(), m_rtt_frame_buffer, NULL);
        m_vk->handleDeletedTextures();

        m_image_view.reset();
        m_image = VK_NULL_HANDLE;
        m_vma_allocation = VK_NULL_HANDLE;
    }
    if (m_rtt_render_pass != VK_NULL_HANDLE)
        vkDestroyRenderPass(m_vk->getDevice(), m_rtt_render_pass, NULL);
}   // ~GEVulkanFBOTexture

// ----------------------------------------------------------------------------
void GEVulkanFBOTexture::createRTT()
{
    if (!m_depth_texture)
        return;

    std::array<VkAttachmentDescription, 2> attchment_desc = {};
    // Color attachment
    attchment_desc[0].format = m_internal_format;
    attchment_desc[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attchment_desc[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attchment_desc[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attchment_desc[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attchment_desc[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attchment_desc[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attchment_desc[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    // Depth attachment
    attchment_desc[1].format = m_depth_texture->getInternalFormat();
    attchment_desc[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attchment_desc[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attchment_desc[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attchment_desc[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attchment_desc[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attchment_desc[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attchment_desc[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_reference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference depth_reference = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass_desc = {};
    subpass_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_desc.colorAttachmentCount = 1;
    subpass_desc.pColorAttachments = &color_reference;
    subpass_desc.pDepthStencilAttachment = &depth_reference;

    // Use subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 2> dependencies;

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Create the actual render pass
    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = attchment_desc.size();
    render_pass_info.pAttachments = attchment_desc.data();
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass_desc;
    render_pass_info.dependencyCount = dependencies.size();
    render_pass_info.pDependencies = dependencies.data();

    if (vkCreateRenderPass(m_vk->getDevice(), &render_pass_info, NULL,
        &m_rtt_render_pass) != VK_SUCCESS)
        throw std::runtime_error("vkCreateRenderPass failed in createFBOdata");

    std::array<VkImageView, 2> attachments =
    {{
        *(m_image_view.get()),
        (VkImageView)m_depth_texture->getTextureHandler(),
    }};

    VkFramebufferCreateInfo framebuffer_info = {};
    framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.renderPass = m_rtt_render_pass;
    framebuffer_info.attachmentCount = attachments.size();
    framebuffer_info.pAttachments = attachments.data();
    framebuffer_info.width = m_size.Width;
    framebuffer_info.height = m_size.Height;
    framebuffer_info.layers = 1;

    if (vkCreateFramebuffer(m_vk->getDevice(), &framebuffer_info,
        NULL, &m_rtt_frame_buffer) != VK_SUCCESS)
        throw std::runtime_error("vkCreateFramebuffer failed in createFBOdata");
}   // createRTT

// ----------------------------------------------------------------------------
VkRenderPass GEVulkanFBOTexture::createRenderPass(VkFormat final_format) const
{
    std::array<VkAttachmentDescription, 3> attachment_desc = {{ }};
    // Color attachment
    attachment_desc[0].format = m_internal_format;
    attachment_desc[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachment_desc[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment_desc[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment_desc[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment_desc[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment_desc[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment_desc[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    // Main swapchain image attachment
    attachment_desc[1].format = final_format;
    attachment_desc[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachment_desc[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment_desc[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment_desc[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment_desc[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment_desc[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment_desc[1].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    // Depth attachment
    attachment_desc[2].format = m_depth_texture->getInternalFormat();
    attachment_desc[2].samples = VK_SAMPLE_COUNT_1_BIT;
    attachment_desc[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment_desc[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment_desc[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment_desc[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment_desc[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment_desc[2].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Two subpasses
    std::array<VkSubpassDescription, 2> subpass_descriptions = {{ }};

    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_ref = {};
    depth_attachment_ref.attachment = 2;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    subpass_descriptions[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_descriptions[0].colorAttachmentCount = 1;
    subpass_descriptions[0].pColorAttachments = &color_attachment_ref;
    subpass_descriptions[0].pDepthStencilAttachment = &depth_attachment_ref;

    VkAttachmentReference final_attachment_ref = {};
    final_attachment_ref.attachment = 1;
    final_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference input_attachment_ref = color_attachment_ref;
    input_attachment_ref.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    subpass_descriptions[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_descriptions[1].colorAttachmentCount = 1;
    subpass_descriptions[1].pColorAttachments = &final_attachment_ref;
    subpass_descriptions[1].inputAttachmentCount = 1;
    subpass_descriptions[1].pInputAttachments = &input_attachment_ref;

    // Subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 3> dependencies = {{ }};

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // This dependency transitions the input attachment from color attachment
    // to shader read
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = 1;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[2].srcSubpass = 0;
    dependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[2].srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[2].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[2].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = attachment_desc.size();
    render_pass_info.pAttachments = attachment_desc.data();
    render_pass_info.subpassCount = subpass_descriptions.size();
    render_pass_info.pSubpasses = subpass_descriptions.data();
    render_pass_info.dependencyCount = dependencies.size();
    render_pass_info.pDependencies = dependencies.data();

    VkRenderPass render_pass = VK_NULL_HANDLE;
    if (vkCreateRenderPass(m_vk->getDevice(), &render_pass_info,
        NULL, &render_pass) != VK_SUCCESS)
    {
        throw std::runtime_error("vkCreateFramebuffer failed in "
            "GEVulkanFBOTexture::createRenderPass");
    }
    return render_pass;
}   // createRenderPass

// ----------------------------------------------------------------------------
VkFramebuffer GEVulkanFBOTexture::createFramebuffer(VkImageView main) const
{
    std::array<VkImageView, 3> attachments =
    {{
        *(m_image_view.get()),
        main,
        (VkImageView)m_depth_texture->getTextureHandler()
    }};
    VkFramebufferCreateInfo framebuffer_info = {};
    framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.renderPass = m_vk->getRenderPass();
    framebuffer_info.attachmentCount = attachments.size();
    framebuffer_info.pAttachments = attachments.data();
    framebuffer_info.width = m_vk->getCurrentRenderTargetSize().Width;
    framebuffer_info.height = m_vk->getCurrentRenderTargetSize().Height;
    framebuffer_info.layers = 1;

    VkFramebuffer swap_chain_framebuffer = VK_NULL_HANDLE;
    VkResult result = vkCreateFramebuffer(m_vk->getDevice(), &framebuffer_info,
        NULL, &swap_chain_framebuffer);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("vkCreateFramebuffer failed in "
            "GEVulkanFBOTexture::createFramebuffer");
    }
    return swap_chain_framebuffer;
}   // createFramebuffer

}
