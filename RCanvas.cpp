#include "pch.h"
#include "RCanvas.h"
#include "RAdoptConsts.h"

namespace RA {
    void Canvas::ValidateBuffers()
    {
        if (!m_text_buf_valid) {
            m_text_buf->SetState(TextGlyphVertex3D::Layout(), int(m_text.size()), m_text.data());
            m_text_buf_valid = true;
        }
        if (!m_tris_buf_valid) {
            m_tris_buf->SetState(TrisVertex::Layout(), int(m_tris.size()), m_tris.data());
            m_tris_buf_valid = true;
        }
        if (!m_lines_buf_valid) {
            m_lines_buf->SetState(LineVertex::Layout(), int(m_lines.size()), m_lines.data());
            m_lines_buf_valid = true;
        }
    }
    void Canvas::PushBatch(BatchKind kind, int size)
    {
        BatchKind prev_batch = BatchKind::None;
        if (m_batches.size()) {
            prev_batch = m_batches.back().kind;
        }
        if (prev_batch != kind) {
            Batch new_batch;
            new_batch.kind = kind;
            switch (kind) {
            case BatchKind::Glyphs: {
                new_batch.ranges.x = int(m_text.size()) - size;
                break;
            }
            case BatchKind::Tris: {
                new_batch.ranges.x = int(m_tris.size()) - size;
                break;
            }
            case BatchKind::Lines: {
                new_batch.ranges.x = int(m_lines.size()) - size;
                break;
            }
            default:
                assert(false);
            }
            new_batch.ranges.y = size;
            m_batches.push_back(new_batch);
        }
        else {
            m_batches.back().ranges.y += size;
        }
    }
    void Canvas::InitProgram(BatchKind kind, CameraBase& camera, const glm::mat3& transform_2d)
    {
        if (m_prog_was_inited[int(kind)]) return;
        m_prog_was_inited[int(kind)] = true;

        float dpi_scale = (m_canvas_common_object) ? m_canvas_common_object->GetDPIScale() : 1.0f;

        glm::vec2 view_pixel_size = glm::vec2(2.0f) / glm::vec2(m_dev->ActiveFrameBuffer()->GetSize()) / dpi_scale;
        glm::mat4 m4;
        m4[0] = glm::vec4(transform_2d[0].x, transform_2d[0].y, 0.0, 0.0);
        m4[1] = glm::vec4(transform_2d[1].x, transform_2d[1].y, 0.0, 0.0);
        m4[2] = glm::vec4(0.0, 0.0, 1.0, 0.0);
        m4[3] = glm::vec4(transform_2d[2].x, transform_2d[2].y, 0.0, 1.0);

        switch (kind) {
        case BatchKind::Tris: {
            m_tris_out_prog->SetResource("camera", camera.GetUBO());
            m_tris_out_prog->SetValue("view_pixel_size", view_pixel_size);
            m_tris_out_prog->SetValue("pos3d", m_pos);
            m_tris_out_prog->SetValue("transform_2d", m4);
            m_tris_out_prog->SetResource("sprites_data", m_sprite_atlas->GlyphsSBO());
            m_tris_out_prog->SetResource("atlas", m_sprite_atlas->Texture());
            m_tris_out_prog->SetResource("atlasSampler", RA::cSampler_Linear);
            m_tris_out_prog->SetInputBuffers(m_tris_buf, nullptr, nullptr);
            break;
        }
        case BatchKind::Glyphs: {
            m_text_out_prog->SetResource("camera", camera.GetUBO());
            m_text_out_prog->SetValue("view_pixel_size", view_pixel_size);
            m_text_out_prog->SetValue("pos3d", m_pos);
            m_text_out_prog->SetValue("transform_2d", m4);
            m_text_out_prog->SetResource("atlas", m_glyphs_atlas->Texture());
            m_text_out_prog->SetResource("atlasSampler", RA::cSampler_Linear);
            m_text_out_prog->SetInputBuffers(nullptr, nullptr, m_text_buf);
            break;
        }
        case BatchKind::Lines: {
            m_lines_out_prog->SetResource("camera", camera.GetUBO());
            m_lines_out_prog->SetValue("view_pixel_size", view_pixel_size);
            m_lines_out_prog->SetValue("dpi_scale", dpi_scale);
            m_lines_out_prog->SetValue("pos3d", m_pos);
            m_lines_out_prog->SetValue("transform_2d", m4);
            m_lines_out_prog->SetInputBuffers(nullptr, nullptr, m_lines_buf);
            break;
        }
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
    Pen& Canvas::Pen()
    {
        return m_pen;
    }
    AtlasSpritePtr Canvas::GetSprite(const fs::path& filename)
    {
        return m_sprite_atlas->ObtainSprite(filename);
    }
    void Canvas::AddSprite(const glm::vec2& pos, const glm::vec2& origin, const glm::vec2& size, float rotation, const glm::ivec4& sprite_cliprect, const AtlasSpritePtr& sprite)
    {
        TrisVertex v[4];

        float x1 = -size.x * origin.x;
        float x2 = x1 + size.x;
        float y1 = -size.y * origin.y;
        float y2 = y1 + size.y;
        v[0].coord = glm::vec2(x1, y1);
        v[1].coord = glm::vec2(x1, y2);
        v[2].coord = glm::vec2(x2, y1);
        v[3].coord = glm::vec2(x2, y2);
        for (int i = 0; i < 4; i++) {
            v[i].coord = glm::rotate(v[i].coord, rotation) + pos;
            v[i].color = m_pen.GetColor();
            v[i].hinting = m_pen.GetHinting() ? 1.0f : 0.0f;
            v[i].sprite_idx = sprite->Index();
        }

        glm::vec4 rct = sprite->Rect();
        float u1 = float(sprite_cliprect.x) / rct.z;
        float u2 = float(sprite_cliprect.x + sprite_cliprect.z) / rct.z;
        float v1 = float(sprite_cliprect.y) / rct.w;
        float v2 = float(sprite_cliprect.y + sprite_cliprect.w) / rct.w;
        v[0].texcoord = glm::vec2(u1, v1);
        v[1].texcoord = glm::vec2(u1, v2);
        v[2].texcoord = glm::vec2(u2, v1);
        v[3].texcoord = glm::vec2(u2, v2);

        m_tris.push_back(v[0]);
        m_tris.push_back(v[1]);
        m_tris.push_back(v[2]);
        m_tris.push_back(v[2]);
        m_tris.push_back(v[1]);
        m_tris.push_back(v[3]);
        m_tris_buf_valid = false;

        m_used_sprites.push_back(sprite);

        PushBatch(BatchKind::Tris, 6);
    }
    void Canvas::AddSprite(const glm::vec2& pos, const glm::vec2& origin, const glm::vec2& size, const AtlasSpritePtr& sprite)
    {
        AddSprite(pos, origin, size, 0, glm::ivec4(0, 0, sprite->Size()), sprite);
    }
    void Canvas::Add9Patch(const glm::AABR& rect, const glm::vec2& scale, int x1, int x2, int y1, int y2, const AtlasSpritePtr& sprite)
    {
        TrisVertex v[16];
        float xx[4];
        xx[0] = rect.min.x;
        xx[1] = rect.min.x + x1 * scale.x;
        xx[2] = rect.max.x - x2 * scale.x;
        xx[3] = rect.max.x;
        if (xx[1] > xx[2]) {
            xx[1] = (xx[1] + xx[2]) * 0.5f;
            xx[2] = xx[1];
        }
        float yy[4];
        yy[0] = rect.min.y;
        yy[1] = rect.min.y + y1 * scale.y;
        yy[2] = rect.max.y - y2 * scale.y;
        yy[3] = rect.max.y;
        if (yy[1] > yy[2]) {
            yy[1] = (yy[1] + yy[2]) * 0.5f;
            yy[2] = yy[1];
        }
        glm::vec4 rct = sprite->Rect();
        float tx[4];
        tx[0] = 0.0f;
        tx[1] = x1 / rct.z;
        tx[2] = (rct.z - x2) / rct.z;
        tx[3] = 1.0f;
        float ty[4];
        ty[0] = 0.0f;
        ty[1] = y1 / rct.w;
        ty[2] = (rct.w - y2) / rct.w;
        ty[3] = 1.0f;
        for (int j = 0; j < 4; j++) {
            for (int i = 0; i < 4; i++) {            
                int n = j * 4 + i;
                v[n].coord.x = xx[i];
                v[n].coord.y = yy[j];
                v[n].color = m_pen.GetColor();
                v[n].hinting = m_pen.GetHinting() ? 1.0f : 0.0f;
                v[n].sprite_idx = sprite->Index();
                v[n].texcoord.x = tx[i];
                v[n].texcoord.y = ty[j];
            }
        }

        for (int j = 0; j < 3; j++) {
            for (int i = 0; i < 3; i++) {
                int offset = i + j * 4;
                m_tris.push_back(v[offset + 0]);
                m_tris.push_back(v[offset + 4]);
                m_tris.push_back(v[offset + 1]);
                m_tris.push_back(v[offset + 1]);
                m_tris.push_back(v[offset + 4]);
                m_tris.push_back(v[offset + 5]);
            }
        }

        m_tris_buf_valid = false;

        m_used_sprites.push_back(sprite);

        PushBatch(BatchKind::Tris, 6 * 9);
    }
    void Canvas::AddLine(const glm::vec2& pt1, const glm::vec2& pt2)
    {
        LineVertex v;
        v.width = { m_pen.GetWidth(), m_pen.GetMinPixWidth() };
        v.color = m_pen.GetColor();
        v.hinting = m_pen.GetHinting() ? 1.0f : 0.0f;

        v.coords.x = pt1.x;
        v.coords.y = pt1.y;

        v.coords.z = pt2.x;
        v.coords.w = pt2.y;

        glm::vec2 n = { (pt2.x - pt1.x), (pt2.y - pt1.y) };
        float n_lensqr = glm::dot(n, n);
        if (n_lensqr == 0) {
            n_lensqr = 1.0f;
            n = { 1, 0 };
        }
        n /= glm::sqrt(n_lensqr);
        v.normals.x = n.x;
        v.normals.y = n.y;
        v.normals.z = n.x;
        v.normals.w = n.y;        

        m_lines.push_back(v);
        m_lines_buf_valid = false;

        PushBatch(BatchKind::Lines, 1);
    }
    void Canvas::AddRectangle(const glm::AABR& rect)
    {
        AddRectangle(glm::vec4(rect.min, rect.max));
    }
    void Canvas::AddRectangle(const glm::vec4& bounds)
    {
        glm::vec2 size = bounds.zw() - bounds.xy();
        if ((size.x == 0) || (size.y == 0)) return;
        glm::vec2 n = glm::sign(size);
        if (m_pen.GetAlign() == PenAlign::left) n = -n;

        LineVertex v;
        v.width = { m_pen.GetWidth(), m_pen.GetMinPixWidth() };
        v.color = m_pen.GetColor();
        v.hinting = m_pen.GetHinting() ? 1.0f : 0.0f;

        glm::vec2 p[4];
        p[0] = { bounds.x, bounds.y };
        p[1] = { bounds.z, bounds.y };
        p[2] = { bounds.z, bounds.w };
        p[3] = { bounds.x, bounds.w };
        glm::vec2 np[4];
        np[0] = { n.x, -n.y };
        np[1] = { -n.x, -n.y };
        np[2] = { -n.x, n.y };
        np[3] = { n.x, n.y };

        for (int i = 0; i < 4; i++) {
            v.coords = { p[i], p[(i + 1) % 4] };
            v.normals = { np[i], np[(i + 1) % 4] };
            m_lines.push_back(v);
        }
        m_lines_buf_valid = false;
        PushBatch(BatchKind::Lines, 4);
    }
    void Canvas::AddFillRect(const glm::vec4& bounds)
    {
        TrisVertex v[4];

        float x1 = bounds.x;
        float x2 = bounds.z;
        float y1 = bounds.y;
        float y2 = bounds.w;
        v[0].coord = glm::vec2(x1, y1);
        v[1].coord = glm::vec2(x1, y2);
        v[2].coord = glm::vec2(x2, y1);
        v[3].coord = glm::vec2(x2, y2);
        for (int i = 0; i < 4; i++) {
            v[i].color = m_pen.GetColor();
            v[i].hinting = m_pen.GetHinting() ? 1.0f : 0.0f;
            v[i].sprite_idx = -1;
        }

        v[0].texcoord = glm::vec2(0, 0);
        v[1].texcoord = glm::vec2(0, 0);
        v[2].texcoord = glm::vec2(0, 0);
        v[3].texcoord = glm::vec2(0, 0);

        m_tris.push_back(v[0]);
        m_tris.push_back(v[1]);
        m_tris.push_back(v[2]);
        m_tris.push_back(v[2]);
        m_tris.push_back(v[1]);
        m_tris.push_back(v[3]);
        m_tris_buf_valid = false;

        PushBatch(BatchKind::Tris, 6);
    }
    void Canvas::AddText(const ITextLinesPtr& lines)
    {
        for (const TextGlyphVertex& v : lines->AllGlyphs()) {
            TextGlyphVertex3D vv(lines, v);
            m_text.emplace_back(lines, v);
        }        
        m_text_buf_valid = false;
        PushBatch(BatchKind::Glyphs, int(lines->AllGlyphs().size()));
    }
    void Canvas::Clear()
    {
        m_used_sprites.clear();
        m_tris.clear();
        m_tris_buf_valid = false;

        m_text.clear();
        m_text_buf_valid = false;

        m_lines.clear();
        m_lines_buf_valid = false;

        m_batches.clear();
    }
    CanvasBuffers Canvas::GetBuffers()
    {
        ValidateBuffers();
        CanvasBuffers res;
        res.batches = &m_batches;
        res.text_buf = m_text_buf->VertexCount() ? &m_text_buf : nullptr;
        res.tris_buf = m_tris_buf->VertexCount() ? &m_tris_buf : nullptr;
        return res;
    }
    void Canvas::Render(CameraBase& camera, const glm::mat3& transform_2d)
    {
        ValidateBuffers();

        for (int i = 0; i < 4; i++) {
            m_prog_was_inited[i] = false;
        }

        for (auto batch : m_batches) {
            InitProgram(batch.kind, camera, transform_2d);
            switch (batch.kind) {
            case BatchKind::Tris: {
                m_tris_out_prog->SelectProgram();
                m_tris_out_prog->Draw(PrimTopology::Triangle, batch.ranges.x, batch.ranges.y, 0, 0);
                break;
            }
            case BatchKind::Glyphs: {
                m_text_out_prog->SelectProgram();
                m_text_out_prog->Draw(PrimTopology::Trianglestrip, 0, 4, batch.ranges.y, batch.ranges.x);
                break;
            }
            case BatchKind::Lines: {
                m_lines_out_prog->SelectProgram();
                m_lines_out_prog->Draw(PrimTopology::Trianglestrip, 0, 4, batch.ranges.y, batch.ranges.x);
            }
            }
        }
    }
    void Canvas::Render(CameraBase& camera)
    {
        Render(camera, glm::mat3(1.0f));
    }
    Canvas::Canvas(
        const DevicePtr& dev, 
        const Atlas_GlyphsSDFPtr& glyphs, 
        const AtlasPtr& sprites,
        const ProgramPtr& text_out_prog,
        const ProgramPtr& tris_out_prog,
        const ProgramPtr& lines_out_prog) :
        m_glyphs_atlas(glyphs), 
        m_sprite_atlas(sprites),
        m_tris_out_prog(tris_out_prog),
        m_text_out_prog(text_out_prog),
        m_lines_out_prog(lines_out_prog),
        m_pos(0)
    {
        m_dev = dev;

        m_text_buf = m_dev->Create_VertexBuffer();
        m_text_buf_valid = true;

        m_tris_buf = m_dev->Create_VertexBuffer();
        m_tris_buf_valid = true;

        m_lines_buf = m_dev->Create_VertexBuffer();
        m_lines_buf_valid = true;

        m_tb = Create_TextBuilder(glyphs.get());
    }
    Canvas::Canvas(const CanvasCommonObject& canvas_common_object) :
        Canvas(
            canvas_common_object.m_dev,
            canvas_common_object.m_glyphs_atlas,
            canvas_common_object.m_sprite_atlas,
            canvas_common_object.m_text_out_prog,
            canvas_common_object.m_tris_out_prog,
            canvas_common_object.m_lines_out_prog
        )        
    {
        m_canvas_common_object = &canvas_common_object;
    }
    glm::vec4 Pen::GetColor()
    {
        return m_color;
    }
    void Pen::SetColor(const glm::vec4& color)
    {
        m_color = color;
    }
    bool Pen::GetHinting()
    {
        return m_hinting;
    }
    void Pen::SetHinting(bool hinting)
    {
        m_hinting = hinting;
    }
    float Pen::GetWidth()
    {
        return m_width;
    }
    void Pen::SetWidth(float width)
    {
        m_width = width;
    }
    float Pen::GetMinPixWidth()
    {
        return m_min_pix_width;
    }
    void Pen::SetMinPixWidth(float width)
    {
        m_min_pix_width = width;
    }
    PenAlign Pen::GetAlign()
    {
        return m_penalign;
    }
    void Pen::SetAlign(PenAlign align)
    {
        m_penalign = align;
    }
    Pen::Pen() : m_color(1), m_hinting(true), m_width(1), m_min_pix_width(1), m_penalign(PenAlign::right)
    {
    }
    const Layout* Canvas::TrisVertex::Layout()
    {
        return LB()
            ->Add("coord", LayoutType::Float, 2)
            ->Add("texcoord", LayoutType::Float, 2)
            ->Add("color", LayoutType::Float, 4)
            ->Add("hinting", LayoutType::Float, 1)
            ->Add("sprite_idx", LayoutType::UInt, 1)
            ->Finish();
    }
    float CanvasCommonObject::GetDPIScale() const
    {
        return m_dpi_scale;
    }
    void CanvasCommonObject::SetDPIScale(float scale)
    {
        m_dpi_scale = scale;
    }
    AtlasPtr CanvasCommonObject::GetAtlas() const
    {
        return m_sprite_atlas;
    }
    void CanvasCommonObject::Validate()
    {
        m_glyphs_atlas->Texture();
    }
    DevicePtr CanvasCommonObject::Device()
    {
        return m_dev;
    }
    CanvasCommonObject::CanvasCommonObject(const DevicePtr& dev)
    {
        m_dev = dev;

        m_text_out_prog = m_dev->Create_Program();
        m_text_out_prog->Load("RAdopt_canvas_text_out", cLoadShadersFromRes, "D:\\Projects\\RAdopt\\shaders\\!Out");

        m_tris_out_prog = m_dev->Create_Program();
        m_tris_out_prog->Load("RAdopt_canvas_tris_out", cLoadShadersFromRes, "D:\\Projects\\RAdopt\\shaders\\!Out");

        m_lines_out_prog = m_dev->Create_Program();
        m_lines_out_prog->Load("RAdopt_canvas_lines_out", cLoadShadersFromRes, "D:\\Projects\\RAdopt\\shaders\\!Out");

        m_glyphs_atlas = std::make_shared<Atlas_GlyphsSDF>(m_dev);
        m_sprite_atlas = std::make_shared<Atlas>(m_dev);
    }
    const Layout* Canvas::LineVertex::Layout()
    {
        return LB()
            ->Add("coords", LayoutType::Float, 4)
            ->Add("normals", LayoutType::Float, 4)
            ->Add("color", LayoutType::Float, 4)
            ->Add("width", LayoutType::Float, 2)
            ->Add("hinting", LayoutType::Float, 1)
            ->Finish();
    }
    const Layout* Canvas::TextGlyphVertex3D::Layout()
    {
        return LB()->Add("pos", LayoutType::Float, 2)
            ->Add("size", LayoutType::Float, 2)
            ->Add("color", LayoutType::Float, 4)
            ->Add("halign", LayoutType::Float, 1)
            ->Add("sdfoffset", LayoutType::Float, 1)
            ->Add("sprite_xy", LayoutType::Float, 2)
            ->Add("sprite_size", LayoutType::Float, 2)
            ->Add("slice_idx", LayoutType::UInt, 1)
            ->Add("dummy", LayoutType::Float, 1)
            ->Add("bounds2d", LayoutType::Float, 4)
            ->Add("valign", LayoutType::Float, 1)
            ->Finish();
    }
}