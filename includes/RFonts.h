#pragma once
#include "RAdopt.h"
#include "RAtlas.h"

namespace RA {
    class Sprite_Glyph;
    using Sprite_GlyphPtr = std::shared_ptr<Sprite_Glyph>;
    struct Glyph_Key {
        const char* font;
        wchar_t ch;
        bool bold;
        bool italic;
        bool underline;
        bool strike;
        Glyph_Key(const char* font, wchar_t ch, bool bold, bool italic, bool underline, bool strike) noexcept 
            : font(font), ch(ch), bold(bold), italic(italic), underline(underline), strike(strike)  {}
        bool operator==(const Glyph_Key& d) const noexcept {
            return (font == d.font) && (ch == d.ch) && (bold == d.bold) && (italic == d.italic) && (underline == d.underline) && (strike == d.strike);
        }

    };
    struct Glyph_Key_Hasher {
        size_t operator()(const Glyph_Key& d) const noexcept {
            return
                std::hash<const char*>()(d.font) ^
                std::hash<wchar_t>()(d.ch) ^
                std::hash<int>()((d.bold ? 0 : 1) | (d.italic ? 0 : 2) | (d.underline ? 0 : 4) | (d.strike ? 0 : 8));
        }
    };
    struct Glyph_Data {
        Glyph_Key key;

        glm::ivec2 size;
        glm::vec3 XXX;
        glm::vec4 YYYY;                
        std::vector<glm::vec2> segments;
        Glyph_Data(const Glyph_Key& key);
    };

    class Atlas_GlyphsSDF : public BaseAtlas {
    protected:
        RA::ProgramPtr m_gen_glyph_prog;
        RA::StructuredBufferPtr m_segments_sbo;

        std::vector<std::string> m_fonts;
        std::unordered_map<Glyph_Key, Sprite_GlyphPtr, Glyph_Key_Hasher> m_sprites;
        const char* ObtainFontPtr(const char* font);
        void ValidateTexture() override;
    public:
        Atlas_GlyphsSDF(const DevicePtr& dev);
        Sprite_GlyphPtr ObtainSprite(const char* font, wchar_t ch, bool bold, bool italic, bool underline, bool strike);
    };
    using Atlas_GlyphsSDFPtr = std::shared_ptr<Atlas_GlyphsSDF>;

    class Sprite_Glyph : public BaseAtlasSprite {
        friend class Atlas_GlyphsSDF;
    protected:
        Glyph_Data m_data;
        Sprite_Glyph(BaseAtlas* owner, Glyph_Data data);
    public:
        glm::vec3 XXXMetricsScaled(float font_size);
        glm::vec4 YYYYMetricsScaled(float font_size);
    };

    struct TextGlyphVertex {
        glm::vec2 pos;
        glm::vec2 size;
        glm::vec4 color;
        float halign;
        float sdfoffset;

        glm::vec2 sprite_xy;
        glm::vec2 sprite_size;
        int slice_idx;
        float dummy;
    };

    class ITextLines {
    public:
        virtual glm::vec4 GetBounds() const = 0;
        virtual void SetBounds(const glm::vec4& bounds) = 0;
        virtual float GetVAlign() const = 0;
        virtual void SetVAlign(float valign) = 0;

        virtual int LinesCount() const = 0;
        virtual glm::vec2 LineBounds(const int line_idx) const = 0;
        virtual glm::ivec2 LineGlyphs(const int line_idx) const = 0;
        virtual float MaxLineWidth() const = 0;
        virtual float TotalHeight() const = 0;
        virtual const std::vector<TextGlyphVertex>& AllGlyphs() const = 0;

        virtual ~ITextLines() {};
    };
    using ITextLinesPtr = std::shared_ptr<ITextLines>;

    enum class LineAlign { Left, Center, Right };

    class ITextBuilder {
    public:
        virtual void SetPenPos(glm::vec2 pen_pos) = 0;

        virtual void Font_SetName(const char* name) = 0;
        virtual void Font_SetColor(const glm::vec4& color) = 0;
        virtual void Font_SetSize(float size) = 0;
        virtual void Font_SetSDFOffset(float sdf_offset) = 0;
        virtual void Font_SetStyle(bool bold, bool italic, bool underline, bool strikeout) = 0;

        virtual void SetLineAlign(LineAlign la) = 0;        

        virtual void WriteSpace(float space) = 0;
        virtual void Write(const std::wstring& str) = 0;
        virtual void WriteLine(const std::wstring& str) = 0;
        virtual void WriteMultiline(const std::wstring& str) = 0;
        virtual void WriteWrapped(const std::wstring& str) = 0;
        virtual void WriteWrappedEnd(float max_width, bool justify_align = false, float first_row_offset = 0, float next_row_offset = 0) = 0;
        virtual void WriteWrappedMultiline(const std::wstring& str, float max_width, bool justify_align = false, float first_row_offset = 0, float next_row_offset = 0) = 0;

        virtual ITextLinesPtr Finish() = 0;

        virtual ~ITextBuilder() {};
    };
    using ITextBuilderPtr = std::shared_ptr<ITextBuilder>;

    ITextBuilderPtr Create_TextBuilder(Atlas_GlyphsSDF* atlas);
}