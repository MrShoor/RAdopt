#pragma once
#include "RAdopt.h"
#include "RUtils.h"
#include "RModels.h"
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
        MemRangeIntfPtr m_vertices;
        MemRangeIntfPtr m_indices;
        MemRangeIntfPtr m_materials;

        std::vector<MCMeshMaterialVertex> m_materials_data;
    public:
        std::shared_ptr<MCMesh> SPtr();
        MCMesh(MeshCollection* system, const MeshPtr& mesh, MemRangeIntfPtr vertices, MemRangeIntfPtr indices, MemRangeIntfPtr materials);
        ~MCMesh();
    };
    using MCMeshPtr = std::shared_ptr<MCMesh>;

    struct MCMeshInstanceVertex {
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

        void UpdateInstanceVertex();
    public:
        glm::AABB BBox() const;
        void BindArmature(const MCArmaturePtr& arm);

        glm::mat4 GetTransform();
        void SetTransform(const glm::mat4 m);

        MCMeshInstance(MeshCollection* system, const MCMeshPtr& mesh, const MeshInstancePtr& instance, MemRangeIntfPtr remap_range);
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

        std::unordered_map<fs::path, std::unique_ptr<AVMScene>, path_hasher> m_cache;
        AVMScene* ObtainScene(const fs::path& filename);

        MCMeshPtr ObtainMesh(const MeshPtr& mesh);

        MCMeshInstancePtr Clone_MeshInstance(AVMScene* scene, const std::string& instance_name);
        void ValidateArmatures();
        void FillBuffers(MeshCollectionBuffers* bufs);
        DrawIndexedCmd GetDrawCommand(MCMeshInstance* inst);
    public:
        void PrepareBuffers(MeshCollectionBuffers* bufs, std::vector<DrawIndexedCmd>* draw_commands);
        void PrepareBuffers(const std::vector<MCMeshInstance*>& instances, MeshCollectionBuffers* bufs, std::vector<DrawIndexedCmd>* draw_commands);
        void PrepareBuffers(const std::vector<MCMeshInstancePtr>& instances, MeshCollectionBuffers* bufs, std::vector<DrawIndexedCmd>* draw_commands);

        MCArmaturePtr Create_Armature(const fs::path& filename, const std::string& armature_name);
        MCMeshInstancePtr Clone_MeshInstance(const fs::path& filename, const std::string& instance_name);
        std::vector<MCMeshInstancePtr> Clone_MeshInstances(const fs::path& filename, const std::vector<std::string>& instances);

        MeshCollection(const DevicePtr& dev);
    };
}