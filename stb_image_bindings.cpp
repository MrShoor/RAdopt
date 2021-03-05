#include "pch.h"
#include "RUtils.h"
#include "stb_image_bindings.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace RA {
    TexDataIntf* STB_TexManager::Load(const char* filename)
    {
		fs::path p = std::filesystem::absolute(filename);
		auto it = m_cache.find(p);
		if (it == m_cache.end()) {
			m_cache.insert({ p, std::make_unique<STB_TexData>(p) });
			it = m_cache.find(p);
		}
		return it->second.get();
    }
	TextureFmt STB_TexData::Fmt()
	{
		return TextureFmt::RGBA8;
	}
	const void* STB_TexData::Data()
	{
		return m_data;
	}
	glm::ivec2 STB_TexData::Size()
	{
		return m_size;
	}
	STB_TexData::STB_TexData(const fs::path& filename)
	{
		//stbi_is_hdr - check for floating point format
		//stbi_is_16_bit - check for 16 bif formats?
		//stbi_loadf - load floating point format
		//stbi_failure_reason - get error information

		stbi_set_flip_vertically_on_load(0);

		int dont_care;
		int targetformat = STBI_rgb_alpha;
		m_data = stbi_load(filename.u8string().c_str(), &m_size.x, &m_size.y, &dont_care, targetformat);
		if (!m_data)
			throw std::runtime_error("load failed");
		
	}
	STB_TexData::~STB_TexData()
	{
		if (m_data)
			stbi_image_free(m_data);
	}
}
