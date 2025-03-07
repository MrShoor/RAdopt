#pragma once
#include "RAdopt.h"
#include "GLM.h"
#include "GLMUtils.h"
#include <filesystem>
#include <string>

namespace RA {
    enum class FrustumPlane {Top, Bottom, Right, Left, Near, Far};

    class CameraBase {
    private:
    protected:
        struct CameraBuf {
            glm::mat4 view;
            glm::mat4 proj;
            glm::mat4 view_proj;
            glm::mat4 view_inv;
            glm::mat4 proj_inv;
            glm::mat4 view_proj_inv;
            glm::vec2 z_near_far;
            glm::vec2 dummy;
            void UpdateViewProj() {
                view_proj = proj * view;
                view_inv = glm::inverse(view);
                proj_inv = glm::inverse(proj);
                view_proj_inv = glm::inverse(view_proj);
            }
            CameraBuf() :
                view(1),
                proj(1),
                view_proj(1),
                view_inv(1),
                proj_inv(1),
                view_proj_inv(1),
                z_near_far(0.1, 100.0),
                dummy(0) {
            }
        };
        CameraBuf m_buf;
        UniformBufferPtr m_ubo;
    public:
        CameraBase(const DevicePtr& device);

        glm::mat4 View() const;
        glm::mat4 Proj() const;
        glm::mat4 ViewProj() const;
        glm::mat4 ViewInv() const;
        glm::mat4 ProjInv() const;
        glm::mat4 ViewProjInv() const;

        UniformBufferPtr GetUBO();

        virtual ~CameraBase();
    };

    class Camera : public CameraBase {
    private:
        glm::vec3 m_eye;
        glm::vec3 m_at;
        glm::vec3 m_up;
        
        bool m_is_ortho;
        float m_ortho_width;
        float m_ortho_height;
        float m_aspect;
        float m_fov;
        glm::vec2 m_near_far;
        glm::vec2 m_depth_range;
    public:
        Camera(const DevicePtr& device);
    public:
        glm::Plane GetFrustumPlane(FrustumPlane fp) const;
        bool IsOrtho() const;

        void SetCamera(const glm::vec3& eye, const glm::vec3& at, const glm::vec3& up);
        void SetProjection(float fov, float aspect, const glm::vec2& near_far, const glm::vec2& depth_range = {1, 0});
        void SetOrtho(float width, float height, const glm::vec2& near_far, const glm::vec2& depth_range = { 1, 0 });

        float FoV() const;
        float Aspect() const;

        void SetAspect(HWND wnd);
        void SetOrthoWidhtHeight(HWND wnd);
        void SetOrthoWidhtHeight_ByAspectAndHeight(HWND wnd, float height);
        void SetOrthoWidhtHeight(const glm::vec2& width_height);
        void SetNearFar(const glm::vec2& near_far);

        void FitView(const glm::vec3* points, int num_points);
        void FitView(const glm::AABB& bbox);

        glm::Ray NDCToWorldRay(const glm::vec2& ndc) const;

        glm::vec3 Eye() const;
        glm::vec3 At() const;
        glm::vec3 ViewDir() const;
    };

    class CameraController_YawPitch {
    public:
        glm::vec3 look_at = { 0, 0, 0 };
        float yaw = 0;
        float pitch = 0;
        float distance = 10.0f;
    public:
        CameraController_YawPitch() {}
        glm::vec3 ViewDir() const {
            float s = glm::cos(pitch);
            return -glm::vec3(glm::sin(yaw) * s, glm::sin(pitch), glm::cos(yaw) * s);
        }
        glm::vec3 Up() const {
            return glm::vec3(0, 1, 0);
        }
        glm::vec3 At() const {
            return look_at;
        }
        glm::vec3 Eye() const {
            return look_at - ViewDir() * distance;
        }
        void Rotate(float delta_yaw, float delta_pitch) {
            yaw += delta_yaw;
            pitch += delta_pitch;
            pitch = glm::clamp(pitch, -glm::pi<float>() * 0.498f, glm::pi<float>() * 0.498f);
        }
        void Zoom(float scale) {
            distance *= scale;
        }
    };

    class UICamera : public CameraBase {
    public:
        UICamera(const DevicePtr& device);
        void UpdateFromWnd();
    };

    class TexDataIntf {
    public:
        virtual TextureFmt Fmt() const = 0;
        virtual const void* Data() const = 0;
        virtual glm::ivec2 Size() const = 0;
        virtual const void* Pixel(int x, int y) const = 0;
    };
    class TexManagerIntf {
    public:
        virtual TexDataIntf* Load(const std::filesystem::path& filename, bool premultiply = true) = 0;
    };
    TexManagerIntf* TM();

    class MemRangeIntf {
    public:
        virtual int Offset() const = 0;
        virtual int Size() const = 0;
        virtual glm::ivec2 OffsetSize() const = 0;
        virtual ~MemRangeIntf() {};
    };
    using MemRangeIntfPtr = std::unique_ptr<MemRangeIntf>;
    class RangeManagerIntf {
    public:
        virtual MemRangeIntfPtr Alloc(int size) = 0;
        virtual int Size() const = 0;
        virtual int Allocated() const = 0;
        virtual int FreeSpace() const = 0;

        virtual void AddSpace(int new_space) = 0;
        //virtual void Defrag() = 0;
    };
    using RangeManagerIntfPtr = std::unique_ptr<RangeManagerIntf>;
    RangeManagerIntfPtr Create_RangeManager(int size);

    class ManagedSBO;

    class ManagedSBO_Range {
        friend class ManagedSBO;
    private:
        ManagedSBO* m_sbo;
        uint32_t m_idx;
        MemRangeIntfPtr m_range;
        std::vector<char> m_data;
        void UpdateSBOData();
    public:
        int Offset() const;
        int Size() const;
        void SetData(const void* data);
        ManagedSBO_Range(ManagedSBO* owner, MemRangeIntfPtr range);
        ~ManagedSBO_Range();
    };
    using ManagedSBO_RangePtr = std::unique_ptr<ManagedSBO_Range>;

    class ManagedSBO {
        friend class ManagedSBO_Range;
    private:
        RangeManagerIntfPtr m_man;
        StructuredBufferPtr m_buffer;
        std::vector<ManagedSBO_Range*> m_ranges;
        bool m_buffer_valid;
        void ValidateBuffer();
    public:
        ManagedSBO_RangePtr Alloc(int vertex_count);
        StructuredBufferPtr Buffer();
        ManagedSBO(const DevicePtr& dev, int stride_size);
    };

    class ManagedTexSlices {
    private:
        RangeManagerIntfPtr m_man;
        Texture2DPtr m_tex;
    public:
        MemRangeIntfPtr Alloc(int slices_count, bool* tex_reallocated);
        Texture2DPtr Texture();
        ManagedTexSlices(const DevicePtr& dev, TextureFmt fmt, const glm::ivec2& tex_size);
    };

    class QPC {
    private:
        uint64_t m_start;
        uint64_t m_freq;
        bool m_paused = false;
        uint64_t m_paused_time;
    public:
        uint64_t TimeMcS() const;

        uint64_t Time() const;
        bool Paused() const;
        void Pause();
        void Unpause();
        QPC();
    };

    class StepTimer {
    private:
        uint64_t m_last_time;
        int m_step_interval;
        QPC m_qpc;
    public:
        void Pause();
        void Unpause();
        int StepInterval() const;
        void Do(const std::function<void(int step_count)>& process);
        StepTimer(int step_interval);
    };

    struct OctreeNode {
        glm::AABB box;
        std::unique_ptr<OctreeNode> childs[8];
        std::vector<int> triangles;
        OctreeNode(glm::AABB box) : box(box) {}
    };

    class Octree {
    private:
        int m_max_triangles_to_split;
        std::vector<glm::vec3> m_triangles;
        std::unique_ptr<OctreeNode> m_root;
        void SplitBox(const glm::AABB& box, glm::AABB* childs);
        bool Intersect(const glm::AABB& box, int tri_idx);
        void SplitRecursive(OctreeNode* node);
        void RayCastRecursive(OctreeNode* node, const glm::vec3& ray_start, const glm::vec3& ray_end, float* t, glm::vec3* normal);
        void EnumRecursive(OctreeNode* node, const std::function<void(const OctreeNode*, bool& enum_child)>& cb) const;
    public:
        const std::vector<glm::vec3>& Triangles() const;

        float RayCast(const glm::vec3& ray_start, const glm::vec3& ray_end);
        bool RayCast(const glm::vec3& ray_start, const glm::vec3& ray_end, float* t, glm::vec3* normal);

        void EnumNodes(const glm::vec3& pos, float rad, const std::function<void(const OctreeNode*)>& cb) const;
        void EnumNodes(const glm::AABB& box, const std::function<void(const OctreeNode*)>& cb) const;

        Octree(std::vector<glm::vec3> triangles, int max_triangles_to_split);
    };

    struct BVHNode {
        glm::AABB bounds;
        std::unique_ptr<BVHNode> left;
        std::unique_ptr<BVHNode> right;
        std::vector<uint32_t> triangleIndices;

        BVHNode(std::vector<uint32_t>&& inputTriangleIndices, const std::vector<glm::vec3>& vertices, const std::vector<uint32_t>& indices, int depth = 0) {
            if (inputTriangleIndices.size() <= 2 || depth > 20) {
                triangleIndices = std::move(inputTriangleIndices);
                bounds = ComputeAABB(triangleIndices, indices, vertices);
                return;
            }

            bounds = ComputeAABB(inputTriangleIndices, indices, vertices);

            std::vector<glm::vec3> centroids;
            centroids.reserve(inputTriangleIndices.size());
            for (uint32_t triIndex : inputTriangleIndices) {
                centroids.push_back(ComputeCentroid(triIndex, indices, vertices));
            }

            int axis = depth % 3;
            auto comparator = [axis, &centroids](uint32_t a, uint32_t b) {
                return centroids[a][axis] < centroids[b][axis];
            };

            std::sort(inputTriangleIndices.begin(), inputTriangleIndices.end(), comparator);

            size_t mid = inputTriangleIndices.size() / 2;
            std::vector<uint32_t> leftIndices(inputTriangleIndices.begin(), inputTriangleIndices.begin() + mid);
            std::vector<uint32_t> rightIndices(inputTriangleIndices.begin() + mid, inputTriangleIndices.end());

            left = std::make_unique<BVHNode>(std::move(leftIndices), vertices, indices, depth + 1);
            right = std::make_unique<BVHNode>(std::move(rightIndices), vertices, indices, depth + 1);
        }

        glm::AABB ComputeAABB(const std::vector<uint32_t>& triangleIndices, const std::vector<uint32_t>& indices, const std::vector<glm::vec3>& vertices) {
            glm::AABB aabb;
            for (uint32_t triIndex : triangleIndices) {
                aabb += vertices[indices[triIndex * 3]];
                aabb += vertices[indices[triIndex * 3 + 1]];
                aabb += vertices[indices[triIndex * 3 + 2]];
            }
            return aabb;
        }

        glm::vec3 ComputeCentroid(uint32_t triIndex, const std::vector<uint32_t>& indices, const std::vector<glm::vec3>& vertices) {
            const glm::vec3& v0 = vertices[indices[triIndex * 3]];
            const glm::vec3& v1 = vertices[indices[triIndex * 3 + 1]];
            const glm::vec3& v2 = vertices[indices[triIndex * 3 + 2]];
            return (v0 + v1 + v2) / 3.0f;
        }
    };

    struct File {
    private:
        std::filesystem::path m_path;
        FILE* m_f;
    public:
        std::filesystem::path Path() {
            return m_path;
        }
        bool Good() {
            return m_f;
        }
        File(const std::filesystem::path& filename, bool write = false) {
            m_path = std::filesystem::absolute(filename);
#ifdef _WIN32
            _wfopen_s(&m_f, m_path.wstring().c_str(), write ? L"wb" : L"rb");
#else
            fopen_s(&m_f, m_path.string().c_str(), write ? L"wb" : L"rb");
#endif
        }
        ~File() {
            if (m_f)
                fclose(m_f);
        }
        inline void ReadBuf(void* v, int size) {
            fread(v, size, 1, m_f);
        }
        inline void WriteBuf(const void* v, int size) {
            fwrite(v, size, 1, m_f);
        }
        template <typename T>
        inline T& Read(T& x) {
            ReadBuf(&x, sizeof(x));
            return x;
        }
        template <typename T>
        inline void Write(const T& x) {
            WriteBuf(&x, sizeof(x));
        }
        inline std::string ReadString() {
            uint32_t n;
            Read(n);
            std::string res;
            if (n) {
                res.resize(n);
                ReadBuf(const_cast<char*>(res.data()), n);
            }
            return res;
        }
        inline void WriteString(const std::string& s) {
            uint32_t n = uint32_t(s.size());
            Write(n);
            if (n) {
                WriteBuf(s.data(), n);
            }
        }
        inline int Tell() {
            return ftell(m_f);
        }
    };

    std::wstring UTF8ToWString(const std::string& utf8);
    std::string WStringToUTF8(const std::wstring& wstr);
}