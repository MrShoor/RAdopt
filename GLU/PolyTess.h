#pragma once
#include <GLMUtils.h>
#include <memory>

namespace RA {

    enum class TessMode { TriList, TriStrip, TriFan, LineLoop };

    class ITess {
    private:
    public:
        virtual void StartPolygon() = 0;
        virtual void StartContour() = 0;
        virtual void AddVertex(const glm::dvec2& d, int idx) = 0;
        virtual void EndContour() = 0;
        virtual void EndPolygon() = 0;
        virtual ~ITess() = default;
    };
    using ITessPtr = std::unique_ptr<ITess>;
    ITessPtr Create_tesselator(
        const std::function<void(TessMode type)>& begin,
        const std::function<void(int idx)>& vertex,
        const std::function<void()>& end,
        const std::function<int(const glm::dvec2& new_pos, const glm::vec4& indices, const glm::vec4& weight)>& lerp,
        bool triangulate
    );

    template<typename T>
    struct Contour {
        std::vector<T> pts;
        bool hole = false;

        double SignedArea() const {
            double summ = 0;
            glm::dvec2 pt_curr = pts.back().GetDPos();
            glm::dvec2 pt_prev;
            for (const auto& pt : pts) {            
                pt_prev = pt_curr;
                pt_curr = pt.GetDPos();
                summ += (pt_curr.x - pt_prev.x) * (pt_curr.y + pt_prev.y);
            }
            return summ * 0.5;
        }
    };

    template<typename T>
    struct Tris {
        std::vector<T> vertices;
        std::vector<int32_t> indices;
    };
    
    template<typename T>
    struct Poly {
        std::vector<Contour<T>> contours;

        Tris<T> Trinagulate(const std::function<T(const glm::dvec2& new_pos, const T& v0, const T& v1, const T& v2, const T& v3, const glm::vec4& weights)>& vert_lerp) {
            std::vector<T> verts;
            std::vector<int32_t> inds;
            
            TessMode curr_mode;
            std::vector<int32_t> tmp_inds;

            auto cb_lerp = [&verts, &vert_lerp](const glm::dvec2& new_pos, const glm::vec4& indices, const glm::vec4& weight)->int {
                verts.push_back(vert_lerp(new_pos, verts[indices.x], verts[indices.y], verts[indices.z], verts[indices.w], weight));
                return int(verts.size()) - 1;
            };
            auto cb_begin = [&curr_mode, &tmp_inds](TessMode type) {
                curr_mode = type;
                tmp_inds.clear();
            };
            auto cb_vertex = [&tmp_inds](int idx) {
                tmp_inds.push_back(idx);
            };
            auto cb_end = [&verts, &inds, &tmp_inds]() {
                switch (curr_mode) {
                    case TessMode::TriList: {
                        for (const auto& i : tmp_inds)
                            inds.push_back(i);
                        return;
                    }
                    case TessMode::TriStrip: {
                        int i0 = tmp_inds[0];
                        int i1 = tmp_inds[1];
                        int flip = 0;
                        for (int i = 2; i < tmp_inds.size(); i++) {
                            if (flip) {
                                inds.push_back(tmp_inds[i1]);
                                inds.push_back(tmp_inds[i0]);
                                inds.push_back(tmp_inds[i]);
                            }
                            else {
                                inds.push_back(tmp_inds[i0]);
                                inds.push_back(tmp_inds[i1]);
                                inds.push_back(tmp_inds[i]);
                            }
                            i0 = i1;
                            i1 = i;
                            flip ^= 1;
                        }
                        return;
                    }
                    case TessMode::TriFan: {
                        int i0 = tmp_inds[0];
                        int iprev = tmp_inds[1];
                        for (int i = 2; i < tmp_inds.size(); i++) {
                            inds.push_back(tmp_inds[i0]);
                            inds.push_back(tmp_inds[iprev]);
                            inds.push_back(tmp_inds[i]);
                            iprev = i;
                        }
                        return;
                    }
                }
            };
            
            ITessPtr tess = Create_tesselator(cb_begin, cb_vertex, cb_end, cb_lerp, true);

            tess->StartPolygon();
            for (const Contour<T>& cntr : contours) {
                tess->StartContour();
                bool reverse = (cntr.SignedArea() < 0) ^ cntr.hole;
                if (reverse) {
                    for (auto it = cntr.pts.rbegin(); it != cntr.pts.rend(); ++it) {
                        verts.push_back(*it);
                        tess->AddVertex(it->GetDPos(), int(verts.size()) - 1);
                    }
                }
                else {
                    for (const T& pt : cntr.pts) {
                        verts.push_back(pt);
                        tess->AddVertex(pt.GetDPos(), int(verts.size()) - 1);
                    }
                }
                tess->EndContour();
            }
            tess->EndPolygon();

            Tris<T> res;
            res.vertices = std::move(verts);
            res.indices = std::move(inds);
            return res;
        }
    };

    enum class ClipOperation { Union, Diff, Intersection, Xor };

    template<typename T>
    Poly<T> ClipPolygons(const Poly<T>& a, const Poly<T>& b, ClipOperation op);
}