#ifndef HEADER_GE_VULKAN_PARTICLE_MANAGER_HPP
#define HEADER_GE_VULKAN_PARTICLE_MANAGER_HPP

#include "vulkan_wrapper.h"

#include "ge_main.hpp"
#include "ge_spin_lock.hpp"

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
    unsigned m_unused[2];
};

class GEVulkanDriver;
class GEVulkanDynamicBuffer;
class GEVulkanSceneManager;

class GEVulkanParticle;

class GEVulkanParticleManager
{
    GEVulkanDriver* m_vk;

    std::set<GEVulkanParticle*> m_rendering_particles;

    VkCommandPool m_command_pool;
    VkFence m_command_fence;
    VkCommandBuffer m_command_buffer;
    VkSemaphore m_semaphore;

    bool m_wait_semaphore;

    unsigned m_graphics_queue_idx;

    GEParticleGlobalConfig m_global_config;

    VkDescriptorSetLayout m_particle_set_layout, m_global_set_layout;
    VkDescriptorPool m_descriptor_pool;
    std::vector<VkDescriptorSet> m_descriptor_set;

    VkPipelineLayout m_pipeline_layout;
    VkPipeline m_pipeline;

    GEVulkanDynamicBuffer* m_ubo;

    GEVulkanDynamicBuffer* m_generated_data;

    GESpinLock m_manager_lock;
    // ------------------------------------------------------------------------
    void updateDescriptorSet();
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
    void endCommand(bool blocking = false);
    // ------------------------------------------------------------------------
    void addRenderingParticle(GEVulkanParticle* p)
                                           { m_rendering_particles.insert(p); }
    // ------------------------------------------------------------------------
    VkSemaphore getSemaphore() const
                    { return m_wait_semaphore ? m_semaphore : VK_NULL_HANDLE; }
    // ------------------------------------------------------------------------
    void renderParticles(GEVulkanSceneManager* sm);
    // ------------------------------------------------------------------------
    void reset()                             { m_rendering_particles.clear(); }
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
};   // GEVulkanParticleManager

}

#endif
