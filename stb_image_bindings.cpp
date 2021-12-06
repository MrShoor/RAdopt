#include "pch.h"
#include "RUtils.h"
#include "stb_image_bindings.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace RA {
	STB_TexManager::ImageKey STB_TexManager::BuildKey(const fs::path& filename, bool premultiply)
	{
		ImageKey k;
		k.fname = filename;
		k.premultiply = premultiply;
		return k;
	}
	TexDataIntf* STB_TexManager::Load(const fs::path& filename, bool premultiply)
    {
		ImageKey k = BuildKey(std::filesystem::absolute(filename), premultiply);
		auto it = m_cache.find(k);
		if (it == m_cache.end()) {
			it = m_cache.insert({ std::move(k), std::make_unique<STB_TexData>(k.fname) }).first;
			if (premultiply) {
				it->second->DoPremultiply();
			}
		}
		return it->second.get();
    }
	void STB_TexData::DoPremultiply()
	{
		int n = m_size.x * m_size.y;
		//glm::u8vec4* c = static_cast<glm::u8vec4*>(m_data);
		glm::u8vec4* c = (glm::u8vec4*)m_data;
		for (int i = 0; i < n; i++) {
			float a = c->w / 255.0f;
			c->x = int(glm::round(float(c->x) * a));
			c->y = int(glm::round(float(c->y) * a));
			c->z = int(glm::round(float(c->z) * a));
			c++;
		}
	}
	TextureFmt STB_TexData::Fmt() const
	{
		return TextureFmt::RGBA8;
	}
	const void* STB_TexData::Data() const
	{
		return m_data;
	}
	glm::ivec2 STB_TexData::Size() const
	{
		return m_size;
	}
	const void* STB_TexData::Pixel(int x, int y) const
	{
		if (x < 0) return nullptr;
		if (y < 0) return nullptr;
		if (x >= m_size.x) return nullptr;
		if (y >= m_size.y) return nullptr;
		return &m_data[(y * m_size.x + x) * 4];
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
			throw std::runtime_error("load failed: "+filename.u8string());
		
	}
	STB_TexData::~STB_TexData()
	{
		if (m_data)
			stbi_image_free(m_data);
	}
}
