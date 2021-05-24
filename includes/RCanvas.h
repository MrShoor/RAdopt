#pragma once
#include "RAdopt.h"
#include "RFonts.h"
#include "RUtils.h"

namespace RA {
    class CanvasCommonObject {
        friend class Canvas;
    private:
        DevicePtr m_dev;
        ProgramPtr m_text_out_prog;
        ProgramPtr m_tris_out_prog;
        ProgramPtr m_lines_out_prog;
        Atlas_GlyphsSDFPtr m_glyphs_atlas;
        AtlasPtr m_sprite_atlas;
    public:
        DevicePtr Device();
        CanvasCommonObject(const DevicePtr& dev);
    };
    using CanvasCommonObjectPtr = std::shared_ptr<CanvasCommonObject>;

    enum class BatchKind { None, Glyphs, Tris, Lines };
    struct Batch {
        BatchKind kind;
        glm::ivec2 ranges;
    };

    struct CanvasBuffers {
        const std::vector<Batch>* batches;
        const StructuredBufferPtr* text_buf;
        const VertexBufferPtr* tris_buf;
    };

    enum class PenAlign { left, right };

    class Pen {
    private:
        glm::vec4 m_color;
        bool m_hinting;
        float m_width;
        float m_min_pix_width;
        PenAlign m_penalign;
    public:
        glm::vec4 GetColor();
        void SetColor(const glm::vec4& color);
        bool GetHinting();
        void SetHinting(bool hinting);
        float GetWidth();
        void SetWidth(float width);
        float GetMinPixWidth();
        void SetMinPixWidth(float width);
        PenAlign GetAlign();
        void SetAlign(PenAlign align);
        Pen();
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
        struct TrisVertex {
            glm::vec2 coord;
            glm::vec2 texcoord;
            glm::vec4 color;
            float hinting;
            uint32_t sprite_idx;
            static const Layout* Layout();
        };
        struct LineVertex {
            glm::vec4 coords;  //xy - start point, zw - end point
            glm::vec4 normals; //xy - normal at start point, zw - normal at end point
            glm::vec4 color;
            glm::vec2 width;   //x - real width, y - minimal width in pixels
            float hinting;
            static const Layout* Layout();
        };
    private:
        Pen m_pen;

        DevicePtr m_dev;

        Atlas_GlyphsSDFPtr m_glyphs_atlas;
        AtlasPtr m_sprite_atlas;

        ProgramPtr m_text_out_prog;
        ProgramPtr m_tris_out_prog;
        ProgramPtr m_lines_out_prog;

        std::vector<Batch> m_batches;

        std::vector<TextGlyphVertex3D> m_text;
        StructuredBufferPtr m_text_buf;
        bool m_text_buf_valid;

        std::vector<AtlasSpritePtr> m_used_sprites;
        std::vector<TrisVertex> m_tris;
        VertexBufferPtr m_tris_buf;
        bool m_tris_buf_valid;

        std::vector<LineVertex> m_lines;
        VertexBufferPtr m_lines_buf;
        bool m_lines_buf_valid;

        ITextBuilderPtr m_tb;

        glm::vec3 m_pos;

        void ValidateBuffers();
        void PushBatch(BatchKind kind, int size);
    private:
        bool m_prog_was_inited[4];
        void InitProgram(BatchKind kind, Camera& camera, const glm::mat3& transform_2d);
    public:
        glm::vec3 GetPos();
        void SetPos(const glm::vec3& pt);        

        Pen& Pen();

        AtlasSpritePtr GetSprite(const fs::path& filename);
        void AddSprite(const glm::vec2& pos,
                       const glm::vec2& origin,
                       const glm::vec2& size,
                       float rotation,
                       const glm::ivec4& sprite_cliprect, //xy - min coord, zw - rect size
                       const AtlasSpritePtr& sprite);

        void AddLine(const glm::vec2& pt1, const glm::vec2& pt2);
        void AddRectangle(const glm::vec4& bounds);

        ITextBuilder* TB();
        void AddText(const ITextLinesPtr& lines);
        void Clear();

        CanvasBuffers GetBuffers();

        void Render(Camera& camera);
        void Render(Camera& camera, const glm::mat3& transform_2d);

        Canvas(const DevicePtr& dev, 
               const Atlas_GlyphsSDFPtr& glyphs, 
               const AtlasPtr& sprites,
               const ProgramPtr& text_out_prog,
               const ProgramPtr& tris_out_prog,
               const ProgramPtr& lines_out_prog);

        Canvas(const CanvasCommonObject& canvas_common_object);

        virtual ~Canvas() {};
    };
    using CanvasPtr = std::shared_ptr<Canvas>;
}