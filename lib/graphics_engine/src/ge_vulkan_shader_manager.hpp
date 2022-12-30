#ifndef HEADER_GE_VULKAN_SHADER_MANAGER_HPP
#define HEADER_GE_VULKAN_SHADER_MANAGER_HPP

#include "vulkan_wrapper.h"
#include <array>
#include <memory>
#include <string>
#include <shaderc/shaderc.h>

namespace GE
{
struct ShaderConstantsData
{
VkBool32 m_bind_textures_at_once;
VkBool32 m_bind_mesh_textures_at_once;
VkBool32 m_different_texture_per_draw;
uint32_t m_sampler_size;
uint32_t m_total_mesh_texture_layer;

std::array<VkSpecializationMapEntry, 5> m_map_entries;
VkSpecializationInfo m_info;
// ----------------------------------------------------------------------------
void init();
};

class GEVulkanDriver;
namespace GEVulkanShaderManager
{
// ----------------------------------------------------------------------------
void init(GEVulkanDriver*);
// ----------------------------------------------------------------------------
void destroy();
// ----------------------------------------------------------------------------
void loadAllShaders();
// ----------------------------------------------------------------------------
VkShaderModule getShader(const std::string& filename);
// ----------------------------------------------------------------------------
VkShaderModule loadShader(shaderc_shader_kind, const std::string&);
// ----------------------------------------------------------------------------
unsigned getSamplerSize();
// ----------------------------------------------------------------------------
unsigned getMeshTextureLayer();
// ----------------------------------------------------------------------------
std::unique_ptr<ShaderConstantsData> getShaderConstantsData();
};   // GEVulkanShaderManager

}

#endif
