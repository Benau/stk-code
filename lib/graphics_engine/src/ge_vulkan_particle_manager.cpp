#include "ge_vulkan_particle_manager.hpp"

#include "ge_main.hpp"
#include "ge_vulkan_command_loader.hpp"
#include "ge_vulkan_draw_call.hpp"
#include "ge_vulkan_driver.hpp"
#include "ge_vulkan_dynamic_buffer.hpp"
#include "ge_vulkan_features.hpp"
#include "ge_vulkan_particle.hpp"
#include "ge_vulkan_scene_manager.hpp"
#include "ge_vulkan_shader_manager.hpp"

#include <array>
#include <exception>
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

    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    result = vkCreateSemaphore(m_vk->getDevice(), &semaphore_info, NULL,
        &m_semaphore);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error(
            "GEVulkanParticleManager: vkCreateSemaphore failed");
    }
    m_wait_semaphore = false;

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

    // m_descriptor_pool
    std::array<VkDescriptorPoolSize, 2> pool_size =
    {{
        {
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_vk->getMaxFrameInFlight()
        },
        {
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_vk->getMaxFrameInFlight()
        }
    }};

    VkDescriptorPoolCreateInfo descriptor_pool_info = {};
    descriptor_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptor_pool_info.flags = 0;
    descriptor_pool_info.maxSets = m_vk->getMaxFrameInFlight();
    descriptor_pool_info.poolSizeCount = pool_size.size();
    descriptor_pool_info.pPoolSizes = pool_size.data();
    if (vkCreateDescriptorPool(m_vk->getDevice(), &descriptor_pool_info, NULL,
        &m_descriptor_pool) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "GEVulkanParticleManager: vkCreateDescriptorPool failed");
    }

    // m_descriptor_set
    m_descriptor_set.resize(m_vk->getMaxFrameInFlight());
    std::vector<VkDescriptorSetLayout> layouts(m_descriptor_set.size(),
        m_global_set_layout);

    VkDescriptorSetAllocateInfo set_alloc_info = {};
    set_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set_alloc_info.descriptorPool = m_descriptor_pool;
    set_alloc_info.descriptorSetCount = layouts.size();
    set_alloc_info.pSetLayouts = layouts.data();

    if (vkAllocateDescriptorSets(m_vk->getDevice(), &set_alloc_info,
        m_descriptor_set.data()) != VK_SUCCESS)
    {
        throw std::runtime_error(
            "GEVulkanParticleManager: vkAllocateDescriptorSets failed");
    }

    // m_pipeline_layout
    std::array<VkDescriptorSetLayout, 2> all_layouts =
    {{
        m_particle_set_layout,
        m_global_set_layout
    }};

    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = all_layouts.size();
    pipeline_layout_info.pSetLayouts = all_layouts.data();

    VkPushConstantRange push_constant;
    push_constant.offset = 0;
    push_constant.size = sizeof(GEParticleGlobalConfig);
    push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pipeline_layout_info.pPushConstantRanges = &push_constant;
    pipeline_layout_info.pushConstantRangeCount = 1;

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
        sizeof(GEParticleGlobalConfig), GEVulkanDriver::getMaxFrameInFlight(),
        GEVulkanDriver::getMaxFrameInFlight());

    m_generated_data = new GEVulkanDynamicBuffer(
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        100000, GEVulkanDriver::getMaxFrameInFlight(),
        GEVulkanDriver::getMaxFrameInFlight());

    updateDescriptorSet();
    m_global_config = {};
}   // GEVulkanParticleManager

// ----------------------------------------------------------------------------
GEVulkanParticleManager::~GEVulkanParticleManager()
{
    GEVulkanCommandLoader::waitIdle();
    delete m_ubo;
    delete m_generated_data;
    vkDestroyPipeline(m_vk->getDevice(), m_pipeline, NULL);
    vkDestroyPipelineLayout(m_vk->getDevice(), m_pipeline_layout, NULL);
    vkDestroyDescriptorPool(m_vk->getDevice(), m_descriptor_pool, NULL);
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
VkCommandBuffer GEVulkanParticleManager::beginCommand()
{
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(m_command_buffer, &begin_info);
    return m_command_buffer;
}   // beginCommand

// ----------------------------------------------------------------------------
void GEVulkanParticleManager::endCommand(bool blocking)
{
    vkEndCommandBuffer(m_command_buffer);

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &m_command_buffer;
    if (!m_rendering_particles.empty())
    {
        m_wait_semaphore = true;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &m_semaphore;
    }
    else
        m_wait_semaphore = false;

    auto submit = [this, submit_info]()
    {
        VkQueue queue = VK_NULL_HANDLE;
        std::unique_lock<std::mutex> ul = getRequiredQueue(queue);
        vkQueueSubmit(queue, 1, &submit_info, m_command_fence);
        ul.unlock();

        vkWaitForFences(m_vk->getDevice(), 1, &m_command_fence,
            VK_TRUE, std::numeric_limits<uint64_t>::max());
        vkResetFences(m_vk->getDevice(), 1, &m_command_fence);
        vkResetCommandPool(m_vk->getDevice(), m_command_pool, 0);
        unlockManager();
    };
    if (blocking)
        submit();
    else
        GEVulkanCommandLoader::addMultiThreadingCommand(submit);
}   // endCommand

// ----------------------------------------------------------------------------
void GEVulkanParticleManager::renderParticles(GEVulkanSceneManager* sm)
{
    if (m_rendering_particles.empty())
        return;
    lockManager();
    size_t required_size = 0;
    for (GEVulkanParticle* p : m_rendering_particles)
        required_size += p->getConfig().m_max_count * sizeof(ObjectData);
    required_size *= sm->getDrawCalls().size();
    if (m_generated_data->resizeIfNeeded(required_size))
        updateDescriptorSet();

    VkCommandBuffer cmd = beginCommand();
    
    endCommand();
    m_rendering_particles.clear();
}   // renderParticles

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
void GEVulkanParticleManager::updateDescriptorSet()
{
   /* for (unsigned i = 0; i < 2; i++)
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
*/
}   // updateDescriptorSet

}
