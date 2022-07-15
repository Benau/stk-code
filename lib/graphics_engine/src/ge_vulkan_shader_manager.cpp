#include "ge_vulkan_shader_manager.hpp"

#include "ge_main.hpp"
#include "ge_vulkan_driver.hpp"
#include "ge_vulkan_features.hpp"

#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <stdexcept>

#include "IFileSystem.h"

#include <glslang/SPIRV/GlslangToSpv.h>

namespace GE
{
namespace GEVulkanShaderManager
{
// ============================================================================
GEVulkanDriver* g_vk = NULL;
irr::io::IFileSystem* g_file_system = NULL;

std::string g_predefines = "";
uint32_t g_sampler_size = 256;

std::map<std::string, VkShaderModule> g_shaders;
}   // GEVulkanShaderManager

// ============================================================================
void GEVulkanShaderManager::init(GEVulkanDriver* vk)
{
    glslang::InitializeProcess();

    g_vk = vk;
    g_file_system = vk->getFileSystem();

    std::ostringstream oss;
    oss << "#version 450\n";
    oss << "#define SAMPLER_SIZE " << g_sampler_size << "\n";
    if (GEVulkanFeatures::supportsBindTexturesAtOnce())
        oss << "#define BIND_TEXTURES_AT_ONCE\n";

    if (GEVulkanFeatures::supportsDifferentTexturePerDraw())
    {
        oss << "#extension GL_EXT_nonuniform_qualifier : enable\n";
        oss << "#define GE_SAMPLE_TEX_INDEX nonuniformEXT\n";
    }
    else
        oss << "#define GE_SAMPLE_TEX_INDEX int\n";
    g_predefines = oss.str();

    loadAllShaders();
}   // init

// ----------------------------------------------------------------------------
void GEVulkanShaderManager::destroy()
{
    glslang::FinalizeProcess();

    if (!g_vk)
        return;
    for (auto& p : g_shaders)
        vkDestroyShaderModule(g_vk->getDevice(), p.second, NULL);
    g_shaders.clear();
}   // destroy

// ----------------------------------------------------------------------------
void GEVulkanShaderManager::loadAllShaders()
{
    irr::io::IFileList* files = g_file_system->createFileList(
        getShaderFolder().c_str());
    for (unsigned i = 0; i < files->getFileCount(); i++)
    {
        if (files->isDirectory(i))
            continue;
        std::string filename = files->getFileName(i).c_str();
        std::string ext = filename.substr(filename.find_last_of(".") + 1);
        EShLanguage stage;
        if (ext == "vert")
            stage = EShLangVertex;
        else if (ext == "frag")
            stage = EShLangFragment;
        else if (ext == "comp")
            stage = EShLangCompute;
        else if (ext == "tesc")
            stage = EShLangTessControl;
        else if (ext == "tese")
            stage = EShLangTessEvaluation;
        else
            continue;
        g_shaders[filename] = loadShader(stage, filename);
    }
    files->drop();
}   // loadAllShaders

// ----------------------------------------------------------------------------
VkShaderModule GEVulkanShaderManager::loadShader(EShLanguage stage,
                                                 const std::string& name)
{
    const TBuiltInResource default_resource =
    {
        /* .MaxLights = */ 32,
        /* .MaxClipPlanes = */ 6,
        /* .MaxTextureUnits = */ 32,
        /* .MaxTextureCoords = */ 32,
        /* .MaxVertexAttribs = */ 64,
        /* .MaxVertexUniformComponents = */ 4096,
        /* .MaxVaryingFloats = */ 64,
        /* .MaxVertexTextureImageUnits = */ 32,
        /* .MaxCombinedTextureImageUnits = */ 80,
        /* .MaxTextureImageUnits = */ 32,
        /* .MaxFragmentUniformComponents = */ 4096,
        /* .MaxDrawBuffers = */ 32,
        /* .MaxVertexUniformVectors = */ 128,
        /* .MaxVaryingVectors = */ 8,
        /* .MaxFragmentUniformVectors = */ 16,
        /* .MaxVertexOutputVectors = */ 16,
        /* .MaxFragmentInputVectors = */ 15,
        /* .MinProgramTexelOffset = */ -8,
        /* .MaxProgramTexelOffset = */ 7,
        /* .MaxClipDistances = */ 8,
        /* .MaxComputeWorkGroupCountX = */ 65535,
        /* .MaxComputeWorkGroupCountY = */ 65535,
        /* .MaxComputeWorkGroupCountZ = */ 65535,
        /* .MaxComputeWorkGroupSizeX = */ 1024,
        /* .MaxComputeWorkGroupSizeY = */ 1024,
        /* .MaxComputeWorkGroupSizeZ = */ 64,
        /* .MaxComputeUniformComponents = */ 1024,
        /* .MaxComputeTextureImageUnits = */ 16,
        /* .MaxComputeImageUniforms = */ 8,
        /* .MaxComputeAtomicCounters = */ 8,
        /* .MaxComputeAtomicCounterBuffers = */ 1,
        /* .MaxVaryingComponents = */ 60,
        /* .MaxVertexOutputComponents = */ 64,
        /* .MaxGeometryInputComponents = */ 64,
        /* .MaxGeometryOutputComponents = */ 128,
        /* .MaxFragmentInputComponents = */ 128,
        /* .MaxImageUnits = */ 8,
        /* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
        /* .MaxCombinedShaderOutputResources = */ 8,
        /* .MaxImageSamples = */ 0,
        /* .MaxVertexImageUniforms = */ 0,
        /* .MaxTessControlImageUniforms = */ 0,
        /* .MaxTessEvaluationImageUniforms = */ 0,
        /* .MaxGeometryImageUniforms = */ 0,
        /* .MaxFragmentImageUniforms = */ 8,
        /* .MaxCombinedImageUniforms = */ 8,
        /* .MaxGeometryTextureImageUnits = */ 16,
        /* .MaxGeometryOutputVertices = */ 256,
        /* .MaxGeometryTotalOutputComponents = */ 1024,
        /* .MaxGeometryUniformComponents = */ 1024,
        /* .MaxGeometryVaryingComponents = */ 64,
        /* .MaxTessControlInputComponents = */ 128,
        /* .MaxTessControlOutputComponents = */ 128,
        /* .MaxTessControlTextureImageUnits = */ 16,
        /* .MaxTessControlUniformComponents = */ 1024,
        /* .MaxTessControlTotalOutputComponents = */ 4096,
        /* .MaxTessEvaluationInputComponents = */ 128,
        /* .MaxTessEvaluationOutputComponents = */ 128,
        /* .MaxTessEvaluationTextureImageUnits = */ 16,
        /* .MaxTessEvaluationUniformComponents = */ 1024,
        /* .MaxTessPatchComponents = */ 120,
        /* .MaxPatchVertices = */ 32,
        /* .MaxTessGenLevel = */ 64,
        /* .MaxViewports = */ 16,
        /* .MaxVertexAtomicCounters = */ 0,
        /* .MaxTessControlAtomicCounters = */ 0,
        /* .MaxTessEvaluationAtomicCounters = */ 0,
        /* .MaxGeometryAtomicCounters = */ 0,
        /* .MaxFragmentAtomicCounters = */ 8,
        /* .MaxCombinedAtomicCounters = */ 8,
        /* .MaxAtomicCounterBindings = */ 1,
        /* .MaxVertexAtomicCounterBuffers = */ 0,
        /* .MaxTessControlAtomicCounterBuffers = */ 0,
        /* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
        /* .MaxGeometryAtomicCounterBuffers = */ 0,
        /* .MaxFragmentAtomicCounterBuffers = */ 1,
        /* .MaxCombinedAtomicCounterBuffers = */ 1,
        /* .MaxAtomicCounterBufferSize = */ 16384,
        /* .MaxTransformFeedbackBuffers = */ 4,
        /* .MaxTransformFeedbackInterleavedComponents = */ 64,
        /* .MaxCullDistances = */ 8,
        /* .MaxCombinedClipAndCullDistances = */ 8,
        /* .MaxSamples = */ 4,
        /* .maxMeshOutputVerticesNV = */ 256,
        /* .maxMeshOutputPrimitivesNV = */ 512,
        /* .maxMeshWorkGroupSizeX_NV = */ 32,
        /* .maxMeshWorkGroupSizeY_NV = */ 1,
        /* .maxMeshWorkGroupSizeZ_NV = */ 1,
        /* .maxTaskWorkGroupSizeX_NV = */ 32,
        /* .maxTaskWorkGroupSizeY_NV = */ 1,
        /* .maxTaskWorkGroupSizeZ_NV = */ 1,
        /* .maxMeshViewCountNV = */ 4,
        /* .maxDualSourceDrawBuffersEXT = */ 1,

        /* .limits = */
        {
            /* .nonInductiveForLoops = */ 1,
            /* .whileLoops = */ 1,
            /* .doWhileLoops = */ 1,
            /* .generalUniformIndexing = */ 1,
            /* .generalAttributeMatrixVectorIndexing = */ 1,
            /* .generalVaryingIndexing = */ 1,
            /* .generalSamplerIndexing = */ 1,
            /* .generalVariableIndexing = */ 1,
            /* .generalConstantMatrixVectorIndexing = */ 1,
        }
    };

    std::string shader_fullpath = getShaderFolder() + name;
    irr::io::IReadFile* r =
        g_file_system->createAndOpenFile(shader_fullpath.c_str());
    if (!r)
    {
        throw std::runtime_error(std::string("File ") + shader_fullpath +
            " is missing");
    }

    std::string shader_data;
    shader_data.resize(r->getSize());
    int nb_read = 0;
    if ((nb_read = r->read(&shader_data[0], r->getSize())) != r->getSize())
    {
        r->drop();
        throw std::runtime_error(
            std::string("File ") + name + " failed to be read");
    }
    r->drop();
    shader_data = g_predefines + shader_data;

    const char* shader_source = shader_data.c_str();
    glslang::TShader shader(stage);
    shader.setStrings(&shader_source, 1);
    shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan,
        100);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
    EShMessages messages = (EShMessages)
        (EShMsgDefault | EShMsgSpvRules | EShMsgVulkanRules);

    if (!shader.parse(&default_resource, 100, true, messages))
        throw std::runtime_error(shader.getInfoLog());

    glslang::TProgram program;
    program.addShader(&shader);
    if (!program.link(messages))
        throw std::runtime_error(shader.getInfoLog());

    std::vector<uint32_t> shader_bytecode;
    glslang::SpvOptions options;
    options.disableOptimizer = false;
    glslang::GlslangToSpv(*program.getIntermediate(stage), shader_bytecode,
        &options);

    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.pNext = NULL;
    create_info.codeSize = shader_bytecode.size() * sizeof(uint32_t);
    create_info.pCode = shader_bytecode.data();

    VkShaderModule shader_module;
    if (vkCreateShaderModule(g_vk->getDevice(), &create_info, NULL,
        &shader_module) != VK_SUCCESS)
    {
        throw std::runtime_error(
            std::string("vkCreateShaderModule failed for ") + name);
    }
    return shader_module;
}   // loadShader

// ----------------------------------------------------------------------------
unsigned GEVulkanShaderManager::getSamplerSize()
{
    return g_sampler_size;
}   // getSamplerSize

// ----------------------------------------------------------------------------
VkShaderModule GEVulkanShaderManager::getShader(const std::string& filename)
{
    return g_shaders.at(filename);
}   // getShader

}
