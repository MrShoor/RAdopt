#pragma once
#include "RAdopt.h"
#include "RFonts.h"
#include "RUtils.h"

namespace RA {
    struct CanvasBuffers {
        const StructuredBufferPtr* text_buf;
    };

    class Canvas {
    private:
        struct TextGlyphVertex3D {
            TextGlyphVertex v2d;            
            glm::vec4 bounds2d;
            float valign;
            TextGlyphVertex3D(const ITextLinesPtr& lines, const TextGlyphVertex& v2d) : 
                v2d(v2d), bounds2d(lines->GetBounds()), valign(lines->GetVAlign()) {
            }
        };
    private:
        DevicePtr m_dev;
        Altas_GlyphsSDFPtr m_glyphs_atlas;

        ProgramPtr m_text_out_prog;

        std::vector<TextGlyphVertex3D> m_text;
        StructuredBufferPtr m_text_buf;
        bool m_text_buf_valid;

        ITextBuilderPtr m_tb;

        glm::vec3 m_pos;

        void ValidateBuffers();
    public:
        glm::vec3 GetPos();
        void SetPos(const glm::vec3& pt);

        ITextBuilder* TB();

        void AddText(const ITextLinesPtr& lines);
        void Clear();

        CanvasBuffers GetBuffers();

        Canvas(const DevicePtr& dev, const Altas_GlyphsSDFPtr& atlas);
        virtual ~Canvas() {};
    };
    using CanvasPtr = std::shared_ptr<Canvas>;
}