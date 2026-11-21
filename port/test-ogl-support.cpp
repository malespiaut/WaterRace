// OGL_Support.cp test (needs a display: ctest runs it under xvfb-run). Draws pre-projected Gouraud
// triangles, meshes and lines through the RAVE calls into a 640x480 window and checks pixels for
// colour interpolation, depth test, blend modes, scissor and the ImageBufferInitialize notice method.
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include "RAVE.h"

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

	CHECK(glGetError() == GL_NO_ERROR);
	QADrawContextDelete(c);
	SDL_Quit();

	std::printf("ogl-support: %s (%d failures)\n", gFailures ? "FAILED" : "passed", gFailures);
	return gFailures ? 1 : 0;
}
