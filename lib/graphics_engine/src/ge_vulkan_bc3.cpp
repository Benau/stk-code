#include "ge_vulkan_bc3.hpp"

#include "ge_main.hpp"
#include "ge_vulkan_command_loader.hpp"
#include "ge_vulkan_driver.hpp"
#include "ge_vulkan_features.hpp"
#include "ge_vulkan_shader_manager.hpp"

#include <algorithm>

namespace GE
{
// ============================================================================
VkDescriptorSetLayout GEVulkanBC3::m_layout = VK_NULL_HANDLE;
std::vector<VkDescriptorPool> GEVulkanBC3::m_descriptor_pool;
std::vector<std::array<VkDescriptorSet, 12> > GEVulkanBC3::m_descriptor_sets;
VkPipelineLayout GEVulkanBC3::m_pipeline_layout = VK_NULL_HANDLE;
VkPipeline GEVulkanBC3::m_pipeline = VK_NULL_HANDLE;
GEVulkanDriver* GEVulkanBC3::m_vk = NULL;
// ============================================================================
void GEVulkanBC3::init(GEVulkanDriver* vk)
{
    if (!GEVulkanFeatures::supportsBC3TextureCompression())
        return;

    m_vk = vk;

    VkDescriptorSetLayoutBinding texture_layout_binding = {};
    texture_layout_binding.binding = 0;
    texture_layout_binding.descriptorCount = 1;
    texture_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    texture_layout_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding out_data_layout_binding = {};
    out_data_layout_binding.binding = 1;
    out_data_layout_binding.descriptorCount = 1;
    out_data_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    out_data_layout_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings =
    {{
         texture_layout_binding,
         out_data_layout_binding
    }};

    VkDescriptorSetLayoutCreateInfo setinfo = {};
    setinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    setinfo.pBindings = bindings.data();
    setinfo.bindingCount = bindings.size();
    if (vkCreateDescriptorSetLayout(m_vk->getDevice(), &setinfo, NULL,
        &m_layout) != VK_SUCCESS)
    {
        throw std::runtime_error("vkCreateDescriptorSetLayout failed for "
            "GEVulkanBC3::init");
    }

    unsigned loader_count = GEVulkanCommandLoader::getLoaderCount();
    m_descriptor_sets.resize(loader_count);
    for (unsigned i = 0; i < loader_count; i++)
    {
        std::array<VkDescriptorPoolSize, 2> sizes =
        {{
            {
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                (uint32_t)m_descriptor_sets[i].size()
            },
            {
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                (uint32_t)m_descriptor_sets[i].size()
            }
        }};

        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.maxSets = m_descriptor_sets[i].size();
        pool_info.poolSizeCount = sizes.size();
        pool_info.pPoolSizes = sizes.data();

        VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
        if (vkCreateDescriptorPool(m_vk->getDevice(), &pool_info, NULL,
            &descriptor_pool) != VK_SUCCESS)
        {
            throw std::runtime_error("createDescriptorPool for "
                "GEVulkanBC3::init");
        }
        m_descriptor_pool.push_back(descriptor_pool);

        std::vector<VkDescriptorSetLayout> data_layouts(
            m_descriptor_sets[i].size(), m_layout);
        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = m_descriptor_pool[i];
        alloc_info.descriptorSetCount = data_layouts.size();
        alloc_info.pSetLayouts = data_layouts.data();

        if (vkAllocateDescriptorSets(vk->getDevice(), &alloc_info,
            m_descriptor_sets[i].data()) != VK_SUCCESS)
        {
            throw std::runtime_error("vkAllocateDescriptorSets failed for "
                "GEVulkanBC3::init");
        }
    }

    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &m_layout;

    VkPushConstantRange push_constant;
    push_constant.offset = 0;
    push_constant.size = 128u;
    push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pipeline_layout_info.pPushConstantRanges = &push_constant;
    pipeline_layout_info.pushConstantRangeCount = 1;

    if (vkCreatePipelineLayout(m_vk->getDevice(), &pipeline_layout_info,
        NULL, &m_pipeline_layout) != VK_SUCCESS)
    {
        throw std::runtime_error("vkCreatePipelineLayout failed for "
            "GEVulkanBC3::init");
    }

    VkComputePipelineCreateInfo compute_info = {};
    compute_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compute_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    compute_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    compute_info.stage.module = GEVulkanShaderManager::getShader("bc3.comp");
    compute_info.stage.pName = "main";
    compute_info.layout = m_pipeline_layout;

    if (vkCreateComputePipelines(m_vk->getDevice(), NULL, 1, &compute_info,
        NULL, &m_pipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("vkCreateComputePipelines failed for "
            "GEVulkanBC3::init");
    }
}   // init

// ----------------------------------------------------------------------------
void GEVulkanBC3::destroy()
{
    if (!m_vk)
        return;

    if (m_pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(m_vk->getDevice(), m_pipeline, NULL);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_pipeline_layout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(m_vk->getDevice(), m_pipeline_layout, NULL);
        m_pipeline_layout = VK_NULL_HANDLE;
    }
    for (auto& pool : m_descriptor_pool)
        vkDestroyDescriptorPool(m_vk->getDevice(), pool, NULL);
    m_descriptor_pool.clear();
    m_descriptor_sets.clear();
    if (m_layout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(m_vk->getDevice(), m_layout, NULL);
        m_layout = VK_NULL_HANDLE;
    }
    m_vk = NULL;
}   // destroy

// ----------------------------------------------------------------------------
GEVulkanBC3::GEVulkanBC3(VkImage image, VkFormat internal_format,
                         const std::vector<std::pair<irr::core::dimension2du,
                         unsigned> >& mipmap_sizes)
           : m_image(image), m_internal_format(internal_format),
             m_mipmap_sizes(mipmap_sizes)
{
}   // GEVulkanBC3

// ----------------------------------------------------------------------------
GEVulkanBC3::~GEVulkanBC3()
{
    for (unsigned i = 0; i < m_buffer.size(); i++)
    {
        vmaDestroyBuffer(m_vk->getVmaAllocator(), m_buffer[i], m_memory[i]);
        vkDestroyImageView(m_vk->getDevice(), m_image_view[i], NULL);
    }
}   // ~GEVulkanBC3

// ----------------------------------------------------------------------------
constexpr int MAX_BLOCK_BATCH = 64;
#define BLOCK_SIZE_Y 4
#define BLOCK_SIZE_X 4
#define BLOCK_SIZE   (BLOCK_SIZE_Y * BLOCK_SIZE_X)
void GEVulkanBC3::compress(VkCommandBuffer cmd)
{
    const int id = GEVulkanCommandLoader::getLoaderId();
    if (m_mipmap_sizes.size() > m_descriptor_sets[id].size())
    {
        throw std::runtime_error(
            "Too many mipmap levels for GEVulkanBC3::compress");
    }

    // From b3_encode_kernel.hlsl, unused variable is commented out
    struct b3_cbuffer
    {
        //uint32_t  g_tex_width;
        uint32_t  g_num_block_x;
        //uint32_t  g_format;
        //uint32_t  g_mode_id;
        uint32_t  g_start_block_id;
        //uint32_t  g_num_total_blocks;
        //float g_alpha_weight;
        float g_quality;
    } options;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
    for (unsigned i = 0; i < m_mipmap_sizes.size(); i++)
    {
        unsigned width = m_mipmap_sizes[i].first.Width;
        unsigned height = m_mipmap_sizes[i].first.Height;
        unsigned level_size = get4x4CompressedTextureSize(width, height);
        unsigned num_blocks = level_size / 16;

        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation memory = VK_NULL_HANDLE;
        VmaAllocationCreateInfo buffer_create_info = {};
        buffer_create_info.usage = VMA_MEMORY_USAGE_AUTO;

        if (!m_vk->createBuffer(level_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, buffer_create_info,
            buffer, memory))
        {
            throw std::runtime_error(
                "createBuffer failed for GEVulkanBC3::compress");
        }
        m_buffer.push_back(buffer);
        m_memory.push_back(memory);

        VkImageView image_view = VK_NULL_HANDLE;
        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = m_image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = m_internal_format;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = i;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;
        if (vkCreateImageView(m_vk->getDevice(), &view_info, NULL,
            &image_view) != VK_SUCCESS)
        {
            throw std::runtime_error(
                "vkCreateImageView failed for GEVulkanBC3::compress");
        }
        m_image_view.push_back(image_view);

        VkDescriptorImageInfo image_info = {};
        image_info.imageView = image_view;
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        std::array<VkWriteDescriptorSet, 2> data_set = {};
        data_set[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        data_set[0].dstSet = m_descriptor_sets[id][i];
        data_set[0].dstBinding = 0;
        data_set[0].dstArrayElement = 0;
        data_set[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        data_set[0].descriptorCount = 1;
        data_set[0].pImageInfo = &image_info;

        VkDescriptorBufferInfo sbo_info;
        sbo_info.buffer = buffer;
        sbo_info.offset = 0;
        sbo_info.range = level_size;

        data_set[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        data_set[1].dstSet = m_descriptor_sets[id][i];
        data_set[1].dstBinding = 1;
        data_set[1].dstArrayElement = 0;
        data_set[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        data_set[1].descriptorCount = 1;
        data_set[1].pBufferInfo = &sbo_info;

        vkUpdateDescriptorSets(m_vk->getDevice(), data_set.size(),
            data_set.data(), 0, NULL);

        unsigned start_block_id = 0;
        //options.g_tex_width = width;
        options.g_num_block_x = std::max(4u, width) / BLOCK_SIZE_X;
        //options.g_format = 0;
        //options.g_mode_id = 0;
        options.g_start_block_id = start_block_id;
        //options.g_num_total_blocks = num_of_blocks;
        //options.g_alpha_weight = 1.0f;

        // g_quality supports from 0 - 1.0f
        options.g_quality = 0.75f;
        vkCmdPushConstants(cmd, m_pipeline_layout,
            VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(options), &options);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
            m_pipeline_layout, 0, 1, &m_descriptor_sets[id][i], 0, NULL);
        while (num_blocks > 0)
        {
            int n = 0;
            int thread_group_count = 0;
            if (num_blocks >= MAX_BLOCK_BATCH)
            {
                n = MAX_BLOCK_BATCH;
                thread_group_count = 16;
            }
            else
            {
                n = num_blocks;
                thread_group_count = std::max(n / 4, 1);
                if ((thread_group_count * 4) < num_blocks)
                    thread_group_count++;
            }
            vkCmdPushConstants(cmd, m_pipeline_layout,
                VK_SHADER_STAGE_COMPUTE_BIT, 4, 4, &start_block_id);
            vkCmdDispatch(cmd, thread_group_count, 1, 1);
            start_block_id += n;
            num_blocks -= n;
        }  // while num_blocks

    }
}   // compress

}
