#include "pch.h"
#include "RFonts.h"
#include <functional>

namespace RA {
    void BaseAtlas::ValidateSBO()
    {
        std::vector<SpriteSBOVertex> data;
        data.reserve(m_sprites.size());
        for (const auto& s : m_sprites) {
            data.emplace_back(s);
        }
        m_glyphs_sbo->SetState(sizeof(SpriteSBOVertex), int(data.size()), false, false, data.data());
    }
    void BaseAtlas::ValidateAll()
    {
        if (!m_tex_valid) {
            m_tex_valid = true;
            ValidateTexture();
            ValidateSBO();
        }
    }
    void BaseAtlas::RegisterSprite(BaseAtlasSpritePtr* sprite)
    {
        int slice = 0;
        while (true) {
            if (m_roots[slice]->Insert(*sprite)) break;
            slice++;
            if (m_roots.size() == slice) {
                m_roots.push_back(std::make_unique<Node>(m_tex->Size()));
            }
        }
        (*sprite)->m_slice = slice;
    }
    void BaseAtlas::InvalidateTex()
    {
        m_tex_valid = false;
    }
    StructuredBufferPtr BaseAtlas::GlyphsSBO()
    {
        ValidateAll();
        return m_glyphs_sbo;
    }
    Texture2DPtr BaseAtlas::Texture()
    {
        ValidateAll();
        return m_tex;
    }
    BaseAtlas::BaseAtlas(const DevicePtr& dev)
    {
        m_dev = dev;
        m_tex_valid = false;
        m_glyphs_sbo = m_dev->Create_StructuredBuffer();
    }
    BaseAtlas::~BaseAtlas()
    {
        for (auto& s : m_sprites) {
            s->m_owner = nullptr;
        }
    }
    BaseAtlasSprite::BaseAtlasSprite(BaseAtlas* owner, const glm::ivec2& size)
    {
        m_slice = 0;
        m_owner = owner;
        m_idx = int(m_owner->m_sprites.size());
        m_owner->m_sprites.push_back(this);
        m_size = size;
        m_rect = { 0,0,0,0 };
    }
    int BaseAtlasSprite::Index() const
    {
        return m_idx;
    }
    int BaseAtlasSprite::Slice() const
    {
        return m_slice;
    }
    glm::ivec2 BaseAtlasSprite::AtlasSize() const
    {        
        return m_owner->Texture()->Size();
    }
    glm::ivec2 BaseAtlasSprite::Pos() const
    {
        return m_rect.xy();
    }
    glm::ivec2 BaseAtlasSprite::Size() const
    {
        return m_size;
    }
    glm::ivec4 BaseAtlasSprite::Rect() const
    {
        return m_rect;
    }
    BaseAtlasSprite::~BaseAtlasSprite()
    {
        if (m_owner) {
            m_owner->m_sprites[m_idx] = m_owner->m_sprites.back();
            m_owner->m_sprites[m_idx]->m_idx = m_idx;
            m_owner->m_sprites.pop_back();
        }
    }
    BaseAtlas::Node* BaseAtlas::Node::Insert(const BaseAtlasSpritePtr& img)
    {
        Node* newNode = nullptr;
        if (child[0]) { //we're not a leaf then
            newNode = child[0]->Insert(img);
            if (newNode) return newNode;
            return child[1]->Insert(img);
        }
        else {
            if (sprite.lock()) return nullptr;
            glm::ivec2 size = img->Size();
            if ((size.x > rect.z) || (size.y > rect.w)) return nullptr;

            if ((size.x == rect.z) && (size.y == rect.w)) {
                sprite = img;
                img->m_rect = rect;
                return this;
            }

            this->child[0] = std::make_unique<Node>();
            this->child[1] = std::make_unique<Node>();

            int dw = rect.z - size.x;
            int dh = rect.w - size.y;

            if (dw > dh) {
                child[0]->rect = glm::vec4(rect.x, rect.y, size.x, rect.w);
                child[1]->rect = glm::vec4(rect.x + size.x, rect.y, rect.z - size.x, rect.w);
            }
            else {
                child[0]->rect = glm::vec4(rect.x, rect.y, rect.z, size.y);
                child[1]->rect = glm::vec4(rect.x, rect.y + size.y, rect.z, rect.w - size.y);
            }

            return child[0]->Insert(img);
        }
    }
    BaseAtlas::Node::Node()
    {
        child[0] = nullptr;
        child[1] = nullptr;
        rect = { 0,0,0,0 };
    }
    BaseAtlas::Node::Node(const glm::ivec2& root_size)
    {
        child[0] = nullptr;
        child[1] = nullptr;
        rect = { 0, 0, root_size.x, root_size.y };
    }
    void Atlas::ValidateTexture()
    {
        if (m_tex->SlicesCount() != m_roots.size())
        {
            m_tex->SetState(m_tex->Format(), m_tex->Size(), 0, int(m_roots.size()), nullptr);
            for (const auto& s : m_sprites) {
                m_invalid_sprites.insert(static_cast<AtlasSprite*>(s));
            }
        }
        for (const auto& it : m_invalid_sprites) {
            assert(static_cast<AtlasSprite*>(it)->m_data->Fmt() == m_tex->Format());
            m_tex->SetSubData(it->Pos(), it->Size(), it->Slice(), 0, static_cast<AtlasSprite*>(it)->m_data->Data());
        }
        m_invalid_sprites.clear();
    }
    Atlas::Atlas(const DevicePtr& dev) : Atlas(dev, { 1024, 1024 })
    {
    }
    Atlas::Atlas(const DevicePtr& dev, const glm::ivec2& atlas_size) : BaseAtlas(dev)
    {
        m_tm = TM();

        m_tex = m_dev->Create_Texture2D();
        m_tex->SetState(TextureFmt::RGBA8, atlas_size);
        m_roots.push_back(std::make_unique<Node>(m_tex->Size()));        
    }
    AtlasSpritePtr Atlas::ObtainSprite(const fs::path& filename)
    {
        TexDataIntf* tex = TM()->Load(filename);        
        auto it = m_data.find(tex);
        if (it == m_data.end()) {
            AtlasSpritePtr new_sprite(new AtlasSprite(this, tex));
            BaseAtlasSpritePtr tmp = std::static_pointer_cast<AtlasSprite>(new_sprite);
            RegisterSprite(&tmp);
            m_data[tex] = new_sprite;
            m_invalid_sprites.insert(new_sprite.get());
            InvalidateTex();
            return new_sprite;
        }
        return it->second;
    }
    AtlasSprite::AtlasSprite(Atlas* owner, TexDataIntf* data) : BaseAtlasSprite(owner, data->Size())
    {
        m_data = data;
    }
    const TexDataIntf* AtlasSprite::TexData() const 
    {
        return m_data;
    }
}