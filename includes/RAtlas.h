#pragma once
#include "RAdopt.h"
#include "RUtils.h"
#include <filesystem>
#include <unordered_set>

namespace RA {
    namespace fs = std::filesystem;

    class BaseAtlasSprite;
    using BaseAtlasSpritePtr = std::shared_ptr<BaseAtlasSprite>;

    struct SpriteSBOVertex {
        int slice;
        glm::vec2 xy;
        glm::vec2 size;
        SpriteSBOVertex(const BaseAtlasSprite* sprite);
    };

    class BaseAtlas {
        friend class BaseAtlasSprite;        
    public:
        struct Node {
            std::unique_ptr<Node> child[2];
            glm::ivec4 rect;
            std::weak_ptr<BaseAtlasSprite> sprite;
            Node* Insert(const BaseAtlasSpritePtr& img);
            Node();
            Node(const glm::ivec2& root_size);
        };
    protected:
        DevicePtr m_dev;
        Texture2DPtr m_tex;
        StructuredBufferPtr m_glyphs_sbo;
        bool m_tex_valid;

        std::vector<BaseAtlasSprite*> m_sprites;

        std::vector<std::unique_ptr<Node>> m_roots;

        virtual void ValidateTexture() = 0;
        void ValidateSBO();
        void ValidateAll();
        void RegisterSprite(BaseAtlasSpritePtr* img);
        void InvalidateTex();
    public:
        StructuredBufferPtr GlyphsSBO();
        Texture2DPtr Texture();
        BaseAtlas(const DevicePtr& dev);
        virtual ~BaseAtlas();
    };

    class BaseAtlasSprite {
        friend class BaseAtlas;
        friend struct BaseAtlas::Node;
        friend struct SpriteSBOVertex;
    protected:
        BaseAtlas* m_owner;
        int m_idx;
        int m_slice;
        glm::ivec4 m_rect; //xy - min coord, zw - rect size
        glm::ivec2 m_size;
        BaseAtlasSprite(BaseAtlas* owner, const glm::ivec2& size);
    public:
        int Index() const;
        int Slice() const;
        glm::ivec2 AtlasSize() const;
        glm::ivec2 Pos() const;
        glm::ivec2 Size() const;
        glm::ivec4 Rect() const; //xy - min coord, zw - rect size
        virtual ~BaseAtlasSprite();
    };

    class Atlas;

    class AtlasSprite : public BaseAtlasSprite {
        friend class Atlas;
    protected:
        TexDataIntf* m_data;
        AtlasSprite(Atlas* owner, TexDataIntf* data);
    public:
        const TexDataIntf* TexData() const;
    };
    using AtlasSpritePtr = std::shared_ptr<AtlasSprite>;

    class Atlas : public BaseAtlas {
        friend class AtlasSprite;
    protected:
        TexManagerIntf* m_tm;
        std::unordered_map<TexDataIntf*, AtlasSpritePtr> m_data;
        std::unordered_set<AtlasSprite*> m_invalid_sprites;
        void ValidateTexture() override;
    public:
        Atlas(const DevicePtr& dev);
        Atlas(const DevicePtr& dev, const glm::ivec2& atlas_size);
        AtlasSpritePtr ObtainSprite(const fs::path& filename);
    };
    using AtlasPtr = std::shared_ptr<Atlas>;
}