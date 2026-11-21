// FX shaders on GL (issue #46): opens the game window (xvfb-run) and draws one frame of a scene through the engine's
// own shaders: a sky (Terrain_RenderSky), a sprite shape (Sprite_DrawShape), two particle bursts (Particle_DrawShape),
// a lens shape (Shape_DrawLens), a wireframe cube (WireFrame_DrawShape_NoShading) and the Post FX lens flare.
// Each FX is checked against a software composite of the quads the shader submitted (vTextureBuffer) over the
// pixels read back before it, with the RAVE texture op and blend mode, so the alpha and blending are checked, not
// only the geometry. The frame is written to fx-scene.png in the current directory.
// Linked with every game file; Boot.cp is included here so its main() can be renamed.
// Usage: test-fx [GAME_ROOT]   (GAME_ROOT holds "WaterRace Data/")

#include <algorithm>
#include <cmath>
#include <map>
#include <unistd.h>

#define main WaterRace_Main
#include "Boot.cp"
#undef main

#include "Infinity rendering.h"
#include "Infinity Post FX.h"
#include "Textures.h"
#include "TextureLoader.h"

static int		gFailures = 0;

#define CHECK(cond) do { if(!(cond)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++gFailures; } } while(0)

struct TQATexture { GLuint name; bool mipmap; };	// mirrors OGL_Support.cp

typedef std::vector<unsigned char> Image;			// 640x480 RGBA, top row first: the context covers the window

static const int	kW = kWindowWidth,
					kH = kWindowHeight;

static Image ReadContext()
{
	Image			raw(kW * kH * 4),
					image(kW * kH * 4);

	glReadBuffer(GL_BACK);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, kW, kH, GL_RGBA, GL_UNSIGNED_BYTE, raw.data());
	for(int y = 0; y < kH; ++y)
	std::copy(raw.data() + (kH - 1 - y) * kW * 4, raw.data() + (kH - y) * kW * 4, image.data() + y * kW * 4);
	return image;
}

struct Texels { int width, height; Image pixels; };

// The texture as GL holds it (level 0), so the reference samples exactly what was uploaded.
static Texels ReadTexture(TQATexture* texture)
{
	Texels			t;

	glBindTexture(GL_TEXTURE_2D, texture->name);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &t.width);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &t.height);
	t.pixels.resize(t.width * t.height * 4);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, t.pixels.data());
	return t;
}

static TQATexture* LoadTexture(TQAEngine* engine, const char* png, short alphaDepth)
{
	std::vector<TextureLevel>	levels;
	TQAImage					image = {};
	TQATexture*					texture = nil;

	CHECK(Texture_LoadPNG(std::string("WaterRace Data/Textures/") + png, 0, alphaDepth, 0, levels));
	if(levels.empty())
	return nil;
	image.width = levels[0].width;
	image.height = levels[0].height;
	image.rowBytes = levels[0].width * 4;
	image.pixmap = (Ptr) levels[0].pixels.data();
	CHECK(QATextureNew(engine, kQATexture_NoCompression, kQAPixel_ARGB32, &image, &texture) == kQANoErr);
	return texture;
}

// Software RAVE for the screen-aligned quads in vTextureBuffer (top-left, top-right, bottom-right, bottom-left,
// constant invW, as Texture_DrawSprites and Texture_DrawSpriteOnScreen build them): nearest texel, texture op
// (Modulate by kd, Highlight adds ks), then Interpolate or PreMultiply blending over image. Returns the quad count;
// the buffer is cleared to invW 0 before the draw, which ends the list.
static long Composite(Image& image, const Texels& tex, long textureOp, long blend)
{
	long			quads = 0;

	for(const TQAVTexture* q = vTextureBuffer; q->invW != 0; q += 4, ++quads) {
		float	iw = q[0].invW,
				l = q[0].x, t = q[0].y, r = q[2].x, b = q[2].y,
				u0 = q[0].uOverW / iw, u1 = q[2].uOverW / iw,
				t0 = 1 - q[0].vOverW / iw, t1 = 1 - q[2].vOverW / iw;	// GL t runs down from the top row

		for(int y = (int) std::ceil(t - 0.5f); y + 0.5f < b; ++y)
		for(int x = (int) std::ceil(l - 0.5f); x + 0.5f < r; ++x) {
			if(x < 0 || y < 0 || x >= kW || y >= kH)
			continue;
			int				tx = std::clamp((int) std::floor((u0 + (x + 0.5f - l) / (r - l) * (u1 - u0)) * tex.width), 0, tex.width - 1),
							ty = std::clamp((int) std::floor((t0 + (y + 0.5f - t) / (b - t) * (t1 - t0)) * tex.height), 0, tex.height - 1);
			const unsigned char*	texel = &tex.pixels[(ty * tex.width + tx) * 4];
			unsigned char*			dst = &image[(y * kW + x) * 4];
			float			kd[3] = {q[0].kd_r, q[0].kd_g, q[0].kd_b},
							ks[3] = {q[0].ks_r, q[0].ks_g, q[0].ks_b},
							a = texel[3] / 255.0f * q[0].a;

			for(int c = 0; c < 3; ++c) {
				float	src = texel[c] / 255.0f * ((textureOp & kQATextureOp_Modulate) ? kd[c] : 1) + ((textureOp & kQATextureOp_Highlight) ? ks[c] : 0),
						d = dst[c] / 255.0f;

				src = std::min(src, 1.0f);
				d = blend == kQABlend_PreMultiply ? src + d * (1 - a) : src * a + d * (1 - a);
				dst[c] = (unsigned char) std::lround(std::min(d, 1.0f) * 255);
			}
		}
	}
	return quads;
}

static void ClearTextureBuffer(long count)
{
	for(long i = 0; i < count; ++i)
	vTextureBuffer[i].invW = 0;
}

// Compares the frame with the expected image: returns how many pixels the FX changed, after logging and counting
// as failures the pixels off by more than 6/255 (more than 1% of the changed ones fails: edges may round apart).
static long Compare(const char* what, const Image& before, const Image& expected, const Image& after)
{
	long			changed = 0,
					wrong = 0;

	for(int i = 0; i < kW * kH; ++i) {
		int		d = 0,
				c = 0;

		for(int k = 0; k < 3; ++k) {
			d = std::max(d, std::abs(expected[i * 4 + k] - after[i * 4 + k]));
			c = std::max(c, std::abs(expected[i * 4 + k] - before[i * 4 + k]));
		}
		changed += c > 6;
		wrong += d > 6;
	}
	printf("%s: %ld pixels changed, %ld off the reference\n", what, changed, wrong);
	CHECK(wrong * 100 <= changed);
	return changed;
}

static ShapePtr NewShape(unsigned short shading, TQATexture* texture, Ptr data)
{
	ShapePtr		shape = (ShapePtr) NewPtrClear(sizeof(Shape));

	shape->shading = shading;
	shape->texture = texture != nil ? 'test' : kNoTexture;
	shape->texturePtr = texture;
	shape->dataPtr = data;
	shape->alpha = 1;
	Shape_ResolveRendering(shape);
	return shape;
}

// World point (camera space: the camera is the identity) to screen pixels, as the shaders project.
static void Project(StatePtr state, const Vector& p, float* x, float* y)
{
	*x = p.x / p.z * state->pixelConversion + state->projectionOffset_X;
	*y = p.y / p.z * state->pixelConversion + state->projectionOffset_Y;
}

int main(int argc, char** argv)
{
	const char*		root = argc > 1 ? argv[1] : WATERRACE_ROOT;
	TQAEngine*		engine;
	StatePtr		state;
	Point			size = {kH, kW},
					position = {0, 0};
	Matrix			identity;
	Vector			cameraPos = {0, 0, 0};
	Image			before,
					expected,
					after;
	std::string		shot = std::string(getcwd(nil, 0)) + "/fx-scene.png";	// written where the test runs, before chdir

	if(chdir(root) != 0) {
		perror(root);
		return 1;
	}
	Pomme::Init();
	engine = QADeviceGetFirstEngine(nil);
	if(!Boot_InitGL() || InfinityRendering_AllocateBuffers() != noErr || InfinityRendering_CreateContext(nil, engine, &state, nil, size, position, 0) != noErr) {
		fprintf(stderr, "FAIL: GL window or rendering context\n");
		return 1;
	}
	Matrix_Clear(&identity);

	TQATexture		*skyTexture = LoadTexture(engine, "Core/Practice Zone/sky .png", 0),
					*spriteTexture = LoadTexture(engine, "Core/Practice Zone/Flr1.png", 8),
					*smokeTexture = LoadTexture(engine, "Locations/Arctic/Tsmk.png", 8),
					*wakeTexture = LoadTexture(engine, "Core/G-Boat/Fx05.png", 8),
					*lensTexture = nil;
	CHECK(Texture_NewARGB32TextureFromPictResource(engine, &lensTexture, 7001, 0) == noErr);
	CHECK(PostFX_LensFlare_Init(state, 0) == noErr);
	if(gFailures)
	return 1;

	InfinityRendering_ColorClear(state, 1, 0, 1);
	QARenderStart(state->drawContext, nil, nil);

	// Sky: a textured quad at z 50 filling the view, u left to right and v bottom to top (texture op None, alpha c).
	// At that depth the mapping is affine: pixel (x, y) shows texel ((x + 0.5) / 640, (y + 0.5) / 480).
	{
		Vertex			points[4] = {{{10, 7.5, 50}, 0, 1, 1}, {{-10, 7.5, 50}, 1, 1, 1}, {{-10, -7.5, 50}, 1, 0, 1}, {{10, -7.5, 50}, 0, 0, 1}};
		TriFace			triangles[2] = {{{0, 1, 2}}, {{2, 3, 0}}};
		ShapePtr		sky = NewShape(kShader_None, skyTexture, nil);
		Texels			texels = ReadTexture(skyTexture);
		int				checked = 0;

		sky->pointCount = 4;
		sky->pointList = points;
		sky->triangleCount = 2;
		sky->triangleList = triangles;
		Terrain_RenderSky(sky, state, &identity, &identity);
		after = ReadContext();
		for(int y = 4; y < kH; y += 9)
		for(int x = 3; x < kW; x += 7) {
			float			s = (x + 0.5f) / kW * texels.width,
							t = (y + 0.5f) / kH * texels.height;
			if(std::fabs(s - std::floor(s) - 0.5f) > 0.4f || std::fabs(t - std::floor(t) - 0.5f) > 0.4f)
			continue;	// too near a texel edge for nearest sampling
			const unsigned char*	texel = &texels.pixels[((int) t * texels.width + (int) s) * 4];
			const unsigned char*	p = &after[(y * kW + x) * 4];
			if(std::abs(p[0] - texel[0]) > 3 || std::abs(p[1] - texel[1]) > 3 || std::abs(p[2] - texel[2]) > 3) {
				fprintf(stderr, "FAIL sky pixel (%d,%d) = %d,%d,%d, texel %d,%d,%d\n", x, y, p[0], p[1], p[2], texel[0], texel[1], texel[2]);
				++gFailures;
			}
			++checked;
		}
		printf("sky: %d pixels checked against the texture\n", checked);
		CHECK(checked > 1000);
		DisposePtr((Ptr) sky);
	}

	// Sprite shape (top-left quarter): 64 pixels wide at z 2, Modulate + Highlight (hiColor), Interpolate blend.
	{
		ShapeData_SpritePtr	data = (ShapeData_SpritePtr) NewPtrClear(sizeof(ShapeData_Sprite));
		ShapePtr			shape = NewShape(kShader_Sprite, spriteTexture, (Ptr) data);
		float				x, y;

		data->localPosition = {0.2, 0.15, 2};
		data->sprite.size = 0.08;
		data->sprite.transparency = 0.75;
		data->sprite.intensity = 0.9;
		data->sprite.hiColor_r = 0.1;
		data->sprite.hiColor_g = 0.2;
		data->sprite.hiColor_b = 0;
		before = expected = ReadContext();
		ClearTextureBuffer(8);
		Shape_CallRenderingProc(shape, state, &identity, &identity, &cameraPos, nil, kUndetermined);
		after = ReadContext();
		CHECK(Composite(expected, ReadTexture(spriteTexture), kQATextureOp_Modulate | kQATextureOp_Highlight, kQABlend_Interpolate) == 1);
		Project(state, data->localPosition, &x, &y);
		CHECK(std::fabs(x - 160) < 0.01 && std::fabs(y - 120) < 0.01 && std::fabs(vTextureBuffer[0].x - 128) < 0.01 && std::fabs(vTextureBuffer[2].y - 152) < 0.01);
		CHECK(Compare("sprite", before, expected, after) > 500);
		DisposePtr((Ptr) shape);
		DisposePtr((Ptr) data);
	}

	// Particle bursts, 40 particles from one point 0.25 s after the start, drawn without the Z buffer; energy fades
	// their alpha and hiColor. Smoke (Tsmk) blends by its alpha as in the Locations files (top-right quarter); the
	// boat wake (Fx05, alpha 0) is premultiplied as in the Ships files, so it adds its colour (centre).
	for(int premultiplied = 0; premultiplied < 2; ++premultiplied) {
		const long				kCount = 40;
		ShapeData_ParticlePtr	data = (ShapeData_ParticlePtr) NewPtrClear(sizeof(ShapeData_Particle));
		InfinityParticlePtr		particles = (InfinityParticlePtr) NewPtrClear(kCount * sizeof(InfinityParticle));
		TQATexture*				texture = premultiplied ? wakeTexture : smokeTexture;
		ShapePtr				shape = NewShape(kShader_Particle, texture, (Ptr) data);
		long					drawn;

		data->particleCount = kCount;
		data->flags = premultiplied ? kFlag_Premultiplied | kFlag_NoZBuffer : kFlag_NoZBuffer;
		data->gravity = -1;
		data->velocityDecrease = 0.5;
		data->energyDecrease = 0.5;
		data->initPosition = premultiplied ? Vector{0, 0, 2} : Vector{-0.2, 0.15, 2};
		data->initVelocityRandom = {-0.6, -0.6, -0.2};		// negative: spread both ways
		data->initEnergy = 1;
		data->initEnergyRandom = -0.4;
		data->initSize = 0.03;
		data->initSizeRandom = -0.01;
		data->modulate_r = 0.5;
		data->modulate_g = 0.3;
		data->modulate_b = 0.1;
		for(long i = 0; i < kCount; ++i)
		Particle_Init(&particles[i], data, &identity);
		shape->pointCount = kCount;
		shape->pointList = (VertexPtr) particles;
		state->frameTime = 25;
		before = expected = ReadContext();
		ClearTextureBuffer(4 * kCount + 4);
		Shape_CallRenderingProc(shape, state, &identity, &identity, &cameraPos, nil, kUndetermined);
		after = ReadContext();
		drawn = Composite(expected, ReadTexture(texture), kQATextureOp_Modulate | kQATextureOp_Highlight, premultiplied ? kQABlend_PreMultiply : kQABlend_Interpolate);
		printf("particles (%s): %ld of %ld drawn\n", premultiplied ? "premultiplied wake" : "smoke", drawn, kCount);
		CHECK(drawn > kCount / 2 && data->lastTime == 25 && QAGetInt(state->drawContext, kQATag_Blend) == kQABlend_Interpolate);
		CHECK(Compare("particles", before, expected, after) > 2000);
		state->frameTime = 0;
		DisposePtr((Ptr) shape);
		DisposePtr((Ptr) particles);
		DisposePtr((Ptr) data);
	}

	// Lens shape (bottom-left quarter): fake position, 64 pixels wide at depth 0.5, Modulate, PreMultiply blend.
	{
		ShapeData_LensPtr	data = (ShapeData_LensPtr) NewPtrClear(sizeof(ShapeData_Lens));
		ShapePtr			shape = NewShape(kShader_Lens, lensTexture, (Ptr) data);

		data->size = 0.1;
		data->order = 1;
		data->depth = 0.5;
		data->flags = kFlag_FakePosition;
		data->fakePosition = {0.2, -0.15, 2};
		before = expected = ReadContext();
		ClearTextureBuffer(8);
		Shape_CallRenderingProc(shape, state, &identity, &identity, &cameraPos, nil, kUndetermined);
		after = ReadContext();
		CHECK(Composite(expected, ReadTexture(lensTexture), kQATextureOp_Modulate, kQABlend_PreMultiply) == 1);
		CHECK(std::fabs(vTextureBuffer[0].x - 128) < 0.01 && std::fabs(vTextureBuffer[0].y - 328) < 0.01 && QAGetInt(state->drawContext, kQATag_Blend) == kQABlend_Interpolate);
		CHECK(Compare("lens", before, expected, after) > 500);
		DisposePtr((Ptr) shape);
		DisposePtr((Ptr) data);
	}

	// Wireframe cube (bottom-right quarter), turned 30 degrees about y and 20 about x, green lines: every edge
	// midpoint is green within a pixel, and the faces stay unfilled.
	{
		Vertex			points[8];
		TriFace			triangles[12] = {{{0, 1, 3}}, {{0, 3, 2}}, {{4, 6, 7}}, {{4, 7, 5}}, {{0, 4, 5}}, {{0, 5, 1}},
										{{2, 3, 7}}, {{2, 7, 6}}, {{0, 2, 6}}, {{0, 6, 4}}, {{1, 5, 7}}, {{1, 7, 3}}};
		ShapePtr		shape = NewShape(kShader_None, nil, nil);
		float			cy = std::cos(0.5236f), sy = std::sin(0.5236f), cx = std::cos(0.349f), sx = std::sin(0.349f);
		long			changed = 0,
						edges = 0;

		for(int i = 0; i < 8; ++i) {
			float		x = (i & 4) ? 0.05f : -0.05f, y = (i & 2) ? 0.05f : -0.05f, z = (i & 1) ? 0.05f : -0.05f,
						x1 = x * cy + z * sy, z1 = -x * sy + z * cy,
						y2 = y * cx - z1 * sx, z2 = y * sx + z1 * cx;
			points[i] = {{-0.2f + x1, -0.15f + y2, 2 + z2}, 0, 0, 0};
		}
		shape->pointCount = 8;
		shape->pointList = points;
		shape->triangleCount = 12;
		shape->triangleList = triangles;
		WireFrame_SetColorMode(0, 1, 0, true);
		before = ReadContext();
		WireFrame_DrawShape_NoShading(state, shape, &identity, &identity, &cameraPos, nil, kNoClipping);
		after = ReadContext();
		for(int i = 0; i < 12; ++i)
		for(int j = 0; j < 3; ++j) {
			float		x0, y0, x1, y1;
			bool		found = false;

			Project(state, points[triangles[i].corner[j]].point, &x0, &y0);
			Project(state, points[triangles[i].corner[(j + 1) % 3]].point, &x1, &y1);
			int			mx = (int) ((x0 + x1) / 2), my = (int) ((y0 + y1) / 2);
			for(int dy = -1; dy <= 1; ++dy)
			for(int dx = -1; dx <= 1; ++dx) {
				const unsigned char*	p = &after[((my + dy) * kW + mx + dx) * 4];
				found |= p[0] < 4 && p[1] > 251 && p[2] < 4;
			}
			CHECK(found);
			++edges;
		}
		for(int i = 0; i < kW * kH * 4; ++i)
		changed += (i % 4 == 1) && after[i] != before[i];
		printf("wireframe: %ld edges checked, %ld pixels changed\n", edges, changed);
		CHECK(changed > 200 && changed < 2000);	// lines, not the ~6000-pixel filled cube
		DisposePtr((Ptr) shape);
	}

	// Post FX lens flare: the sun at (560, 80); four premultiplied flare sprites, then the whole view brightened
	// by kMaxBrightness (0.5) * (1 - FastSqrt(2 (dx^2 + dy^2) / h^2)) from the centre, here about 0.075.
	{
		float			dx = 240, dy = -160,
						brightness = 0.5f * (1 - FastSqrt(2 * (dx * dx + dy * dy) / (kH * kH)));
		Vector			sun = {-0.15, 0.1, 1};

		state->lightVector = sun;
		before = ReadContext();
		PostFX_LensFlare_Display(state, 50, &identity, &cameraPos);
		after = ReadContext();
		printf("lens flare: brightness %g\n", brightness);
		CHECK(brightness > 0.05 && brightness < 0.1);
		for(int i : {(470 * kW + 630) * 4, (470 * kW + 330) * 4, (10 * kW + 10) * 4})	// outside the four flares
		for(int k = 0; k < 3; ++k)
		CHECK(std::abs(after[i + k] - (before[i + k] * (1 - brightness) + 255 * brightness)) < 3);
		int		gain = 0;
		for(int k = 0; k < 3; ++k)
		gain += after[(80 * kW + 560) * 4 + k] - before[(80 * kW + 560) * 4 + k];
		CHECK(gain > 60);	// the flare over the sun
		CHECK(QAGetInt(state->drawContext, kQATag_Blend) == kQABlend_Interpolate && QAGetInt(state->drawContext, kQATag_ZFunction) == kQAZFunction_LT);
	}

	stbi_flip_vertically_on_write(0);
	CHECK(stbi_write_png(shot.c_str(), kW, kH, 4, after.data(), kW * 4));
	QARenderEnd(state->drawContext, nil);
	CHECK(glGetError() == GL_NO_ERROR);

	PostFX_LensFlare_Quit(state);
	for(TQATexture* t : {skyTexture, spriteTexture, smokeTexture, wakeTexture, lensTexture})
	QATextureDelete(engine, t);
	InfinityRendering_DisposeContext(state);
	InfinityRendering_DisposeBuffers();
	SDL_Quit();
	if(gFailures)
	fprintf(stderr, "%d failure(s)\n", gFailures);
	else
	printf("OK\n");
	return gFailures ? 1 : 0;
}
