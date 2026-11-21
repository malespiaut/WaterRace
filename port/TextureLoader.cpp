// PNG texture loader, see TextureLoader.h.

#include "TextureLoader.h"

#include <cstdio>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h" // v2.30, unmodified copy of nothings/stb@013ac3beddff3dbffafd5177e7972067cd2b5083

// Same values as Textures.h and Infinity Structures.h, which can't be included without the Mac headers
#define				kTextureFlag_3DfxMode		(1 << 0)
#define				kTextureFlag_Reduce4X		(1 << 2)
#define				kTextureFlag_Reduce16X		(1 << 3)
#define				kTextureFlag_MipMap			(1 << 4)
#define				kTextureFlag_ForceMipMap	(1 << 5)
#define				kFlag_MipMap				(1 << 0)
#define				kAlpha_1Bit					1
#define				kMinTextureReduce			32
#define				kMaxTextureSizeH			1024
#define				kMaxTextureSizeV			512
#define				kMaxMipmapSizeH				512
#define				kMaxMipmapSizeV				kMaxMipmapSizeH
#define				kMax3DfxTextureSizeH		256
#define				kMax3DfxTextureSizeV		kMax3DfxTextureSizeH
#define				kMaskLimit					127

// Mac Roman 0x80-0xFF to Unicode, matching Python's "mac_roman" codec used by convert_textures.py
static const uint16_t kMacRoman[128] = {
	0xc4, 0xc5, 0xc7, 0xc9, 0xd1, 0xd6, 0xdc, 0xe1, 0xe0, 0xe2, 0xe4, 0xe3, 0xe5, 0xe7, 0xe9, 0xe8,
	0xea, 0xeb, 0xed, 0xec, 0xee, 0xef, 0xf1, 0xf3, 0xf2, 0xf4, 0xf6, 0xf5, 0xfa, 0xf9, 0xfb, 0xfc,
	0x2020, 0xb0, 0xa2, 0xa3, 0xa7, 0x2022, 0xb6, 0xdf, 0xae, 0xa9, 0x2122, 0xb4, 0xa8, 0x2260, 0xc6, 0xd8,
	0x221e, 0xb1, 0x2264, 0x2265, 0xa5, 0xb5, 0x2202, 0x2211, 0x220f, 0x3c0, 0x222b, 0xaa, 0xba, 0x3a9, 0xe6, 0xf8,
	0xbf, 0xa1, 0xac, 0x221a, 0x192, 0x2248, 0x2206, 0xab, 0xbb, 0x2026, 0xa0, 0xc0, 0xc3, 0xd5, 0x152, 0x153,
	0x2013, 0x2014, 0x201c, 0x201d, 0x2018, 0x2019, 0xf7, 0x25ca, 0xff, 0x178, 0x2044, 0x20ac, 0x2039, 0x203a, 0xfb01, 0xfb02,
	0x2021, 0xb7, 0x201a, 0x201e, 0x2030, 0xc2, 0xca, 0xc1, 0xcb, 0xc8, 0xcd, 0xce, 0xcf, 0xcc, 0xd3, 0xd4,
	0xf8ff, 0xd2, 0xda, 0xdb, 0xd9, 0x131, 0x2c6, 0x2dc, 0xaf, 0x2d8, 0x2d9, 0x2da, 0xb8, 0x2dd, 0x2db, 0x2c7,
};

// shortcut: a MetaFile holding the same name twice maps to the first PNG (later ones are <name>_dup.png), pass the occurrence index if they ever differ
std::string Texture_PNGPath(const std::string& texturesDir, const std::string& metaFile, uint32_t name)
{
	std::string		path = texturesDir + "/" + metaFile + "/";

	for(int shift = 24; shift >= 0; shift -= 8) {
		unsigned char	c = (name >> shift) & 0xFF;

		if(c == 0) //NUL pads short names
		continue;
		if(c == '/')
		path += '_';
		else if(c < 0x80)
		path += (char) c;
		else { //UTF-8 encode
			uint16_t	u = kMacRoman[c - 0x80];

			if(u < 0x800)
			path += (char) (0xC0 | (u >> 6));
			else {
				path += (char) (0xE0 | (u >> 12));
				path += (char) (0x80 | ((u >> 6) & 0x3F));
			}
			path += (char) (0x80 | (u & 0x3F));
		}
	}

	return path + ".png";
}

// Box filter by integer factors; fx, fy divide the source size (textures are powers of two)
static TextureLevel Downsample(const TextureLevel& src, int fx, int fy, short alphaDepth)
{
	TextureLevel	dst;
	int				n = fx * fy;

	dst.width = src.width / fx;
	dst.height = src.height / fy;
	dst.pixels.resize((size_t) dst.width * dst.height * 4);
	for(int y = 0; y < dst.height; ++y)
	for(int x = 0; x < dst.width; ++x)
	for(int c = 0; c < 4; ++c) {
		int		sum = 0;

		for(int j = 0; j < fy; ++j)
		for(int i = 0; i < fx; ++i)
		sum += src.pixels[(((size_t) (y * fy + j) * src.width) + x * fx + i) * 4 + c];
		dst.pixels[((size_t) y * dst.width + x) * 4 + c] = (sum + n / 2) / n;
	}
	if(alphaDepth == kAlpha_1Bit) //keep the mask binary, as the original re-thresholded it at every size
	for(size_t i = 3; i < dst.pixels.size(); i += 4)
	dst.pixels[i] = (dst.pixels[i] > kMaskLimit) ? 255 : 0;

	return dst;
}

bool Texture_LoadPNG(const std::string& path, uint32_t textureFlags, short alphaDepth, long flags, std::vector<TextureLevel>& levels)
{
	TextureLevel	image;
	int				channels,
					factor = 1;
	bool			mipmap = false;
	unsigned char*	data = stbi_load(path.c_str(), &image.width, &image.height, &channels, 4);

	levels.clear();
	if(data == nullptr) {
		fprintf(stderr, "Texture_LoadPNG: %s: %s\n", path.c_str(), stbi_failure_reason());
		return false;
	}
	image.pixels.assign(data, data + (size_t) image.width * image.height * 4);
	stbi_image_free(data);
	if((image.width > kMaxTextureSizeH) || (image.height > kMaxTextureSizeV))
	return false;

	//Reduce picture if required
	if((image.width > kMinTextureReduce) && (image.height > kMinTextureReduce)) {
		if(flags & kTextureFlag_Reduce16X)
		factor = 4;
		else if(flags & kTextureFlag_Reduce4X)
		factor = 2;
	}

	//3Dfx can't load textures bigger than 256x256: scale by the larger ratio, as the original
	if((flags & kTextureFlag_3DfxMode) && ((image.width / factor > kMax3DfxTextureSizeH) || (image.height / factor > kMax3DfxTextureSizeV))) {
		int		hRatio = image.width / factor / kMax3DfxTextureSizeH,
				vRatio = image.height / factor / kMax3DfxTextureSizeV;

		factor *= (hRatio > vRatio) ? hRatio : vRatio;
	}

	if(factor > 1)
	image = Downsample(image, factor, factor, alphaDepth);

	//Do we have to mipmap the texture?
	if(((flags & kTextureFlag_MipMap) && (textureFlags & kFlag_MipMap)) || (flags & kTextureFlag_ForceMipMap))
	mipmap = true;
	if((image.width > kMaxMipmapSizeH) || (image.height > kMaxMipmapSizeV))
	mipmap = false;

	levels.push_back(std::move(image));
	while(mipmap && ((levels.back().width > 1) || (levels.back().height > 1))) {
		const TextureLevel&		last = levels.back();
		TextureLevel			level = Downsample(last, last.width > 1 ? 2 : 1, last.height > 1 ? 2 : 1, alphaDepth);

		levels.push_back(std::move(level)); //last is dangling from here
	}

	return true;
}
