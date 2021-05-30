#pragma once
#include "RAdopt.h"
#include "GLM.h"
#include "GLMUtils.h"
#include <filesystem>

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

        void SetAspect(HWND wnd);
        void SetNearFar(const glm::vec2& near_far);

        void FitView(const glm::vec3* points, int num_points);
        void FitView(const glm::AABB& bbox);

        glm::vec3 Eye() const;
        glm::vec3 At() const;
        glm::vec3 ViewDir() const;        
    };

    class UICamera : public CameraBase {
    public:
        UICamera(const DevicePtr& device);
        void UpdateFromWnd();
    };

    class TexDataIntf {
    public:
        virtual TextureFmt Fmt() = 0;
        virtual const void* Data() = 0;
        virtual glm::ivec2 Size() = 0;
    };
    class TexManagerIntf {
    public:
        virtual TexDataIntf* Load(const std::filesystem::path& filename) = 0;
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
    public:
        uint64_t Time();
        QPC();
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
    public:
        float RayCast(const glm::vec3& ray_start, const glm::vec3& ray_end);
        bool RayCast(const glm::vec3& ray_start, const glm::vec3& ray_end, float* t, glm::vec3* normal);
        Octree(std::vector<glm::vec3> triangles, int max_triangles_to_split);
    };
}