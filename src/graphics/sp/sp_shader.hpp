//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2017 SuperTuxKart-Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#ifndef HEADER_SP_SHADER_HPP
#define HEADER_SP_SHADER_HPP

#include "graphics/gl_headers.hpp"
#include "graphics/sp/sp_per_object_uniform.hpp"
#include "utils/log.hpp"
#include "utils/no_copy.hpp"

#include <cstring>
#include <functional>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <SMaterial.h>

namespace SP
{

enum SamplerType: unsigned int
{
    ST_NEAREST,
    ST_NEAREST_CLAMPED,
    ST_TRILINEAR,
    ST_TRILINEAR_CLAMPED,
    ST_BILINEAR,
    ST_BILINEAR_CLAMPED,
    ST_SEMI_TRILINEAR,
    ST_SHADOW,
    ST_TEXTURE_BUFFER,
    ST_COUNT
};

enum RenderPass: unsigned int
{
    RP_1ST = 0,
    RP_2ND,
    RP_SHADOW,
    RP_RSM,
    RP_COUNT
};

inline std::ostream& operator<<(std::ostream& os, const RenderPass& rp)
{
    switch (rp)
    {
        case RP_1ST:
            return os << "first pass";
        case RP_2ND:
            return os << "second pass";
        default:
            return os;
    }
}

class SPUniformAssigner;

class SPShader : public NoCopy, public SPPerObjectUniform
{
private:
    std::string m_name;

    GLuint m_program[RP_COUNT];

    std::vector<std::pair<unsigned, unsigned> > m_samplers[RP_COUNT];

    std::vector<std::tuple<unsigned, std::string, SamplerType,
        GLuint> >m_prefilled_samplers[RP_COUNT];

    std::unordered_map<std::string, SPUniformAssigner*>
        m_uniforms[RP_COUNT];

    std::unordered_map<std::string, std::function<GLuint()> >
        m_custom_prefilled_getter[RP_COUNT];

    std::function<void()> m_use_function[RP_COUNT], m_unuse_function[RP_COUNT];

    const bool m_transparent_shader;

public:
    // ------------------------------------------------------------------------
    SPShader(const std::string& name, unsigned pass_count = 4,
             bool transparent_shader = false)
           : m_name(name),  m_transparent_shader(transparent_shader)
    {
        memset(m_program, 0, sizeof(GLuint) * 4);
#ifndef SERVER_ONLY
        for (unsigned rp = RP_1ST; rp < pass_count; rp++)
        {
            m_program[rp] = glCreateProgram();
        }
#endif
    }
    // ------------------------------------------------------------------------
    ~SPShader();
    // ------------------------------------------------------------------------
    bool hasShader(RenderPass rp)                { return m_program[rp] != 0; }
    // ------------------------------------------------------------------------
    void use(RenderPass rp = RP_1ST)
    {
        if (m_use_function[rp] != NULL)
        {
            m_use_function[rp]();
        }
#ifndef SERVER_ONLY
        glUseProgram(m_program[rp]);
#endif
    }
    // ------------------------------------------------------------------------
    void unuse(RenderPass rp = RP_1ST)
    {
        if (m_unuse_function[rp] != NULL)
        {
            m_unuse_function[rp]();
        }
    }
    // ------------------------------------------------------------------------
    void addShaderFile(const std::string& name,
                       GLint shader_type, RenderPass rp = RP_1ST);
    // ------------------------------------------------------------------------
    void linkShaderFiles(RenderPass rp = RP_1ST);
    // ------------------------------------------------------------------------
    void addAllTextures(RenderPass rp = RP_1ST);
    // ------------------------------------------------------------------------
    void addCustomPrefilledTextures(SamplerType st, GLuint texture_type,
                                    const std::string& name,
                                    std::function<GLuint()> func,
                                    RenderPass rp = RP_1ST);
    // ------------------------------------------------------------------------
    void bindPrefilledTextures(RenderPass rp = RP_1ST);
    // ------------------------------------------------------------------------
    void bindTextures(const irr::video::SMaterial& m, RenderPass rp = RP_1ST);
    // ------------------------------------------------------------------------
    void addBasicUniforms(RenderPass rp = RP_1ST)
    {
#ifndef SERVER_ONLY
        GLuint block_index = glGetUniformBlockIndex(m_program[rp],
            "SPMatrices");
        if (block_index != GL_INVALID_INDEX)
            glUniformBlockBinding(m_program[rp], block_index, 2);
        block_index = glGetUniformBlockIndex(m_program[rp], "SPFogData");
        if (block_index != GL_INVALID_INDEX)
            glUniformBlockBinding(m_program[rp], block_index, 3);
#endif
    }
    // ------------------------------------------------------------------------
    const std::string& getName() const                       { return m_name; }
    // ------------------------------------------------------------------------
    SPUniformAssigner* getUniformAssigner(const std::string& name,
                                          RenderPass rp = RP_1ST);
    // ------------------------------------------------------------------------
    void addUniform(const std::string& name, const std::type_info& ti,
                    RenderPass rp = RP_1ST);
    // ------------------------------------------------------------------------
    void setUniformsPerObject(SPPerObjectUniform* sppou,
                              std::vector<SPUniformAssigner*>* ua_used,
                              RenderPass rp = RP_1ST);
    // ------------------------------------------------------------------------
    void setUseFunction(std::function<void()> func, RenderPass rp = RP_1ST)
    {
        m_use_function[rp] = func;
    }
    // ------------------------------------------------------------------------
    void setUnuseFunction(std::function<void()> func, RenderPass rp = RP_1ST)
    {
        m_unuse_function[rp] = func;
    }
    // ------------------------------------------------------------------------
    bool isTransparent() const                 { return m_transparent_shader; }

};

}

#endif
