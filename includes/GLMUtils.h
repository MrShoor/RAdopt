#pragma once
#include "GLM.h"
#include <limits>

namespace glm {
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
            glm::vec3 tmp3 = tmp.xyz();
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
}