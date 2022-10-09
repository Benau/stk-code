#include "ge_vulkan_particle.hpp"

#include "ge_main.hpp"
#include "ge_vulkan_dynamic_buffer.hpp"
#include "ge_vulkan_driver.hpp"
#include "ge_vulkan_particle_manager.hpp"

#include "IParticleSystemSceneNode.h"

#include <array>
#include <exception>

namespace GE
{
// ----------------------------------------------------------------------------
GEVulkanParticle::GEVulkanParticle()
{
    m_config = {};
    m_node = NULL;
    m_first_execution = false;
    m_initial_particles = NULL;
    m_particles_generating = NULL;

    // m_descriptor_pool
    VkDescriptorPoolSize pool_size;
    pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pool_size.descriptorCount = 2;

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = 0;
    pool_info.maxSets = 1;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;

    GEVulkanDriver* vk = getVKDriver();
    if (vkCreateDescriptorPool(vk->getDevice(), &pool_info, NULL,
        &m_descriptor_pool) != VK_SUCCESS)
    {
        throw std::runtime_error("vkCreateDescriptorPool failed for "
            "GEVulkanParticle");
    }

    // m_descriptor_set
    GEVulkanParticleManager* pm = vk->getParticleManager();
    std::vector<VkDescriptorSetLayout> layouts(1, pm->getParticleSetLayout());

    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = m_descriptor_pool;
    alloc_info.descriptorSetCount = layouts.size();
    alloc_info.pSetLayouts = layouts.data();

    if (vkAllocateDescriptorSets(vk->getDevice(), &alloc_info,
        &m_descriptor_set) != VK_SUCCESS)
    {
        throw std::runtime_error("vkAllocateDescriptorSets failed for "
            "GEVulkanParticle");
    }
}   // GEVulkanParticle

// ----------------------------------------------------------------------------
GEVulkanParticle::~GEVulkanParticle()
{
    GEVulkanDriver* vk = getVKDriver();
    vk->setDisableWaitIdle(true);
    vk->getParticleManager()->waitIdle();
    vkDestroyDescriptorPool(vk->getDevice(), m_descriptor_pool, NULL);
    delete m_initial_particles;
    delete m_particles_generating;
    vk->setDisableWaitIdle(false);
}   // ~GEVulkanParticle

// ----------------------------------------------------------------------------
void GEVulkanParticle::init(void* initial, void* initial_generating,
                            irr::scene::IParticleSystemSceneNode* node,
                            float size_increase_factor,
                            irr::video::SColor color_from,
                            irr::video::SColor color_to,
                            bool flips, bool pre_generating)
{
    m_node = node;
    m_config = {};
    m_config.m_max_count = node->getMaxCount();
    m_config.m_active_count = node->getActiveCount();
    m_config.m_size_increase_factor = size_increase_factor;
    m_config.m_color_from = color_from;
    m_config.m_color_to = color_to;
    m_config.m_first_execution = 1;
    m_config.m_flips = flips;
    m_config.m_pre_generating = pre_generating;

    GEVulkanDriver* vk = getVKDriver();
    vk->setDisableWaitIdle(true);
    GEVulkanParticleManager* pm = vk->getParticleManager();
    pm->waitIdle();

    size_t total = m_config.m_max_count * sizeof(GEGPUParticleData);
    if (!m_initial_particles)
    {
        m_initial_particles = new GEVulkanDynamicBuffer(
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, total, 1, 1);
    }
    else
        m_initial_particles->resizeIfNeeded(total);

    if (!m_particles_generating)
    {
        m_particles_generating = new GEVulkanDynamicBuffer(
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, total, 1, 1);
    }
    else
        m_particles_generating->resizeIfNeeded(total);

    std::array<VkDescriptorBufferInfo, 2> buffer_infos;
    buffer_infos[0].buffer = m_initial_particles->getCurrentBuffer();
    buffer_infos[0].offset = 0;
    buffer_infos[0].range = total;
    buffer_infos[1].buffer = m_particles_generating->getCurrentBuffer();
    buffer_infos[1].offset = 0;
    buffer_infos[1].range = total;

    VkWriteDescriptorSet write_descriptor_set = {};
    write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_descriptor_set.dstBinding = 0;
    write_descriptor_set.dstArrayElement = 0;
    write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write_descriptor_set.descriptorCount = buffer_infos.size();
    write_descriptor_set.pBufferInfo = buffer_infos.data();
    write_descriptor_set.dstSet = m_descriptor_set;
    vkUpdateDescriptorSets(vk->getDevice(), 1, &write_descriptor_set, 0,
        NULL);

    pm->lockManager();
    VkCommandBuffer cmd = pm->beginCommand();
    m_initial_particles->setCurrentData(initial, total, cmd);
    m_particles_generating->setCurrentData(initial_generating, total, cmd);
    pm->endCommand();
    vk->setDisableWaitIdle(false);
    m_first_execution = true;
}   // init

}
