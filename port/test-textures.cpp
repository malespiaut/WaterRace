// Headless test for Textures.cp: loads every cptx texture of the real MetaFiles through Texture_UnpackHeader,
// Texture_GetMetaFileFolder and Texture_NewFromCompressedTexture, records the TQAImage chain handed to
// QATextureNew and checks its sizes and mip counts against "WaterRace Data/Textures/manifest.json".
// Usage: test-textures [GAME_ROOT]   (GAME_ROOT holds "WaterRace Data/")

#include <Rave.h>

#include <cstring>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <vector>

#include "PommeFiles.h"
#include "Infinity Structures.h"
#include "Textures.h"

static int		gFailures = 0;

#define CHECK(cond) do { if(!(cond)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++gFailures; } } while(0)

// QATextureNew stand-in: copies the TQAImage chain. RAVE gets no level count: with kQATexture_Mipmap it runs down to 1x1.
static std::vector<TQAImage>				gImages;
static std::vector<std::vector<uint8_t>>	gPixels;

TQAError QATextureNew(TQAEngine*, long flags, long pixelFormat, const TQAImage* image, TQATexture** texture)
{
	CHECK(pixelFormat == kQAPixel_ARGB32);
	gImages.clear();
	gPixels.clear();
	for(long i = 0; ; ++i) {
		gImages.push_back(image[i]);
		gPixels.emplace_back((uint8_t*) image[i].pixmap, (uint8_t*) image[i].pixmap + image[i].rowBytes * image[i].height);
		if(!(flags & kQATexture_Mipmap) || ((image[i].width == 1) && (image[i].height == 1)))
		break;
	}
	*texture = (TQATexture*) &gImages;
	return kQANoErr;
}
TQAError QATextureDetach(TQAEngine*, TQATexture*) { return kQANoErr; }
void QATextureDelete(TQAEngine*, TQATexture*) {}
TQAError QABitmapNew(TQAEngine*, long, long, const TQAImage*, TQABitmap**) { return kQANoErr; }
TQAError QABitmapDetach(TQAEngine*, TQABitmap*) { return kQANoErr; }
void QABitmapDelete(TQAEngine*, TQABitmap*) {}

struct Entry {
	std::string		source;
	int				flags,
					width,
					height,
					alphaDepth;
};

// manifest.json is written one key per line by convert_textures.py, entries in MetaFile table order
static std::vector<Entry> ReadManifest(const std::string& path)
{
	std::ifstream		f(path);
	std::stringstream	ss;
	std::vector<Entry>	entries;
	size_t				pos = 0;

	ss << f.rdbuf();
	std::string			s = ss.str();
	while((pos = s.find('{', pos)) != std::string::npos) {
		std::string		obj = s.substr(pos, s.find('}', pos) - pos);
		auto			field = [&](const char* key) { return atoi(obj.c_str() + obj.find(key) + strlen(key)); };
		size_t			src = obj.find("\"source\": \"") + 11;
		Entry			e;

		e.source = obj.substr(src, obj.find('"', src) - src);
		e.flags = field("\"flags\": ");
		e.width = field("\"width\": ");
		e.height = field("\"height\": ");
		e.alphaDepth = field("\"alphaDepth\": ");
		entries.push_back(e);
		++pos;
	}
	return entries;
}

static uint32_t BE32(const std::string& d, size_t p)
{
	return ((uint8_t) d[p] << 24) | ((uint8_t) d[p + 1] << 16) | ((uint8_t) d[p + 2] << 8) | (uint8_t) d[p + 3];
}

// Load one texture and check what QATextureNew got against the manifest entry
static void CheckTexture(CompressedTexturePtr texture, const Entry& e, long flags, int expectW, int expectH)
{
	TQATexture*		raveTexture = nil;
	int				levels = 1;

	gImages.clear();
	CHECK(Texture_NewFromCompressedTexture(nil, &raveTexture, texture, flags) == noErr);
	if(gImages.empty())
	return;
	if((flags & (kTextureFlag_MipMap | kTextureFlag_ForceMipMap)) && (e.flags & kFlag_MipMap) && (expectW <= kMaxMipmapSizeH) && (expectH <= kMaxMipmapSizeV))
	for(int s = expectW > expectH ? expectW : expectH; s > 1; s >>= 1)
	++levels;

	CHECK((int) gImages.size() == levels);
	for(size_t l = 0; l < gImages.size(); ++l) {
		const TQAImage&		im = gImages[l];
		int					w = expectW >> l,
							h = expectH >> l,
							binary = 1,
							opaque = 1;

		CHECK(im.width == (w ? w : 1) && im.height == (h ? h : 1));
		CHECK(im.rowBytes == im.width * 4);
		for(size_t i = 3; i < gPixels[l].size(); i += 4) {
			opaque &= gPixels[l][i] == 255;
			binary &= gPixels[l][i] == 0 || gPixels[l][i] == 255;
		}
		if(e.alphaDepth == kAlpha_None)
		CHECK(opaque);
		else if(e.alphaDepth == kAlpha_1Bit)
		CHECK(binary);
	}
	if(flags == kTextureFlag_MipMap)
	printf("  %c%c%c%c %dx%d alpha=%d mips=%d\n", (char) (texture->name >> 24), (char) (texture->name >> 16), (char) (texture->name >> 8), (char) texture->name, (int) gImages[0].width, (int) gImages[0].height, e.alphaDepth, (int) gImages.size());
}

int main(int argc, char** argv)
{
	const char*			root = argc > 1 ? argv[1] : WATERRACE_ROOT;
	std::vector<Entry>	manifest;
	size_t				next = 0;

	if(chdir(root) != 0) {
		perror(root);
		return 1;
	}
	Pomme::Files::Init();
	manifest = ReadManifest("WaterRace Data/Textures/manifest.json");
	CHECK(!manifest.empty());

	while(next < manifest.size()) {
		const std::string	source = manifest[next].source;
		std::string			macPath = ":WaterRace Data:" + source;
		std::ifstream		f("WaterRace Data/" + source, std::ios::binary);
		std::stringstream	ss;
		FSSpec				spec;
		char				folder[256];
		int					count = 0;

		//The MetaFile's PNG folder, from the FSSpec the game opens it with
		for(char& c : macPath)
		if(c == '/')
		c = ':';
		CHECK(FSMakeFSSpec(0, 0, macPath.c_str(), &spec) == noErr);
		Texture_GetMetaFileFolder(&spec, folder, sizeof(folder));
		CHECK(source == folder);
		printf("%s (folder \"%s\")\n", source.c_str(), folder);

		//Big-endian MetaFile: tag table position at 8, table {count, 24-byte entries {.., tag at 4, dataSize at 16, dataPosition at 20}}
		ss << f.rdbuf();
		std::string			d = ss.str();
		uint32_t			table = BE32(d, 8),
							n = BE32(d, table);

		for(uint32_t i = 0; i < n; ++i) {
			size_t				entry = table + 4 + i * 24;

			if(d.compare(entry + 4, 4, "cptx") != 0)
			continue;
			CHECK((next < manifest.size()) && (manifest[next].source == source));
			if((next >= manifest.size()) || (manifest[next].source != source))
			break;

			const Entry&			e = manifest[next++];
			CompressedTexture		texture;

			Texture_UnpackHeader(&texture, (const unsigned char*) d.data() + BE32(d, entry + 20));
			CHECK(texture.width == e.width && texture.height == e.height && texture.flags == (unsigned long) e.flags && texture.alphaDepth == e.alphaDepth);
			texture.pngPath = Texture_NewPNGPath(folder, texture.name);
			CheckTexture(&texture, e, kTextureFlag_MipMap, e.width, e.height);
			if((e.width > kMinTextureReduce) && (e.height > kMinTextureReduce))
			CheckTexture(&texture, e, kTextureFlag_MipMap | kTextureFlag_Reduce4X, e.width / 2, e.height / 2);
			DisposePtr(texture.pngPath);
			++count;
		}
		CHECK(count > 0);
		if(count == 0)
		break;
	}
	CHECK(next == manifest.size());

	printf(gFailures ? "%zu textures, %d failures\n" : "%zu textures, all passed\n", next, gFailures);
	return gFailures != 0;
}
