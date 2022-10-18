#ifndef HEADER_GE_VULKAN_PARTICLE_MANAGER_HPP
#define HEADER_GE_VULKAN_PARTICLE_MANAGER_HPP

#include "vulkan_wrapper.h"

#include "ge_main.hpp"
#include "ge_spin_lock.hpp"

#include <cstdint>
#include <cstdio>
#include <mutex>
#include <set>
#include <vector>

#include "quaternion.h"

namespace GE
{
struct GEParticleGlobalConfig
{
    irr::core::quaternion m_camera_rotation[getMaxCameraCount()];
    unsigned m_camera_count;
    float m_dt;
    unsigned m_unused_1;
    unsigned m_unused_2;
};

struct GEGPUParticleConfig;

class GEVulkanDriver;
class GEVulkanDynamicBuffer;
class GEVulkanSceneManager;

class GEVulkanParticle;

class GEVulkanParticleManager
{
    GEVulkanDriver* m_vk;

    std::set<GEVulkanParticle*> m_rendering_particles;

    std::vector<GEGPUParticleConfig> m_rendering_configs;

    std::vector<uint8_t> m_particle_config_padding;

    VkCommandPool m_command_pool;
    VkFence m_command_fence;
    VkCommandBuffer m_command_buffer;
    VkSemaphore m_semaphore;

    unsigned m_graphics_queue_idx;

    GEParticleGlobalConfig m_global_config;

    VkDescriptorSetLayout m_particle_set_layout, m_global_set_layout,
        m_config_set_layout;
    VkDescriptorPool m_descriptor_pool;
    std::vector<VkDescriptorSet> m_global_descriptor_sets,
        m_config_descriptor_sets;

    VkPipelineLayout m_pipeline_layout;
    VkPipeline m_pipeline;

    GEVulkanDynamicBuffer* m_ubo;

    GEVulkanDynamicBuffer* m_generated_data;

    GEVulkanDynamicBuffer* m_particle_config;

    GESpinLock m_manager_lock, m_semaphore_lock;

    int m_current_frame;

    // ------------------------------------------------------------------------
    void updateDescriptorSets();
    // ------------------------------------------------------------------------
    void renderParticlesInternal();
public:
    // ------------------------------------------------------------------------
    GEVulkanParticleManager(GEVulkanDriver*);
    // ------------------------------------------------------------------------
    ~GEVulkanParticleManager();
    // ------------------------------------------------------------------------
    void lockManager()                               { m_manager_lock.lock(); }
    // ------------------------------------------------------------------------
    void unlockManager()                           { m_manager_lock.unlock(); }
    // ------------------------------------------------------------------------
    VkCommandBuffer getCommandBuffer() const       { return m_command_buffer; }
    // ------------------------------------------------------------------------
    VkCommandBuffer beginCommand();
    // ------------------------------------------------------------------------
    void endCommand();
    // ------------------------------------------------------------------------
    void addRenderingParticle(GEVulkanParticle* p)
                                           { m_rendering_particles.insert(p); }
    // ------------------------------------------------------------------------
    VkSemaphore getSemaphore() const
    {
        if (m_rendering_particles.empty())
            return VK_NULL_HANDLE;
        m_semaphore_lock.lock();
        m_semaphore_lock.unlock();
        return m_semaphore;
    }
    // ------------------------------------------------------------------------
    void renderParticles(GEVulkanSceneManager* sm);
    // ------------------------------------------------------------------------
    void finishRendering();
    // ------------------------------------------------------------------------
    std::unique_lock<std::mutex> getRequiredQueue(VkQueue& queue);
    // ------------------------------------------------------------------------
    void waitIdle()
    {
        VkQueue queue = VK_NULL_HANDLE;
        std::unique_lock<std::mutex> ul = getRequiredQueue(queue);
        vkQueueWaitIdle(queue);
        ul.unlock();
    }
    // ------------------------------------------------------------------------
    VkDescriptorSetLayout getParticleSetLayout() const
                                              { return m_particle_set_layout; }
    // ------------------------------------------------------------------------
    GEVulkanDynamicBuffer* getGeneratedData() const
            { return m_rendering_particles.empty() ? NULL : m_generated_data; }
    // ------------------------------------------------------------------------
    void recreateSemaphore();
    // ------------------------------------------------------------------------
    int getCurrentFrame() const                     { return m_current_frame; }
};   // GEVulkanParticleManager

}

#endif
