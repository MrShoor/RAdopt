#pragma once
#include <unordered_map>
#include <filesystem>
#define STBI_WINDOWS_UTF8
#include "stb_image.h"

namespace RA {
    namespace fs = std::filesystem;

    class STB_TexData : public TexDataIntf {
    private:
        glm::ivec2 m_size;
        stbi_uc* m_data;
    public:
        void DoPremultiply();

        TextureFmt Fmt() const override;
        const void* Data() const override;
        glm::ivec2 Size() const override;
        const void* Pixel(int x, int y) const override;

        STB_TexData(const fs::path& filename);
        ~STB_TexData();
    };

    class STB_TexManager : public TexManagerIntf {
    private:
        struct ImageKey {
            fs::path fname;
            bool premultiply;
            bool operator==(const ImageKey& k) const {
                return k.fname == fname && k.premultiply == premultiply;
            }
            std::size_t operator()(const ImageKey& k) const {
                return fs::hash_value(k.fname) ^ std::hash<bool>()(k.premultiply);
            }
        };
    private:
        std::unordered_map<ImageKey, std::unique_ptr<STB_TexData>, ImageKey> m_cache;
        ImageKey BuildKey(const fs::path& filename, bool premultiply);
    public:
        TexDataIntf* Load(const fs::path& filename, bool premultiply = true) override;
    };
}