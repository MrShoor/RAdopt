#include "pch.h"
#include "RUtils.h"
#include "stb_image_bindings.h"
#include <map>
#include <unordered_set>

namespace RA {
    glm::Plane Camera::GetFrustumPlane(FrustumPlane fp)
    {
        glm::vec4 pt[3];
        switch (fp) {
        case (FrustumPlane::Top):
            pt[0] = { -1, 1, m_depth_range.x, 1 };
            pt[1] = {  1, 1, m_depth_range.x, 1 };
            pt[2] = { -1, 1, m_depth_range.y, 1 };
            break;
        case (FrustumPlane::Bottom):
            pt[0] = { -1, -1, m_depth_range.x, 1 };
            pt[1] = { -1, -1, m_depth_range.y, 1 };
            pt[2] = {  1, -1, m_depth_range.x, 1 };            
            break;
        case (FrustumPlane::Left):
            pt[0] = { -1,  1, m_depth_range.x, 1 };            
            pt[1] = { -1,  1, m_depth_range.y, 1 };
            pt[2] = { -1, -1, m_depth_range.x, 1 };
            break;
        case (FrustumPlane::Right):
            pt[0] = {  1,  1, m_depth_range.x, 1 };
            pt[1] = {  1, -1, m_depth_range.x, 1 };
            pt[2] = {  1,  1, m_depth_range.y, 1 };
            break;
        case (FrustumPlane::Near):
            pt[0] = { -1, -1, m_depth_range.x, 1 };
            pt[1] = {  1,  1, m_depth_range.x, 1 };
            pt[2] = { -1,  1, m_depth_range.x, 1 };
            break;
        case (FrustumPlane::Far):
            pt[0] = { -1, -1, m_depth_range.y, 1 };
            pt[1] = { -1,  1, m_depth_range.y, 1 };
            pt[2] = {  1,  1, m_depth_range.y, 1 };
            break;
        }
        for (int i = 0; i < 3; i++) {
            pt[i] = m_buf.view_proj_inv * pt[i];
            pt[i] /= pt[i].w;
        }
        return glm::Plane(pt[0].xyz(), pt[1].xyz(), pt[2].xyz());
    }
    Camera::Camera(const DevicePtr& device)
    {
        m_ubo = device->Create_UniformBuffer();
        m_ubo->SetState(LB()->Finish(sizeof(CameraBuf)), 1, nullptr);

        SetCamera(glm::vec3(0, 0, -5), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        SetProjection(glm::pi<float>() * 0.25f, 1.0f, glm::vec2(0.1, 100.0));
    }
    void Camera::SetCamera(const glm::vec3& eye, const glm::vec3& at, const glm::vec3& up)
    {
        m_eye = eye;
        m_at = at;
        m_up = up;
        m_buf.view = glm::lookAtLH(m_eye, m_at, m_up);

        m_buf.UpdateViewProj();

        m_ubo->SetSubData(0, 1, &m_buf);
    }
    void Camera::SetProjection(float fov, float aspect, const glm::vec2& near_far, const glm::vec2& depth_range)
    {
        m_fov = fov;
        m_aspect = aspect;
        m_near_far = near_far;
        m_depth_range = depth_range;
        
        float h = 1.0f / tan(m_fov * 0.5f);
        float w = h * m_aspect;
        float q = 1.0f / (m_near_far.x - m_near_far.y);
        float depth_size = m_depth_range.y - m_depth_range.x;

        m_buf.proj = glm::mat4(0);
        m_buf.proj[0][0] = w;
        m_buf.proj[1][1] = h;
        m_buf.proj[2][2] = m_depth_range.x - depth_size * m_near_far.y * q;
        m_buf.proj[2][3] = 1.0;
        m_buf.proj[3][2] = depth_size * m_near_far.x * m_near_far.y * q;

        m_buf.UpdateViewProj();

        m_ubo->SetSubData(0, 1, &m_buf);
    }
    void Camera::SetAspect(HWND wnd)
    {
        RECT rct;
        GetClientRect(wnd, &rct);
        float aspect = float((rct.bottom - rct.top)) / (rct.right - rct.left);
        SetProjection(m_fov, aspect, m_near_far, m_depth_range);
    }
    void Camera::SetNearFar(const glm::vec2& near_far)
    {
        SetProjection(m_fov, m_aspect, near_far, m_depth_range);
    }
    void Camera::FitView(const glm::vec3* points, int num_points)
    {
        glm::vec3 view_dir = m_at - m_eye;
        glm::Plane planes[4];
        float dist[4] = { std::numeric_limits<float>::max(),
                         std::numeric_limits<float>::max(),
                         std::numeric_limits<float>::max(),
                         std::numeric_limits<float>::max() };
        planes[0] = GetFrustumPlane(FrustumPlane::Left);
        planes[1] = GetFrustumPlane(FrustumPlane::Right);
        planes[2] = GetFrustumPlane(FrustumPlane::Top);
        planes[3] = GetFrustumPlane(FrustumPlane::Bottom);
        for (int i = 0; i < 4; i++) {
            planes[i].v /= length(planes[i].v.xyz());
        }
        for (int i = 0; i < num_points; i++) {
            for (int j = 0; j < 4; j++) {
                dist[j] = glm::min(dist[j], planes[j].Distance(points[i]));
            }
        }
        for (int i = 0; i < 4; i++) {
            planes[i].v.w -= dist[i];
        }
        glm::vec3 intpt1;
        glm::vec3 intpt2;
        if (!Intersect(planes[0], planes[2], planes[3], &intpt1)) return;
        if (!Intersect(planes[1], planes[2], planes[3], &intpt2)) return;
        glm::vec3 tmp1 = (intpt1 + intpt2) * 0.5f;
        if (!Intersect(planes[0], planes[1], planes[2], &intpt1)) return;
        if (!Intersect(planes[0], planes[1], planes[3], &intpt2)) return;
        glm::vec3 tmp2 = (intpt1 + intpt2) * 0.5f;
        glm::vec3 new_eye;
        if (glm::dot(tmp1, view_dir) < glm::dot(tmp2, view_dir)) {
            new_eye = tmp1;
        }
        else {
            new_eye = tmp2;
        }
        SetCamera(new_eye, new_eye + view_dir, m_up);
    }
    glm::vec3 Camera::Eye()
    {
        return m_eye;
    }
    glm::vec3 Camera::At() 
    {
        return m_at;
    }
    void Camera::FitView(const glm::AABB& bbox)
    {
        glm::vec3 points[8];
        for (int i = 0; i < 8; i++)
            points[i] = bbox.Point(i);
        FitView(points, 8);
    }
    glm::vec3 Camera::ViewDir()
    {
        return m_at - m_eye;
    }
    UniformBufferPtr Camera::GetUBO()
    {
        m_ubo->ValidateDynamicData();
        return m_ubo;
    }

    STB_TexManager gvTexManager;
    TexManagerIntf* TM()
    {
        return &gvTexManager;
    }
    RangeManagerIntfPtr Create_RangeManager()
    {
        return RangeManagerIntfPtr();
    }

    class RangeManager;

    class MemRange : public MemRangeIntf {
        friend class RangeManager;
    private:
        RangeManager* m_man;
        int m_idx;
        glm::ivec2 m_offset_size;
    public:
        int Offset() const override {
            return m_offset_size.x;
        }
        int Size() const override {
            return m_offset_size.y;
        }
        glm::ivec2 OffsetSize() const override {
            return m_offset_size;
        }
        MemRange(RangeManager* man, const glm::ivec2& offset_size);
        ~MemRange();
    };

    class RangeManager : public RangeManagerIntf {
        friend class MemRange;
    private:
        int m_size;
        int m_allocated_space;
        std::vector<MemRange*> m_allocated;

        std::map<int, std::unordered_set<int>> m_free_chunks; //key - chunk size, value - chunk offsets
        std::unordered_map<int, int> m_free_ranges; //chunk points (start offset, end offset)

        void FreeRange(const glm::ivec2& offset_size) {
            m_allocated_space -= offset_size.y;
            int seg_offset = offset_size.x;
            int seg_end = seg_offset + offset_size.y;
            auto it = m_free_ranges.find(seg_offset);
            if (it != m_free_ranges.end()) { //merge with prev segment
                int seg_prev_offset = it->second;
                m_free_ranges.erase(seg_prev_offset);
                m_free_ranges.erase(seg_offset);
                auto it2 = m_free_chunks.find(seg_offset - seg_prev_offset);
                it2->second.erase(seg_prev_offset);
                if (!it2->second.size())
                    m_free_chunks.erase(it2->first);
                seg_offset = seg_prev_offset;
            }
            it = m_free_ranges.find(seg_end);
            if (it != m_free_ranges.end()) { //merge with next segment
                int seg_next_end = it->second;
                m_free_ranges.erase(seg_end);
                m_free_ranges.erase(seg_next_end);
                auto it2 = m_free_chunks.find(seg_next_end - seg_end);
                it2->second.erase(seg_end);
                if (!it2->second.size())
                    m_free_chunks.erase(it2->first);
                seg_end = seg_next_end;
            }

            int new_size = seg_end - seg_offset;
            auto it3 = m_free_chunks.find(new_size);
            if (it3 == m_free_chunks.end()) {
                m_free_chunks.insert({ new_size, std::unordered_set({seg_offset}) });
            }
            else {
                it3->second.insert(seg_offset);
            }
            m_free_ranges[seg_offset] = seg_end;
            m_free_ranges[seg_end] = seg_offset;
        }
    public:
        MemRangeIntfPtr Alloc(int size) override {
            if (size <= 0) return MemRangeIntfPtr(new MemRange(nullptr, glm::ivec2(0, 0)));

            auto it = m_free_chunks.upper_bound(size);
            if (it == m_free_chunks.end()) return nullptr;
            int seg_offset = *(it->second.begin());
            it->second.erase(seg_offset);
            if (!it->second.size()) {
                m_free_chunks.erase(it->first);
            }

            int seg_end = m_free_ranges[seg_offset];
            m_free_ranges.erase(seg_offset);
            m_free_ranges.erase(seg_end);

            MemRangeIntfPtr res(new MemRange(this, glm::ivec2(seg_offset, size)) );

            seg_offset += size;
            if (seg_offset < seg_end) {
                m_free_ranges[seg_offset] = seg_end;
                m_free_ranges[seg_end] = seg_offset;
                m_free_chunks[seg_end - seg_offset].insert(seg_offset);
            }
            m_allocated_space += size;
            return res;
        }
        int Size() const override {
            return m_size;
        }
        int Allocated() const override {
            return m_allocated_space;
        }
        int FreeSpace() const override {
            return m_size - m_allocated_space;
        }
        void AddSpace(int new_space) override {
            FreeRange(glm::ivec2(m_size, new_space));
            m_size += new_space;
        }
        //void Defrag() override { }
        RangeManager(int new_space) {
            AddSpace(new_space);
        }
        ~RangeManager() {
            for (auto& m : m_allocated) {
                m->m_man = nullptr;
            }
        }
    };
    using RangeManagerIntfPtr = std::unique_ptr<RangeManagerIntf>;

    MemRange::MemRange(RangeManager* man, const glm::ivec2& offset_size) {
        m_man = man;
        if (m_man) {
            m_man->m_allocated.push_back(this);
            m_idx = int(m_man->m_allocated.size()) - 1;
        }
        else {
            m_idx = -1;
        }
        m_offset_size = offset_size;
    }
    MemRange::~MemRange() {
        if (m_man) {
            m_man->m_allocated[m_idx] = m_man->m_allocated[m_man->m_allocated.size() - 1];
            m_man->m_allocated[m_idx]->m_idx = m_idx;
            m_man->m_allocated.pop_back();
            m_man->FreeRange(m_offset_size);
        }
    }

    RangeManagerIntfPtr Create_RangeManager(int size) {
        return std::make_unique<RangeManager>(size);
    }
    uint64_t QPC::Time()
    {
        uint64_t tmp;
        QueryPerformanceCounter((LARGE_INTEGER*)&tmp);
        return (tmp - m_start) * 1000 / m_freq;
    }
    QPC::QPC()
    {
        QueryPerformanceCounter((LARGE_INTEGER*)&m_start);
        QueryPerformanceFrequency((LARGE_INTEGER*)&m_freq);
    }
}