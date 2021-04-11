#include "pch.h"
#include "RCanvas.h"

namespace RA {
    void Canvas::ValidateBuffers()
    {
        if (!m_text_buf_valid) {
            m_text_buf->SetState(sizeof(TextGlyphVertex3D), int(m_text.size()), false, false, m_text.data());
            m_text_buf_valid = true;
        }
    }
    glm::vec3 Canvas::GetPos() 
    {
        return m_pos;
    }
    void Canvas::SetPos(const glm::vec3& pt)
    {
        m_pos = pt;
    }
    ITextBuilder* Canvas::TB()
    {
        return m_tb.get();
    }
    void Canvas::AddText(const ITextLinesPtr& lines)
    {
        for (const TextGlyphVertex& v : lines->AllGlyphs()) {
            TextGlyphVertex3D vv(lines, v);
            m_text.emplace_back(lines, v);
        }
        m_text_buf_valid = false;
    }
    void Canvas::Clear()
    {
        m_text.clear();
        m_text_buf_valid = false;
    }
    CanvasBuffers Canvas::GetBuffers()
    {
        ValidateBuffers();
        CanvasBuffers res;
        res.text_buf = m_text_buf->VertexCount() ? &m_text_buf : nullptr;
        return res;
    }
    Canvas::Canvas(const DevicePtr& dev, const Altas_GlyphsSDFPtr& atlas) : 
        m_glyphs_atlas(atlas)
    {
        m_dev = dev;
        m_text_buf = m_dev->Create_StructuredBuffer();
        m_text_buf_valid = true;
        m_tb = Create_TextBuilder(atlas.get());
    }
}