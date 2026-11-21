// PNG texture loader for the converted textures in "WaterRace Data/Textures" (PORTING.md 5.3).
// Pure C++/stdlib: no Pomme, no GL.

#ifndef __PORT_TEXTURE_LOADER__
#define __PORT_TEXTURE_LOADER__

#include <cstdint>
#include <string>
#include <vector>

struct TextureLevel {
	int						width,
							height;
	std::vector<uint8_t>	pixels; // RGBA8, rows top to bottom, no padding
};

// PNG path of a CompressedTexture: <texturesDir>/<metaFile>/<name>.png, as written by tools/convert_textures.py.
// metaFile is the MetaFile path relative to the data folder (e.g. "Locations/Venice").
std::string Texture_PNGPath(const std::string& texturesDir, const std::string& metaFile, uint32_t name);

// Load a texture PNG as RGBA8 and build its mip chain, following Texture_NewFromCompressedTexture:
// textureFlags/alphaDepth are the CompressedTexture fields, flags the kTextureFlag_* creation flags
// (Reduce4X, Reduce16X, 3DfxMode, MipMap, ForceMipMap are honoured). levels[0] is the full image.
bool Texture_LoadPNG(const std::string& path, uint32_t textureFlags, short alphaDepth, long flags, std::vector<TextureLevel>& levels);

#endif
