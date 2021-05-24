#include "pch.h"
#include "RFonts.h"
#include <functional>
#include <Windows.h>

namespace RA {
    static const int cFontSize = 32;

    void SplitString(const std::wstring& s, const std::wstring& separators, const std::function<void(std::wstring, wchar_t)>& callback) {
        auto isSeparator = [&separators](wchar_t ch)->bool {
            for (const auto& s : separators) {
                if (s == ch) return true;
            }
            return false;
        };
        size_t start_i = 0;
        for (size_t i = 0; i < s.size(); i++) {
            if (isSeparator(s[i])) {
                callback(s.substr(start_i, i - start_i), s[i]);
                start_i = i + 1;
            }
        }
        if (s.size() - start_i)
            callback(s.substr(start_i, s.size() - start_i), 0);
    }

    const char* Atlas_GlyphsSDF::ObtainFontPtr(const char* font)
    {
        for (const auto& s : m_fonts) {
            if (s == font) {
                return s.c_str();
            }
        }
        m_fonts.emplace_back(font);
        return m_fonts.back().c_str();
    }
    void Atlas_GlyphsSDF::ValidateTexture()
    {
        if (m_tex->SlicesCount() != m_roots.size())
            m_tex->SetState(m_tex->Format(), m_tex->Size(), 0, int(m_roots.size()), nullptr);
        m_gen_glyph_prog->CS_SetUAV(0, m_tex, 0, 0, int(m_roots.size()));
        //m_gen_glyph_prog->CS_ClearUAV(0, glm::vec4(100000000.0));
        for (const auto& it : m_sprites) {
            if (it.second->m_data.segments.size()) {
                m_segments_sbo->SetState(sizeof(glm::vec4), int(it.second->m_data.segments.size() / 2), false, false, it.second->m_data.segments.data());
            }
            else {
                m_segments_sbo->SetState(sizeof(glm::vec4), 1, false, false, nullptr);
            }
            m_gen_glyph_prog->SetResource("segments", m_segments_sbo);
            m_gen_glyph_prog->SetValue("segments_count", int(it.second->m_data.segments.size() / 2));
            m_gen_glyph_prog->SetValue("rect", glm::vec4(it.second->m_rect));
            m_gen_glyph_prog->SetValue("slice", it.second->m_slice);
            m_gen_glyph_prog->Dispatch({ (it.second->m_size.x + 31) / 32, (it.second->m_size.y + 31) / 32, 1 });
        }
        m_gen_glyph_prog->CS_SetUAV(0, nullptr);
    }
    Atlas_GlyphsSDF::Atlas_GlyphsSDF(const DevicePtr& dev) : BaseAtlas(dev)
    {
        m_gen_glyph_prog = m_dev->Create_Program();
        m_gen_glyph_prog->Load("generate_sdf_glyph", false, "D:\\Projects\\Beatty\\Beatty\\shaders\\!Out");

        m_segments_sbo = m_dev->Create_StructuredBuffer();
        
        m_tex = m_dev->Create_Texture2D();
        m_tex->SetState(TextureFmt::R32f, glm::ivec2(512, 512));
        m_roots.push_back(std::make_unique<Node>(m_tex->Size()));
    }
    Sprite_GlyphPtr Atlas_GlyphsSDF::ObtainSprite(const char* font, wchar_t ch, bool bold, bool italic, bool underline, bool strike)
    {        
        Glyph_Key k(ObtainFontPtr(font), ch, bold, italic, underline, strike);
        auto it = m_sprites.find(k);
        if (it == m_sprites.end()) {
            std::shared_ptr<Sprite_Glyph> new_sprite(new Sprite_Glyph(this, Glyph_Data(k)));
            m_sprites.emplace(k, new_sprite);
            int slice = 0;
            while (true) {
                if (m_roots[slice]->Insert(new_sprite)) break;
                slice++;
                if (m_roots.size() == slice) {
                    m_roots.push_back(std::make_unique<Node>(m_tex->Size()));
                }
            }
            new_sprite->m_slice = slice;
            return new_sprite;
        }
        return it->second;
    }
    Sprite_Glyph::Sprite_Glyph(BaseAtlas* owner, Glyph_Data data) : BaseAtlasSprite(owner, data.size), m_data(std::move(data))
    {
    }
    glm::vec3 Sprite_Glyph::XXXMetricsScaled(float font_size)
    {        
        return m_data.XXX * (font_size / cFontSize);
    }
    glm::vec4 Sprite_Glyph::YYYYMetricsScaled(float font_size)
    {
        return m_data.YYYY * (font_size / cFontSize);
    }
    Glyph_Data::Glyph_Data(const Glyph_Key& key) : key(key)
    {
        static const int cSpacing = 16;        

        auto RaiseLastOSError = []() {};
        auto FixedToFloat = [](const FIXED v)->float {
            return (*((int32_t*)&v)) / 65536.0f;
        };
        auto FixedToVec = [&FixedToFloat](const POINTFX pt)->glm::vec2 {
            return glm::vec2(FixedToFloat(pt.x), FixedToFloat(pt.y));
        };

        std::wstring wfont;
        wfont.resize(MultiByteToWideChar(CP_UTF8, 0, key.font, -1, NULL, 0));
        MultiByteToWideChar(CP_UTF8, 0, key.font, -1, const_cast<wchar_t*>(wfont.c_str()), int(wfont.size()));
        HDC dc = CreateDC(TEXT("DISPLAY"), NULL, NULL, NULL);
        HFONT hfont = CreateFontW(-cFontSize, 0, 0, 0, key.bold ? FW_BOLD : FW_NORMAL, key.italic, key.underline, key.strike, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FF_DONTCARE, wfont.c_str());
        SelectObject(dc, hfont);

        GLYPHMETRICS gm;
        MAT2 transform;
        transform.eM11.fract = 0;
        transform.eM11.value = 1;
        transform.eM12.fract = 0;
        transform.eM12.value = 0;
        transform.eM21.fract = 0;
        transform.eM21.value = 0;
        transform.eM22.fract = 0;
        transform.eM22.value = 1;

        int buf_size = GetGlyphOutlineW(dc, key.ch, GGO_NATIVE | GGO_UNHINTED, &gm, 0, nullptr, &transform);
        if (buf_size == GDI_ERROR) RaiseLastOSError();
        std::vector<char> buffer;
        buffer.resize(buf_size);
        GetGlyphOutlineW(dc, key.ch, GGO_NATIVE | GGO_UNHINTED, &gm, buf_size, buffer.data(), &transform);
        static const float cToleranceScale = 0.0025f;
        float cTol = glm::max(gm.gmBlackBoxX, gm.gmBlackBoxY) * cToleranceScale;

        std::vector<std::vector<glm::vec2>> poly;
        std::vector<glm::vec2>* cntr = nullptr;

        LPTTPOLYGONHEADER poly_header = nullptr;
        LPTTPOLYCURVE poly_curve = nullptr;
        int curveoffset = 0;
        int startoffset = 0;
        while (curveoffset < buf_size) {
            if (!poly_header || (curveoffset - startoffset == poly_header->cb)) {
                poly_header = (LPTTPOLYGONHEADER)&buffer[curveoffset];
                assert(poly_header->dwType == TT_POLYGON_TYPE);
                startoffset = curveoffset;
                curveoffset += sizeof(TTPOLYGONHEADER);
                poly.push_back(std::vector<glm::vec2>());
                cntr = &poly.back();
                cntr->push_back(FixedToVec(poly_header->pfxStart));
            }

            poly_curve = (LPTTPOLYCURVE)&buffer[curveoffset];
            switch (poly_curve->wType) {
                case TT_PRIM_LINE : {
                    for (int i = 0; i < poly_curve->cpfx; i++) {
                        glm::vec2 v = FixedToVec(poly_curve->apfx[i]);
                        if (cntr->back() != v) cntr->push_back(v);
                    }
                    break;
                }
                case TT_PRIM_QSPLINE: {
                    for (int i = 0; i < poly_curve->cpfx - 1; i++) {
                        glm::Bezier2_2d b;
                        b.pt[1] = FixedToVec(poly_curve->apfx[i]);
                        b.pt[2] = FixedToVec(poly_curve->apfx[i + 1]);
                        if (i < poly_curve->cpfx - 2) {
                            b.pt[2] = (b.pt[1] + b.pt[2]) * 0.5f;
                        }
                        b.pt[0] = cntr->back();
                        b.Aprrox(cTol, [&cntr](const glm::vec2 v) {
                                if (cntr->back() != v) cntr->push_back(v);
                            }
                        );
                    }
                    break;
                }
                default:
                    assert(false);
            }
            curveoffset += sizeof(poly_curve->wType) + sizeof(poly_curve->cpfx) + sizeof(POINTFX) * poly_curve->cpfx;
        }

        size = glm::ivec2(gm.gmBlackBoxX + cSpacing * 2, gm.gmBlackBoxY + cSpacing * 2);

        TEXTMETRICW tm;
        if (!GetTextMetricsW(dc, &tm))
            RaiseLastOSError();
        YYYY.x = float(tm.tmAscent - int(gm.gmptGlyphOrigin.y));
        YYYY.y = float(gm.gmptGlyphOrigin.y);
        YYYY.z = float(gm.gmBlackBoxY - YYYY.y);
        YYYY.w = float(tm.tmAscent + tm.tmDescent - YYYY.x - YYYY.y - YYYY.z);

        XXX.x = float(gm.gmptGlyphOrigin.x);
        XXX.y = float(gm.gmBlackBoxX);
        XXX.z = float(gm.gmCellIncX - int(gm.gmBlackBoxX) - gm.gmptGlyphOrigin.x);

        for (auto& cntr : poly) {
            for (int i = 0; i < int(cntr.size()); i++) {
                cntr[i].x -= XXX.x;
                cntr[i].y = YYYY.y - cntr[i].y;
            }
        }

        XXX.x = XXX.x - cSpacing;
        XXX.y = XXX.y + 2 * cSpacing;
        XXX.z = XXX.z - cSpacing;
        YYYY.x = YYYY.x - cSpacing;
        YYYY.y = YYYY.y + cSpacing;
        YYYY.z = YYYY.z + cSpacing;
        YYYY.w = YYYY.w - cSpacing;

        for (auto& cntr : poly) {
            for (int i = 0; i < int(cntr.size()); i++) {
                cntr[i].x += cSpacing;
                cntr[i].y += cSpacing;
            }
        }

        for (const auto& cntr : poly) {
            for (size_t i = 0; i < cntr.size(); i++) {
                segments.push_back(cntr[i]);
                segments.push_back(cntr[(i + 1) % cntr.size()]);
            }
        }
        DeleteDC(dc);
    }
    SpriteSBOVertex::SpriteSBOVertex(const BaseAtlasSprite* sprite)
    {
        slice = sprite->m_slice;
        xy = sprite->m_rect.xy();
        size = sprite->m_rect.zw();
    }

    struct LineInfo {
        LineAlign align;
        glm::vec2 yymetrics;
        float width;
        glm::ivec2 glyphs;
        std::vector<glm::vec3> xxxmetrics;
    };

    class DefTextLines : public ITextLines {
    private:
        glm::vec4 m_bounds;
        std::vector<TextGlyphVertex> m_glyphs;
        std::vector<LineInfo> m_lines;
        float m_max_line_width;
        float m_total_height;
        float m_valign;
    public:
        glm::vec4 GetBounds() const override { return m_bounds; }
        void SetBounds(const glm::vec4& bounds) override { m_bounds = bounds; }
        float GetVAlign() const override { return m_valign; }
        void SetVAlign(float valign) override { m_valign = valign; }

        int LinesCount() const override { return int(m_lines.size()); }
        glm::vec2 LineBounds(const int line_idx) const override { return glm::vec2(m_lines[line_idx].width, m_lines[line_idx].yymetrics.x + m_lines[line_idx].yymetrics.y); }
        glm::ivec2 LineGlyphs(const int line_idx) const override { return m_lines[line_idx].glyphs; }
        float MaxLineWidth() const override { return m_max_line_width; }
        float TotalHeight() const override { return m_total_height; }
        const std::vector<TextGlyphVertex>& AllGlyphs() const override { return m_glyphs; }

        DefTextLines(std::vector<TextGlyphVertex> glyphs, std::vector<LineInfo> lines) :
            m_glyphs(std::move(glyphs)), m_lines(std::move(lines))
        {
            m_max_line_width = 0;
            m_total_height = 0;
            m_valign = 0;
            for (const LineInfo& line : m_lines) {
                m_max_line_width = glm::max(m_max_line_width, line.width);
                m_total_height += line.yymetrics.x + line.yymetrics.y;
            }
            m_bounds = glm::vec4(0, 0, m_max_line_width, m_total_height);
        }
    };

    class DefTextBuilder : public ITextBuilder {
    private:
        struct WordInfo {
            std::vector<TextGlyphVertex> glyphs;
            std::vector<glm::vec3> xxxmetrics;
            std::vector<glm::vec4> yyyymetrics;
            glm::vec3 xxxspace;
            float width;
            glm::vec2 yymetrics;
        };
    private:
        Atlas_GlyphsSDF* m_atlas;
        std::string m_fontname;
        glm::vec4 m_color;
        float m_font_size;
        float m_sdf_offset;
        bool m_bold;
        bool m_italic;
        bool m_underline;
        bool m_strikeout;
        LineAlign m_line_align;

        std::vector<TextGlyphVertex> m_glyphs;
        std::vector<LineInfo> m_lines;

        bool m_line_inited;
        glm::vec2 m_pos;
        int m_line_start;
        LineInfo m_line_info;
        std::vector<glm::vec4> m_line_yyyy_metrics;
        std::vector<WordInfo> m_wrapped_words;    

        Sprite_GlyphPtr ObtainSprite(wchar_t w);
        void InitLine();
        glm::vec2 CalcBounds(const std::wstring& str);
        void WriteWordInternal(const std::wstring& str);
        void WriteInternal(const std::wstring& str);
        void WriteLnInternal();
    public:
        void SetPenPos(glm::vec2 pen_pos) override { m_pos = pen_pos; }

        void Font_SetName(const char* name) override { m_fontname = name; };
        void Font_SetColor(const glm::vec4& color) override { m_color = color; };
        void Font_SetSize(float size) override { m_font_size = size; };
        void Font_SetSDFOffset(float sdf_offset) override { m_sdf_offset = sdf_offset; };
        void Font_SetStyle(bool bold, bool italic, bool underline, bool strikeout) override { m_bold = bold; m_italic = italic; m_underline = underline; m_strikeout = strikeout; };

        void SetLineAlign(LineAlign la) override { m_line_align = la; };

        void WriteSpace(float space) override;
        void Write(const std::wstring& str) override;
        void WriteLine(const std::wstring& str) override;
        void WriteMultiline(const std::wstring& str) override;
        void WriteWrapped(const std::wstring& str) override;
        void WriteWrappedEnd(float max_width, bool justify_align = false, float first_row_offset = 0, float next_row_offset = 0) override;
        void WriteWrappedMultiline(const std::wstring& str, float max_width, bool justify_align = false, float first_row_offset = 0, float next_row_offset = 0) override;

        ITextLinesPtr Finish() override;

        DefTextBuilder(Atlas_GlyphsSDF* atlas) : 
            m_atlas(atlas), 
            m_pos(0, 0),
            m_line_start(0),
            m_line_inited(false),
            m_color(1,1,1,1),
            m_font_size(32),
            m_sdf_offset(0),
            m_bold(false),
            m_italic(false),
            m_underline(false),
            m_strikeout(false),
            m_line_align(LineAlign::Left)
        {};
    };

    ITextBuilderPtr Create_TextBuilder(Atlas_GlyphsSDF* atlas)
    {
        return std::make_shared<DefTextBuilder>(atlas);
    }
    Sprite_GlyphPtr DefTextBuilder::ObtainSprite(wchar_t w)
    {
        return m_atlas->ObtainSprite(m_fontname.c_str(), w, m_bold, m_italic, m_underline, m_strikeout);
    }
    void DefTextBuilder::InitLine()
    {
        if (!m_line_inited) {
            m_line_inited = true;
            m_line_start = int(m_glyphs.size());
            m_line_info.align = m_line_align;
            m_line_info.width = 0;
            m_line_info.glyphs = { m_glyphs.size(), m_glyphs.size() };
            m_line_info.xxxmetrics.clear();
        }
    }
    glm::vec2 DefTextBuilder::CalcBounds(const std::wstring& str)
    {
        glm::vec2 result = { 0,0 };
        glm::vec2 yy = { 0,0 };
        for (const auto& w : str) {
            Sprite_GlyphPtr glyph = ObtainSprite(w);
            glm::vec3 xxx = glyph->XXXMetricsScaled(m_font_size);
            glm::vec4 yyyy = glyph->YYYYMetricsScaled(m_font_size);
            yy = glm::max(yy, glm::vec2(yyyy.x + yyyy.y, yyyy.z + yyyy.w));
            result.x = result.x + xxx.x + xxx.y + xxx.z;
        }
        result.y = yy.x + yy.y;
        return result;
    }
    void DefTextBuilder::WriteWordInternal(const std::wstring& str)
    {
        WordInfo word;
        word.yymetrics = { 0,0 };
        word.xxxspace = { 0,0,0 };
        word.width = 0;

        Sprite_GlyphPtr dummy = ObtainSprite(' ');
        glm::vec3 xxx = dummy->XXXMetricsScaled(m_font_size);
        glm::vec4 yyyy = dummy->YYYYMetricsScaled(m_font_size);
        word.xxxspace = xxx;

        float posx = 0;
        for (wchar_t w : str) {
            Sprite_GlyphPtr glyph = ObtainSprite(w);
            glm::vec3 xxx = glyph->XXXMetricsScaled(m_font_size);
            glm::vec4 yyyy = glyph->YYYYMetricsScaled(m_font_size);

            TextGlyphVertex gv;
            word.yyyymetrics.push_back(yyyy);
            word.width = word.width + xxx.x + xxx.y + xxx.z;
            word.xxxmetrics.push_back(xxx);

            posx += xxx.x + xxx.y * 0.5f;
            gv.pos.x = posx;
            gv.pos.y = 0;
            posx += xxx.y * 0.5f + xxx.z;

            gv.size.x = xxx.y;
            gv.size.y = yyyy.y + yyyy.z;

            gv.sdfoffset = m_sdf_offset;
            gv.color = m_color;

            gv.slice_idx = glyph->Index();
            gv.sprite_xy = glyph->Pos();
            gv.sprite_size = glyph->Size();

            word.yymetrics = glm::max(word.yymetrics, glm::vec2(yyyy.x + yyyy.y, yyyy.z + yyyy.w));

            word.glyphs.push_back(gv);
        }
        m_wrapped_words.push_back(std::move(word));
    }
    void DefTextBuilder::WriteInternal(const std::wstring& str)
    {
        InitLine();
        for (wchar_t w : str) {
            Sprite_GlyphPtr glyph = ObtainSprite(w);
            glm::vec3 xxx = glyph->XXXMetricsScaled(m_font_size);
            glm::vec4 yyyy = glyph->YYYYMetricsScaled(m_font_size);
            m_line_yyyy_metrics.push_back(yyyy);
            m_line_info.width += xxx.x + xxx.y + xxx.z;
            m_line_info.xxxmetrics.push_back(xxx);

            TextGlyphVertex gv;
            m_pos.x += xxx.x + xxx.y * 0.5f;
            gv.pos.x = m_pos.x;
            gv.pos.y = 0;
            m_pos.x += xxx.y * 0.5f + xxx.z;

            gv.size.x = xxx.y;
            gv.size.y = yyyy.y + yyyy.z;

            gv.sdfoffset = m_sdf_offset;
            gv.color = m_color;

            gv.slice_idx = glyph->Slice();
            gv.sprite_xy = glyph->Pos();
            gv.sprite_size = glyph->Size();

            m_glyphs.push_back(gv);

            m_line_info.yymetrics = glm::max(m_line_info.yymetrics, glm::vec2(yyyy.x + yyyy.y, yyyy.z + yyyy.w));
        }
    }
    void DefTextBuilder::WriteLnInternal()
    {
        if (!m_line_inited) {
            m_pos.y += m_font_size;
        }
        else {
            if (m_line_yyyy_metrics.size()) {
                TextGlyphVertex* glyph = &m_glyphs[m_line_start];
                for (int i = 0; i < int(m_line_yyyy_metrics.size()); i++) {
                    switch (m_line_info.align) {
                        case LineAlign::Left: {
                            glyph->halign = 0;
                            break;
                        }
                        case LineAlign::Center: {
                            glyph->pos.x -= m_line_info.width * 0.5f;
                            glyph->halign = 0.5f;
                            break;
                        }
                        case LineAlign::Right: {
                            glyph->pos.x -= m_line_info.width;
                            glyph->halign = 1.0f;
                            break;
                        }
                    }
                    glyph->pos.y = m_pos.y + m_line_info.yymetrics.x - m_line_yyyy_metrics[i].y + glyph->size.y * 0.5f;
                    glyph++;
                }
            }
            m_line_info.glyphs.y = int(m_glyphs.size());
            m_lines.push_back(std::move(m_line_info));
            m_line_yyyy_metrics.clear();

            m_pos.y += m_line_info.yymetrics.x + m_line_info.yymetrics.y;
        }
        m_pos.x = 0;
        m_line_inited = false;
    }
    void DefTextBuilder::WriteSpace(float space)
    {
        InitLine();
        m_line_info.width += space;
        m_pos.x += space;
    }
    void DefTextBuilder::Write(const std::wstring& str)
    {
        WriteInternal(str);
    }
    void DefTextBuilder::WriteLine(const std::wstring& str)
    {
        WriteInternal(str);
        WriteLnInternal();
    }
    void DefTextBuilder::WriteMultiline(const std::wstring& str)
    {
        SplitString(str, L"\n", [this](std::wstring line, wchar_t sep) {
                WriteLine(line);
            });
    }
    void DefTextBuilder::WriteWrapped(const std::wstring& str)
    {
        SplitString(str, L" ", [this](std::wstring line, wchar_t sep) {
                WriteWordInternal(line);
            });
    }
    void DefTextBuilder::WriteWrappedEnd(float max_width, bool justify_align, float first_row_offset, float next_row_offset)
    {
        if (m_line_inited)
            WriteLnInternal();
        if (m_line_align != LineAlign::Left) {
            first_row_offset = 0;
            next_row_offset = 0;
        }

        std::vector<glm::ivec2> lines;
        float remain_width = 0;
        for (size_t i = 0; i < m_wrapped_words.size(); i++) {
            const WordInfo& word = m_wrapped_words[i];
            bool is_new_line = false;
            if (remain_width < word.width + word.xxxspace.x + word.xxxspace.y * 0.5) {
                remain_width = max_width - ((i > 0) ? next_row_offset : first_row_offset);
                lines.push_back(glm::ivec2(i, 0));
                is_new_line = true;
            }
            remain_width -= (is_new_line)
                                ? (word.width + word.xxxspace.y * 0.5f + word.xxxspace.z)
                                : (word.width + word.xxxspace.x + word.xxxspace.y + word.xxxspace.z);
            lines.back() += glm::ivec2(0, 1);
        }

        float offset;
        for (size_t i = 0; i < lines.size(); i++) {
            offset = i ? next_row_offset : first_row_offset;
            if (i == lines.size() - 1) justify_align = false;
            float justify_space = 0;
            if (justify_align) {
                float curr_line_width = 0;
                for (int j = 0; j < lines[i].y; j++)
                    curr_line_width += m_wrapped_words[lines[i].x + j].width;                    
                justify_space = (max_width - offset - curr_line_width) / (lines[i].y - 1.0f) * 0.5f;
            }

            const WordInfo* pw = nullptr;
            WriteSpace(offset);
            for (int j = 0; j < lines[i].y; j++) {
                if (pw) {
                    WriteSpace(justify_align ? justify_space : (pw->xxxspace.x + pw->xxxspace.y * 0.5f));
                }
                pw = &m_wrapped_words[lines[i].x + j];
                float xoffset = m_pos.x;
                for (size_t k = 0; k < pw->glyphs.size(); k++) {
                    TextGlyphVertex glyph = pw->glyphs[k];
                    glyph.pos.x = glyph.pos.x + xoffset;
                    m_glyphs.push_back(glyph);

                    glm::vec4 yyyy = pw->yyyymetrics[k];
                    glm::vec3 xxx = pw->xxxmetrics[k];

                    m_line_yyyy_metrics.push_back(yyyy);
                    m_line_info.width += xxx.x + xxx.y + xxx.z;
                    m_line_info.xxxmetrics.push_back(xxx);
                    m_pos.x += xxx.x + xxx.y + xxx.z;
                    m_line_info.yymetrics = glm::max(m_line_info.yymetrics, { yyyy.x + yyyy.y, yyyy.z + yyyy.w });
                }
                if (j != lines[i].y) {
                    WriteSpace(justify_align ? justify_space : (pw->xxxspace.y * 0.5f + pw->xxxspace.z));
                }
            }
            WriteLnInternal();
        }
        m_wrapped_words.clear();
    }
    void DefTextBuilder::WriteWrappedMultiline(const std::wstring& str, float max_width, bool justify_align, float first_row_offset, float next_row_offset)
    {
        SplitString(str, L"\n", [this, max_width, justify_align, first_row_offset, next_row_offset](std::wstring line, wchar_t sep) {
            WriteWrapped(line);
            WriteWrappedEnd(max_width, justify_align, first_row_offset, next_row_offset);
            });
    }
    ITextLinesPtr DefTextBuilder::Finish()
    {
        if (m_line_inited) WriteLnInternal();
        ITextLinesPtr res = std::make_shared<DefTextLines>(std::move(m_glyphs), std::move(m_lines));
        m_glyphs.clear();
        m_lines.clear();
        m_line_inited = false;
        m_line_start = 0;
        m_line_yyyy_metrics.clear();
        m_pos = { 0,0 };
        return res;
    }
}