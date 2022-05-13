#include "PolyTess.h"
#include "src/tess.h"

namespace RA {
    class Tess : public ITess {
    private:
        GLUtesselator* m_tess;
        const std::function<void(TessMode type)>& m_begin;
        const std::function<void(int idx)>& m_vertex;
        const std::function<void()>& m_end;
        const std::function<int(const glm::dvec2& new_pos, const glm::vec4& indices, const glm::vec4& weight)>& m_lerp;
    public:
        void StartPolygon() override {
            gluTessBeginPolygon(m_tess, this);
        }
        void StartContour() override {
            gluTessBeginContour(m_tess);
        }
        void AddVertex(const glm::dvec2& d, int idx) override {
            glm::dvec3 tmp(d, 0);
            intptr_t tmp_idx = idx;
            gluTessVertex(m_tess, (GLdouble*)&tmp, (GLvoid*)tmp_idx);
        }
        void EndContour() override {
            gluTessEndContour(m_tess);
        }
        void EndPolygon() override {
            gluTessEndPolygon(m_tess);
        }
        void Begin(GLenum type) {
            switch (type) {
            case GL_TRIANGLES: m_begin(TessMode::TriList); break;
            case GL_TRIANGLE_FAN: m_begin(TessMode::TriFan); break;
            case GL_TRIANGLE_STRIP: m_begin(TessMode::TriStrip); break;
            case GL_LINE_LOOP: m_begin(TessMode::LineLoop); break;
            }
        }
        void Vertex(int idx) {
            m_vertex(idx);
        }
        void End() {
            m_end();
        }
        int Lerp(const glm::dvec2& new_pos, const glm::ivec4& inds, const glm::vec4& weights) {
            return m_lerp(new_pos, inds, weights);
        }
        Tess(const std::function<void(TessMode type)>& begin,
             const std::function<void(int idx)>& vertex,
             const std::function<void()>& end,
             const std::function<int(const glm::dvec2& new_pos, const glm::vec4& indices, const glm::vec4& weight)>& lerp,
             bool triangulate
        );
        ~Tess() override {
            gluDeleteTess(m_tess);
        }
    };

    ITessPtr Create_tesselator(
        const std::function<void(TessMode type)>& begin,
        const std::function<void(int idx)>& vertex,
        const std::function<void()>& end,
        const std::function<int(const glm::dvec2& new_pos, const glm::vec4& indices, const glm::vec4& weight)>& lerp,
        bool triangulate
    )
    {
        return std::make_unique<Tess>(begin, vertex, end, lerp, triangulate);
    }

    void cbTess_Vertex(intptr_t idx, Tess* obj) {
        obj->Vertex(int(idx));
    }
    void cbTess_Begin(GLenum type, Tess* obj) {
        obj->Begin(type);
    }
    void cbTess_End(Tess* obj) {
        obj->End();
    }
    void cbTess_Error(GLboolean boundaryEdge, Tess* obj) {
    }
    void cbTess_Combine(
        GLdouble coords[3],
        intptr_t idx[4],
        GLfloat weight[4],
        intptr_t* outData,
        Tess* obj) {
        int new_idx = obj->Lerp(
            glm::dvec2(coords[0], coords[1]),
            glm::ivec4(idx[0], idx[1], idx[2], idx[3]), 
            glm::vec4(weight[0], weight[1], weight[2], weight[3])
        );
        *outData = new_idx;
    }
    void cbTess_EdgeFlag(GLenum errnum, Tess* polygonData) {
    }
    Tess::Tess(
        const std::function<void(TessMode type)>& begin,
        const std::function<void(int idx)>& vertex,
        const std::function<void()>& end,
        const std::function<int(const glm::dvec2& new_pos, const glm::vec4& indices, const glm::vec4& weight)>& lerp,
        bool triangulate) :
        m_begin(begin),
        m_vertex(vertex),
        m_end(end),
        m_lerp(lerp)
    {
        m_tess = gluNewTess();
        gluTessCallback(m_tess, GLU_TESS_VERTEX_DATA, (_GLUfuncptr*)&cbTess_Vertex);
        gluTessCallback(m_tess, GLU_TESS_BEGIN_DATA, (_GLUfuncptr*)&cbTess_Begin);
        gluTessCallback(m_tess, GLU_TESS_END_DATA, (_GLUfuncptr*)&cbTess_End);
        gluTessCallback(m_tess, GLU_TESS_ERROR_DATA, (_GLUfuncptr*)&cbTess_Error);
        gluTessCallback(m_tess, GLU_TESS_COMBINE_DATA, (_GLUfuncptr*)&cbTess_Combine);
        gluTessCallback(m_tess, GLU_TESS_EDGE_FLAG_DATA, (_GLUfuncptr*)&cbTess_EdgeFlag);

        gluTessProperty(m_tess, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_ODD);
        if (triangulate)
            gluTessProperty(m_tess, GLU_TESS_BOUNDARY_ONLY, GL_FALSE);
        else
            gluTessProperty(m_tess, GLU_TESS_BOUNDARY_ONLY, 1);
    }


}