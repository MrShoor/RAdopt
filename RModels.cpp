#include "pch.h"
#include "RModels.h"
#include <fstream>
#include <cassert>

namespace RA {
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
        File(const fs::path& filename) {
            m_path = std::filesystem::absolute(filename);
#ifdef _WIN32
        _wfopen_s(&m_f, m_path.wstring().c_str(), L"rb");
#else
        fopen_s(&m_f, m_path.string().c_str(), "rb");
#endif
        }
        ~File() {
            if (m_f)
                fclose(m_f);
        }
        void ReadBuf(void* v, int size) {
            fread(v, size, 1, m_f);
        }
        template <typename T>
        inline T& Read(T& x) { 
            ReadBuf(&x, sizeof(x));
            return x; 
        }
        std::string ReadString() {
            uint32_t n;
            Read(n);
            std::string res;
            if (n) {
                res.resize(n);
                ReadBuf(const_cast<char*>(res.data()), n);
            }
            return res;
        }
        int Tell() {
            return ftell(m_f);
        }
    };

    std::unique_ptr<Bone> LoadAVM_Bone(File& f, std::string& parent_name)
    {
        std::unique_ptr<Bone> bone = std::make_unique<Bone>();
        bone->name = f.ReadString();
        parent_name = f.ReadString();
        f.Read(bone->idx);
        f.Read(bone->transform);
        f.Read(bone->head);
        f.Read(bone->tail);
        bone->parent = nullptr;
        return bone;
    }

    std::unique_ptr<Anim> LoadAVM_Animation(File& f)
    {
        std::unique_ptr<Anim> anim = std::make_unique<Anim>();
        anim->name = f.ReadString();

        int32_t affected_bones_count;
        f.Read(affected_bones_count);
        anim->bone_mapping.resize(affected_bones_count);
        for (int i = 0; i < affected_bones_count; i++) {
            f.Read(anim->bone_mapping[i]);
        }

        f.Read(anim->frame_start);
        f.Read(anim->frame_end);
        int frame_count = anim->frame_end - anim->frame_start;
        assert(frame_count >= 0);
        anim->bone_transform.resize(frame_count);
        for (int i = 0; i < anim->frame_end - anim->frame_start; i++) {
            anim->bone_transform[i].resize(affected_bones_count);
            for (int j = 0; j < affected_bones_count; j++) {
                f.Read(anim->bone_transform[i][j]);
            }
        }

        return anim;
    }

    ArmaturePtr LoadAVM_Armature(File& f)
    {
        ArmaturePtr arm = std::make_shared<Armature>();
        arm->name = f.ReadString();
        f.Read(arm->transform);

        //load bones
        int32_t bones_count;
        f.Read(bones_count);
        std::vector<std::string> bone_parents;
        arm->bones.reserve(bones_count);
        for (int i = 0; i < bones_count; i++) {
            std::string parent;
            arm->bones.push_back(LoadAVM_Bone(f, parent));
            bone_parents.push_back(parent);
        }
        //assign parents
        for (int i = 0; i < bones_count; i++) {
            if (bone_parents[i].size() == 0) continue;
            for (int j = 0; j < bones_count; j++) {
                if (j == i) continue;
                if (arm->bones[j]->name == bone_parents[i]) {
                    arm->bones[i]->parent = arm->bones[j].get();
                    break;
                }
            }
        }

        //load animations
        int32_t anim_count;
        f.Read(anim_count);
        arm->anims.reserve(anim_count);
        for (int i = 0; i < anim_count; i++) {
            arm->anims.push_back(LoadAVM_Animation(f));
        }

        return arm;
    }

    Material LoadAVM_Material(File& f)
    {
        Material m;
        char valid_material;
        f.Read(valid_material);
        if (!valid_material) return m;

        f.Read(m.albedo);
        f.Read(m.metallic);
        f.Read(m.roughness);
        f.Read(m.emission);
        f.Read(m.emission_strength);
        f.Read(m.albedo.w);

        fs::path p = f.Path().parent_path().string() + "\\";
        m.albedo_map = f.ReadString();
        if (m.albedo_map != "") m.albedo_map = p.u8string() + m.albedo_map.u8string();
        m.metallic_map = f.ReadString();
        if (m.metallic_map != "") m.metallic_map = p.u8string() + m.metallic_map.u8string();
        m.roughness_map = f.ReadString();
        if (m.roughness_map != "") m.roughness_map = p.u8string() + m.roughness_map.u8string();
        m.emission_map = f.ReadString();
        if (m.emission_map != "") m.emission_map = p.u8string() + m.emission_map.u8string();
        m.normal_map = f.ReadString();
        if (m.normal_map != "") m.normal_map = p.u8string() + m.normal_map.u8string();

        return m;
    }

    MeshPtr LoadAVM_Mesh(File& f)
    {
        MeshPtr m = std::make_shared<Mesh>();
        m->name = f.ReadString();
        m->bbox.SetEmpty();

        int32_t mat_count;
        f.Read(mat_count);
        m->materials.resize(mat_count);
        for (int i = 0; i < mat_count; i++)
            m->materials[i] = LoadAVM_Material(f);

        int32_t vertgroups_count;
        f.Read(vertgroups_count);
        m->vgroups.resize(vertgroups_count);
        for (int i = 0; i < vertgroups_count; i++) {
            m->vgroups[i] = f.ReadString();
        }

        std::vector<MeshVertex> vcoord;
        int32_t vert_count;
        f.Read(vert_count);
        for (int i = 0; i < vert_count; i++) {
            MeshVertex vert;
            f.Read(vert.coord);
            f.Read(vert.norm);
            m->bbox += vert.coord;
            vert.bone_idx = { 0, 0, 0, 0, };
            vert.bone_weight = { 0, 0, 0, 0, };
            int32_t vg_count;
            f.Read(vg_count);
            for (int j = 0; j < vg_count; j++) {
                int32_t tmp;
                f.Read(tmp); 
                vert.bone_idx[j] = float(tmp);
                f.Read(vert.bone_weight[j]);
            }
            vcoord.push_back(vert);
        }

        std::vector<MeshVertex>& vfull = m->vertices;
        std::vector<int>& ifull = m->indices;
        std::unordered_map<MeshVertex, int, MeshVertex> vmap;

        int32_t face_count;
        f.Read(face_count);
        for (int i = 0; i < face_count; i++) {
            int32_t mat_idx;
            f.Read(mat_idx);
            char smooth;
            f.Read(smooth);
            glm::vec3 face_norm;
            f.Read(face_norm);
            for (int j = 0; j < 3; j++) {
                int32_t vert_idx;
                f.Read(vert_idx);
                glm::vec2 uv;
                f.Read(uv);
                MeshVertex vert = vcoord[vert_idx];
                vert.mat_idx = float(mat_idx);
                if (!smooth) vert.norm = face_norm;
                vert.uv = uv;

                auto it = vmap.find(vert);
                if (it == vmap.end()) {
                    int new_idx = int(vfull.size());
                    vfull.push_back(vert);
                    ifull.push_back(new_idx);
                }
                else {
                    ifull.push_back(it->second);
                }   
            }
        }

        return m;
    }

    MeshInstancePtr LoadAVM_Instance(File& f, const std::vector<MeshPtr>& meshes, std::string* parent_name) {
        std::string inst_name = f.ReadString();
        *parent_name = f.ReadString();
        
        glm::mat4 inst_transform;
        f.Read(inst_transform);

        std::string mesh_name = f.ReadString();
        MeshPtr inst_mesh;
        for (const auto& m : meshes) {
            if (m->name == mesh_name) {
                inst_mesh = m;
                break;
            }
        }

        return std::make_shared<MeshInstance>(inst_mesh, inst_name, inst_transform);
    }

    void LoadAVM(const fs::path& filename, std::vector<MeshPtr>& meshes, std::vector<MeshInstancePtr>& instances, std::vector< ArmaturePtr>& armatures)
    {
        File f(filename);
        if (!f.Good()) throw std::runtime_error(std::string("can't open file: ") + filename.string());

        int32_t armatures_count;
        f.Read(armatures_count);
        std::vector<ArmaturePosePtr> m_poses;
        for (int i = 0; i < armatures_count; i++) {
            armatures.push_back( LoadAVM_Armature(f) );
            m_poses.push_back(std::make_shared<ArmaturePose>(armatures[armatures.size()-1]));
        }

        int32_t meshes_count;
        f.Read(meshes_count);
        for (int i = 0; i < meshes_count; i++) {
            meshes.push_back( LoadAVM_Mesh(f) );
        }

        int32_t instances_count;
        f.Read(instances_count);
        for (int i = 0; i < instances_count; i++) {
            std::string parent;
            instances.push_back( LoadAVM_Instance(f, meshes, &parent) );
            for (size_t j = 0; j < armatures.size(); j++) {
                if (armatures[j]->name == parent) {
                    instances[instances.size() - 1]->BindPose(m_poses[j]);
                    break;
                }
            }
        }
    }
    const Layout* MeshVertex::Layout()
    {
        return LB()
            ->Add("coord", LayoutType::Float, 3)
            ->Add("norm", LayoutType::Float, 3)
            ->Add("bone_idx", LayoutType::Float, 4)
            ->Add("bone_weight", LayoutType::Float, 4)
            ->Add("mat_idx", LayoutType::Float, 1)
            ->Add("uv", LayoutType::Float, 2)
            ->Finish(sizeof(MeshVertex));
    }
    void Anim::EvalFrame(float frame_pos, glm::mat4* transforms)
    {
        float frameK = glm::fract(frame_pos);
        int frame = int(frame_pos) % (frame_end - frame_start);
        if (frame < 0) frame += (frame_end - frame_start);
        for (size_t i = 0; i < bone_transform[frame].size(); i++) {
            transforms[i] = bone_transform[frame][i] * (1.0f - frameK);
        }
        frame = (frame + 1) % (frame_end - frame_start);
        for (size_t i = 0; i < bone_transform[frame].size(); i++) {
            transforms[i] += bone_transform[frame][i] * frameK;
        }
    }
    MeshPtr MeshInstance::Mesh()
    {
        return m_mesh;
    }
    const std::string& MeshInstance::Name() const
    {
        return m_name;
    }
    void MeshInstance::BindPose(const ArmaturePosePtr& pose)
    {
        m_pose = pose;
        for (size_t i = 0; i < m_group_to_bone_remap.size(); i++) {
            m_group_to_bone_remap[i] = pose->Armature()->FindBoneIdx(m_mesh->vgroups[i].c_str());
        }
        m_bind_transform = glm::inverse(pose->Armature()->transform);
    }
    ArmaturePose* MeshInstance::Pose()
    {
        return m_pose.get();
    }
    MeshInstance::MeshInstance(const MeshPtr& mesh, const std::string& name, const glm::mat4& transform)
    {
        m_mesh = mesh;
        m_name = name;
        m_transform = transform;
        m_group_to_bone_remap.resize(mesh->vgroups.size());
        for (int& g : m_group_to_bone_remap) g = -1;
        m_bind_transform = glm::mat4(1.0f);
    }
    MeshInstance::MeshInstance(const MeshInstance& inst, bool copy_armature_pose)
    {
        if (copy_armature_pose) {
            m_pose = inst.m_pose;
            m_group_to_bone_remap = inst.m_group_to_bone_remap;
        }
        else {
            m_pose = nullptr;
            m_group_to_bone_remap = inst.m_group_to_bone_remap;
            for (int& g : m_group_to_bone_remap) g = -1;
        }
        m_transform = inst.m_transform;
        m_name = inst.m_name;
        m_mesh = inst.m_mesh;
        m_bind_transform = inst.m_bind_transform;
    }
    const glm::mat4& MeshInstance::GetBindTransform() const
    {
        return m_bind_transform;
    }
    const glm::mat4& MeshInstance::GetTransform() const
    {
        return m_transform;
    }
    void MeshInstance::SetTransform(const glm::mat4 m)
    {
        m_transform = m;
    }
    const std::vector<int32_t>& MeshInstance::BoneMapping() const
    {
        return m_group_to_bone_remap;
    }
    glm::AABB MeshInstance::BBox()
    {
        if (m_pose) {
            return m_transform * m_pose->Armature()->transform * m_mesh->bbox;
        }
        return m_transform * m_mesh->bbox;
    }
    int Armature::FindBoneIdx(const char* bone_name)
    {
        for (size_t i = 0; i < bones.size(); i++) {
            if (bones[i]->name == bone_name) return int(i);
        }
        return -1;
    }
    int Armature::FindAnimIdx(const char* anim_name)
    {
        for (size_t i = 0; i < anims.size(); i++) {
            if (anims[i]->name == anim_name) return int(i);
        }
        return -1;
    }
    void ArmaturePose::UpdateAbsTransform(int idx)
    {
        glm::mat4 parent_abs;
        if (m_arm->bones[idx]->parent) {
            parent_abs = m_abs_pose[m_arm->bones[idx]->parent->idx];
            if (parent_abs[3][3] == 0) {
                UpdateAbsTransform(m_arm->bones[idx]->parent->idx);
                parent_abs = m_abs_pose[m_arm->bones[idx]->parent->idx];
            }
        }
        else {
            parent_abs = m_pose_transform * m_arm->transform;// glm::mat4(1.0);
        }
        m_abs_pose[idx] = parent_abs * m_local_pose[idx];
    }
    void ArmaturePose::UpdateAbsTransform()
    {
        for (glm::mat4& m : m_abs_pose) m = glm::mat4(0.0);
        for (size_t i = 0; i < m_abs_pose.size(); i++) 
            UpdateAbsTransform(int(i));
    }
    glm::mat4 ArmaturePose::GetTransform()
    {
        return m_pose_transform;
    }
    void ArmaturePose::SetTransform(const glm::mat4 m)
    {
        m_pose_transform = m;
        UpdateAbsTransform();
    }
    Armature* ArmaturePose::Armature()
    {
        return m_arm.get();
    }
    void ArmaturePose::SetPose(int anim_idx, float frame_pos)
    {
        AnimState anim;
        anim.anim_idx = anim_idx;
        anim.frame = frame_pos;
        anim.weight = 1.0;
        SetPose(std::vector<AnimState>({ anim }));
    }
    void ArmaturePose::SetPose(const std::vector<AnimState>& anims)
    {
        for (glm::mat4& m : m_local_pose) m = glm::mat4(0.0);

        for (const AnimState& astate : anims) {
            int anim_idx = astate.anim_idx;
            if (anim_idx < 0) continue;
            if (anim_idx >= int(m_arm->anims.size())) continue;
            std::unique_ptr<Anim>& a = m_arm->anims[anim_idx];
            a->EvalFrame(astate.frame, m_anim_pose.data());

            for (size_t i = 0; i < a->bone_mapping.size(); i++) {
                m_local_pose[a->bone_mapping[i]] += m_anim_pose[i]*astate.weight;
            }
        }
        for (glm::mat4& m : m_local_pose) {
            if (m[3][3])
                m /= m[3][3];
            else
                m = glm::mat4(1.0);
        }
        UpdateAbsTransform();
    }
    const std::vector<glm::mat4>& ArmaturePose::GetAbsTransform()
    {
        return m_abs_pose;
    }
    ArmaturePose::ArmaturePose(const ArmaturePose& pose)
    {
        m_arm = pose.m_arm;
        m_anim_pose = pose.m_anim_pose;
        m_local_pose = pose.m_local_pose;
        m_abs_pose = pose.m_abs_pose;
        m_pose_transform = pose.m_pose_transform;
    }
    ArmaturePose::ArmaturePose(const ArmaturePtr& armature)
    {
        m_arm = armature;
        m_anim_pose.resize(m_arm->bones.size());
        m_local_pose.resize(m_arm->bones.size());
        for (glm::mat4& m : m_local_pose) m = glm::mat4(1.0);
        m_abs_pose.resize(m_arm->bones.size());
        for (glm::mat4& m : m_abs_pose) m = m_arm->transform;
        m_pose_transform = glm::mat4(1);
    }
    void AnimController::SetSingleAnimationWithDuration(int anim_idx, AnimDirection direction, uint64_t duration, uint64_t fade_in_selected, uint64_t fade_out_rest)
    {
        if (anim_idx < 0) return;
        for (const AnimState& as : m_state) {
            if (as.anim_idx != anim_idx) {
                Stop(as.anim_idx, fade_out_rest);
            }
        }
        Start(anim_idx, direction, duration, fade_in_selected);
    }
    void AnimController::Start(int anim_idx, AnimDirection direction, uint64_t anim_duration, uint64_t fade_in)
    {
        if (anim_idx < 0) return;

        uint64_t real_duration = GetAnimationDuration(anim_idx, IsLooped(direction));
        float speed_factor = float(real_duration) / anim_duration;
        if ((direction == AnimDirection::Backward) || (direction == AnimDirection::BackwardLoop))
            speed_factor = -speed_factor;

        uint64_t stop_time = 0xffffffffffffffff;
        if (!IsLooped(direction)) {
            stop_time = m_current_time + uint64_t(real_duration / speed_factor);
        }

        float fade_in_w = (fade_in >= 1.0f) ? (1.0f / fade_in) : 1.0f;
        for (int i = 0; i < int(m_state.size()); i++) {
            if (m_state[i].anim_idx == anim_idx) {
                if (!IsLooped(direction)) {
                    m_state[i].frame = 0;
                    m_times[i].start = m_current_time;
                }
                m_times[i].direction = direction;
                m_times[i].speed_factor = speed_factor;
                m_times[i].fade_in_w = fade_in_w;
                m_times[i].stop = stop_time;
                m_times[i].fade_out_w = 1.0;
                return;
            }
        }
        AnimState as;
        as.frame = 0;
        as.anim_idx = anim_idx;
        as.weight = 0;
        AnimTimes at;
        at.direction = direction;
        at.speed_factor = speed_factor;
        at.start = m_current_time;
        at.fade_in_w = fade_in_w;
        at.stop = stop_time;
        at.fade_out_w = 1.0;
        m_state.push_back(as);
        m_times.push_back(at);
    }
    void AnimController::Stop(int anim_idx, uint64_t fade_out)
    {
        if (anim_idx < 0) return;
        float fade_out_w = (fade_out >= 1.0f) ? (1.0f / fade_out) : 1.0f;
        for (int i = 0; i < int(m_state.size()); i++) {
            if (m_state[i].anim_idx == anim_idx) {
                m_times[i].stop = m_current_time;
                m_times[i].fade_out_w = fade_out_w;
                return;
            }
        }
    }
    bool AnimController::IsLooped(AnimDirection direction) const
    {
        return (direction == AnimDirection::BackwardLoop) || (direction == AnimDirection::ForwardLoop);
    }
    uint64_t AnimController::GetAnimationDuration(int anim_idx, bool looped) const
    {
        int num_frames = m_pose->Armature()->anims[anim_idx]->frame_end - m_pose->Armature()->anims[anim_idx]->frame_start;
        if (!looped) num_frames--;
        float duration = float(num_frames * 1000) / cAnimFramesPerSecond;
        return uint64_t(glm::round(duration));
    }
    ArmaturePosePtr AnimController::GetPose()
    {
        return m_pose;
    }
    void AnimController::SetPose(const ArmaturePosePtr& pose)
    {
        m_pose = pose;
    }
    const std::vector<AnimState>& AnimController::State() const
    {
        if (m_state.size()) {
            return m_state;            
        }
        else {
            return m_last_frame_state;
        }
    }
    bool AnimController::IsInState(const char* anim_name) const
    {
        if (!m_pose) return false;
        int anim_idx = m_pose->Armature()->FindAnimIdx(anim_name);
        if (anim_idx < 0) return false;
        for (const auto& astate : m_state) {
            if (astate.anim_idx == anim_idx) return true;
        }
        if (m_state.size() == 0) {
            for (const auto& astate : m_last_frame_state) {
                if (astate.anim_idx == anim_idx) return true;
            }
        }
        return false;
    }
    uint64_t AnimController::GetAnimationDuration(const char* anim_name, bool looped) const
    {
        if (!m_pose) return 0;
        int anim_idx = m_pose->Armature()->FindAnimIdx(anim_name);
        if (anim_idx < 0) return 0;
        return GetAnimationDuration(anim_idx, looped);
    }
    void AnimController::Start(const char* anim_name, AnimDirection direction, uint64_t fade_in)
    {
        if (!m_pose) return;
        int anim_idx = m_pose->Armature()->FindAnimIdx(anim_name);
        Start(anim_idx, direction, GetAnimationDuration(anim_idx, IsLooped(direction)), fade_in);
    }
    void AnimController::Stop(const char* anim_name, uint64_t fade_out)
    {
        if (!m_pose) return;
        Stop(m_pose->Armature()->FindAnimIdx(anim_name), fade_out);
    }
    void AnimController::SetSingleAnimation(const char* anim_name, AnimDirection direction, uint64_t fade_in_selected, uint64_t fade_out_rest)
    {
        if (!m_pose) return;
        int anim_idx = m_pose->Armature()->FindAnimIdx(anim_name);
        if (anim_idx < 0) return;
        SetSingleAnimationWithDuration(anim_idx, direction, GetAnimationDuration(anim_idx, IsLooped(direction)), fade_in_selected, fade_out_rest);
    }
    void AnimController::SetSingleAnimationWithDuration(const char* anim_name, AnimDirection direction, uint64_t anim_duration, uint64_t fade_in_selected, uint64_t fade_out_rest)
    {
        if (!m_pose) return;
        SetSingleAnimationWithDuration(m_pose->Armature()->FindAnimIdx(anim_name), direction, anim_duration, fade_in_selected, fade_out_rest);
    }
    void AnimController::SetTime(uint64_t time)
    {
        uint64_t dt = time - m_current_time;
        if (dt <= 0) return;
        float fd = (dt / 1000.0f) * cAnimFramesPerSecond;
        for (int i = int(m_state.size() - 1); i >= 0; i--) {
            m_state[i].frame += fd * m_times[i].speed_factor;
            if ((m_times[i].direction == AnimDirection::Forward) || (m_times[i].direction == AnimDirection::Backward)) {
                Anim* a = m_pose->Armature()->anims[m_state[i].anim_idx].get();
                m_state[i].frame = glm::clamp(m_state[i].frame, 0.0f, float(a->frame_end - a->frame_start - 1.0f));
            }
            float dw = 0;
            if (time >= m_times[i].stop) {
                uint64_t dt2 = m_current_time - m_times[i].stop;
                dw = - ((dt + ((dt2 < 0) ? 0 : dt2)) * m_times[i].fade_out_w);
            }
            else {
                if (time > m_times[i].start) {
                    uint64_t dt2 = m_current_time - m_times[i].start;
                    dw = (dt + ((dt2 < 0) ? 0 : dt2)) * m_times[i].fade_in_w;
                }
            }
            m_last_frame_times.stop = 0;
            m_state[i].weight = glm::clamp(m_state[i].weight + dw, 0.0f, 1.0f);

            if ((m_state[i].weight == 0)&&(dw < 0)) {
                m_state[i] = m_state[m_state.size() - 1];
                m_times[i] = m_times[m_times.size() - 1];
                AnimState st = m_state.back();
                if (m_times[i].stop > m_last_frame_times.stop) {
                    m_last_frame_times = m_times[i];
                    m_last_frame_state.resize(1);
                    m_last_frame_state[0] = m_state[i];
                    m_last_frame_state[0].weight = 1.0f;
                }
                m_state.pop_back();
                m_times.pop_back();
            }
        }
        m_current_time = time;
    }
}