#pragma once
#include <memory>
#include <vector>
#include <string>
#include <filesystem>
#include "GLM.h"
#include "RAdopt.h"
#include "GLMUtils.h"

namespace RA {
    namespace fs = std::filesystem;
    struct path_hasher {
        std::size_t operator()(const fs::path& path) const {
            return fs::hash_value(path);
        }
    };

    struct Material {
        glm::vec4 albedo = {0,0,0,0};
        float metallic = 0;
        float roughness = 0;
        glm::vec4 emission = {0,0,0,0};
        float emission_strength = 0;

        fs::path albedo_map;
        fs::path metallic_map;
        fs::path roughness_map;
        fs::path emission_map;
        fs::path normal_map;
    };

    struct MeshVertex {
        glm::vec3 coord = {0,0,0};
        glm::vec3 norm = {0,0,0};
        glm::vec4 bone_idx = {0,0,0,0};
        glm::vec4 bone_weight = {0,0,0,0};
        float mat_idx = 0;
        glm::vec2 uv = {0,0};
        const static Layout* Layout();

        bool operator==(const MeshVertex& p) const {
            return memcmp(this, &p, sizeof(MeshVertex)) == 0;
        }
        std::size_t operator() (const MeshVertex& s) const
        {
            return MurmurHash2(this, sizeof(MeshVertex));
        }
    };

    struct Mesh;
    struct Armature;
    class MeshInstance;
    class ArmaturePose;
    using MeshPtr = std::shared_ptr<Mesh>;    
    using ArmaturePtr = std::shared_ptr<Armature>;
    using MeshInstancePtr = std::shared_ptr<MeshInstance>;
    using ArmaturePosePtr = std::shared_ptr<ArmaturePose>;

    struct Bone {
        const Bone* parent = nullptr;
        std::string name;
        glm::mat4 transform = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
        glm::vec3 head = {0,0,0};
        glm::vec3 tail = {0,0,0};
        int32_t idx = 0;
    };

    struct Anim {
        std::string name;
        std::vector<int32_t> bone_mapping;
        int32_t frame_start;
        int32_t frame_end;
        std::vector<std::vector<glm::mat4>> bone_transform;

        void EvalFrame(float frame_pos, glm::mat4* transforms);
    };

    struct AnimState {
        int anim_idx;
        float frame;
        float weight;
    };

    class ArmaturePose {
    private:
        ArmaturePtr m_arm;

        std::vector<glm::mat4> m_anim_pose;

        std::vector<glm::mat4> m_local_pose;
        std::vector<glm::mat4> m_abs_pose;

        void UpdateAbsTransform(int idx);
        void UpdateAbsTransform();
    public:
        glm::mat4 GetTransform();
        void SetTransform(const glm::mat4 m);

        Armature* Armature();
        void SetPose(int anim_idx, float frame_pos);
        void SetPose(const std::vector<AnimState>& anims);
        const std::vector<glm::mat4>& GetAbsTransform();
        ArmaturePose(const ArmaturePose& pose);
        ArmaturePose(const ArmaturePtr& armature);
    };

    enum class AnimDirection {Forward, ForwardLoop, Backward, BackwardLoop};

    static const int cAnimFramesPerSecond = 30;

    class AnimController {
    private:
        struct AnimTimes {
            AnimDirection direction;
            float speed_factor;
            uint64_t start;
            float    fade_in_w;
            uint64_t stop;
            float    fade_out_w;
        };
    private:
        ArmaturePosePtr m_pose;

        uint64_t m_current_time = 0;
        std::vector<AnimState> m_state;
        std::vector<AnimTimes> m_times;

        std::vector<AnimState> m_last_frame_state;
        AnimTimes m_last_frame_times;

        void SetSingleAnimationWithDuration(int anim_idx, AnimDirection direction, uint64_t duration, uint64_t fade_in_selected = 100, uint64_t fade_out_rest = 100);
        void Start(int anim_idx, AnimDirection direction, uint64_t anim_duration, uint64_t fade_in);
        void Stop(int anim_idx, uint64_t fade_out);

        bool IsLooped(AnimDirection direction);
        uint64_t GetAnimationDuration(int anim_idx, bool looped);
    public:
        ArmaturePosePtr GetPose();
        void SetPose(const ArmaturePosePtr& pose);

        const std::vector<AnimState>& State();

        bool IsInState(const char* anim_name);

        void Start(const char* anim_name, AnimDirection direction, uint64_t fade_in);
        void Stop (const char* anim_name, uint64_t fade_out);
        void SetSingleAnimation(const char* anim_name, AnimDirection direction, uint64_t fade_in_selected = 100, uint64_t fade_out_rest = 100);
        void SetSingleAnimationWithDuration(const char* anim_name, AnimDirection direction, uint64_t anim_duration, uint64_t fade_in_selected = 100, uint64_t fade_out_rest = 100);
        void SetTime(uint64_t time);
    };

    struct Armature {
        std::string name;
        std::vector<std::unique_ptr<Bone>> bones;
        std::vector<std::unique_ptr<Anim>> anims;
        glm::mat4 transform;

        int FindBoneIdx(const char* bone_name);
        int FindAnimIdx(const char* anim_name);
    };

    struct Mesh {
        std::string name;
        std::vector<MeshVertex> vertices;
        std::vector<int32_t> indices;
        std::vector<std::string> vgroups;
        std::vector<Material> materials;
        glm::AABB bbox;
    };

    class MeshInstance {
    private:
        ArmaturePosePtr m_pose;
        std::vector<int32_t> m_group_to_bone_remap;
        glm::mat4 m_transform;
        std::string m_name;
        MeshPtr m_mesh;
    public:
        MeshInstance(const MeshPtr& mesh, const std::string& name, const glm::mat4& transform);
        MeshInstance(const MeshInstance& inst, bool copy_armature_pose);
        glm::mat4 GetTransform();
        void SetTransform(const glm::mat4 m);
        MeshPtr Mesh();
        std::string Name();
        void BindPose(const ArmaturePosePtr& pose);
        ArmaturePose* Pose();        
        const std::vector<int32_t>& BoneMapping();
        glm::AABB BBox();
    };

    void LoadAVM(const fs::path& filename, std::vector<MeshPtr>& meshes, 
                                           std::vector<MeshInstancePtr>& instances,
                                           std::vector<ArmaturePtr>& armatures);
}