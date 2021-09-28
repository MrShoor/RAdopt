#include "pch.h"
#include "RSystems.h"

namespace RA {
    MeshCollection::AVMScene::AVMScene(const fs::path& p)
    {
        filename = p;
        std::vector<MeshPtr> ms;
        std::vector<MeshInstancePtr> inst;
        std::vector<ArmaturePtr> arms;
        LoadAVM(p, ms, inst, arms);
        for (const MeshPtr& m : ms) {
            meshes.insert({ m->name, m });
        }
        for (const MeshInstancePtr& i : inst) {
            instances.insert({ i->Name(), i });
        }
        for (const ArmaturePtr& a : arms) {
            armatures.insert({ a->name, a });
        }
    }
    MeshCollection::AVMScene* MeshCollection::ObtainScene(const fs::path& filename)
    {
        fs::path p = std::filesystem::absolute(filename);
        auto it = m_cache.find(p);
        if (it == m_cache.end()) {
            m_cache.emplace(p, std::make_unique<AVMScene>(p));
            it = m_cache.find(p);
        }
        auto q = it->second.get();
        return q;
    }
    MCMeshPtr MeshCollection::ObtainMesh(const MeshPtr& mesh)
    {
        assert(mesh);
        auto it = m_meshes.find(mesh);
        if (it == m_meshes.end()) {
            MemRangeIntfPtr v_range = m_mesh_vbuf_ranges->Alloc(int(mesh->vertices.size()));
            if (!v_range) {
                m_mesh_vbuf_ranges->AddSpace(glm::max(int(mesh->vertices.size()), m_mesh_vbuf_ranges->Size() * 2));
                v_range = m_mesh_vbuf_ranges->Alloc(int(mesh->vertices.size()));
                assert(v_range);
                m_mesh_vbuf->SetState(m_mesh_vbuf->GetLayout(), m_mesh_vbuf_ranges->Size());
                for (const auto& pair : m_meshes) {
                    glm::ivec2 r = pair.second->m_vertices->OffsetSize();
                    m_mesh_vbuf->SetSubData(r.x, r.y, pair.second->m_mesh->vertices.data());
                }
            }

            MemRangeIntfPtr i_range = m_mesh_ibuf_ranges->Alloc(int(mesh->indices.size()));
            if (!i_range) {
                m_mesh_ibuf_ranges->AddSpace(glm::max(int(mesh->indices.size()), m_mesh_ibuf_ranges->Size() * 2));
                i_range = m_mesh_ibuf_ranges->Alloc(int(mesh->indices.size()));
                assert(i_range);
                m_mesh_ibuf->SetState(m_mesh_ibuf_ranges->Size());
                for (const auto& pair : m_meshes) {
                    glm::ivec2 r = pair.second->m_indices->OffsetSize();
                    m_mesh_ibuf->SetSubData(r.x, r.y, pair.second->m_mesh->indices.data());
                }
            }

            MemRangeIntfPtr mat_range = m_mesh_matbuf_ranges->Alloc(int(mesh->materials.size()));
            if (!mat_range) {
                m_mesh_matbuf_ranges->AddSpace(glm::max(int(mesh->materials.size()), m_mesh_matbuf_ranges->Size() * 2));
                mat_range = m_mesh_matbuf_ranges->Alloc(int(mesh->materials.size()));
                assert(mat_range);
                m_mesh_matbuf->SetState(sizeof(MCMeshMaterialVertex), m_mesh_matbuf_ranges->Size());
                for (const auto& pair : m_meshes) {
                    glm::ivec2 r = pair.second->m_materials->OffsetSize();
                    m_mesh_matbuf->SetSubData(r.x, r.y, pair.second->m_materials_data.data());
                }
            }

            glm::ivec2 vr = v_range->OffsetSize();
            glm::ivec2 ir = i_range->OffsetSize();
            glm::ivec2 mr = mat_range->OffsetSize();
            MCMeshPtr result = std::make_shared<MCMesh>(this, mesh, std::move(v_range), std::move(i_range), std::move(mat_range));
            m_mesh_vbuf->SetSubData(vr.x, vr.y, mesh->vertices.data());
            m_mesh_ibuf->SetSubData(ir.x, ir.y, mesh->indices.data());
            m_mesh_matbuf->SetSubData(mr.x, mr.y, result->m_materials_data.data());
            return result;
        }
        return it->second->SPtr();
    }
    void MeshCollection::PrepareBuffers(MeshCollectionBuffers* bufs, MeshCollectionDrawCommands* draw_commands)
    {
        PrepareBuffers(m_instances, bufs, draw_commands);
    }
    void MeshCollection::PrepareBuffers(const std::vector<MCMeshInstance*>& instances, MeshCollectionBuffers* bufs, MeshCollectionDrawCommands* draw_commands)
    {
        ValidateArmatures();
        FillBuffers(bufs);
        for (const auto& inst : instances) {
            draw_commands->commands[inst->GetGroupID()].push_back(GetDrawCommand(inst));
        }
    }
    void MeshCollection::PrepareBuffers(const std::vector<MCMeshInstancePtr>& instances, MeshCollectionBuffers* bufs, MeshCollectionDrawCommands* draw_commands)
    {
        ValidateArmatures();
        FillBuffers(bufs);
        for (const auto& inst : instances) {            
            draw_commands->commands[inst->GetGroupID()].push_back(GetDrawCommand(inst.get()));
        }
    }
    MCArmaturePtr MeshCollection::Create_Armature(const fs::path& filename, const std::string& armature_name)
    {
        AVMScene* scene = ObtainScene(filename);
        auto it = scene->armatures.find(armature_name);
        if (it == scene->armatures.end()) return nullptr;        
        ArmaturePosePtr pose = std::make_shared<ArmaturePose>(it->second);
        MemRangeIntfPtr range = m_bones_ranges->Alloc(int(pose->GetAbsTransform().size()));
        if (range == nullptr) {
            m_bones_ranges->AddSpace(glm::max(int(pose->GetAbsTransform().size()), m_bones_ranges->Size()*2));
            range = m_bones_ranges->Alloc(int(pose->GetAbsTransform().size()));
            assert(range);
            for (const auto& a : m_armatures) m_dirty_armatures.insert(a);
            m_bones->SetState(sizeof(glm::mat4), m_bones_ranges->Size());
        }
        return std::make_shared<MCArmature>(this, pose, std::move(range));
    }
    MCMeshInstancePtr MeshCollection::Clone_MeshInstance(AVMScene* scene, const std::string& instance_name)
    {
        auto it = scene->instances.find(instance_name);
        if (it == scene->instances.end()) return nullptr;
        if (!it->second->Mesh()) return nullptr;
        MeshInstancePtr new_inst = std::make_shared<MeshInstance>(*it->second.get(), false);
        MCMeshPtr rmesh = ObtainMesh(new_inst->Mesh());

        MemRangeIntfPtr range = m_bone_remap_ranges->Alloc(int(new_inst->BoneMapping().size()));
        if (range == nullptr) {
            m_bone_remap_ranges->AddSpace(glm::max(int(new_inst->BoneMapping().size()), m_bone_remap_ranges->Size() * 2));
            range = m_bone_remap_ranges->Alloc(int(new_inst->BoneMapping().size()));
            assert(range);
            m_bone_remap->SetState(sizeof(int32_t), m_bone_remap_ranges->Size());
            for (const auto& inst : m_instances) {
                glm::ivec2 r = inst->m_remap_range->OffsetSize();
                m_bone_remap->SetSubData(r.x, r.y, inst->m_inst->BoneMapping().data());
            }
        }
        glm::ivec2 r = range->OffsetSize();
        m_bone_remap->SetSubData(r.x, r.y, new_inst->BoneMapping().data());

        return std::make_unique<MCMeshInstance>(this, rmesh, new_inst, std::move(range));

    }
    void MeshCollection::ValidateArmatures()
    {
        for (const auto& arms : m_dirty_armatures) {
            glm::ivec2 r = arms->m_range->OffsetSize();
            m_bones->SetSubData(r.x, r.y, arms->m_pose->GetAbsTransform().data());
        }
        m_dirty_armatures.clear();
    }
    void MeshCollection::FillBuffers(MeshCollectionBuffers* bufs)
    {
        bufs->vertices = &m_mesh_vbuf;
        bufs->indices = &m_mesh_ibuf;
        bufs->materials = &m_mesh_matbuf;
        bufs->instances = &m_inst_vbuf;
        bufs->bones = &m_bones;
        bufs->bones_remap = &m_bone_remap;
    }
    DrawIndexedCmd MeshCollection::GetDrawCommand(MCMeshInstance* inst)
    {
        DrawIndexedCmd cmd;
        cmd.StartIndex = inst->m_mesh->m_indices->OffsetSize().x;
        cmd.IndexCount = inst->m_mesh->m_indices->OffsetSize().y;
        cmd.BaseVertex = inst->m_mesh->m_vertices->OffsetSize().x;
        cmd.BaseInstance = inst->m_idx;
        cmd.InstanceCount = 1;
        return cmd;
    }
    MCMeshInstancePtr MeshCollection::Clone_MeshInstance(const fs::path& filename, const std::string& instance_name)
    {
        AVMScene* scene = ObtainScene(filename);
        return Clone_MeshInstance(scene, instance_name);
    }
    std::vector<MCMeshInstancePtr> MeshCollection::Clone_MeshInstances(const fs::path& filename, const std::vector<std::string>& instances, uint32_t groupID)
    {
        std::vector<MCMeshInstancePtr> res;
        AVMScene* scene = ObtainScene(filename);
        if (!scene) return res;        
        if (instances.size()) {
            res.reserve(instances.size());
            for (const auto& name : instances) {
                res.push_back(Clone_MeshInstance(scene, name));
                res.back()->SetGroupID(groupID);
            }
        }
        else {
            res.reserve(scene->instances.size());
            for (const auto& it : scene->instances) {
                res.push_back(Clone_MeshInstance(scene, it.first));
                res.back()->SetGroupID(groupID);
            }
        }
        return res;
    }
    MeshCollection::MeshCollection(const DevicePtr& dev)
    {
        m_dev = dev;

        m_bones_ranges = Create_RangeManager(256);
        m_bones = m_dev->Create_StructuredBuffer();
        m_bones->SetState(sizeof(glm::mat4), m_bones_ranges->Size());

        m_mesh_vbuf_ranges = Create_RangeManager(65536);
        m_mesh_vbuf = m_dev->Create_VertexBuffer();
        m_mesh_vbuf->SetState(MeshVertex::Layout(), m_mesh_vbuf_ranges->Size());

        m_mesh_ibuf_ranges = Create_RangeManager(65536);
        m_mesh_ibuf = m_dev->Create_IndexBuffer();
        m_mesh_ibuf->SetState(m_mesh_ibuf_ranges->Size());

        m_mesh_matbuf_ranges = Create_RangeManager(256);
        m_mesh_matbuf = m_dev->Create_StructuredBuffer();
        m_mesh_matbuf->SetState(sizeof(MCMeshMaterialVertex), m_mesh_matbuf_ranges->Size());

        m_bone_remap_ranges = Create_RangeManager(256);
        m_bone_remap = m_dev->Create_StructuredBuffer();
        m_bone_remap->SetState(sizeof(int32_t), m_bone_remap_ranges->Size());

        m_inst_vbuf = m_dev->Create_VertexBuffer();
        m_inst_vbuf->SetState(MCMeshInstanceVertex::Layout(), 128);
    }
    void MCArmature::Invalidate()
    {
        if (m_sys) {
            m_sys->m_dirty_armatures.insert(this);
        }
    }
    glm::mat4 MCArmature::GetTransform()
    {
        return m_pose->GetTransform();
    }
    void MCArmature::SetTransform(const glm::mat4& m)
    {
        m_pose->SetTransform(m);
        Invalidate();
    }
    ArmaturePosePtr MCArmature::Pose()
    {
        return m_pose;
    }
    void MCArmature::SetPose(const char* anim_name, float frame_pos)
    {
        int anim_idx = m_pose->Armature()->FindAnimIdx(anim_name);
        m_pose->SetPose(anim_idx, frame_pos);
        Invalidate();
    }
    void MCArmature::SetPose(const std::vector<AnimState> anims)
    {
        m_pose->SetPose(anims);
        Invalidate();
    }
    int MCArmature::BufOffset()
    {
        return m_range->OffsetSize().x;
    }
    MCArmature::MCArmature(MeshCollection* system, const ArmaturePosePtr& pose, MemRangeIntfPtr range)
    {
        m_sys = system;
        m_sys->m_armatures.push_back(this);
        m_idx = int(m_sys->m_armatures.size() - 1);

        m_range = std::move(range);
        m_pose = pose;

        Invalidate();
    }
    MCArmature::~MCArmature()
    {
        if (m_sys) {
            m_sys->m_armatures[m_idx] = m_sys->m_armatures[m_sys->m_armatures.size() - 1];
            m_sys->m_armatures[m_idx]->m_idx = m_idx;
            m_sys->m_armatures.pop_back();
            m_sys->m_dirty_armatures.erase(this);
        }
    }
    void MCMeshInstance::UpdateInstanceVertex()
    {
        MCMeshInstanceVertex new_vert;
        new_vert.bind_transform = GetBindTransform();
        new_vert.transform = GetTransform();
        new_vert.materials_offset = m_mesh ? m_mesh->m_materials->OffsetSize().x : 0;
        new_vert.bone_offset = m_arm ? m_arm->BufOffset() : -1;
        new_vert.bone_remap_offset = m_remap_range->OffsetSize().x;
        m_sys->m_inst_vbuf->SetSubData(m_idx, 1, &new_vert);
    }
    const MeshPtr& MCMeshInstance::MeshData() const {
        return m_mesh->MeshData();
    }
    const MeshInstancePtr& MCMeshInstance::InstanceData() const {
        return m_inst;
    }
    uint32_t MCMeshInstance::GetGroupID()
    {
        return m_group_id;
    }
    void MCMeshInstance::SetGroupID(uint32_t group_id)
    {
        m_group_id = group_id;
    }
    glm::AABB MCMeshInstance::BBox() const {
        return m_inst->BBox();
    }
    void MCMeshInstance::BindArmature(const MCArmaturePtr& arm)
    {
        if (m_arm == arm) return;
        m_arm = arm;
        m_inst->BindPose(m_arm->Pose());

        glm::ivec2 r = m_remap_range->OffsetSize();
        m_sys->m_bone_remap->SetSubData(r.x, r.y, m_inst->BoneMapping().data());

        UpdateInstanceVertex();
    }
    const glm::mat4& MCMeshInstance::GetBindTransform() const
    {
        return m_inst->GetBindTransform();
    }
    const glm::mat4& MCMeshInstance::GetTransform() const
    {
        return m_inst->GetTransform();
    }
    void MCMeshInstance::SetTransform(const glm::mat4 m)
    {
        m_inst->SetTransform(m);
        UpdateInstanceVertex();
    }
    MCMeshInstance::MCMeshInstance(MeshCollection* system, const MCMeshPtr& mesh, const MeshInstancePtr& instance, MemRangeIntfPtr remap_range)
    {
        m_group_id = 0;
        m_sys = system;
        m_sys->m_instances.push_back(this);
        m_idx = int(m_sys->m_instances.size() - 1);
        m_mesh = mesh;
        m_inst = instance;
        m_remap_range = std::move(remap_range);

        if (m_idx > m_sys->m_inst_vbuf->VertexCount()) {
            m_sys->m_inst_vbuf->SetState(m_sys->m_inst_vbuf->GetLayout(), m_sys->m_inst_vbuf->VertexCount() * 2);
            for (const auto& inst : m_sys->m_instances)
                inst->UpdateInstanceVertex();
        }
        else {
            UpdateInstanceVertex();
        }
    }
    MCMeshInstance::~MCMeshInstance()
    {
        if (m_sys) {
            if (m_idx == m_sys->m_instances.size() - 1) {
                m_sys->m_instances.pop_back();
            } 
            else {
                m_sys->m_instances[m_idx] = m_sys->m_instances[m_sys->m_instances.size() - 1];
                m_sys->m_instances[m_idx]->m_idx = m_idx;
                m_sys->m_instances.pop_back();
                m_sys->m_instances[m_idx]->UpdateInstanceVertex();
            }
        }
    }
    const MeshPtr& MCMesh::MeshData() const
    {
        return m_mesh;
    }
    std::shared_ptr<MCMesh> MCMesh::SPtr()
    {
        return shared_from_this();
    }
    MCMesh::MCMesh(MeshCollection* system, const MeshPtr& mesh, MemRangeIntfPtr vertices, MemRangeIntfPtr indices, MemRangeIntfPtr materials)
    {
        m_sys = system;
        m_sys->m_meshes.insert({ mesh, this });

        m_mesh = mesh;
        m_vertices = std::move(vertices);
        m_indices = std::move(indices);
        m_materials = std::move(materials);

        m_materials_data.reserve(mesh->materials.size());
        for (const auto& m : mesh->materials) {
            m_materials_data.emplace_back(m);
        }
    }
    MCMesh::~MCMesh()
    {
        if (m_sys) {
            m_sys->m_meshes.erase(m_mesh);
        }
    }
    const Layout* MCMeshInstanceVertex::Layout()
    {        
        return LB()
            ->Add("bind_trow0_", LayoutType::Float, 4)
            ->Add("bind_trow1_", LayoutType::Float, 4)
            ->Add("bind_trow2_", LayoutType::Float, 4)
            ->Add("bind_trow3_", LayoutType::Float, 4)
            ->Add("trow0_", LayoutType::Float, 4)
            ->Add("trow1_", LayoutType::Float, 4)
            ->Add("trow2_", LayoutType::Float, 4)
            ->Add("trow3_", LayoutType::Float, 4)
            ->Add("materials_offset", LayoutType::UInt, 1, false)
            ->Add("bone_offset", LayoutType::UInt, 1, false)
            ->Add("bone_remap_offset", LayoutType::UInt, 1, false)
            ->Finish();
    }
    MCMeshMaterialVertex::MCMeshMaterialVertex(const Material& mat)
    {
        albedo = mat.albedo;
        metallic = mat.metallic;
        roughness = mat.roughness;
        emission = mat.emission;
        emission_strength = mat.emission_strength;
    }
    void DecalsManager::AllocateNewSBO()
    {
        int n = glm::nextPowerOfTwo(int(m_decals.size()));
        m_sbo->SetState(sizeof(DecalData), n);
        m_sbo->SetSubData(0, int(m_decals_data.size()), m_decals_data.data());
    }
    void DecalsManager::UpdateDecal(int idx)
    {
        if (m_sbo->VertexCount() < int(m_decals.size())) {
            AllocateNewSBO();
        }
        else {
            if (m_sbo->VertexCount() > int(m_decals.size() * 3)) {
                AllocateNewSBO();
            }
            else {
                m_sbo->SetSubData(idx, 1, &m_decals_data[idx]);
            }
        }
    }
    DecalsManager_Buffers DecalsManager::GetBuffers()
    {
        DecalsManager_Buffers res;
        res.decals_count = int(m_decals_data.size());
        res.atlas = &m_atlas;
        res.sbo = &m_sbo;
        return res;
    }
    DecalsManager::DecalsManager(const DevicePtr& dev)
    {
        m_dev = dev;
        m_atlas = std::make_shared<Atlas>(m_dev);
        m_sbo = m_dev->Create_StructuredBuffer();
    }
    DecalPtr DecalsManager::Create_Decal(const fs::path& texture_filename)
    {        
        return std::shared_ptr<Decal>(new Decal(this, m_atlas->ObtainSprite(texture_filename)));
    }
    void Decal::UpdateDecal()
    {
        m_man->UpdateDecal(m_idx);
    }
    Decal::Decal(DecalsManager* man, const AtlasSpritePtr sprite)
    {
        m_man = man;
        m_idx = int(m_man->m_decals.size());
        m_man->m_decals.push_back(this);
        m_man->m_decals_data.emplace_back(sprite.get());
    }
    glm::vec3 Decal::GetPos() const
    {
        return m_man->m_decals_data[m_idx].pos;
    }
    void Decal::SetPos(const glm::vec3& pos)
    {
        m_man->m_decals_data[m_idx].pos = pos;
        UpdateDecal();
    }
    glm::quat Decal::GetRotate() const
    {
        return m_man->m_decals_data[m_idx].rotate;
    }
    void Decal::SetRotate(const glm::quat& rotate)
    {
        m_man->m_decals_data[m_idx].rotate = rotate;
        UpdateDecal();
    }
    glm::vec3 Decal::GetSize() const
    {
        return m_man->m_decals_data[m_idx].size;
    }
    void Decal::SetSize(const glm::vec3& size)
    {
        m_man->m_decals_data[m_idx].size = size;
        UpdateDecal();
    }
    glm::vec4 Decal::GetColor() const
    {
        return m_man->m_decals_data[m_idx].color;
    }
    void Decal::SetColor(const glm::vec4& color)
    {
        m_man->m_decals_data[m_idx].color = color;
        UpdateDecal();
    }
    void Decal::SetState(const glm::vec3& pos, const glm::quat& rot, const glm::vec3& size, const glm::vec4& color)
    {
        DecalData& data = m_man->m_decals_data[m_idx];
        data.pos = pos;
        data.rotate = rot;
        data.size = size;
        data.color = color;
        UpdateDecal();
    }
    Decal::~Decal()
    {
        m_man->m_decals[m_idx] = m_man->m_decals.back();
        m_man->m_decals_data[m_idx] = m_man->m_decals_data.back();
        m_man->m_decals[m_idx]->m_idx = m_idx;
        m_man->m_decals.pop_back();
        m_man->m_decals_data.pop_back();
        if (m_man->m_decals_data.size() != m_idx)
            m_man->m_decals[m_idx]->UpdateDecal();
    }
}