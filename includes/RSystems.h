#pragma once
#include "RAdopt.h"
#include "RTypes.h"
#include "RUtils.h"
#include "RModels.h"
#include "RAtlas.h"
#include <unordered_set>

namespace RA {
    class MeshCollection;

    class MCArmature {
        friend class MeshCollection;
    private:
        MeshCollection* m_sys;
        int m_idx;

        MemRangeIntfPtr m_range;
        ArmaturePosePtr m_pose;
        void Invalidate();
    public:
        glm::mat4 GetTransform();
        void SetTransform(const glm::mat4& m);

        ArmaturePosePtr Pose();
        void SetPose(const char* anim_name, float frame_pos);
        void SetPose(const std::vector<AnimState> anims);

        int BufOffset();

        MCArmature(MeshCollection* system, const ArmaturePosePtr& pose, MemRangeIntfPtr range);
        ~MCArmature();
    };
    using MCArmaturePtr = std::shared_ptr<MCArmature>;

    struct MCMeshMaterialVertex {
        glm::vec4 albedo;
        float metallic;
        float roughness;
        glm::vec4 emission;
        float emission_strength;
        MCMeshMaterialVertex(const Material& mat);
    };

    class MCMesh : public std::enable_shared_from_this<MCMesh> {
        friend class MeshCollection;
        friend class MCMeshInstance;
    private:
        MeshCollection* m_sys;

        MeshPtr m_mesh;
        RA::Texture2DPtr m_albedo;
        MemRangeIntfPtr m_vertices;
        MemRangeIntfPtr m_indices;
        MemRangeIntfPtr m_materials;

        std::vector<MCMeshMaterialVertex> m_materials_data;

        RA::UPtr<Octree> m_octree;
    public:
        RA::Texture2DPtr Albedo() const;
        const MeshPtr& MeshData() const;

        float HitTest(const glm::Ray& ray);

        std::shared_ptr<MCMesh> SPtr();
        MCMesh(MeshCollection* system, 
                const MeshPtr& mesh, 
                MemRangeIntfPtr vertices, 
                MemRangeIntfPtr indices, 
                MemRangeIntfPtr materials,
                RA::Texture2DPtr albedo);
        ~MCMesh();
    };
    using MCMeshPtr = std::shared_ptr<MCMesh>;

    struct MCMeshInstanceVertex {
        glm::mat4 bind_transform;
        glm::mat4 transform;
        int32_t materials_offset;
        int32_t bone_offset;
        int32_t bone_remap_offset;
        static const Layout* Layout();
    };

    class MCMeshInstance {
        friend class MeshCollection;
    private:
        MeshCollection* m_sys;
        int m_idx;

        MCArmaturePtr m_arm;
        MCMeshPtr m_mesh;
        MeshInstancePtr m_inst;
        MemRangeIntfPtr m_remap_range;

        uint32_t m_group_id;

        void UpdateInstanceVertex();
    public:
        void* user_data = nullptr;

        const MeshPtr& MeshData() const;
        const MeshInstancePtr& InstanceData() const;

        uint32_t GetGroupID();
        void SetGroupID(uint32_t group_id);

        float HitTest(const glm::Ray& ray);
        glm::AABB BBox() const;
        void BindArmature(const MCArmaturePtr& arm);

        const glm::mat4& GetBindTransform() const;
        const glm::mat4& GetTransform() const;
        void SetTransform(const glm::mat4 m);

        MCMeshInstance(MeshCollection* system, const MCMeshPtr& mesh, const MeshInstancePtr& instance, MemRangeIntfPtr remap_range) noexcept;
        ~MCMeshInstance();
    };
    using MCMeshInstancePtr = std::unique_ptr<MCMeshInstance>;

    struct MeshCollectionBuffers {
        const VertexBufferPtr* vertices;
        const IndexBufferPtr* indices;
        const StructuredBufferPtr* materials;
        const VertexBufferPtr* instances;
        const StructuredBufferPtr* bones;
        const StructuredBufferPtr* bones_remap;
    };

    struct MeshCollectionDrawCommands {
        std::unordered_map<uint32_t, std::unordered_map<Texture2DPtr, std::vector<DrawIndexedCmd>>> commands;
    };

    class MeshCollection {
        friend class MCArmature;
        friend class MCMesh;
        friend class MCMeshInstance;
    private:
        struct AVMScene {
            fs::path filename;
            std::unordered_map<std::string, MeshPtr> meshes;
            std::unordered_map<std::string, MeshInstancePtr> instances;
            std::unordered_map<std::string, ArmaturePtr> armatures;
            AVMScene(const fs::path& p);
        };
    private:
        DevicePtr m_dev;

        std::vector<MCArmature*> m_armatures;
        std::unordered_set<MCArmature*> m_dirty_armatures;
        StructuredBufferPtr m_bones;
        RangeManagerIntfPtr m_bones_ranges;

        std::unordered_map<MeshPtr, MCMesh*> m_meshes;
        VertexBufferPtr m_mesh_vbuf;
        IndexBufferPtr m_mesh_ibuf;
        StructuredBufferPtr m_mesh_matbuf;
        RangeManagerIntfPtr m_mesh_vbuf_ranges;
        RangeManagerIntfPtr m_mesh_ibuf_ranges;
        RangeManagerIntfPtr m_mesh_matbuf_ranges;

        std::vector<MCMeshInstance*> m_instances;
        VertexBufferPtr m_inst_vbuf;
        StructuredBufferPtr m_bone_remap;
        RangeManagerIntfPtr m_bone_remap_ranges;

        RA::Texture2DPtr m_tex_white_pixel;
        std::unordered_map<std::filesystem::path, RA::Texture2DPtr> m_maps;

        std::unordered_map<fs::path, std::unique_ptr<AVMScene>, path_hasher> m_cache;
        AVMScene* ObtainScene(const fs::path& filename);

        MCMeshPtr ObtainMesh(const MeshPtr& mesh);

        MCMeshInstancePtr Clone_MeshInstance(AVMScene* scene, const std::string& instance_name);
        void ValidateArmatures();
        void FillBuffers(MeshCollectionBuffers* bufs);        
        RA::Texture2DPtr ObtainTexture(const std::filesystem::path& path, bool srgb);
        DrawIndexedCmd GetDrawCommand(MCMeshInstance* inst);
    public:
        float HitTest(const glm::Ray& ray, MCMeshInstance*& hit_inst);

        DrawIndexedCmd GetDrawCommand(MCMeshInstance* inst, Texture2DPtr& albedo);

        void PrepareBuffers(MeshCollectionBuffers* bufs, MeshCollectionDrawCommands* draw_commands);
        void PrepareBuffers(const std::vector<MCMeshInstance*>& instances, MeshCollectionBuffers* bufs, MeshCollectionDrawCommands* draw_commands);
        void PrepareBuffers(const std::vector<MCMeshInstancePtr>& instances, MeshCollectionBuffers* bufs, MeshCollectionDrawCommands* draw_commands);

        MCArmaturePtr Create_Armature(const fs::path& filename, const std::string& armature_name);
        MCMeshInstancePtr Clone_MeshInstance(const fs::path& filename, const std::string& instance_name);
        std::vector<MCMeshInstancePtr> Clone_MeshInstances(const fs::path& filename, const std::vector<std::string>& instances, uint32_t groupID);
        void AllMeshInstances(const fs::path& filename, const std::function<void(std::string)>& cb);

        MeshCollection(const DevicePtr& dev);
        ~MeshCollection();
    };

    class DecalsManager;

    struct DecalData {
        glm::vec4 color;
        glm::quat rotate;        
        glm::vec3 size;
        float dummy;
        glm::vec3 pos;
        int slice;
        glm::vec4 rect;
        DecalData(const AtlasSprite* sprite) : 
            color(1.0), 
            rotate(0,0,0,1),
            size(1.0, 1.0, 1.0),
            dummy(0),
            pos(0,0,0),
            slice(sprite->Slice()), 
            rect(sprite->Rect()) {}
    };

    class Decal {
        friend class DecalsManager;
    private:
        DecalsManager* m_man;
        int m_idx;
        AtlasSpritePtr m_sprite;

        void UpdateDecal();
        Decal(DecalsManager* man, const AtlasSpritePtr sprite);
    public:
        glm::vec3 GetPos() const;
        void SetPos(const glm::vec3& pos);
        glm::quat GetRotate() const;
        void SetRotate(const glm::quat& transform);
        glm::vec3 GetSize() const;
        void SetSize(const glm::vec3& size);
        glm::vec4 GetColor() const;
        void SetColor(const glm::vec4& color);

        void SetState(const glm::vec3& pos, const glm::quat& rot, const glm::vec3& size, const glm::vec4& color);

        ~Decal();
    };
    using DecalPtr = std::shared_ptr<Decal>;

    struct DecalsManager_Buffers {
        int decals_count;
        const AtlasPtr* atlas;
        const StructuredBufferPtr* sbo;
    };

    class DecalsManager {
        friend class Decal;
    private:
        DevicePtr m_dev;
        std::vector<Decal*> m_decals;
        std::vector<DecalData> m_decals_data;

        AtlasPtr m_atlas;
        StructuredBufferPtr m_sbo;
        void AllocateNewSBO();
        void UpdateDecal(int idx);
    public:
        DecalsManager_Buffers GetBuffers();
        DecalsManager(const DevicePtr& dev);
        DecalPtr Create_Decal(const fs::path& texture_filename);
    };
    using DecalsManagerPtr = std::shared_ptr<DecalsManager>;
}