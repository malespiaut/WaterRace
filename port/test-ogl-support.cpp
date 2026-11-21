// OGL_Support.cp test (needs a display: ctest runs it under xvfb-run). Draws pre-projected Gouraud
// triangles, meshes and lines through the RAVE calls into a 640x480 window and checks pixels for
// colour interpolation, depth test, blend modes, scissor and the ImageBufferInitialize notice method.
// Then real converted textures (WaterRace Data/Textures) are drawn with each texture op and filter,
// as bitmaps, and checked against their texels. Fog is checked against the RAVE formulas over w, and the
// BufferCopy initialise method against an interface PNG (screenshot: ogl-support-buffercopy.png).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include "RAVE.h"
#include "TextureLoader.h"
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

static int gFailures = 0;
#define CHECK(c) do { if(!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++gFailures; } } while(0)

// Identity compositor: the window is the 640x480 interface.
void Boot_InterfaceToWindow(const Rect* r, int* x, int* y, int* width, int* height)
{
	*x = r->left;
	*y = 480 - r->bottom;
	*width = r->right - r->left;
	*height = r->bottom - r->top;
}

// Context origin in the window, so pixel checks are in context pixels.
static const int kLeft = 100, kTop = 50, kWidth = 320, kHeight = 240;

// Checks the pixel at context pixel (x, y), y down, against r,g,b in 0..1.
static void CheckPixel(int line, int x, int y, float r, float g, float b)
{
	unsigned char	p[4];

	glReadPixels(kLeft + x, 480 - (kTop + y) - 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, p);
	if(std::fabs(p[0] - r * 255) > 3 || std::fabs(p[1] - g * 255) > 3 || std::fabs(p[2] - b * 255) > 3) {
		std::fprintf(stderr, "FAIL line %d: pixel (%d,%d) = %d,%d,%d, expected %.0f,%.0f,%.0f\n", line, x, y, p[0], p[1], p[2], r * 255, g * 255, b * 255);
		++gFailures;
	}
}
#define CHECK_PIXEL(x, y, r, g, b) CheckPixel(__LINE__, x, y, r, g, b)

static TQAVGouraud V(float x, float y, float z, float invW, float r, float g, float b, float a)
{
	TQAVGouraud v = {};
	v.x = x; v.y = y; v.z = z; v.invW = invW;
	v.r = r; v.g = g; v.b = b; v.a = a;
	return v;
}

// Draws an axis-aligned quad as a two-triangle mesh.
static void Quad(TQADrawContext* c, float l, float t, float r, float b, float z, float invW, float cr, float cg, float cb, float ca)
{
	TQAVGouraud			v[4] = {V(l, t, z, invW, cr, cg, cb, ca), V(r, t, z, invW, cr, cg, cb, ca),
								V(r, b, z, invW, cr, cg, cb, ca), V(l, b, z, invW, cr, cg, cb, ca)};
	TQAIndexedTriangle	tris[2] = {{{0, 1, 2}, 0}, {{2, 3, 0}, 0}};

	QASubmitVerticesGouraud(c, 4, v);
	QADrawTriMeshGouraud(c, 2, tris);
}

static int gNoticeCalls = 0;
static void Notice(const TQADrawContext*, const TQADevice*, const TQARect*, void* refCon)
{
	gNoticeCalls += (refCon == &gNoticeCalls);
}

// Texel (x, y) of a level, 0..1.
static void Texel(const TextureLevel& l, int x, int y, float t[4])
{
	for(int i = 0; i < 4; ++i)
		t[i] = l.pixels[(y * l.width + x) * 4 + i] / 255.0f;
}

static TQAVTexture VT(float x, float y, float invW, float u, float v, float a)
{
	TQAVTexture t = {};
	t.x = x; t.y = y; t.z = 0.5; t.invW = invW;
	t.uOverW = u * invW; t.vOverW = v * invW;
	t.kd_r = 1; t.kd_g = 0.5; t.kd_b = 0.25; t.a = a;
	t.ks_r = 0.25; t.ks_g = 0.125; t.ks_b = 0.5;
	return t;
}

// Textured quad (l,t)-(r,b) showing the whole texture, RAVE v = 1 at the top; invW per left and right edge.
static void TexQuad(TQADrawContext* c, float l, float t, float r, float b, float invWL, float invWR, float a)
{
	TQAVTexture			v[4] = {VT(l, t, invWL, 0, 1, a), VT(r, t, invWR, 1, 1, a), VT(r, b, invWR, 1, 0, a), VT(l, b, invWL, 0, 0, a)};
	TQAIndexedTriangle	tris[2] = {{{0, 1, 2}, 0}, {{2, 3, 0}, 0}};

	QASubmitVerticesTexture(c, 4, v);
	QADrawTriMeshTexture(c, 2, tris);
}

static bool LoadImages(const char* png, long flags, std::vector<TextureLevel>& levels, TQAImage image[16])
{
	if(!Texture_LoadPNG(std::string(WATERRACE_TEXTURES_DIR "/") + png, 0, 8, flags, levels) || levels.size() > 16)
		return false;
	for(size_t i = 0; i < levels.size(); ++i) {
		image[i].width = levels[i].width;
		image[i].height = levels[i].height;
		image[i].rowBytes = levels[i].width * 4;
		image[i].pixmap = (Ptr) levels[i].pixels.data();
	}
	return true;
}

static void TestTextures(TQAEngine* engine, TQADrawContext* c)
{
	std::vector<TextureLevel>	venice, flare;
	TQAImage					veniceImage[16], flareImage[16];
	TQATexture					*vto1, *flr2, *bad;
	TQABitmap					*bitmap, *gworld;
	float						t[4];
	const int					kForceMipMap = 1 << 5;

	// Venice 'Vto1' (256x256, transparent windows, mip chain of 9) and Arctic 'Tsmk' (64x64 smoke, 8-bit alpha).
	CHECK(LoadImages("Locations/Venice/Vto1.png", kForceMipMap, venice, veniceImage) && venice.size() == 9);
	CHECK(LoadImages("Locations/Arctic/Tsmk.png", 0, flare, flareImage) && flare.size() == 1);
	if(gFailures)
		return;
	CHECK(QATextureNew(engine, kQATexture_NoCompression | kQATexture_Mipmap, kQAPixel_ARGB32, veniceImage, &vto1) == kQANoErr && QATextureDetach(engine, vto1) == kQANoErr);
	CHECK(QATextureNew(engine, kQATexture_NoCompression, kQAPixel_ARGB32, flareImage, &flr2) == kQANoErr && QATextureDetach(engine, flr2) == kQANoErr);
	CHECK(QATextureNew(engine, 0, kQAPixel_RGB16, flareImage, &bad) == paramErr && bad == nullptr);

	QASetInt(c, kQATag_ZFunction, kQAZFunction_None);
	QASetFloat(c, kQATag_ColorBG_r, 0);
	QASetFloat(c, kQATag_ColorBG_g, 0);
	QASetFloat(c, kQATag_ColorBG_b, 1);
	QARenderStart(c, nullptr, nullptr);
	QASetPtr(c, kQATag_Texture, vto1);
	CHECK(QAGetInt(c, kQATag_TextureFilter) == kQATextureFilter_Fast);

	// One texel per pixel, w = 2 everywhere: None shows the texel, Modulate texel * kd, Highlight adds ks.
	// The points are on opaque texels of levels 0 and 2.
	static const int kPoints[][2] = {{3, 5}, {40, 47}, {100, 100}, {120, 90}, {204, 120}, {250, 230}};
	for(auto& p : kPoints) {
		Texel(venice[0], p[0], p[1], t);
		CHECK(t[3] > 0.99f);
		Texel(venice[2], p[0] / 4, p[1] / 4, t);
		CHECK(t[3] > 0.99f);
	}
	for(long op : {0L, 1L, 3L}) {	// None, Modulate, Modulate + Highlight {
		QASetInt(c, kQATag_TextureOp, op);
		TexQuad(c, 0, 0, 256, 256, 0.5, 0.5, 1);
		for(auto& p : kPoints) {
			Texel(venice[0], p[0], p[1], t);
			if(op & kQATextureOp_Modulate) { t[0] *= 1; t[1] *= 0.5; t[2] *= 0.25; }
			if(op & kQATextureOp_Highlight) { t[0] = std::fmin(1, t[0] + 0.25f); t[1] = std::fmin(1, t[1] + 0.125f); t[2] = std::fmin(1, t[2] + 0.5f); }
			CHECK_PIXEL(p[0], p[1], t[0], t[1], t[2]);
		}
	}

	// Perspective: w 1 on the left edge, 4 on the right. In the top-right triangle, at screen fractions
	// (s, y) with y < s, 1/w = 1 - 3s/4, so u = (s/4) / (1 - 3s/4) and t = 1 - v = (y/4) / (1 - 3s/4).
	QASetInt(c, kQATag_TextureOp, kQATextureOp_None);
	TexQuad(c, 0, 0, 256, 256, 1, 0.25, 1);
	// Only opaque texels whose centre is within a quarter texel of the sample point are checked.
	int checked = 0;
	for(int y = 4; y < 240; y += 8)
		for(int x = y + 4; x < 256; x += 8) {
			float s = (x + 0.5f) / 256, iw = 1 - 0.75f * s, u = (s * 0.25f / iw) * 256, v = ((y + 0.5f) / 256 * 0.25f / iw) * 256;
			if(std::fabs(u - (int) u - 0.5f) > 0.25f || std::fabs(v - (int) v - 0.5f) > 0.25f)
				continue;
			Texel(venice[0], (int) u, (int) v, t);
			if(t[3] < 0.99f)
				continue;
			CHECK_PIXEL(x, y, t[0], t[1], t[2]);
			++checked;
		}
	CHECK(checked > 50);

	// Filters, Vto1 magnified 3x (texel k centred on pixel 3k+1): Fast is the nearest texel, Mid
	// interpolates. Then Vto1 shown at quarter size: Best samples mip level 2 (bilinear on level 0 would not).
	QARenderStart(c, nullptr, nullptr);
	TexQuad(c, 0, 0, 768, 768, 1, 1, 1);
	Texel(venice[0], 32, 33, t);
	CHECK_PIXEL(32 * 3 + 2, 33 * 3 + 1, t[0], t[1], t[2]);	// nearest, though off the texel centre
	QASetInt(c, kQATag_TextureFilter, kQATextureFilter_Mid);
	TexQuad(c, 0, 0, 768, 768, 1, 1, 1);
	{
		// Pixel centre 98.5 is texel coordinate 32.833: 2/3 of texel 32 and 1/3 of texel 33 (row 33).
		float a[4], b[4];
		Texel(venice[0], 32, 33, a);
		Texel(venice[0], 33, 33, b);
		CHECK(a[3] > 0.99f && b[3] > 0.99f);
		CHECK(std::fabs(a[0] - b[0]) + std::fabs(a[1] - b[1]) + std::fabs(a[2] - b[2]) > 0.05f);	// distinguishes Mid from Fast
		CHECK_PIXEL(32 * 3 + 2, 33 * 3 + 1, (a[0] * 2 + b[0]) / 3, (a[1] * 2 + b[1]) / 3, (a[2] * 2 + b[2]) / 3);
	}
	QASetInt(c, kQATag_TextureFilter, kQATextureFilter_Best);
	TexQuad(c, 0, 0, 64, 64, 1, 1, 1);
	for(auto& p : kPoints) {
		Texel(venice[2], p[0] / 4, p[1] / 4, t);
		CHECK_PIXEL(p[0] / 4, p[1] / 4, t[0], t[1], t[2]);
	}

	// Alpha blend over the blue background: texel alpha times vertex a, with op None and Modulate.
	QARenderStart(c, nullptr, nullptr);
	QASetPtr(c, kQATag_Texture, flr2);
	QASetInt(c, kQATag_TextureFilter, kQATextureFilter_Fast);
	int ax = -1, ay = -1;
	for(int y = 0; y < 64 && ax < 0; ++y)
		for(int x = 0; x < 64 && ax < 0; ++x) {
			Texel(flare[0], x, y, t);
			if(t[3] > 0.3f && t[3] < 0.7f) { ax = x; ay = y; }
		}
	CHECK(ax >= 0);
	Texel(flare[0], ax, ay, t);
	for(long op : {(long) kQATextureOp_None, (long) kQATextureOp_Modulate}) {
		QARenderStart(c, nullptr, nullptr);
		QASetInt(c, kQATag_TextureOp, op);
		TexQuad(c, 0, 0, 64, 64, 1, 1, 0.5);
		float a = t[3] * 0.5f, g = (op == kQATextureOp_Modulate) ? 0.5f : 1, b = (op == kQATextureOp_Modulate) ? 0.25f : 1;
		CHECK_PIXEL(ax, ay, t[0] * a, t[1] * g * a, t[2] * b * a + (1 - a));
	}

	// Bitmaps: Tsmk at (100, 20) unscaled, blended by its alpha; a Pomme-style A,R,G,B GWorld (RGB32, alpha ignored).
	unsigned char		argb[2 * 4] = {0x00, 0xFF, 0x00, 0x00,  0x00, 0x00, 0x80, 0xFF};
	TQAImage			gworldImage = {2, 1, 8, (Ptr) argb};
	TQAVGouraud			bv = V(100, 20, 0.5, 1, 1, 1, 1, 1);
	CHECK(QABitmapNew(engine, kQABitmap_NoCompression, kQAPixel_ARGB32, flareImage, &bitmap) == kQANoErr && QABitmapDetach(engine, bitmap) == kQANoErr);
	CHECK(QABitmapNew(engine, kQABitmap_None, kQAPixel_RGB32, &gworldImage, &gworld) == kQANoErr);
	QARenderStart(c, nullptr, nullptr);
	QADrawBitmap(c, &bv, bitmap);
	CHECK_PIXEL(100 + ax, 20 + ay, t[0] * t[3], t[1] * t[3], t[2] * t[3] + (1 - t[3]));
	CHECK_PIXEL(100 + 64, 20 + 33, 0, 0, 1);	// right of the bitmap
	bv.x = 10; bv.y = 200;
	QADrawBitmap(c, &bv, gworld);
	CHECK_PIXEL(10, 200, 1, 0, 0);
	CHECK_PIXEL(11, 200, 0, 0x80 / 255.0f, 1);
	QARenderEnd(c, nullptr);

	QASetInt(c, kQATag_ZFunction, kQAZFunction_LT);
	QABitmapDelete(engine, gworld);
	QABitmapDelete(engine, bitmap);
	QATextureDelete(engine, flr2);
	QATextureDelete(engine, vto1);
}

// Expected fog blend of red with fogColour at eye depth w, by the RAVE formulas.
static void CheckFog(int line, int x, int y, long mode, float w, const float fog[3])
{
	float f = mode == kQAFogMode_Linear ? (10 - w) / (10 - 2) : mode == kQAFogMode_Exponential ? std::exp(-0.2f * w) : std::exp(-(0.15f * w) * (0.15f * w));
	f = std::clamp(f, 0.0f, 1.0f);
	CheckPixel(line, x, y, f + (1 - f) * fog[0], (1 - f) * fog[1], (1 - f) * fog[2]);
}

static void TestFog(TQADrawContext* c)
{
	const float		fog[3] = {0.2f, 0.6f, 1.0f};
	const float		ws[] = {1, 2, 4, 6, 8, 12};

	QASetInt(c, kQATag_ZFunction, kQAZFunction_None);
	QASetFloat(c, kQATag_FogColor_r, fog[0]);
	QASetFloat(c, kQATag_FogColor_g, fog[1]);
	QASetFloat(c, kQATag_FogColor_b, fog[2]);
	QASetFloat(c, kQATag_FogColor_a, 1);
	QASetFloat(c, kQATag_FogStart, 2);
	QASetFloat(c, kQATag_FogEnd, 10);
	QASetFloat(c, kQATag_FogMaxDepth, 100);
	QARenderStart(c, nullptr, nullptr);

	// Red quads at constant w through the mesh path (top row) and QADrawTriGouraud (second row).
	for(long mode : {(long) kQAFogMode_Linear, (long) kQAFogMode_Exponential, (long) kQAFogMode_ExponentialSquared}) {
		QASetInt(c, kQATag_FogMode, mode);
		QASetFloat(c, kQATag_FogDensity, mode == kQAFogMode_Exponential ? 0.2f : 0.15f);
		for(int i = 0; i < 6; ++i) {
			float			l = i * 50, t = (mode - 1) * 60;
			TQAVGouraud		v0 = V(l, t + 30, 0.5, 1 / ws[i], 1, 0, 0, 1), v1 = V(l + 40, t + 30, 0.5, 1 / ws[i], 1, 0, 0, 1), v2 = V(l, t + 50, 0.5, 1 / ws[i], 1, 0, 0, 1);
			Quad(c, l, t, l + 40, t + 20, 0.5, 1 / ws[i], 1, 0, 0, 1);
			QADrawTriGouraud(c, &v0, &v1, &v2, 0);
			CheckFog(__LINE__, l + 20, t + 10, mode, ws[i], fog);
			CheckFog(__LINE__, l + 5, t + 35, mode, ws[i], fog);
		}
	}

	// Linear fog from 0 to 8 across a quad with w 1 on the left and 4 on the right: the fog follows the
	// true eye depth 1 / (1 - 3s/4) at screen fraction s, not w interpolated linearly in screen space.
	QASetInt(c, kQATag_FogMode, kQAFogMode_Linear);
	QASetFloat(c, kQATag_FogStart, 0);
	QASetFloat(c, kQATag_FogEnd, 8);
	TQAVGouraud			v[4] = {V(0, 190, 0.5, 1, 1, 0, 0, 1), V(256, 190, 0.5, 0.25, 1, 0, 0, 1), V(256, 240, 0.5, 0.25, 1, 0, 0, 1), V(0, 240, 0.5, 1, 1, 0, 0, 1)};
	TQAIndexedTriangle	tris[2] = {{{0, 1, 2}, 0}, {{2, 3, 0}, 0}};
	QASubmitVerticesGouraud(c, 4, v);
	QADrawTriMeshGouraud(c, 2, tris);
	for(int x = 16; x < 256; x += 32) {
		float s = (x + 0.5f) / 256, w = 1 / (1 - 0.75f * s), f = (8 - w) / 8;
		CHECK_PIXEL(x, 215, f + (1 - f) * fog[0], (1 - f) * fog[1], (1 - f) * fog[2]);
	}

	// Fog None: unfogged.
	QASetInt(c, kQATag_FogMode, kQAFogMode_None);
	Quad(c, 300, 0, 320, 20, 0.5, 1 / 12.0f, 1, 0, 0, 1);
	CHECK_PIXEL(310, 10, 1, 0, 0);
	QARenderEnd(c, nullptr);
	QASetInt(c, kQATag_ZFunction, kQAZFunction_LT);
}

// Pomme-style 640x480 ARGB32 GWorld of the interface and the copied sub-rect, for BufferCopy below.
static std::vector<unsigned char>	gInterface;
static const long					kInterfaceRowBytes = 640 * 4;

// Same copy as Buffer_InitializeMethod_BufferCopy (Infinity rendering.cp): the context-sized sub-rect
// at (kLeft, kTop) of the GWorld into the memory device, bottom row first, 8 bytes at a time.
static void BufferCopy(const TQADrawContext*, const TQADevice* buffer, const TQARect*, void*)
{
	if(buffer->deviceType != kQADeviceMemory)
		return;
	const TQADeviceMemory*	memoryDevice = &buffer->device.memoryDevice;
	Ptr						sourceBaseAddress = (Ptr) gInterface.data() + kInterfaceRowBytes * kTop + 4 * kLeft + kHeight * kInterfaceRowBytes,
							destBaseAddress = memoryDevice->baseAddr + kHeight * memoryDevice->rowBytes;
	long					height = kHeight;
	do {
		sourceBaseAddress -= kInterfaceRowBytes;
		destBaseAddress -= memoryDevice->rowBytes;
		std::copy((double*) sourceBaseAddress, (double*) sourceBaseAddress + kWidth * 4 / 8, (double*) destBaseAddress);
	} while(--height);
}

// BufferCopy initialise method: the interface pixels show behind a 3D quad, whose depth starts cleared.
static void TestBufferCopy(TQADrawContext* c)
{
	int				w, h, n;
	unsigned char*	rgb = stbi_load(WATERRACE_TEXTURES_DIR "/../Interface PNG/2000.png", &w, &h, &n, 3);
	TQANoticeMethod	method;

	CHECK(rgb != nullptr && w == 640 && h == 480);
	if(rgb == nullptr)
		return;
	gInterface.resize(640 * 480 * 4);
	for(int i = 0; i < 640 * 480; ++i) {
		gInterface[i * 4 + 0] = 0xFF;
		gInterface[i * 4 + 1] = rgb[i * 3 + 0];
		gInterface[i * 4 + 2] = rgb[i * 3 + 1];
		gInterface[i * 4 + 3] = rgb[i * 3 + 2];
	}

	glDisable(GL_SCISSOR_TEST);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	method.bufferNoticeMethod = BufferCopy;
	QASetNoticeMethod(c, kQAMethod_ImageBufferInitialize, method, nullptr);
	for(int frame = 0; frame < 2; ++frame) {	// the second frame reuses the background texture
		QARenderStart(c, nullptr, nullptr);
		Quad(c, 80, 60, 240, 180, 0.99, 1, 1, 0.5, 0, 1);	// far, but the background wrote no depth
	}
	int		checked = 0;
	for(int y = 0; y < kHeight; y += 7)
		for(int x = 0; x < kWidth; x += 11) {
			const unsigned char*	p = rgb + ((kTop + y) * 640 + kLeft + x) * 3;
			if(x >= 80 && x < 240 && y >= 60 && y < 180)
				CHECK_PIXEL(x, y, 1, 0.5, 0);
			else
				CHECK_PIXEL(x, y, p[0] / 255.0f, p[1] / 255.0f, p[2] / 255.0f), ++checked;
		}
	CHECK(checked > 500);
	CHECK_PIXEL(-5, 5, 0, 0, 0);	// outside the context: untouched
	QARenderEnd(c, nullptr);

	std::vector<unsigned char>	shot(640 * 480 * 4);
	glReadPixels(0, 0, 640, 480, GL_RGBA, GL_UNSIGNED_BYTE, shot.data());
	stbi_flip_vertically_on_write(1);
	CHECK(stbi_write_png("ogl-support-buffercopy.png", 640, 480, 4, shot.data(), 640 * 4));
	stbi_image_free(rgb);
}

int main()
{
	TQAEngine*			engine;
	TQADrawContext*		c;
	TQARect				bounds = {kLeft, kTop, kLeft + kWidth, kTop + kHeight};
	long				l;
	char				name[16] = {};
	TQANoticeMethod		method;
	void*				refCon;

	CHECK(SDL_Init(SDL_INIT_VIDEO));
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_Window* window = SDL_CreateWindow("test-ogl-support", 640, 480, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
	if(window == nullptr || SDL_GL_CreateContext(window) == nullptr) {
		std::fprintf(stderr, "FAIL: no GL window: %s\n", SDL_GetError());
		return 1;
	}

	// Engine enumeration
	engine = QADeviceGetFirstEngine(nullptr);
	CHECK(engine != nullptr);
	CHECK(QADeviceGetNextEngine(nullptr, engine) == nullptr);
	CHECK(QAEngineEnable(kQAVendor_Apple, kQAEngine_AppleHW) == kQANoErr);
	CHECK(QAEngineGestalt(engine, kQAGestalt_OptionalFeatures2, &l) == kQANoErr && (l & kQAOptional2_DrawContextScale) && (l & kQAOptional2_BitmapScale));
	CHECK(QAEngineGestalt(engine, kQAGestalt_ASCIINameLength, &l) == kQANoErr && l > 0 && l < (long) sizeof(name));
	CHECK(QAEngineGestalt(engine, kQAGestalt_ASCIIName, name) == kQANoErr && name[0] != 0);

	CHECK(QADrawContextNew(nullptr, &bounds, nullptr, engine, 0, &c) == kQANoErr);
	CHECK(QAGetInt(c, kQATag_ZFunction) == kQAZFunction_LT);
	QASetInt(c, kQATag_Antialias, 1);
	QASetFloat(c, kQATag_BitmapScale_x, 0.5);
	QASetInt(c, kQATag_Blend, kQABlend_Interpolate);
	CHECK(QAGetInt(c, kQATag_Blend) == kQABlend_Interpolate);

	// Clear the window to black, then QARenderStart clears only the context, to ColorBG.
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	QASetFloat(c, kQATag_ColorBG_r, 0);
	QASetFloat(c, kQATag_ColorBG_g, 0);
	QASetFloat(c, kQATag_ColorBG_b, 1);
	QARenderStart(c, nullptr, nullptr);
	CHECK_PIXEL(5, 5, 0, 0, 1);
	CHECK_PIXEL(-5, 5, 0, 0, 0);					// scissored: left of the context
	CHECK_PIXEL(kWidth + 5, kHeight - 5, 0, 0, 0);	// right of the context

	// Colour interpolation: red (0,0), green (320,0), blue (0,240), z 0.5.
	TQAVGouraud v0 = V(0, 0, 0.5, 1, 1, 0, 0, 1), v1 = V(kWidth, 0, 0.5, 1, 0, 1, 0, 1), v2 = V(0, kHeight, 0.5, 1, 0, 0, 1, 1);
	QADrawTriGouraud(c, &v0, &v1, &v2, 0);
	for(int y = 10; y < 120; y += 50)
		for(int x = 10; x < 150; x += 60) {
			float g = (x + 0.5f) / kWidth, b = (y + 0.5f) / kHeight;
			CHECK_PIXEL(x, y, 1 - g - b, g, b);
		}

	// Depth: a white quad behind (z 0.7) shows only outside the triangle; a yellow one in front
	// (z 0.3, invW 0.25) covers it.
	Quad(c, 100, 100, 300, 230, 0.7, 1, 1, 1, 1, 1);
	CHECK_PIXEL(110, 105, 1 - 110.5f / kWidth - 105.5f / kHeight, 110.5f / kWidth, 105.5f / kHeight);	// triangle in front
	CHECK_PIXEL(250, 200, 1, 1, 1);
	Quad(c, 0, 0, 60, 60, 0.3, 0.25, 1, 1, 0, 1);
	CHECK_PIXEL(30, 30, 1, 1, 0);
	Quad(c, 0, 0, 60, 60, 0.4, 1, 0, 1, 1, 1);	// behind the yellow quad: hidden
	CHECK_PIXEL(30, 30, 1, 1, 0);

	// ZFunction None: a far quad still draws.
	QASetInt(c, kQATag_ZFunction, kQAZFunction_None);
	Quad(c, 0, 0, 60, 60, 0.99, 1, 1, 0, 1, 1);
	CHECK_PIXEL(30, 30, 1, 0, 1);

	// Blend over the blue background (bottom right, outside the triangle and quads).
	Quad(c, 305, 100, 315, 110, 0, 1, 1, 0, 0, 0.5);
	CHECK_PIXEL(310, 105, 0.5, 0, 0.5);
	QASetInt(c, kQATag_Blend, kQABlend_PreMultiply);
	Quad(c, 305, 120, 315, 130, 0, 1, 1, 0, 0, 0.5);
	CHECK_PIXEL(310, 125, 1, 0, 0.5);
	QASetInt(c, kQATag_Blend, kQABlend_Interpolate);
	QASetInt(c, kQATag_ZFunction, kQAZFunction_LT);

	// Line from red to green along y = 235.5, over the blue background.
	TQAVGouraud l0 = V(60, 235.5, 0.1, 1, 1, 0, 0, 1), l1 = V(300, 235.5, 0.1, 1, 0, 1, 0, 1);
	QADrawLine(c, &l0, &l1);
	CHECK_PIXEL(61, 235, 1, 0, 0);
	CHECK_PIXEL(180, 235, 0.5, 0.5, 0);
	CHECK_PIXEL(180, 238, 0, 0, 1);	// background below the line, untouched

	QARenderEnd(c, nullptr);
	CHECK(QAFlush(c) == kQANoErr);

	// Notice method: called by QARenderStart instead of the colour clear; depth is still cleared.
	method.bufferNoticeMethod = Notice;
	QASetNoticeMethod(c, kQAMethod_ImageBufferInitialize, method, &gNoticeCalls);
	CHECK(QAGetNoticeMethod(c, kQAMethod_ImageBufferInitialize, &method, &refCon) == kQANoErr && method.bufferNoticeMethod == Notice && refCon == &gNoticeCalls);
	QARenderStart(c, nullptr, nullptr);
	CHECK(gNoticeCalls == 1);
	CHECK_PIXEL(250, 200, 1, 1, 1);			// colour kept
	Quad(c, 240, 190, 260, 210, 0.9, 1, 0, 1, 0, 1);	// behind the old white quad, but depth was cleared
	CHECK_PIXEL(250, 200, 0, 1, 0);
	QARenderEnd(c, nullptr);

	// kQAContext_Scale: a 160x120 drawing context shown in the 320x240 rectangle.
	TQARect scaled[2] = {{kLeft, kTop, kLeft + kWidth / 2, kTop + kHeight / 2}, bounds};
	TQADrawContext* s;
	CHECK(QADrawContextNew(nullptr, scaled, nullptr, engine, kQAContext_Scale, &s) == kQANoErr);
	QARenderStart(s, nullptr, nullptr);
	CHECK_PIXEL(kWidth - 5, kHeight - 5, 0, 0, 0);	// ColorBG defaults to black
	Quad(s, 80, 60, 160, 120, 0.5, 1, 1, 0, 0, 1);	// bottom-right quarter of the drawing
	CHECK_PIXEL(kWidth - 5, kHeight - 5, 1, 0, 0);
	CHECK_PIXEL(kWidth / 2 - 5, kHeight / 2 - 5, 0, 0, 0);
	QARenderEnd(s, nullptr);
	QADrawContextDelete(s);

	// Textures on a fresh context, without the notice method.
	TQADrawContext* tc;
	CHECK(QADrawContextNew(nullptr, &bounds, nullptr, engine, 0, &tc) == kQANoErr);
	TestTextures(engine, tc);
	TestFog(tc);
	TestBufferCopy(tc);
	QADrawContextDelete(tc);

	CHECK(glGetError() == GL_NO_ERROR);
	QADrawContextDelete(c);
	SDL_Quit();

	std::printf("ogl-support: %s (%d failures)\n", gFailures ? "FAILED" : "passed", gFailures);
	return gFailures ? 1 : 0;
}
