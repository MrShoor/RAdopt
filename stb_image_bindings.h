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
        TextureFmt Fmt() const override;
        const void* Data() const override;
        glm::ivec2 Size() const override;
        const void* Pixel(int x, int y) const override;

        STB_TexData(const fs::path& filename);
        ~STB_TexData();
    };

    class STB_TexManager : public TexManagerIntf {
    private:
        struct path_hasher {
            std::size_t operator()(const fs::path& path) const {
                return fs::hash_value(path);
            }
        };
    private:
        std::unordered_map<fs::path, std::unique_ptr<STB_TexData>, path_hasher> m_cache;
    public:
        TexDataIntf* Load(const fs::path& filename) override;
    };
}