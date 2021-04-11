#pragma once
#include "RAdopt.h"
#include "GLM.h"
#include "GLMUtils.h"

namespace RA {
    enum class FrustumPlane {Top, Bottom, Right, Left, Near, Far};

    class Camera {
    private:
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
            CameraBuf() {
                view = glm::mat4(1);
                proj = glm::mat4(1);
                view_proj = glm::mat4(1);
                view_inv = glm::mat4(1);
                proj_inv = glm::mat4(1);
                view_proj_inv = glm::mat4(1);
                z_near_far = glm::vec2(0.1, 100.0);
            }
        };
    private:
        CameraBuf m_buf;
        UniformBufferPtr m_ubo;

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
        glm::mat4 View() const;
        glm::mat4 ViewProj() const;
        glm::mat4 ViewInv() const;
        glm::mat4 ViewProjInv() const;

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

        UniformBufferPtr GetUBO();
    };

    class TexDataIntf {
    public:
        virtual TextureFmt Fmt() = 0;
        virtual const void* Data() = 0;
        virtual glm::ivec2 Size() = 0;
    };
    class TexManagerIntf {
    public:
        virtual TexDataIntf* Load(const char* filename) = 0;
    };
    TexManagerIntf* TM();


    class MemRangeIntf {
    public:
        virtual int Offset() const = 0;
        virtual int Size() const = 0;
        virtual glm::ivec2 OffsetSize() const = 0;
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
        void RayCastRecursive(OctreeNode* node, const glm::vec3& ray_start, const glm::vec3& ray_end, float* t);
    public:
        float RayCast(const glm::vec3& ray_start, const glm::vec3& ray_end);
        Octree(std::vector<glm::vec3> triangles, int max_triangles_to_split);
    };
}