#include "ge_vulkan_particle_manager.hpp"

#include "ge_main.hpp"
#include "ge_vulkan_camera_scene_node.hpp"
#include "ge_vulkan_command_loader.hpp"
#include "ge_vulkan_draw_call.hpp"
#include "ge_vulkan_driver.hpp"
#include "ge_vulkan_dynamic_buffer.hpp"
#include "ge_vulkan_features.hpp"
#include "ge_vulkan_particle.hpp"
#include "ge_vulkan_scene_manager.hpp"
#include "ge_vulkan_shader_manager.hpp"

#include "IParticleSystemSceneNode.h"
#include "mini_glm.hpp"

#include <array>
#include <exception>
#include <functional>
#include <vector>

namespace GE
{
// ----------------------------------------------------------------------------
GEVulkanParticleManager::GEVulkanParticleManager(GEVulkanDriver* vk)
{
    m_vk = vk;
    // Use the last idx to avoid interfere with main rendering
    m_graphics_queue_idx = m_vk->getGraphicsQueueCount() - 1;

    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    if (GEVulkanFeatures::supportsSeparatedComputeQueue())
        pool_info.queueFamilyIndex = m_vk->getComputeFamily();
    else if (GEVulkanFeatures::supportsComputeInMainQueue())
        pool_info.queueFamilyIndex = m_vk->getGraphicsFamily();
    else
    {
        throw std::runtime_error(
            "GEVulkanParticleManager: missing compute queue support");
    }

    VkResult result = vkCreateCommandPool(m_vk->getDevice(), &pool_info,
        NULL, &m_command_pool);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error(
            "GEVulkanParticleManager: vkCreateCommandPool failed");
    }

    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    result = vkCreateFence(m_vk->getDevice(), &fence_info, NULL,
        &m_command_fence);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error(
            "GEVulkanParticleManager: vkCreateFence failed");
    }

    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = m_command_pool;
    alloc_info.commandBufferCount = 1;

    result = vkAllocateCommandBuffers(m_vk->getDevice(), &alloc_info,
        &m_command_buffer);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error(
            "GEVulkanParticleManager: vkAllocateCommandBuffers failed");
    }

    m_semaphore = VK_NULL_HANDLE;
    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    result = vkCreateSemaphore(m_vk->getDevice(), &semaphore_info, NULL,
        &m_semaphore);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error(
            "GEVulkanParticleManager: vkCreateSemaphore failed");
    }

    // m_particle_set_layout
    std::vector<VkDescriptorSetLayoutBinding> particle_layout_binding;
    for (unsigned i = 0; i < 2; i++)
    {
        VkDescriptorSetLayoutBinding binding = {};
        binding.binding = i;
        binding.descriptorCount = 1;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        particle_layout_binding.push_back(binding);
    }

    VkDescriptorSetLayoutCreateInfo setinfo = {};
    setinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    setinfo.pBindings = particle_layout_binding.data();
    setinfo.bindingCount = particle_layout_binding.size();
    if (vkCreateDescriptorSetLayout(m_vk->getDevice(), &setinfo,
        NULL, &m_particle_set_layout) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "GEVulkanParticleManager: vkCreateDescriptorSetLayout failed for "
            "m_particle_set_layout");
    }

    // m_global_set_layout
    std::vector<VkDescriptorSetLayoutBinding> global_layout_binding;
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = 0;
    binding.descriptorCount = 1;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    global_layout_binding.push_back(binding);
    binding.binding = 1;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    global_layout_binding.push_back(binding);

    setinfo.pBindings = global_layout_binding.data();
    setinfo.bindingCount = global_layout_binding.size();
    if (vkCreateDescriptorSetLayout(m_vk->getDevice(), &setinfo,
        NULL, &m_global_set_layout) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "GEVulkanParticleManager: vkCreateDescriptorSetLayout failed for "
            "m_global_set_layout");
    }

    // m_config_set_layout
    std::vector<VkDescriptorSetLayoutBinding> config_layout_binding;
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    config_layout_binding.push_back(binding);
    setinfo.pBindings = config_layout_binding.data();
    setinfo.bindingCount = config_layout_binding.size();
    if (vkCreateDescriptorSetLayout(m_vk->getDevice(), &setinfo,
        NULL, &m_config_set_layout) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "GEVulkanParticleManager: vkCreateDescriptorSetLayout failed for "
            "m_global_set_layout");
    }

    const unsigned total_frame = GEVulkanDriver::getMaxFrameInFlight() + 1;
    // m_descriptor_pool
    std::array<VkDescriptorPoolSize, 3> pool_size =
    {{
        {
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, total_frame
        },
        {
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, total_frame
        },
        {
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, total_frame
        }
    }};

    VkDescriptorPoolCreateInfo descriptor_pool_info = {};
    descriptor_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptor_pool_info.flags = 0;
    descriptor_pool_info.maxSets = total_frame * 2;
    descriptor_pool_info.poolSizeCount = pool_size.size();
    descriptor_pool_info.pPoolSizes = pool_size.data();
    if (vkCreateDescriptorPool(m_vk->getDevice(), &descriptor_pool_info, NULL,
        &m_descriptor_pool) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "GEVulkanParticleManager: vkCreateDescriptorPool failed");
    }

    // m_global_descriptor_sets
    m_global_descriptor_sets.resize(total_frame);
    std::vector<VkDescriptorSetLayout> layouts(m_global_descriptor_sets.size(),
        m_global_set_layout);

    VkDescriptorSetAllocateInfo set_alloc_info = {};
    set_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set_alloc_info.descriptorPool = m_descriptor_pool;
    set_alloc_info.descriptorSetCount = layouts.size();
    set_alloc_info.pSetLayouts = layouts.data();

    if (vkAllocateDescriptorSets(m_vk->getDevice(), &set_alloc_info,
        m_global_descriptor_sets.data()) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "GEVulkanParticleManager: vkAllocateDescriptorSets failed for "
            "m_global_descriptor_sets");
    }

    // m_config_descriptor_sets
    m_config_descriptor_sets.resize(total_frame);
    layouts = std::vector<VkDescriptorSetLayout>(
        m_config_descriptor_sets.size(), m_config_set_layout);
    set_alloc_info.descriptorSetCount = layouts.size();
    set_alloc_info.pSetLayouts = layouts.data();

    if (vkAllocateDescriptorSets(m_vk->getDevice(), &set_alloc_info,
        m_config_descriptor_sets.data()) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "GEVulkanParticleManager: vkAllocateDescriptorSets failed "
            "m_config_descriptor_sets");
    }

    // m_pipeline_layout
    std::array<VkDescriptorSetLayout, 3> all_layouts =
    {{
        m_particle_set_layout,
        m_global_set_layout,
        m_config_set_layout
    }};

    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = all_layouts.size();
    pipeline_layout_info.pSetLayouts = all_layouts.data();

    result = vkCreatePipelineLayout(m_vk->getDevice(), &pipeline_layout_info,
        NULL, &m_pipeline_layout);

    if (result != VK_SUCCESS)
    {
        throw std::runtime_error(
            "GEVulkanParticleManager: vkCreatePipelineLayout failed");
    }

    VkComputePipelineCreateInfo compute_info = {};
    compute_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compute_info.stage.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    compute_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    compute_info.stage.module =
        GEVulkanShaderManager::getShader("normal_particle.comp");
    compute_info.stage.pName = "main";
    compute_info.layout = m_pipeline_layout;

    if (vkCreateComputePipelines(m_vk->getDevice(), NULL, 1, &compute_info,
        NULL, &m_pipeline) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "GEVulkanParticleManager: vkCreateComputePipelines failed");
    }

    m_ubo = new GEVulkanDynamicBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        sizeof(GEParticleGlobalConfig), total_frame, total_frame);

    m_generated_data = new GEVulkanDynamicBuffer(
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        100000, 0, total_frame);

    m_particle_config = new GEVulkanDynamicBuffer(
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(GEGPUParticleConfig) * 50,
        total_frame, total_frame);

    size_t particle_config_padding = getPadding(sizeof(GEGPUParticleConfig),
        m_vk->getPhysicalDeviceProperties().limits
        .minStorageBufferOffsetAlignment);
    m_particle_config_padding.resize(particle_config_padding);

    updateDescriptorSets();
    m_global_config = {};
    m_current_frame = 0;
}   // GEVulkanParticleManager

// ----------------------------------------------------------------------------
GEVulkanParticleManager::~GEVulkanParticleManager()
{
    GEVulkanCommandLoader::waitIdle();
    delete m_ubo;
    delete m_generated_data;
    delete m_particle_config;
    vkDestroyPipeline(m_vk->getDevice(), m_pipeline, NULL);
    vkDestroyPipelineLayout(m_vk->getDevice(), m_pipeline_layout, NULL);
    vkDestroyDescriptorPool(m_vk->getDevice(), m_descriptor_pool, NULL);
    vkDestroyDescriptorSetLayout(m_vk->getDevice(), m_config_set_layout,
        NULL);
    vkDestroyDescriptorSetLayout(m_vk->getDevice(), m_global_set_layout,
        NULL);
    vkDestroyDescriptorSetLayout(m_vk->getDevice(), m_particle_set_layout,
        NULL);
    vkDestroySemaphore(m_vk->getDevice(), m_semaphore, NULL);
    vkFreeCommandBuffers(m_vk->getDevice(), m_command_pool, 1,
        &m_command_buffer);
    vkDestroyCommandPool(m_vk->getDevice(), m_command_pool, NULL);
    vkDestroyFence(m_vk->getDevice(), m_command_fence, NULL);
}   // ~GEVulkanParticleManager

// ----------------------------------------------------------------------------
void GEVulkanParticleManager::recreateSemaphore()
{
    m_semaphore_lock.lock();
    m_semaphore_lock.unlock();
    if (m_semaphore != VK_NULL_HANDLE)
        vkDestroySemaphore(m_vk->getDevice(), m_semaphore, NULL);
    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    if (vkCreateSemaphore(m_vk->getDevice(), &semaphore_info, NULL,
        &m_semaphore) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "GEVulkanParticleManager: vkCreateSemaphore failed");
    }
}   // recreateSemaphore

// ----------------------------------------------------------------------------
VkCommandBuffer GEVulkanParticleManager::beginCommand()
{
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(m_command_buffer, &begin_info);
    return m_command_buffer;
}   // beginCommand

// ----------------------------------------------------------------------------
void GEVulkanParticleManager::endCommand()
{
    vkEndCommandBuffer(m_command_buffer);

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &m_command_buffer;
    if (!m_rendering_particles.empty())
    {
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &m_semaphore;
    }

    VkQueue queue = VK_NULL_HANDLE;
    std::unique_lock<std::mutex> ul = getRequiredQueue(queue);
    vkQueueSubmit(queue, 1, &submit_info, m_command_fence);
    ul.unlock();
    m_semaphore_lock.unlock();

    vkWaitForFences(m_vk->getDevice(), 1, &m_command_fence,
        VK_TRUE, std::numeric_limits<uint64_t>::max());
    vkResetFences(m_vk->getDevice(), 1, &m_command_fence);
    vkResetCommandPool(m_vk->getDevice(), m_command_pool, 0);

    unlockManager();
}   // endCommand

// ----------------------------------------------------------------------------
void GEVulkanParticleManager::renderParticles(GEVulkanSceneManager* sm)
{
    if (m_rendering_particles.empty())
        return;
    lockManager();
    size_t required_size = 0;
    m_global_config.m_camera_count = sm->getDrawCalls().size();
    int idx = 0;
    for (auto& p : sm->getDrawCalls())
    {
        m_global_config.m_camera_rotation[idx++] =
            MiniGLM::getQuaternion(p.first->getViewMatrix());
    }

    unsigned offset = 0;
    for (GEVulkanParticle* p : m_rendering_particles)
    {
        required_size += p->getConfig().m_max_count * sizeof(ObjectData);
        p->getConfig().m_offset = offset;
        offset += p->getConfig().m_max_count * m_global_config.m_camera_count;
        const irr::core::matrix4& model_mat =
            p->getNode()->getAbsoluteTransformation();
        memcpy(&p->getConfig().m_translation.X, &model_mat[12],
            sizeof(irr::core::vector3df));
        irr::core::quaternion rotation(0.0f, 0.0f, 0.0f, 1.0f);
        irr::core::vector3df scale = model_mat.getScale();
        if (scale.X != 0.0f && scale.Y != 0.0f && scale.Z != 0.0f)
        {
            irr::core::matrix4 local_mat = model_mat;
            local_mat[0] = local_mat[0] / scale.X / local_mat[15];
            local_mat[1] = local_mat[1] / scale.X / local_mat[15];
            local_mat[2] = local_mat[2] / scale.X / local_mat[15];
            local_mat[4] = local_mat[4] / scale.Y / local_mat[15];
            local_mat[5] = local_mat[5] / scale.Y / local_mat[15];
            local_mat[6] = local_mat[6] / scale.Y / local_mat[15];
            local_mat[8] = local_mat[8] / scale.Z / local_mat[15];
            local_mat[9] = local_mat[9] / scale.Z / local_mat[15];
            local_mat[10] = local_mat[10] / scale.Z / local_mat[15];
            rotation = MiniGLM::getQuaternion(local_mat);
            // Conjugated quaternion in glsl
            rotation.W = -rotation.W;
        }
        memcpy(&p->getConfig().m_rotation.X, &rotation,
            sizeof(irr::core::quaternion));
        memcpy(&p->getConfig().m_scale.X, &scale,
            sizeof(irr::core::vector3df));
        m_rendering_configs.push_back(p->getConfig());
        p->getConfig().m_first_execution = 0;
    }
    required_size *= m_global_config.m_camera_count;

    size_t particle_config_size = m_rendering_configs.size() * (
        sizeof(GEGPUParticleConfig) + m_particle_config_padding.size());
    if (m_generated_data->resizeIfNeeded(required_size) ||
        m_particle_config->resizeIfNeeded(particle_config_size))
        updateDescriptorSets();

    m_semaphore_lock.lock();
    GEVulkanCommandLoader::addMultiThreadingCommand(
        std::bind(&GEVulkanParticleManager::renderParticlesInternal, this));
}   // renderParticles

// ----------------------------------------------------------------------------
void GEVulkanParticleManager::renderParticlesInternal()
{
    VkCommandBuffer cmd = beginCommand();
    m_ubo->setCurrentData(&m_global_config, sizeof(GEParticleGlobalConfig),
        cmd, m_current_frame);

    if (m_particle_config_padding.empty())
    {
        m_particle_config->setCurrentData(m_rendering_configs.data(),
            m_rendering_configs.size() * sizeof(GEGPUParticleConfig),
            cmd, m_current_frame);
    }
    else
    {
        std::vector<std::pair<void*, size_t> > data_uploading;
        for (GEGPUParticleConfig& config : m_rendering_configs)
        {
            data_uploading.emplace_back(&config, sizeof(GEGPUParticleConfig));
            data_uploading.emplace_back(m_particle_config_padding.data(),
                m_particle_config_padding.size());
        }
        m_particle_config->setCurrentData(data_uploading, cmd,
            m_current_frame);
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
        m_pipeline_layout, 1, 1, &m_global_descriptor_sets[m_current_frame], 0,
        NULL);

    uint32_t dynamic_offset = 0;
    unsigned idx = 0;
    for (GEVulkanParticle* p : m_rendering_particles)
    {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
            m_pipeline_layout, 0, 1, p->getDescriptorSet(), 0, NULL);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
            m_pipeline_layout, 2, 1,
            &m_config_descriptor_sets[m_current_frame], 1, &dynamic_offset);
        vkCmdDispatch(cmd, (m_rendering_configs[idx++].m_max_count / 256) + 1,
            1, 1);
        dynamic_offset += sizeof(GEGPUParticleConfig) +
            m_particle_config_padding.size();
    }

    if (GEVulkanFeatures::supportsSeparatedComputeQueue())
    {
        VkBufferMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.srcQueueFamilyIndex = m_vk->getComputeFamily();
        barrier.dstQueueFamilyIndex = m_vk->getGraphicsFamily();
        barrier.buffer = m_generated_data->getLocalBuffer()[m_current_frame];
        barrier.offset = 0;
        barrier.size = VK_WHOLE_SIZE;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 1, &barrier, 0,
            NULL);
    }

    endCommand();
}   // renderParticlesInternal

// ----------------------------------------------------------------------------
std::unique_lock<std::mutex> GEVulkanParticleManager::getRequiredQueue(
                                                                VkQueue& queue)
{
    std::unique_lock<std::mutex> lock =
        GEVulkanFeatures::supportsSeparatedComputeQueue() ?
        m_vk->getComputeQueue(&queue) :
        m_vk->getGraphicsQueue(&queue, m_graphics_queue_idx);
    return lock;
}   // getRequiredQueue

// ----------------------------------------------------------------------------
void GEVulkanParticleManager::updateDescriptorSets()
{
    for (unsigned i = 0; i < m_global_descriptor_sets.size(); i++)
    {
        std::array<VkWriteDescriptorSet, 2> write_descriptor_sets = {};
        VkDescriptorBufferInfo sbo;
        sbo.buffer = m_generated_data->getLocalBuffer()[i];
        sbo.offset = 0;
        sbo.range = VK_WHOLE_SIZE;

        VkDescriptorBufferInfo ubo;
        ubo.buffer = m_ubo->getLocalBuffer()[i];
        ubo.offset = 0;
        ubo.range = sizeof(GEParticleGlobalConfig);

        write_descriptor_sets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write_descriptor_sets[0].dstBinding = 0;
        write_descriptor_sets[0].dstArrayElement = 0;
        write_descriptor_sets[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write_descriptor_sets[0].descriptorCount = 1;
        write_descriptor_sets[0].pBufferInfo = &sbo;
        write_descriptor_sets[0].dstSet = m_global_descriptor_sets[i];

        write_descriptor_sets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write_descriptor_sets[1].dstBinding = 1;
        write_descriptor_sets[1].dstArrayElement = 0;
        write_descriptor_sets[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write_descriptor_sets[1].descriptorCount = 1;
        write_descriptor_sets[1].pBufferInfo = &ubo;
        write_descriptor_sets[1].dstSet = m_global_descriptor_sets[i];

        vkUpdateDescriptorSets(m_vk->getDevice(), write_descriptor_sets.size(),
            write_descriptor_sets.data(), 0, NULL);
    }

    for (unsigned i = 0; i < m_config_descriptor_sets.size(); i++)
    {
        VkWriteDescriptorSet write_descriptor_set = {};
        VkDescriptorBufferInfo sbo;
        sbo.buffer = m_particle_config->getLocalBuffer()[i];
        sbo.offset = 0;
        sbo.range = sizeof(GEGPUParticleConfig);

        write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write_descriptor_set.dstBinding = 0;
        write_descriptor_set.dstArrayElement = 0;
        write_descriptor_set.descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        write_descriptor_set.descriptorCount = 1;
        write_descriptor_set.pBufferInfo = &sbo;
        write_descriptor_set.dstSet = m_config_descriptor_sets[i];

        vkUpdateDescriptorSets(m_vk->getDevice(), 1, &write_descriptor_set, 0,
            NULL);
    }
}   // updateDescriptorSets

// ----------------------------------------------------------------------------
void GEVulkanParticleManager::finishRendering()
{
    if (!m_rendering_particles.empty())
    {
        m_rendering_particles.clear();
        m_rendering_configs.clear();
        m_current_frame = (m_current_frame + 1) %
            GEVulkanDriver::getMaxFrameInFlight() + 1;
    }
}   // finishRendering

}
