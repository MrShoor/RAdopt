#pragma once
#include "GLM.h"
#include <limits>
#include <functional>
#include <vector>
#include <iostream>

static constexpr float cPI = 3.1415926535897932384626433832795f;
static constexpr float cPI2 = 6.283185307179586476925286766559f;

namespace glm {
    struct AABR {
        vec2 min;
        vec2 max;
        AABR() {
            SetEmpty();
        }
        AABR(const glm::vec2& min, const glm::vec2& max) : min(min), max(max) {
        }
        glm::vec2 Point(int idx) const {
            idx %= 4;
            if (idx < 0) idx += 4;
            switch (idx) {
            case 0: return { min.x, min.y };
            case 1: return { min.x, max.y };
            case 2: return { max.x, min.y };
            case 3: return { max.x, max.y };
            default: return { 0,0 };
            }
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
                min.x < box.max.x&&
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
    inline AABR operator * (const mat3& m, const AABR& b) {
        AABR res;
        for (int i = 0; i < 4; i++) {
            glm::vec3 tmp = (m * glm::vec3(b.Point(i), 1.0f));
            glm::vec2 tmp2 = tmp.xy() / tmp.z;
            res += tmp2;
        }
        return res;
    }

    template <typename T, qualifier Q = defaultp>
    struct Plane_T {
        vec<4, T, Q> v;
        inline Plane_T() {
            v = vec<4, T, Q>(0);
        }
        inline Plane_T(const vec<4, T, Q>& v) {
            this->v = v;
        }
        inline Plane_T(const vec<3, T, Q>& n, const vec<3, T, Q>& pt) {
            v = vec<4, T, Q>(n.x, n.y, n.z, -dot(n, pt));
        }
        inline Plane_T(const vec<3, T, Q>& pt1, const vec<3, T, Q>& pt2, const vec<3, T, Q>& pt3) {
            *this = Plane_T(cross(pt2 - pt1, pt3 - pt1), pt1);
        }
        inline T Distance(const vec<3, T, Q>& pt) const {
            return (dot(pt, v.xyz()) + v.w) / length(v.xyz());
        }
        Plane_T Normalize() {
            Plane_T res = *this;
            T len = glm::length(res.v.xyz());
            res.v /= len;
            return res;
        }
    };

    template <typename T, qualifier Q = defaultp>
    inline Plane_T<T, Q> operator * (const qua<T, Q>& q, const Plane_T<T, Q>& p) {
        vec<3, T, Q> n = q * p.v.xyz();
        Plane_T<T, Q> res;
        res.v.x = n.x;
        res.v.y = n.y;
        res.v.z = n.z;
        res.v.w = p.v.w;
        return res;
    }
    using Plane = Plane_T<float, defaultp>;
    using dPlane = Plane_T<double, defaultp>;

    template <typename T, qualifier Q = defaultp>
    struct AABB_T {
        vec<3, T, Q> min;
        vec<3, T, Q> max;
        AABB_T() noexcept {
            SetEmpty();
        }
        vec<3, T, Q> Point(int idx) const {
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
        Plane_T<T, Q> Plane(int idx) const
        {
            switch (idx) {
            case 0: return Plane_T<T, Q>(Point(0), Point(4), Point(2)); //near plane
            case 1: return Plane_T<T, Q>(Point(7), Point(5), Point(3)); //far plane
            case 2: return Plane_T<T, Q>(Point(2), Point(6), Point(3)); //top plane
            case 3: return Plane_T<T, Q>(Point(0), Point(1), Point(4)); //bottom plane
            case 4: return Plane_T<T, Q>(Point(2), Point(3), Point(0)); //left plane
            case 5: return Plane_T<T, Q>(Point(7), Point(6), Point(5)); //right plane
            default:
                return Plane_T<T, Q>();
            }
        }
        inline bool IsEmpty() const {
            return (min.x >= max.x) || (min.y >= max.y) || (min.z >= max.z);
        }
        inline void GetEdge(int idx, vec<3, T, Q>& pt1, vec<3, T, Q>& pt2) const {
            idx %= 12;
            if (idx < 0) idx += 12;
            switch (idx) {
            case 0:
                pt1 = Point(0);
                pt2 = Point(1);
                return;
            case 1:
                pt1 = Point(1);
                pt2 = Point(3);
                return;
            case 2:
                pt1 = Point(3);
                pt2 = Point(2);
                return;
            case 3:
                pt1 = Point(2);
                pt2 = Point(0);
                return;

            case 4:
                pt1 = Point(4);
                pt2 = Point(5);
                return;
            case 5:
                pt1 = Point(5);
                pt2 = Point(7);
                return;
            case 6:
                pt1 = Point(7);
                pt2 = Point(6);
                return;
            case 7:
                pt1 = Point(6);
                pt2 = Point(4);
                return;

            case 8:
                pt1 = Point(0);
                pt2 = Point(4);
                return;
            case 9:
                pt1 = Point(1);
                pt2 = Point(5);
                return;
            case 10:
                pt1 = Point(2);
                pt2 = Point(6);
                return;
            case 11:
                pt1 = Point(3);
                pt2 = Point(7);
                return;
            }
        }
        inline void SetEmpty() {
            min.x = std::numeric_limits<T>::max();
            min.y = std::numeric_limits<T>::max();
            min.z = std::numeric_limits<T>::max();
            max.x = std::numeric_limits<T>::lowest();
            max.y = std::numeric_limits<T>::lowest();
            max.z = std::numeric_limits<T>::lowest();
        }
        inline vec<3, T, Q> Center() const {
            return (min + max) * T(0.5);
        }
        inline vec<3, T, Q> Size() const {
            return (max - min);
        }
        inline AABB_T& operator += (const vec<3, T, Q>& v) {
            min = glm::min(min, v);
            max = glm::max(max, v);
            return *this;
        }
        inline AABB_T& operator += (const AABB_T& b) {
            min = glm::min(min, b.min);
            max = glm::max(max, b.max);
            return *this;
        }
        inline AABB_T operator + (const AABB_T& b) const {
            AABB_T res;
            res.min = glm::min(min, b.min);
            res.max = glm::max(max, b.max);
            return res;
        }
        inline AABB_T Expand(T expansion) const {
            AABB_T res = *this;
            res.min -= vec<3, T, Q>(expansion, expansion, expansion);
            res.max += vec<3, T, Q>(expansion, expansion, expansion);
            return res;
        }
        inline AABB_T Expand(const vec<3, T, Q>& expansion) const {
            AABB_T res = *this;
            res.min -= expansion;
            res.max += expansion;
            return res;
        }
        inline AABB_T Offset(const vec<3, T, Q>& offset) const {
            AABB_T res = *this;
            res.min += offset;
            res.max += offset;
            return res;
        }
        inline bool Intersects(const AABB_T& other) const {
            return !(
                (max.x < other.min.x) ||
                (max.y < other.min.y) ||
                (max.z < other.min.z) ||
                (other.max.x < min.z) ||
                (other.max.y < min.y) ||
                (other.max.z < min.z)
                );
        }
    };

    template <typename T, qualifier Q = defaultp>
    inline AABB_T<T, Q> operator * (const mat<4, 4, T, Q>& m, const AABB_T<T, Q>& b) {
        AABB_T<T, Q> res;
        for (int i = 0; i < 8; i++) {
            vec<4, T, Q> tmp = (m * vec<4, T, Q>(b.Point(i), T(1.0)));
            vec<3, T, Q> tmp3 = tmp.xyz() / tmp.w;
            res += tmp3;
        }
        return res;
    }
    template <typename T, qualifier Q = defaultp>
    inline std::string to_string(const AABB_T<T, Q>& b) {
        return "AABB(" + glm::to_string(b.min) + ", " + glm::to_string(b.max) + ")";
    }

    using AABB = AABB_T<float>;
    using dAABB = AABB_T<double>;

    template <typename T, qualifier Q = defaultp>
    struct Ray_T {
        vec<3, T, Q> origin;// = vec<3, T, Q>(0, 0, 0);
        vec<3, T, Q> dir;// = vec<3, T, Q>(1, 0, 0);
    };
    template <typename T, qualifier Q = defaultp>
    inline Ray_T<T, Q> operator * (const mat<4, 4, T, Q>& m, const Ray_T<T, Q>& r) {
        Ray_T<T, Q> res;
        vec<4, T, Q> tmp1 = (m * vec<4, T, Q>(r.origin, 1.0f));
        vec<4, T, Q> tmp2 = (m * vec<4, T, Q>(r.origin + r.dir, 1.0f));
        res.origin = tmp1.xyz() / tmp1.w;
        res.dir = tmp2.xyz() / tmp2.w - res.origin;
        return res;
    }
    template <typename T, qualifier Q = defaultp>
    inline Ray_T<T, Q> operator * (const qua<T, Q>& q, const Ray_T<T, Q>& r) {
        Ray_T<T, Q> res;
        res.dir = q * r.dir;
        res.origin = q * r.origin;
        return res;
    }
    using Ray = Ray_T<float>;
    using dRay = Ray_T<double>;

    template <typename T, qualifier Q = defaultp>
    vec<3, T, Q> AnyPerp(const vec<3, T, Q>& n) {
        int i = 0;
        for (int j = 1; j < 3; j++)
            if (glm::abs(n[j]) < glm::abs(n[i]))
                i = j;
        vec<3, T, Q> tmp_tan{ 0,0,0 };
        tmp_tan[i] = 1.0;
        return glm::cross(n, tmp_tan);
    }

    template <typename T, qualifier Q = defaultp>
    inline T ProjectionT(const vec<3, T, Q>& dest, const vec<3, T, Q>& source) {
        T dp = dot(source, dest);
        return dp / glm::length2(dest);
    }

    template <typename T, qualifier Q = defaultp>
    inline T ProjectionT(const Ray_T<T, Q>& ray, const vec<3, T, Q>& pt) {
        vec<3, T, Q> pt_dir = pt - ray.origin;
        return ProjectionT(ray.dir, pt_dir);
    }

    template <typename T, qualifier Q = defaultp>
    inline vec<3, T, Q> Projection(const Ray_T<T, Q>& ray, const vec<3, T, Q>& pt) {
        return ray.origin + ray.dir * ProjectionT(ray, pt);
    }

    template <typename T, qualifier Q = defaultp>
    inline vec<3, T, Q> Projection(const Plane_T<T, Q>& plane, const vec<3, T, Q>& pt) {
        T dist = (dot(plane.v.xyz(), pt) + plane.v.w) / dot(plane.v.xyz(), plane.v.xyz());
        return pt - plane.v.xyz() * dist;
    }

    template <typename T, qualifier Q = defaultp>
    inline bool Intersect(const Plane_T<T, Q>& p1, const Plane_T<T, Q>& p2, const Plane_T<T, Q>& p3, vec<3, T, Q>* intpt) {
        mat<3, 3, T, Q> m = { p1.v.xyz(), p2.v.xyz(), p3.v.xyz() };
        vec<3, T, Q> b = { -p1.v.w, -p2.v.w, -p3.v.w };
        if (determinant(m) == 0) return false;
        mat<3, 3, T, Q> m_inv = inverse(m);
        *intpt = b * m_inv;
        return true;
    }

    template <typename T, qualifier Q = defaultp>
    bool Intersect(const glm::AABB_T<T, Q>& box, const Ray_T<T, Q>& ray, vec<2, T, Q>& t)
    {
        vec<3, T, Q> rad = box.Size() * T(0.5);

        vec<3, T, Q> m = 1.0 / ray.dir;
        vec<3, T, Q> n = m * (ray.origin - box.Center());
        vec<3, T, Q> k = abs(m) * rad;
        vec<3, T, Q> t1 = -n - k;
        vec<3, T, Q> t2 = -n + k;

        t.x = max(max(t1.x, t1.y), t1.z);
        t.y = min(min(t2.x, t2.y), t2.z);

        return !(t.x > t.y || t.y < 0.0);
    }

    // segment/triangle intersection
    template <typename T, qualifier Q = defaultp>
    bool Intersect(const vec<3, T, Q>& pt1, const vec<3, T, Q>& pt2, const vec<3, T, Q>& pt3, const vec<3, T, Q>& seg_start, const vec<3, T, Q>& seg_end, T* t)
    {
        vec<3, T, Q> ray_dir = seg_end - seg_start;
        vec<3, T, Q> a, b, p, q, n;
        T inv_det, det;
        a = pt1 - pt2;
        b = pt3 - pt1;
        n = cross(b, a);
        p = pt1 - seg_start;
        q = cross(p, ray_dir);

        det = dot(ray_dir, n);
        if (!det) return false;
        inv_det = T(1.0) / det;

        vec<3, T, Q> uvw;
        uvw.x = dot(q, b) * inv_det;
        uvw.y = dot(q, a) * inv_det;

        if ((uvw.x < 0) || (uvw.x > 1) || (uvw.y < 0) || (uvw.x + uvw.y > 1)) return false;
        uvw.z = T(1.0) - uvw.x - uvw.y;

        T tmpt = dot(n, p) * inv_det;
        if (tmpt < 0) return false;
        if (tmpt > 1) return false;

        *t = tmpt;
        return true;
    }

    // plane/ray intersection
    template <typename T, qualifier Q = defaultp>
    bool Intersect(const Plane_T<T, Q>& plane, const Ray_T<T, Q>& ray, T* t = nullptr)
    {
        T da = -glm::dot(plane.v.xyz(), ray.origin);
        T db = -glm::dot(plane.v.xyz(), ray.origin + ray.dir);
        if (da == db) return false;
        if (t)
            *t = (plane.v.w - da) / (db - da);
        return true;
    }

    bool Intersect(const glm::AABB& box, const glm::vec3& pt1, const glm::vec3& pt2, const glm::vec3& pt3);
    bool Intersect(const glm::AABB& box, const glm::vec3& seg_start, const glm::vec3& seg_end, bool solid_aabb);
    bool Intersect(const glm::Plane& plane, const glm::Ray& ray, glm::vec3& pt);

    template <typename T, qualifier Q = defaultp>
    vec<2, T, Q> SkewLinesSegment(const Ray_T<T, Q>& r1, const Ray_T<T, Q>& r2) {
        vec<2, T, Q> res{ 0,0 };
        vec<3, T, Q> n = glm::cross(r1.dir, r2.dir);
        if (length2(n) == 0) {
            if (dot(r1.dir, r2.origin - r1.origin) < 0) {
                res.x = 0;
                res.y = ProjectionT(r2, r1.origin);
            }
            else {
                res.y = 0;
                res.x = ProjectionT(r1, r2.origin);
            }
            return res;
        }

        glm::Plane_T<T, Q> p1 = Plane_T(glm::cross(n, r1.dir), r1.origin);
        bool b1 = Intersect(p1, r2, &res.y);
        assert(b1);
        glm::Plane_T<T, Q> p2 = Plane_T(glm::cross(n, r2.dir), r2.origin);
        bool b2 = Intersect(p2, r1, &res.x);
        assert(b2);

        return res;
    }

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
    int WeightedRandom(int items_count, const std::function<int(int idx)> get_weight);
}