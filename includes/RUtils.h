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
            }
        };
    private:
        CameraBuf m_buf;
        UniformBufferPtr m_ubo;

        glm::vec3 m_eye;
        glm::vec3 m_at;
        glm::vec3 m_up;
        
        float m_aspect;
        float m_fov;
        glm::vec2 m_near_far;
        glm::vec2 m_depth_range;
    public:
        glm::Plane GetFrustumPlane(FrustumPlane fp);

        Camera(const DevicePtr& device);

        void SetCamera(const glm::vec3& eye, const glm::vec3& at, const glm::vec3& up);
        void SetProjection(float fov, float aspect, const glm::vec2& near_far, const glm::vec2& depth_range = {1, 0});

        void SetAspect(HWND wnd);
        void SetNearFar(const glm::vec2& near_far);

        void FitView(const glm::vec3* points, int num_points);
        void FitView(const glm::AABB& bbox);

        glm::vec3 Eye();
        glm::vec3 At();
        glm::vec3 ViewDir();

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
}