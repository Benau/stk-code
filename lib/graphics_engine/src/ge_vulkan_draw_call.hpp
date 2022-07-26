#ifndef HEADER_GE_VULKAN_DRAW_CALL_HPP
#define HEADER_GE_VULKAN_DRAW_CALL_HPP

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "vulkan_wrapper.h"

#include "matrix4.h"

namespace irr
{
    namespace scene { class ISceneNode; }
}

namespace GE
{
class GECullingTool;
class GESPMBuffer;
class GEVulkanAnimatedMeshSceneNode;
class GEVulkanCameraSceneNode;
class GEVulkanDriver;
class GEVulkanDynamicBuffer;
class GEVulkanTextureDescriptor;

struct ObjectData
{
    float m_mat_1[4];
    float m_mat_2[4];
    float m_mat_3[4];
    float m_mat_4[4];
    int m_skinning_offset;
    int m_material_id;
    float m_texture_trans[2];
    // ------------------------------------------------------------------------
    ObjectData(irr::scene::ISceneNode* node, int material_id,
               int skinning_offset);
};

struct PipelineSettings
{
    std::string m_vertex_shader;
    std::string m_fragment_shader;
    std::string m_shader_name;
};

class GEVulkanDrawCall
{
private:
    std::unordered_map<GESPMBuffer*, std::vector<irr::scene::ISceneNode*> > m_visible_nodes;

    GECullingTool* m_culling_tool;

    std::vector<std::pair<VkDrawIndexedIndirectCommand, std::string> > m_cmds;

    std::vector<ObjectData> m_visible_objects;

    GEVulkanDynamicBuffer* m_dynamic_data;

    size_t m_object_data_padded_size;

    size_t m_skinning_data_padded_size;

    char* m_data_padding;

    VkDescriptorSetLayout m_data_layout;

    VkDescriptorPool m_descriptor_pool;

    std::vector<VkDescriptorSet> m_data_descriptor_sets;

    VkPipelineLayout m_pipeline_layout;

    std::unordered_map<std::string, std::pair<VkPipeline, PipelineSettings> >
        m_graphics_pipelines;

    std::vector<int> m_materials;

    GEVulkanTextureDescriptor* m_texture_descriptor;

    std::unordered_set<GEVulkanAnimatedMeshSceneNode*> m_skinning_nodes;

    std::vector<std::pair<void*, size_t> > m_data_uploading;

    // ------------------------------------------------------------------------
    void createAllPipelines(GEVulkanDriver* vk);
    // ------------------------------------------------------------------------
    void createPipeline(GEVulkanDriver* vk, const PipelineSettings& settings);
    // ------------------------------------------------------------------------
    void createVulkanData();
public:
    // ------------------------------------------------------------------------
    GEVulkanDrawCall();
    // ------------------------------------------------------------------------
    ~GEVulkanDrawCall();
    // ------------------------------------------------------------------------
    void addNode(irr::scene::ISceneNode* node);
    // ------------------------------------------------------------------------
    void prepare(GEVulkanCameraSceneNode* cam);
    // ------------------------------------------------------------------------
    void generate();
    // ------------------------------------------------------------------------
    void uploadDynamicData(GEVulkanDriver* vk, GEVulkanCameraSceneNode* cam,
                           VkCommandBuffer custom_cmd = VK_NULL_HANDLE);
    // ------------------------------------------------------------------------
    void render(GEVulkanDriver* vk, GEVulkanCameraSceneNode* cam,
                VkCommandBuffer custom_cmd = VK_NULL_HANDLE);
    // ------------------------------------------------------------------------
    unsigned getPolyCount() const
    {
        unsigned result = 0;
        for (auto& cmd : m_cmds)
            result += (cmd.first.indexCount / 3) * cmd.first.instanceCount;
        return result;
    }
    // ------------------------------------------------------------------------
    void reset()
    {
        m_visible_nodes.clear();
        m_cmds.clear();
        m_visible_objects.clear();
        m_materials.clear();
        m_skinning_nodes.clear();
        m_data_uploading.clear();
    }
};   // GEVulkanDrawCall

}

#endif