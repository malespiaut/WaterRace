// Headless test for TextureLoader: loads opaque, 1-bit and 8-bit alpha PNGs and checks them against manifest.json.
// Usage: test-texture-loader [TEXTURES_DIR]

#include "TextureLoader.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

static int		gFailures = 0;

#define CHECK(cond) do { if(!(cond)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++gFailures; } } while(0)

struct Entry {
	int		flags,
			width,
			height,
			alphaDepth;
};

// manifest.json is written one key per line by convert_textures.py; read the fields of the entry whose "png" matches
static bool FindEntry(const std::string& manifest, const std::string& png, Entry& e)
{
	size_t		end = manifest.find("\"png\": \"" + png + "\""),
				start;

	if(end == std::string::npos)
	return false;
	start = manifest.rfind('{', end);
	std::string	obj = manifest.substr(start, end - start);
	auto		field = [&](const char* key) { return atoi(obj.c_str() + obj.find(key) + strlen(key)); };
	e.flags = field("\"flags\": ");
	e.width = field("\"width\": ");
	e.height = field("\"height\": ");
	e.alphaDepth = field("\"alphaDepth\": ");
	return true;
}

static void Test(const std::string& dir, const std::string& manifest, const char* metaFile, uint32_t name, long flags, int expectW, int expectH, int expectLevels, const char* key = nullptr)
{
	std::string					path = Texture_PNGPath(dir, metaFile, name);
	std::vector<TextureLevel>	levels;
	Entry						e;

	printf("%s flags=%ld\n", path.c_str(), flags);
	if(!FindEntry(manifest, key ? key : path.substr(dir.size() + 1), e)) {
		CHECK(!"entry in manifest");
		return;
	}
	if(!Texture_LoadPNG(path, e.flags, e.alphaDepth, flags, levels)) {
		CHECK(!"Texture_LoadPNG");
		return;
	}
	CHECK((int) levels.size() == expectLevels);
	CHECK(levels[0].width == expectW && levels[0].height == expectH);
	if(flags == 0)
	CHECK(levels[0].width == e.width && levels[0].height == e.height);

	for(size_t l = 0; l < levels.size(); ++l) {
		const TextureLevel&		lv = levels[l];
		int						w = expectW >> l,
								h = expectH >> l,
								binary = 1,
								opaque = 1;

		CHECK(lv.width == (w ? w : 1) && lv.height == (h ? h : 1));
		CHECK(lv.pixels.size() == (size_t) lv.width * lv.height * 4);
		for(size_t i = 3; i < lv.pixels.size(); i += 4) {
			opaque &= lv.pixels[i] == 255;
			binary &= lv.pixels[i] == 0 || lv.pixels[i] == 255;
		}
		if(e.alphaDepth == 0)
		CHECK(opaque);
		else if(e.alphaDepth == 1)
		CHECK(binary && (l > 0 || !opaque));
		else if(l == 0) //8-bit: real gradient in the full image
		CHECK(!binary);
	}
}

int main(int argc, char** argv)
{
	std::string			dir = argc > 1 ? argv[1] : WATERRACE_TEXTURES_DIR;
	std::ifstream		f(dir + "/manifest.json");
	std::stringstream	ss;

	ss << f.rdbuf();
	std::string			manifest = ss.str();
	CHECK(!manifest.empty());

	const long			kMipMap = 1 << 4, kReduce4X = 1 << 2, kReduce16X = 1 << 3, k3Dfx = 1 << 0;

	//Opaque, non-square mipmapped: 256x128 down to 1x1
	Test(dir, manifest, "Locations/Venice", 'Vp03', kMipMap, 256, 128, 9);
	Test(dir, manifest, "Locations/Venice", 'Vp03', kMipMap | kReduce4X, 128, 64, 8);
	//Opaque, Mac Roman name (Tdér, UTF-8 on disk, \u-escaped in the manifest), no kFlag_MipMap in the texture so no mips
	CHECK(Texture_PNGPath(dir, "Locations/New York", ('T' << 24) | ('d' << 16) | (0x8E << 8) | 'r') == dir + "/Locations/New York/Td\xC3\xA9r.png");
	Test(dir, manifest, "Locations/New York", ('T' << 24) | ('d' << 16) | (0x8E << 8) | 'r', kMipMap, 128, 128, 1, "Locations/New York/Td\\u00e9r.png");
	//Opaque, 1024x512 scaled to fit 3Dfx 256x256
	Test(dir, manifest, "Core/Cote d'azur", 'sky ', 0, 1024, 512, 1);
	Test(dir, manifest, "Core/Cote d'azur", 'sky ', k3Dfx, 256, 128, 1);
	//1-bit alpha, mask stays binary in every mip level
	Test(dir, manifest, "Locations/Venice", 'Vto1', kMipMap, 256, 256, 9);
	Test(dir, manifest, "Locations/Venice", 'Vto1', kMipMap | kReduce16X, 64, 64, 7);
	//8-bit alpha
	Test(dir, manifest, "Locations/Scotland", 'Tgaz', kMipMap, 64, 64, 7);
	//NUL-padded short name ("sbl\0" -> sbl.png)
	Test(dir, manifest, "Locations/Jamaica", ('s' << 24) | ('b' << 16) | ('l' << 8), 0, 32, 32, 1);

	printf(gFailures ? "%d failures\n" : "all passed\n", gFailures);
	return gFailures != 0;
}
