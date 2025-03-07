#include "pch.h"
#include "RUtils.h"
#include "stb_image_bindings.h"
#include <map>
#include <unordered_set>
#include <Win.h>

namespace RA {
    Camera::Camera(const DevicePtr& device) : CameraBase(device)
    {
        SetCamera(glm::vec3(0, 0, -5), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        SetProjection(glm::pi<float>() * 0.25f, 1.0f, glm::vec2(0.1, 100.0));
    }
    glm::Plane Camera::GetFrustumPlane(FrustumPlane fp) const
    {
        glm::vec4 pt[3];
        switch (fp) {
        case (FrustumPlane::Top):
            pt[0] = { -1, 1, m_depth_range.x, 1 };
            pt[1] = { 1, 1, m_depth_range.x, 1 };
            pt[2] = { -1, 1, m_depth_range.y, 1 };
            break;
        case (FrustumPlane::Bottom):
            pt[0] = { -1, -1, m_depth_range.x, 1 };
            pt[1] = { -1, -1, m_depth_range.y, 1 };
            pt[2] = { 1, -1, m_depth_range.x, 1 };
            break;
        case (FrustumPlane::Left):
            pt[0] = { -1,  1, m_depth_range.x, 1 };
            pt[1] = { -1,  1, m_depth_range.y, 1 };
            pt[2] = { -1, -1, m_depth_range.x, 1 };
            break;
        case (FrustumPlane::Right):
            pt[0] = { 1,  1, m_depth_range.x, 1 };
            pt[1] = { 1, -1, m_depth_range.x, 1 };
            pt[2] = { 1,  1, m_depth_range.y, 1 };
            break;
        case (FrustumPlane::Near):
            pt[0] = { -1, -1, m_depth_range.x, 1 };
            pt[1] = { 1,  1, m_depth_range.x, 1 };
            pt[2] = { -1,  1, m_depth_range.x, 1 };
            break;
        case (FrustumPlane::Far):
            pt[0] = { -1, -1, m_depth_range.y, 1 };
            pt[1] = { -1,  1, m_depth_range.y, 1 };
            pt[2] = { 1,  1, m_depth_range.y, 1 };
            break;
        }
        for (int i = 0; i < 3; i++) {
            pt[i] = m_buf.view_proj_inv * pt[i];
            pt[i] /= pt[i].w;
        }
        return glm::Plane(pt[0].xyz(), pt[1].xyz(), pt[2].xyz());
    }
    bool Camera::IsOrtho() const
    {
        return m_is_ortho;
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
        m_is_ortho = false;
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
        m_buf.z_near_far = m_near_far;

        m_buf.UpdateViewProj();

        m_ubo->SetSubData(0, 1, &m_buf);
    }
    void Camera::SetOrtho(float width, float height, const glm::vec2& near_far, const glm::vec2& depth_range)
    {        
        m_is_ortho = true;
        m_ortho_width = width;
        m_ortho_height = height;
        m_near_far = near_far;

        float q = 1.0f / (near_far.y - near_far.x);
        float depth_size = depth_range.y - depth_range.x;
        m_buf.proj = glm::mat4(1);
        m_buf.proj[0][0] = 2 / width;
        m_buf.proj[1][1] = 2 / height;
        m_buf.proj[2][2] = depth_size * q;
        m_buf.proj[3][2] = depth_range.x - depth_size * near_far.x * q;
        m_buf.z_near_far = m_near_far;

        m_buf.UpdateViewProj();

        m_ubo->SetSubData(0, 1, &m_buf);
    }
    float Camera::FoV() const
    {
        return m_fov;
    }
    float Camera::Aspect() const
    {
        return m_aspect;
    }
    void Camera::SetAspect(HWND wnd)
    {
        RECT rct;
        GetClientRect(wnd, &rct);
        float aspect = float((rct.bottom - rct.top)) / (rct.right - rct.left);
        SetProjection(m_fov, aspect, m_near_far, m_depth_range);
    }
    void Camera::SetOrthoWidhtHeight(HWND wnd)
    {
        RECT rct;
        GetClientRect(wnd, &rct);
        SetOrthoWidhtHeight(glm::vec2(rct.right - rct.left, rct.bottom - rct.top));
    }
    void Camera::SetOrthoWidhtHeight_ByAspectAndHeight(HWND wnd, float height)
    {
        RECT rct;
        GetClientRect(wnd, &rct);
        float w = float(rct.right - rct.left) * (height / (rct.bottom - rct.top));
        SetOrthoWidhtHeight(glm::vec2(w, height));
    }
    void Camera::SetOrthoWidhtHeight(const glm::vec2& width_height)
    {
        SetOrtho(width_height.x, width_height.y, m_near_far, m_depth_range);
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
    glm::vec3 Camera::Eye() const
    {
        return m_eye;
    }
    glm::vec3 Camera::At() const
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
    glm::Ray Camera::NDCToWorldRay(const glm::vec2& ndc) const
    {
        glm::vec4 tmp_near = glm::vec4(ndc, m_depth_range.x, 1.0);
        glm::vec4 tmp_far = glm::vec4(ndc, m_depth_range.y, 1.0);
        tmp_near = m_buf.view_proj_inv * tmp_near;
        tmp_far = m_buf.view_proj_inv * tmp_far;
        glm::Ray res;
        res.origin = tmp_near.xyz() / tmp_near.w;
        res.dir = tmp_far.xyz() / tmp_far.w - res.origin;
        return res;
    }
    glm::vec3 Camera::ViewDir() const
    {
        return m_at - m_eye;
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
        ~MemRange() override;
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
    std::wstring UTF8ToWString(const std::string& utf8)
    {
        std::wstring res;
        if (utf8.empty()) return res;
        auto size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), nullptr, 0);
        if (size <= 0) return res;
        res.resize(size_t(size));
        MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), res.data(), size);
        return res;
    }
    std::string WStringToUTF8(const std::wstring& wstr)
    {
        std::string res;
        if (wstr.empty()) return res;
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), int(wstr.size()), NULL, 0, NULL, NULL);
        if (size <= 0) return res;
        res.resize(size_t(size));
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), int(wstr.size()), res.data(), size, NULL, NULL);
        return res;
    }
    uint64_t QPC::TimeMcS() const
    {
        if (m_paused) {
            return (m_paused_time - m_start) * 1000000 / m_freq;
        }
        else {
            uint64_t tmp;
            QueryPerformanceCounter((LARGE_INTEGER*)&tmp);
            return (tmp - m_start) * 1000000 / m_freq;
        }
    }
    uint64_t QPC::Time() const
    {        
        if (m_paused) {
            return (m_paused_time - m_start) * 1000 / m_freq;
        }
        else {
            uint64_t tmp;
            QueryPerformanceCounter((LARGE_INTEGER*)&tmp);
            return (tmp - m_start) * 1000 / m_freq;
        }
    }
    bool QPC::Paused() const
    {
        return m_paused;
    }
    void QPC::Pause()
    {
        if (m_paused) return;
        m_paused = true;
        QueryPerformanceCounter((LARGE_INTEGER*)&m_paused_time);
    }
    void QPC::Unpause()
    {
        if (!m_paused) return;
        m_paused = false;
        uint64_t t;
        QueryPerformanceCounter((LARGE_INTEGER*)&t);
        m_start += t - m_paused_time;
    }
    QPC::QPC()
    {
        QueryPerformanceCounter((LARGE_INTEGER*)&m_start);
        QueryPerformanceFrequency((LARGE_INTEGER*)&m_freq);
    }
    void Octree::SplitBox(const glm::AABB& box, glm::AABB* childs)
    {
        glm::vec3 pts[3] = {box.min, box.Center(), box.max};
        for (int i = 0; i < 8; i++) {
            int x = i / 4;
            int y = (i / 2) % 2;
            int z = i % 2;
            childs[i].min.x = pts[x].x;
            childs[i].min.y = pts[y].y;
            childs[i].min.z = pts[z].z;
            childs[i].max.x = pts[x+1].x;
            childs[i].max.y = pts[y+1].y;
            childs[i].max.z = pts[z+1].z;
        }
    }
    bool Octree::Intersect(const glm::AABB& box, int tri_idx)
    {
        glm::vec3 pts[3];
        pts[0] = m_triangles[tri_idx * 3];
        pts[1] = m_triangles[tri_idx * 3 + 1];
        pts[2] = m_triangles[tri_idx * 3 + 2];
        return glm::Intersect(box, pts[0], pts[1], pts[2]);
    }
    void Octree::SplitRecursive(OctreeNode* node)
    {
        if (int(node->triangles.size()) < m_max_triangles_to_split) return;

        glm::AABB child_boxes[8];
        SplitBox(node->box, child_boxes);
        for (int i = 0; i < 8; i++) {
            node->childs[i] = std::make_unique<OctreeNode>(child_boxes[i]);
            for (int idx : node->triangles) {
                if (Intersect(child_boxes[i], idx))
                    node->childs[i]->triangles.push_back(idx);
            }
            SplitRecursive(node->childs[i].get());
        }
        node->triangles.clear();
    }
    void Octree::RayCastRecursive(OctreeNode* node, const glm::vec3& ray_start, const glm::vec3& ray_end, float* t, glm::vec3* normal)
    {
        if (!glm::Intersect(node->box, ray_start, ray_end, true)) return;
        if (node->childs[0]) {
            for (int i = 0; i < 8; i++) {
                RayCastRecursive(node->childs[i].get(), ray_start, ray_end, t, normal);
            }
        }
        else {
            for (int idx : node->triangles) {
                float tcurr;
                glm::vec3 p[3] = { m_triangles[idx * 3], m_triangles[idx * 3 + 1], m_triangles[idx * 3 + 2] };
                if (glm::Intersect(p[0], p[1], p[2], ray_start, ray_end, &tcurr)) {
                    if (*t > tcurr) {
                        glm::vec3 n = glm::cross(p[1] - p[0], p[2] - p[0]);
                        if (glm::dot(n, ray_end - ray_start) > 0) n = -n;
                        *normal = glm::normalize(n);
                        *t = tcurr;
                    }
                }
            }
        }
    }
    void Octree::EnumRecursive(OctreeNode* node, const std::function<void(const OctreeNode*, bool& enum_child)>& cb) const
    {
        if (!node) return;
        bool enum_child = true;
        cb(node, enum_child);
        if (enum_child)
            for (int i = 0; i < 8; i++)
                EnumRecursive(node->childs[i].get(), cb);
    }
    const std::vector<glm::vec3>& Octree::Triangles() const
    {
        return m_triangles;
    }
    float Octree::RayCast(const glm::vec3& ray_start, const glm::vec3& ray_end)
    {
        glm::mat4 m1, m2;
        glm::mat4 m3 = m2 * m1;
        float t = 1.0f;
        glm::vec3 tmpn;
        RayCastRecursive(m_root.get(), ray_start, ray_end, &t, &tmpn);
        return t;
    }
    bool Octree::RayCast(const glm::vec3& ray_start, const glm::vec3& ray_end, float* t, glm::vec3* normal)
    {
        float tmpt = 1.0f;
        RayCastRecursive(m_root.get(), ray_start, ray_end, &tmpt, normal);
        if (tmpt == 1.0f) return false;
        *t = tmpt;
        return true;
    }
    void Octree::EnumNodes(const glm::vec3& pos, float rad, const std::function<void(const OctreeNode*)>& cb) const
    {
        glm::AABB box;
        box += pos;
        box = box.Expand(rad);
        EnumNodes(box, cb);
    }
    void Octree::EnumNodes(const glm::AABB& box, const std::function<void(const OctreeNode*)>& cb) const
    {
        EnumRecursive(m_root.get(), [&](const OctreeNode* node, bool& enum_child) {
            enum_child = node->box.Intersects(box);
            if (enum_child)
                cb(node);
            });
    }
    Octree::Octree(std::vector<glm::vec3> triangles, int max_triangles_to_split)
    {
        m_max_triangles_to_split = max_triangles_to_split;
        m_triangles = std::move(triangles);

        glm::AABB root_box;
        for (const auto& v : m_triangles)
            root_box += v;
        
        m_root = std::make_unique<OctreeNode>(root_box);
        m_root->triangles.reserve(m_triangles.size() / 3);
        for (int i = 0; i < int(m_triangles.size() / 3); i++) {
            m_root->triangles.push_back(i);
        }
        SplitRecursive(m_root.get());
    }
    void ManagedSBO::ValidateBuffer()
    {
        if (m_buffer_valid) return;
        m_buffer_valid = true;
        m_buffer->SetState(m_buffer->Stride(), m_man->Size());
        for (const auto& r : m_ranges) {
            r->UpdateSBOData();
        }
    }
    ManagedSBO_RangePtr ManagedSBO::Alloc(int vertex_count)
    {
        MemRangeIntfPtr range = m_man->Alloc(vertex_count);
        if (!range) {            
            m_man->AddSpace(glm::nextPowerOfTwo(m_man->Size() + vertex_count) - m_man->Size());
            range = m_man->Alloc(vertex_count);
            m_buffer_valid = false;
        }
        return std::make_unique<ManagedSBO_Range>(this, std::move(range));
    }
    StructuredBufferPtr ManagedSBO::Buffer()
    {
        ValidateBuffer();
        return m_buffer;
    }
    ManagedSBO::ManagedSBO(const DevicePtr& dev, int stride_size) : m_man(Create_RangeManager(32))
    {
        m_buffer = dev->Create_StructuredBuffer();
        m_buffer->SetState(stride_size, m_man->Size());
        m_buffer_valid = false;
    }
    void ManagedSBO_Range::UpdateSBOData()
    {
        if (m_sbo->m_buffer_valid) {
            m_sbo->m_buffer->SetSubData(m_range->Offset(), m_range->Size(), m_data.data());
        }
    }
    int ManagedSBO_Range::Offset() const
    {
        return m_range->Offset();
    }
    int ManagedSBO_Range::Size() const
    {
        return m_range->Size();
    }
    void ManagedSBO_Range::SetData(const void* data)
    {
        memcpy(m_data.data(), data, m_data.size());
        UpdateSBOData();
    }
    ManagedSBO_Range::ManagedSBO_Range(ManagedSBO* owner, MemRangeIntfPtr range)
    {
        m_sbo = owner;
        m_idx = int(m_sbo->m_ranges.size());
        m_sbo->m_ranges.push_back(this);
        m_range = std::move(range);
        m_data.resize(size_t(m_range->Size() * m_sbo->m_buffer->Stride()));
    }
    ManagedSBO_Range::~ManagedSBO_Range()
    {
        m_sbo->m_ranges[m_idx] = m_sbo->m_ranges.back();
        m_sbo->m_ranges[m_idx]->m_idx = m_idx;
        m_sbo->m_ranges.pop_back();
        if (m_idx != m_sbo->m_ranges.size())
            m_sbo->m_ranges[m_idx]->UpdateSBOData();
    }

    MemRangeIntfPtr ManagedTexSlices::Alloc(int slices_count, bool* tex_reallocated)
    {
        *tex_reallocated = false;
        MemRangeIntfPtr range = m_man->Alloc(slices_count);
        if (!range) {
            m_man->AddSpace(glm::nextPowerOfTwo(m_man->Size() + slices_count));
            range = m_man->Alloc(slices_count);
            *tex_reallocated = true;
        }
        return range;
    }

    Texture2DPtr ManagedTexSlices::Texture()
    {
        return m_tex;
    }

    ManagedTexSlices::ManagedTexSlices(const DevicePtr& dev, TextureFmt fmt, const glm::ivec2& tex_size) : m_man(Create_RangeManager(8)) {
        m_tex = dev->Create_Texture2D();
        m_tex->SetState(fmt, tex_size, 0, m_man->Size());
    }
    CameraBase::CameraBase(const DevicePtr& device)
    {
        m_ubo = device->Create_UniformBuffer();
        m_ubo->SetState(LB()->Finish(sizeof(CameraBuf)), 1, nullptr);
    }
    glm::mat4 CameraBase::View() const
    {
        return m_buf.view;
    }
    glm::mat4 CameraBase::Proj() const
    {
        return m_buf.proj;
    }
    glm::mat4 CameraBase::ViewProj() const
    {
        return m_buf.view_proj;
    }
    glm::mat4 CameraBase::ViewInv() const
    {
        return m_buf.view_inv;
    }
    glm::mat4 CameraBase::ProjInv() const
    {
        return m_buf.proj_inv;
    }
    glm::mat4 CameraBase::ViewProjInv() const
    {
        return m_buf.view_proj_inv;
    }
    UniformBufferPtr CameraBase::GetUBO()
    {
        m_ubo->ValidateDynamicData();
        return m_ubo;
    }
    CameraBase::~CameraBase()
    {
    }
    UICamera::UICamera(const DevicePtr& device) : CameraBase(device)
    {

    }
    void UICamera::UpdateFromWnd()
    {
        RECT rct;
        GetClientRect(m_ubo->GetDevice()->Window(), &rct);
        m_buf.view = glm::mat4(1.0);
        glm::mat4 ms = glm::scale(glm::vec3(2.0f / rct.right, -2.0f / rct.bottom, 1.0f));
        m_buf.proj = glm::translate(glm::vec3(-1.f, 1.f, 0.5f)) * ms;

        m_buf.UpdateViewProj();

        m_ubo->SetSubData(0, 1, &m_buf);
    }
    void StepTimer::Pause()
    {
        m_qpc.Pause();
    }
    void StepTimer::Unpause()
    {
        m_qpc.Unpause();
    }
    int StepTimer::StepInterval() const
    {
        return m_step_interval;
    }
    void StepTimer::Do(const std::function<void(int step_count)>& process)
    {
        uint64_t curr_time = m_qpc.Time();
        int step_count = int((curr_time - m_last_time) / m_step_interval);
        if (step_count)
        {
            process(step_count);
            m_last_time += step_count * m_step_interval;
        }
    }
    StepTimer::StepTimer(int step_interval) : m_step_interval(step_interval)
    {
        m_last_time = m_qpc.Time();
    }
}