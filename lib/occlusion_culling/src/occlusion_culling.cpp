/* ==========================================================================
 * Copyright (c) 2022 SuperTuxKart-Team
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
 * NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 * ==========================================================================
 */
#include "occlusion_culling.hpp"

#include "MaskedOcclusionCulling.h"

#include "ge_main.hpp"
#include "IImage.h"
#include "SColor.h"
#include <ICameraSceneNode.h>
#include <matrix4.h>

#include <condition_variable>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace OcclusionCulling
{
using namespace irr;

struct FrameData
{
    core::dimension2du m_screen_size;
    core::matrix4 m_project;
    core::matrix4 m_view;
    // ------------------------------------------------------------------------
    static FrameData getQuitThread()
    {
        FrameData f;
        f.m_project = core::matrix4(core::matrix4::EM4CONST_IDENTITY);
        f.m_view = core::matrix4(core::matrix4::EM4CONST_IDENTITY);
        return f;
    }
    // ------------------------------------------------------------------------
    FrameData()
    {
    }
    // ------------------------------------------------------------------------
    FrameData(const core::dimension2du& screen_size,
              const core::matrix4& project, const core::matrix4& view)
    {
        m_screen_size = screen_size;
        m_project = project;
        m_view = view;
    }
    // ------------------------------------------------------------------------
    bool isQuitThread() const
    {
        return m_screen_size.getArea() == 0 &&
            m_project == core::matrix4(core::matrix4::EM4CONST_IDENTITY) &&
            m_view == core::matrix4(core::matrix4::EM4CONST_IDENTITY);
    }
};

std::vector<core::triangle3df> g_tris;
MaskedOcclusionCulling* g_moc = NULL;
std::thread g_worker;
std::condition_variable g_frame_data_cv;
std::mutex g_frame_data_mutex;
std::vector<FrameData> g_frame_data;
// ============================================================================
void workerThread()
{
    while (true)
    {
        std::unique_lock<std::mutex> ul(g_frame_data_mutex);
        g_frame_data_cv.wait(ul, []
            {
                return !g_frame_data.empty();
            });
        FrameData last = g_frame_data.back();
        g_frame_data.clear();
        ul.unlock();

        if (last.isQuitThread())
            return;

        core::dimension2du padded_size = last.m_screen_size;
        padded_size.Width = padded_size.Width / 8 * 8;
        padded_size.Height = padded_size.Height / 4 * 4;
        core::dimension2du moc_size;
        g_moc->GetResolution(moc_size.Width, moc_size.Height);
        if (padded_size != moc_size)
            g_moc->SetResolution(padded_size.Width, padded_size.Height);
        g_moc->ClearBuffer();

        std::vector<float> projected;
        projected.resize(g_tris.size() * 3 * 4);
        core::matrix4 pvm = last.m_project * last.m_view;
        MaskedOcclusionCulling::TransformVertices(pvm.pointer(),
            (float*)g_tris.data(), projected.data(), g_tris.size() * 3);

        std::vector<uint32_t> idx;
        for (unsigned i = 0; i < g_tris.size() * 3; i++)
            idx.push_back(i);
        g_moc->RenderTriangles(projected.data(), idx.data(), g_tris.size(),
            /*modelToClipMatrix*/NULL, MaskedOcclusionCulling::BACKFACE_CCW);

#undef WRITE_DEPTH_BUFFER
#ifdef WRITE_DEPTH_BUFFER
        std::vector<float> depth_buffer;
        depth_buffer.resize(padded_size.getArea());
        g_moc->ComputePixelDepthBuffer(depth_buffer.data(), false/*flipY*/);
        video::IImage* img = GE::getDriver()->createImage(video::ECF_A8R8G8B8,
            padded_size);
        video::SColor* data = (video::SColor*)img->lock();
        for (unsigned i = 0; i < padded_size.getArea(); i++)
            data[i] = video::SColor(255, depth_buffer[i] * 255.f, 0, 0);
        GE::getDriver()->writeImageToFile(img, "depthbuffer.jpg");
        img->drop();
#endif
    }
}   // start

// ----------------------------------------------------------------------------
void start()
{
    if (g_tris.empty())
    {
        g_moc = NULL;
        return;
    }
    g_moc = MaskedOcclusionCulling::Create();
    if (g_moc)
        g_worker = std::thread(workerThread);
}   // start

// ----------------------------------------------------------------------------
void addOccluderTriangle(const irr::core::triangle3df& tri)
{
    g_tris.push_back(tri);
}   // addOccluderTriangle

// ----------------------------------------------------------------------------
void pushFrame(const scene::ICameraSceneNode* cam,
               const core::dimension2du& screen_size)
{
    if (!g_moc)
        return;

    std::unique_lock<std::mutex> ul(g_frame_data_mutex);
    g_frame_data.emplace_back(screen_size, cam->getProjectionMatrix(),
        cam->getViewMatrix());
    g_frame_data_cv.notify_one();
}   // pushFrame

// ----------------------------------------------------------------------------
void stop()
{
    if (g_moc)
    {
        std::unique_lock<std::mutex> ul(g_frame_data_mutex);
        g_frame_data.push_back(FrameData::getQuitThread());
        g_frame_data_cv.notify_one();
        ul.unlock();
        g_worker.join();

        MaskedOcclusionCulling::Destroy(g_moc);
        g_moc = NULL;
    }
    g_tris.clear();
}   // stop

}   // namespace OcclusionCulling
