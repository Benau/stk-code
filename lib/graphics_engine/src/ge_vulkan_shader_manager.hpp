#ifndef HEADER_GE_VULKAN_SHADER_MANAGER_HPP
#define HEADER_GE_VULKAN_SHADER_MANAGER_HPP

#include "vulkan_wrapper.h"
#include <string>

#include <glslang/Public/ShaderLang.h>

namespace GE
{
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
VkShaderModule loadShader(EShLanguage, const std::string&);
// ----------------------------------------------------------------------------
unsigned getSamplerSize();
};   // GEVulkanShaderManager

}

#endif
