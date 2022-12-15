#include "ge_vulkan_scaled_fbo.hpp"

#include "ge_main.hpp"
#include "ge_vulkan_depth_texture.hpp"
#include "ge_vulkan_driver.hpp"

#include <array>
#include <exception>

namespace GE
{
GEVulkanScaledFBO::GEVulkanScaledFBO(GEVulkanDriver* vk,
                                     const core::dimension2d<u32>& size)
                 : GEVulkanFBOTexture(vk, size, true/*create_depth*/)
{
    VkDescriptorSetLayoutBinding set_layout;
    set_layout.binding = 0;
    set_layout.descriptorCount = 1;
    set_layout.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    set_layout.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // m_descriptor_set_layout
    VkDescriptorSetLayoutCreateInfo setinfo = {};
    setinfo.flags = 0;
    setinfo.pNext = NULL;
    setinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    setinfo.pBindings = &set_layout;
    setinfo.bindingCount = 1;
    if (vkCreateDescriptorSetLayout(m_vk->getDevice(), &setinfo,
        NULL, &m_descriptor_set_layout) != VK_SUCCESS)
    {
        throw std::runtime_error("vkCreateDescriptorSetLayout failed for "
            "GEVulkanScaledFBO");
    }

    // m_descriptor_pool
    VkDescriptorPoolSize pool_size;
    pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size.descriptorCount = 1;

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = 0;
    pool_info.maxSets = 1;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;
    if (vkCreateDescriptorPool(m_vk->getDevice(), &pool_info, NULL,
        &m_descriptor_pool) != VK_SUCCESS)
    {
        throw std::runtime_error("vkCreateDescriptorPool failed for "
            "GEVulkanScaledFBO");
    }

    // m_descriptor_set
    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = m_descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &m_descriptor_set_layout;

    if (vkAllocateDescriptorSets(m_vk->getDevice(), &alloc_info,
        &m_descriptor_set) != VK_SUCCESS)
    {
        throw std::runtime_error("vkAllocateDescriptorSets failed for "
            "GEVulkanScaledFBO");
    }

    VkDescriptorImageInfo descriptor;
    descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descriptor.imageView = *(m_image_view.get());
    descriptor.sampler = VK_NULL_HANDLE;

    VkWriteDescriptorSet write_descriptor_set = {};
    write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_descriptor_set.dstSet = m_descriptor_set;
    write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    write_descriptor_set.descriptorCount = 1;
    write_descriptor_set.dstBinding = 0;
    write_descriptor_set.pImageInfo = &descriptor;

    vkUpdateDescriptorSets(m_vk->getDevice(), 1, &write_descriptor_set, 0,
        NULL);
}   // GEVulkanScaledFBO

// ----------------------------------------------------------------------------
GEVulkanScaledFBO::~GEVulkanScaledFBO()
{
    delete m_depth_texture;
    m_depth_texture = NULL;
    
}   // ~GEVulkanScaledFBO

// ----------------------------------------------------------------------------
VkRenderPass GEVulkanScaledFBO::createRenderPass(VkFormat final_format)
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
VkFramebuffer GEVulkanScaledFBO::createFramebuffer(VkImageView main)
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
