#pragma once
#include "GLM.h"
#include <limits>
#include <functional>
#include <vector>

static constexpr float cPI = 3.1415926535897932384626433832795f;
static constexpr float cPI2 = 6.283185307179586476925286766559f;

namespace glm {
    struct Plane;

    struct AABR {
        vec2 min;
        vec2 max;
        AABR() {
            SetEmpty();
        }
        AABR(const glm::vec2& min, const glm::vec2& max) : min(min), max(max) {
        }
        inline bool IsEmpty() const {
            return (min.x >= max.x) || (min.y >= max.y);
        }
        inline void SetEmpty() {
            min.x = std::numeric_limits<float>::max();
            min.y = std::numeric_limits<float>::max();
            max.x = std::numeric_limits<float>::lowest();
            max.y = std::numeric_limits<float>::lowest();
        }
        inline glm::vec2 Center() const {
            return (min + max) * 0.5f;
        }
        inline glm::vec2 Size() const {
            return (max - min);
        }
        inline AABR& operator += (const vec2& v) {
            min = glm::min(min, v);
            max = glm::max(max, v);
            return *this;
        }
        inline AABR& operator += (const AABR& b) {
            min = glm::min(min, b.min);
            max = glm::max(max, b.max);
            return *this;
        }
        inline AABR& operator *= (const vec2& v) {
            min *= v;
            max *= v;
            return *this;
        }
        inline AABR operator + (const AABR& b) const {
            AABR res;
            res.min = glm::min(min, b.min);
            res.max = glm::max(max, b.max);
            return res;
        }
        inline AABR Expand(float expansion) const {
            AABR res = *this;
            res.min -= glm::vec2(expansion, expansion);
            res.max += glm::vec2(expansion, expansion);
            return res;
        }
        inline AABR Expand(const glm::vec2& expansion) const {
            AABR res = *this;
            res.min -= expansion;
            res.max += expansion;
            return res;
        }
        inline AABR Offset(const glm::vec2& offset) const {
            AABR res = *this;
            res.min += offset;
            res.max += offset;
            return res;
        }
        inline AABR Intersect(const glm::AABR& box) const {
            AABR res;
            res.min = glm::max(min, box.min);
            res.max = glm::min(max, box.max);
            return res;
        }
        inline bool IsIntersects(const glm::AABR& box) const {
            return
                max.x > box.min.x &&
                min.x < box.max.x &&
                max.y > box.min.y &&
                min.y < box.max.y;
        }
        inline bool PtIn(const glm::vec2& pt) const {
            if (pt.x < min.x) return false;
            if (pt.y < min.y) return false;
            if (pt.x >= max.x) return false;
            if (pt.y >= max.y) return false;
            return true;
        }
    };

    struct AABB {
        vec3 min;
        vec3 max;
        AABB() {
            SetEmpty();
        }
        glm::vec3 Point(int idx) const {
            idx %= 8;
            if (idx < 0) idx += 8;
            switch (idx) {
            case 0: return { min.x, min.y, min.z };
            case 1: return { min.x, min.y, max.z };
            case 2: return { min.x, max.y, min.z };
            case 3: return { min.x, max.y, max.z };
            case 4: return { max.x, min.y, min.z };
            case 5: return { max.x, min.y, max.z };
            case 6: return { max.x, max.y, min.z };
            case 7: return { max.x, max.y, max.z };
            default: return { 0,0,0 };
            }
        }
        glm::Plane Plane(int idx) const;
        inline bool IsEmpty() const {
            return (min.x >= max.x) || (min.y >= max.y) || (min.z >= max.z);
        }
        inline void SetEmpty() {
            min.x = std::numeric_limits<float>::max();
            min.y = std::numeric_limits<float>::max();
            min.z = std::numeric_limits<float>::max();
            max.x = std::numeric_limits<float>::lowest();
            max.y = std::numeric_limits<float>::lowest();
            max.z = std::numeric_limits<float>::lowest();
        }
        inline glm::vec3 Center() const {
            return (min + max) * 0.5f;
        }
        inline glm::vec3 Size() const {
            return (max - min);
        }
        inline AABB& operator += (const vec3& v) {
            min = glm::min(min, v);
            max = glm::max(max, v);
            return *this;
        }
        inline AABB& operator += (const AABB& b) {
            min = glm::min(min, b.min);
            max = glm::max(max, b.max);
            return *this;
        }
        inline AABB operator + (const AABB& b) const {
            AABB res;
            res.min = glm::min(min, b.min);
            res.max = glm::max(max, b.max);
            return res;
        }
        inline AABB Expand(float expansion) const {
            AABB res = *this;
            res.min -= glm::vec3(expansion, expansion, expansion);
            res.max += glm::vec3(expansion, expansion, expansion);
            return res;
        }
        inline AABB Expand(glm::vec3 expansion) const {
            AABB res = *this;
            res.min -= expansion;
            res.max += expansion;
            return res;
        }
    };

    inline AABB operator * (const mat4& m, const AABB& b) {
        AABB res;
        for (int i = 0; i < 8; i++) {
            glm::vec4 tmp = (m * glm::vec4(b.Point(i), 1.0f));
            glm::vec3 tmp3 = tmp.xyz() / tmp.w;
            res += tmp3;
        }
        return res;
    }

    struct Plane {
        vec4 v;
        inline Plane() {
            v = glm::vec4(0);
        }
        inline Plane(const vec4& v) {
            this->v = v;
        }
        inline Plane(const vec3& n, const vec3& pt) {
            v = vec4(n.x, n.y, n.z, -dot(n, pt));
        }
        inline Plane(const vec3& pt1, const vec3& pt2, const vec3& pt3) {
            *this = Plane(cross(pt2 - pt1, pt3 - pt1), pt1);
        }
        inline float Distance(const vec3& pt) const {
            return (dot(pt, v.xyz()) + v.w) / length(v.xyz());
        }
    };

    inline bool Intersect(const Plane& p1, const Plane& p2, const Plane& p3, vec3* intpt) {
        mat3 m = { p1.v.xyz(), p2.v.xyz(), p3.v.xyz() };
        vec3 b = { -p1.v.w, -p2.v.w, -p3.v.w };
        if (determinant(m) == 0) return false;
        mat3 m_inv = inverse(m);
        *intpt = b * m_inv;
        return true;
    }

    bool Intersect(const glm::AABB& box, const glm::vec3& pt1, const glm::vec3& pt2, const glm::vec3& pt3);
    bool Intersect(const glm::AABB& box, const glm::vec3& seg_start, const glm::vec3& seg_end, bool solid_aabb);
    bool Intersect(const vec3& pt1, const vec3& pt2, const vec3& pt3, const vec3& seg_start, const vec3& seg_end, float* t);

    struct Bezier2_2d {
        vec2 pt[3];
        inline void Split(float k, Bezier2_2d* b1, Bezier2_2d* b2) const {
            b1->pt[0] = pt[0];
            b1->pt[1] = mix(pt[0], pt[1], k);

            b2->pt[2] = pt[2];
            b2->pt[1] = mix(pt[1], pt[2], k);

            b1->pt[2] = mix(b1->pt[1], b2->pt[1], k);
            b2->pt[0] = b1->pt[2];
        }
        inline bool IsStraight(float tolerance) const {
            vec2 main_dir = pt[2] - pt[0];
            vec2 cdir1 = pt[1] - pt[0];
            vec2 cdir2 = pt[2] - pt[1];
            float tol_sqr = tolerance * tolerance;
            if (dot(cdir1, main_dir) <= 0) {
                if (dot(cdir1, cdir1) > tol_sqr) return false;
            }
            if (dot(cdir2, main_dir) <= 0) {
                if (dot(cdir2, cdir2) > tol_sqr) return false;
            }
            float main_dir_lensqr = dot(main_dir, main_dir);
            float s = cdir1.x * main_dir.y - cdir1.y * main_dir.x;
            s = s * s;
            if (s > tol_sqr * main_dir_lensqr) return false;
            return true;
        }
        void Aprrox(float tolerance, const std::function<void(const glm::vec2& v)>& add_point) const;
    };

    inline float NormalizeAngle(float v) {
        return fract(v / cPI2) * cPI2;
    }
    inline float NormalizeAngle_Pi_Pi(float v) {
        float a = fract(v / cPI2) * cPI2;
        if (a > cPI) a -= 2 * cPI;
        return a;
    }

    inline float cross2d(const glm::vec2& v1, const glm::vec2& v2) {
        return v1.x * v2.y - v1.y * v2.x;
    }

    inline float PtLineDistance2(const glm::vec2& line_pt, const glm::vec2& line_dir, const glm::vec2& pt) {
        glm::vec2 pt_dir = pt - line_pt;
        float s = cross2d(line_dir, pt_dir);
        return s * s / glm::length2(line_dir);
    }

    inline float PtSegmentDistance2(const glm::vec2& seg_pt1, const glm::vec2& seg_pt2, const glm::vec2& pt) {
        glm::vec2 line_dir = seg_pt2 - seg_pt1;
        glm::vec2 dir1 = pt - seg_pt1;
        glm::vec2 dir2 = pt - seg_pt2;
        if (dot(line_dir, dir1) < 0) {
            return glm::length2(dir1);
        }
        if (dot(line_dir, dir2) > 0) {
            return glm::length2(dir2);
        }
        return PtLineDistance2(seg_pt1, line_dir, pt);
    }

    inline float SegmentSegmentDistance2(const glm::vec2& seg1_pt1, const glm::vec2& seg1_pt2, const glm::vec2& seg2_pt1, const glm::vec2& seg2_pt2) {
        glm::vec2 dir1 = seg1_pt2 - seg1_pt1;
        glm::vec2 dir2 = seg2_pt2 - seg2_pt1;
        float s0 = cross2d(dir1, seg2_pt1 - seg1_pt1);
        float s1 = cross2d(dir1, seg2_pt2 - seg1_pt1);
        float s2 = cross2d(dir2, seg1_pt1 - seg2_pt1);
        float s3 = cross2d(dir2, seg1_pt2 - seg2_pt1);
        if ((s0 * s1 <= 0) && (s2 * s3 <= 0)) return 0; //intersect case

        float d0 = PtSegmentDistance2(seg1_pt1, seg1_pt2, seg2_pt1);
        float d1 = PtSegmentDistance2(seg1_pt1, seg1_pt2, seg2_pt2);
        float d2 = PtSegmentDistance2(seg2_pt1, seg2_pt2, seg1_pt1);
        float d3 = PtSegmentDistance2(seg2_pt1, seg2_pt2, seg1_pt2);
        return glm::min(glm::min(d0, d1), glm::min(d2, d3));
    }

    inline glm::vec3 HUEtoRGB(float h) {
        glm::vec3 res = {
            glm::abs(h * 6.0f - 3.0f) - 1.0f,
            2.0f - glm::abs(h * 6.0f - 2.0f),
            2.0f - glm::abs(h * 6.0f - 4.0f),
        };
        return glm::clamp(res, 0.0f, 1.0f);
    }

    int WeightedRandom(const std::vector<int>& weights);
}