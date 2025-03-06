#include "pch.h"
#include "GLMUtils.h"

namespace glm {
    void Bezier2_2d::Aprrox(float tolerance, const std::function<void(const glm::vec2& v)>& add_point) const
    {        
        if (IsStraight(tolerance)) {
            add_point(pt[2]);
        }
        else {
            Bezier2_2d b1, b2;
            Split(0.5f, &b1, &b2);
            b1.Aprrox(tolerance, add_point);
            b2.Aprrox(tolerance, add_point);
        }
    }
    bool Intersect(const glm::AABB& box, const glm::vec3& pt1, const glm::vec3& pt2, const glm::vec3& pt3)
    {
        Plane p(pt1, pt2, pt3);
        float s = glm::sign(p.Distance(box.Point(0)));
        int i = 1;
        while (i < 8) {
            if (s != glm::sign(p.Distance(box.Point(i)))) break;
            i++;
        }
        if (i == 8) return false;

        for (int pidx = 0; pidx < 6; pidx++) {
            Plane p = box.Plane(pidx);
            if (p.Distance(pt1) > 0) continue;
            if (p.Distance(pt2) > 0) continue;
            if (p.Distance(pt3) > 0) continue;
            return false;
        }
        return true;
    }
    bool Intersect(const glm::AABB& box, const glm::vec3& seg_start, const glm::vec3& seg_end, bool solid_aabb)
    {
        glm::vec3 ray_dir = seg_end - seg_start;
        glm::vec3 t_min, t_max;
        if (ray_dir.x > 0) {
            t_min.x = (box.min.x - seg_start.x) / ray_dir.x;
            t_max.x = (box.max.x - seg_start.x) / ray_dir.x;
        }
        else {
            t_max.x = (box.min.x - seg_start.x) / ray_dir.x;
            t_min.x = (box.max.x - seg_start.x) / ray_dir.x;
        }
        if (ray_dir.y > 0) {
            t_min.y = (box.min.y - seg_start.y) / ray_dir.y;
            t_max.y = (box.max.y - seg_start.y) / ray_dir.y;
        }
        else {
            t_max.y = (box.min.y - seg_start.y) / ray_dir.y;
            t_min.y = (box.max.y - seg_start.y) / ray_dir.y;
        }

        if ((t_min.x > t_max.y) || (t_min.y > t_max.x)) return false;
        t_min.x = glm::max(t_min.x, t_min.y);
        t_max.x = glm::min(t_max.x, t_max.y);

        if (ray_dir.z > 0) {
            t_min.z = (box.min.z - seg_start.z) / ray_dir.z;
            t_max.z = (box.max.z - seg_start.z) / ray_dir.z;
        }
        else {
            t_max.z = (box.min.z - seg_start.z) / ray_dir.z;
            t_min.z = (box.max.z - seg_start.z) / ray_dir.z;
        }

        if ((t_min.x > t_max.z) || (t_min.z > t_max.x)) return false;
        t_min.x = glm::max(t_min.x, t_min.z);
        t_max.x = glm::min(t_max.x, t_max.z);

        if (solid_aabb) {
            t_min.x = glm::max(0.0f, t_min.x);
            if (t_min.x > t_max.x) return false;
        }
        return (t_min.x <= 1.0);
    }
    bool Intersect(const glm::Plane& plane, const glm::Ray& ray, glm::vec3& pt)
    {
        float t;
        if (!Intersect(plane, ray, &t)) return false;
        pt = ray.origin + ray.dir * t;
        return true;
    }
    int WeightedRandom(const std::vector<int>& weights)
    {
        return WeightedRandom(int(weights.size()), [&](int idx) {return weights[idx]; });
    }
    int WeightedRandom(int items_count, const std::function<int(int idx)> get_weight)
    {
        int weight_summ = 1;
        for (int i = 0; i < items_count; i++) weight_summ += get_weight(i);
        int r = int(glm::linearRand(0.f, 1.f) * weight_summ);
        int drop = r % weight_summ;
        int drop_idx = 0;
        while (drop > get_weight(drop_idx) && (drop_idx < items_count)) {
            drop -= get_weight(drop_idx);
            drop_idx++;
        }
        return drop_idx;
    }
}