#ifndef HEADER_GE_VULKAN_PARTICLE_HPP
#define HEADER_GE_VULKAN_PARTICLE_HPP

#include "vulkan_wrapper.h"

#include "matrix4.h"
#include "quaternion.h"
#include "vector3d.h"
#include "SColor.h"

namespace irr
{
    namespace scene
    {
        class IParticleSystemSceneNode;
    }
}

namespace GE
{
struct GEGPUParticleData
{
    irr::core::vector3df m_position;
    float m_lifetime;
    irr::core::vector3df m_direction;
    float m_size;
};

struct GEGPUParticleConfig
{
    irr::core::vector3df m_translation;
    unsigned m_max_count;

    irr::core::quaternion m_rotation;

    irr::core::vector3df m_scale;
    unsigned m_active_count;

    irr::video::SColor m_color_from;
    irr::video::SColor m_color_to;
    float m_size_increase_factor;
    int m_material_id;

    unsigned m_offset;
    unsigned m_first_execution;
    unsigned m_pre_generating;
    unsigned m_flips;
};

class GEVulkanDynamicBuffer;

class GEVulkanParticle
{
    bool m_first_execution;

    irr::scene::IParticleSystemSceneNode* m_node;

    GEGPUParticleConfig m_config;

    GEVulkanDynamicBuffer* m_initial_particles;

    GEVulkanDynamicBuffer* m_particles_generating;

    VkDescriptorPool m_descriptor_pool;

    VkDescriptorSet m_descriptor_set;
public:
    // ------------------------------------------------------------------------
    GEVulkanParticle();
    // ------------------------------------------------------------------------
    ~GEVulkanParticle();
    // ------------------------------------------------------------------------
    void init(void* initial, void* initial_generating,
              irr::scene::IParticleSystemSceneNode* node,
              float size_increase_factor,
              irr::video::SColor color_from, irr::video::SColor color_to,
              bool flips, bool pre_generating);
    // ------------------------------------------------------------------------
    GEGPUParticleConfig& getConfig()                       { return m_config; }
    // ------------------------------------------------------------------------
    VkDescriptorSet getDescriptorSet() const       { return m_descriptor_set; }
};   // GEVulkanParticle

}

#endif
