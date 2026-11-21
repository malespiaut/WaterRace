/*****************************************************************************
 * Copyright (c) 1998-2001, French Touch, SARL
 * http://www.french-touch.net
 * info@french-touch.net
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

// RAVE on OpenGL 1.4 fixed function (PORTING.md sec. 4.3, 4.4): one engine, draw contexts as
// viewport + scissor rectangles, state tags, Gouraud triangles, meshes and lines, textures and bitmaps,
// fog over eye depth and the ImageBufferInitialize notice method.
// The types, tags and prototypes are in port/stubs/RAVE.h.

#include				<algorithm>
#include				<cstring>
#include				<vector>

#define					GL_GLEXT_PROTOTYPES		// GL 1.4 secondary colour and fog coordinate
#include				<SDL3/SDL_endian.h>
#include				<SDL3/SDL_opengl.h>

#include				"RAVE.h"

//CONSTANTES:

#define					kTagCount					(kQATag_FogColor_a + 1)
#define					kEngineName					"OpenGL"
#define					kFastTextureMemory			(256L * 1024 * 1024)
#define					kBackgroundUnwritten		0xA5C3E1F7UL		// ImageBufferInitialize buffer pixel no method wrote

//STRUCTURES:

struct TQAEngine
{
	long					unused;
};

struct TQATexture
{
	GLuint					name;
	bool					mipmap;
};

struct TQABitmap
{
	GLuint					name;
	long					width,
							height;
};

struct TQADrawContext
{
	long					width,				// drawing size: vertices are in pixels of this
							height;
	Rect					displayRect;		// where it lands in the 640x480 interface
	unsigned long			flags;
	long					intTags[kTagCount];
	float					floatTags[kTagCount];
	void*					texture;			// kQATag_Texture, drawn by the texture path
	TQANoticeMethod			initializeMethod;
	void*					initializeRefCon;
	std::vector<GLfloat>	vertices;			// QASubmitVerticesGouraud: X,Y,Z,W clip coordinates then R,G,B,A
	std::vector<GLfloat>	textureVertices;	// QASubmitVerticesTexture: X,Y,Z,W, S,T, kd R,G,B,A, ks R,G,B
	std::vector<GLuint>		indices;
	std::vector<UInt32>		background;			// ImageBufferInitialize memory device, Pomme GWorld A,R,G,B pixels
	TQABitmap				backgroundBitmap;
};

//VARIABLES:

static TQAEngine		gEngine;

//ROUTINES:

void Boot_InterfaceToWindow(const Rect* r, int* x, int* y, int* width, int* height); // compositor, Boot.cp
void Boot_BeginFrame();
void Boot_PresentFrame();

TQAEngine* QADeviceGetFirstEngine(TQADevice* device)
{
	return &gEngine;
}

TQAEngine* QADeviceGetNextEngine(TQADevice* device, TQAEngine* engine)
{
	return nil;
}

TQAError QAEngineEnable(long vendor, long engine)
{
	return kQANoErr;
}

// Responses are written as long, the type every caller passes.
TQAError QAEngineGestalt(TQAEngine* engine, long selector, void* response)
{
	switch(selector) {
		case kQAGestalt_EngineID:			*(long*) response = kQAEngine_AppleHW;			break;
		case kQAGestalt_VendorID:			*(long*) response = kQAVendor_Apple;			break;
		case kQAGestalt_Revision:			*(long*) response = kQAVersion_1_6;				break;
		case kQAGestalt_ASCIINameLength:	*(long*) response = sizeof(kEngineName) - 1;	break;
		case kQAGestalt_ASCIIName:			memcpy(response, kEngineName, sizeof(kEngineName) - 1);	break;
		case kQAGestalt_OptionalFeatures:
		case kQAGestalt_OptionalFeatures2:	*(long*) response = ~0L;						break;
		case kQAGestalt_FastTextureMemory:	*(long*) response = kFastTextureMemory;		break;
		default:							return paramErr;
	}

	return kQANoErr;
}

// bounds[0] is the drawing rectangle in global coordinates; with kQAContext_Scale it is shown
// scaled into bounds[1].
TQAError QADrawContextNew(TQADevice* device, const TQARect* bounds, void* notice, TQAEngine* engine, unsigned long flags, TQADrawContext** drawContext)
{
	TQADrawContext*		context = new TQADrawContext();
	const TQARect*		display = (flags & kQAContext_Scale) ? &bounds[1] : &bounds[0];

	context->width = bounds[0].right - bounds[0].left;
	context->height = bounds[0].bottom - bounds[0].top;
	SetRect(&context->displayRect, display->left, display->top, display->right, display->bottom);
	context->flags = flags;
	context->intTags[kQATag_ZFunction] = (flags & kQAContext_NoZBuffer) ? kQAZFunction_None : kQAZFunction_LT;
	context->intTags[kQATag_Blend] = kQABlend_Interpolate;
	context->intTags[kQATag_PerspectiveZ] = kQAPerspectiveZ_Off;
	context->intTags[kQATag_TextureFilter] = kQATextureFilter_Fast;
	context->floatTags[kQATag_ColorBG_a] = 1.0;

	*drawContext = context;
	return kQANoErr;
}

void QADrawContextDelete(TQADrawContext* drawContext)
{
	if(drawContext->backgroundBitmap.name != 0)
	glDeleteTextures(1, &drawContext->backgroundBitmap.name);
	delete drawContext;
}

// Antialias and PerspectiveZ are only stored. Depth always comes from z, which the engine
// computes for either PerspectiveZ setting.
TQAError QASetInt(TQADrawContext* drawContext, long tag, long value)
{
	if(tag < 0 || tag >= kTagCount)
	return paramErr;

	drawContext->intTags[tag] = value;
	return kQANoErr;
}

long QAGetInt(TQADrawContext* drawContext, long tag)
{
	if(tag < 0 || tag >= kTagCount)
	return 0;

	return drawContext->intTags[tag];
}

// BitmapScale is dropped: the GL viewport does the scaling.
TQAError QASetFloat(TQADrawContext* drawContext, long tag, float value)
{
	if(tag < 0 || tag >= kTagCount)
	return paramErr;

	drawContext->floatTags[tag] = value;
	return kQANoErr;
}

TQAError QASetPtr(TQADrawContext* drawContext, long tag, void* value)
{
	if(tag != kQATag_Texture)
	return paramErr;

	drawContext->texture = value;
	return kQANoErr;
}

TQAError QAGetNoticeMethod(TQADrawContext* drawContext, long method, TQANoticeMethod* noticeMethod, void** refCon)
{
	if(method != kQAMethod_ImageBufferInitialize)
	return paramErr;

	*noticeMethod = drawContext->initializeMethod;
	*refCon = drawContext->initializeRefCon;
	return kQANoErr;
}

TQAError QASetNoticeMethod(TQADrawContext* drawContext, long method, TQANoticeMethod noticeMethod, void* refCon)
{
	if(method != kQAMethod_ImageBufferInitialize)
	return paramErr;

	drawContext->initializeMethod = noticeMethod;
	drawContext->initializeRefCon = refCon;
	return kQANoErr;
}

// RAVE fog over eye depth w = 1/invW: linear (end - w) / (end - start), exp(-density w) and
// exp(-(density w)^2), the GL formulas over a fog coordinate of w. FogMaxDepth and FogColor_a are unused.
static void Context_BindFog(TQADrawContext* drawContext)
{
	static const GLint	fogModes[] = {GL_LINEAR, GL_EXP, GL_EXP2};
	long				mode = drawContext->intTags[kQATag_FogMode];

	if(mode < kQAFogMode_Linear || mode > kQAFogMode_ExponentialSquared) {
		glDisable(GL_FOG);
		return;
	}

	glEnable(GL_FOG);
	glHint(GL_FOG_HINT, GL_NICEST);
	glFogi(GL_FOG_COORD_SRC, GL_FOG_COORD);
	glFogi(GL_FOG_MODE, fogModes[mode - kQAFogMode_Linear]);
	glFogfv(GL_FOG_COLOR, &drawContext->floatTags[kQATag_FogColor_r]);
	glFogf(GL_FOG_START, drawContext->floatTags[kQATag_FogStart]);
	glFogf(GL_FOG_END, drawContext->floatTags[kQATag_FogEnd]);
	glFogf(GL_FOG_DENSITY, drawContext->floatTags[kQATag_FogDensity]);
}

// Loads the context's viewport, scissor, projection (PORTING.md sec. 4.4) and state for Gouraud drawing.
static void Context_Bind(TQADrawContext* drawContext)
{
	int				x,
					y,
					width,
					height;
	const GLfloat	projection[16] = {	// column major
						2.0f / drawContext->width, 0, 0, 0,
						0, -2.0f / drawContext->height, 0, 0,
						0, 0, 2, 0,
						-1, 1, -1, 1};

	Boot_InterfaceToWindow(&drawContext->displayRect, &x, &y, &width, &height);
	glViewport(x, y, width, height);
	glScissor(x, y, width, height);
	glEnable(GL_SCISSOR_TEST);

	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(projection);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glDisable(GL_TEXTURE_2D);
	glDisable(GL_COLOR_SUM);
	glDisable(GL_CULL_FACE);
	Context_BindFog(drawContext);

	switch(drawContext->intTags[kQATag_ZFunction]) {
		case kQAZFunction_LT:
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE);
		break;

		case kQAZFunction_True:
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_ALWAYS);
		glDepthMask(GL_TRUE);
		break;

		default: // kQAZFunction_None: neither tested nor written
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		break;
	}

	glEnable(GL_BLEND);
	if(drawContext->intTags[kQATag_Blend] == kQABlend_PreMultiply)
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	else
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static TQAError Texture_Upload(long flags, long pixelFormat, const TQAImage* image, GLuint* name);

// Runs the ImageBufferInitialize notice method; without one, clears to ColorBG. The depth buffer is always cleared.
// The method gets a memory device the size of the context: Buffer_InitializeMethod_BufferCopy copies the
// background GWorld's sub-rect into it, which is then drawn as a bitmap; Buffer_InitializeMethod_None leaves
// it alone, and so the colour already there.
TQAError QARenderStart(TQADrawContext* drawContext, const TQARect* dirtyRect, void* initialContext)
{
	TQADevice		device;
	TQAImage		image = {drawContext->width, drawContext->height, drawContext->width * 4, nil};
	TQAVGouraud		origin = {};
	auto&			background = drawContext->background;

	Boot_BeginFrame();
	Context_Bind(drawContext);
	glDepthMask(GL_TRUE);
	if(drawContext->initializeMethod.bufferNoticeMethod != nil) {
		background.assign(drawContext->width * drawContext->height, kBackgroundUnwritten);
		image.pixmap = (Ptr) background.data();
		device.deviceType = kQADeviceMemory;
		device.device.memoryDevice.baseAddr = image.pixmap;
		device.device.memoryDevice.rowBytes = image.rowBytes;
		device.device.memoryDevice.width = image.width;
		device.device.memoryDevice.height = image.height;
		device.device.memoryDevice.pixelSize = 32;
		drawContext->initializeMethod.bufferNoticeMethod(drawContext, &device, dirtyRect, drawContext->initializeRefCon);
		if(std::find_if(background.begin(), background.end(), [](UInt32 p) { return p != kBackgroundUnwritten; }) != background.end()
			&& Texture_Upload(0, kQAPixel_RGB32, &image, &drawContext->backgroundBitmap.name) == kQANoErr) {
			drawContext->backgroundBitmap.width = image.width;
			drawContext->backgroundBitmap.height = image.height;
			origin.invW = origin.a = 1;
			QADrawBitmap(drawContext, &origin, &drawContext->backgroundBitmap);
		}
		glClear(GL_DEPTH_BUFFER_BIT);
	}
	else {
		glClearColor(drawContext->floatTags[kQATag_ColorBG_r], drawContext->floatTags[kQATag_ColorBG_g],
			drawContext->floatTags[kQATag_ColorBG_b], drawContext->floatTags[kQATag_ColorBG_a]);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	return kQANoErr;
}

// On the Mac the finished buffer went to the screen here: the compositor presents the frame (PORTING.md sec. 6.3).
// shortcut: one present per context; batch them if a screen ever renders two contexts in one frame.
TQAError QARenderEnd(TQADrawContext* drawContext, void* finalContext)
{
	Boot_PresentFrame();
	return kQANoErr;
}

TQAError QAFlush(TQADrawContext* drawContext)
{
	return kQANoErr;
}

// Vertex as (x·w, y·w, z·w, w), w = 1/invW (PORTING.md sec. 4.4).
static void Vertex_Gouraud(const TQAVGouraud* v)
{
	float			w = 1.0f / v->invW;

	glColor4f(v->r, v->g, v->b, v->a);
	glFogCoordf(w);
	glVertex4f(v->x * w, v->y * w, v->z * w, w);
}

void QADrawTriGouraud(TQADrawContext* drawContext, TQAVGouraud* v0, TQAVGouraud* v1, TQAVGouraud* v2, unsigned long flags)
{
	Context_Bind(drawContext);
	glBegin(GL_TRIANGLES);
	Vertex_Gouraud(v0);
	Vertex_Gouraud(v1);
	Vertex_Gouraud(v2);
	glEnd();
}

void QADrawLine(TQADrawContext* drawContext, TQAVGouraud* v0, TQAVGouraud* v1)
{
	Context_Bind(drawContext);
	glBegin(GL_LINES);
	Vertex_Gouraud(v0);
	Vertex_Gouraud(v1);
	glEnd();
}

void QASubmitVerticesGouraud(TQADrawContext* drawContext, long nVertices, TQAVGouraud* vertices)
{
	GLfloat*		dest;
	float			w;

	drawContext->vertices.resize(nVertices * 8);
	dest = drawContext->vertices.data();
	for(long i = 0; i < nVertices; ++i, ++vertices) {
		w = 1.0f / vertices->invW;
		*dest++ = vertices->x * w;
		*dest++ = vertices->y * w;
		*dest++ = vertices->z * w;
		*dest++ = w;
		*dest++ = vertices->r;
		*dest++ = vertices->g;
		*dest++ = vertices->b;
		*dest++ = vertices->a;
	}
}

// triangleFlags are ignored: the engine culls backfaces itself.
void QADrawTriMeshGouraud(TQADrawContext* drawContext, long nTriangles, TQAIndexedTriangle* triangles)
{
	GLfloat*		vertices = drawContext->vertices.data();

	drawContext->indices.resize(nTriangles * 3);
	for(long i = 0; i < nTriangles; ++i) {
		drawContext->indices[i * 3 + 0] = triangles[i].vertices[0];
		drawContext->indices[i * 3 + 1] = triangles[i].vertices[1];
		drawContext->indices[i * 3 + 2] = triangles[i].vertices[2];
	}

	Context_Bind(drawContext);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_FOG_COORD_ARRAY);
	glVertexPointer(4, GL_FLOAT, 8 * sizeof(GLfloat), vertices);
	glColorPointer(4, GL_FLOAT, 8 * sizeof(GLfloat), vertices + 4);
	glFogCoordPointer(GL_FLOAT, 8 * sizeof(GLfloat), vertices + 3);	// w
	glDrawElements(GL_TRIANGLES, nTriangles * 3, GL_UNSIGNED_INT, drawContext->indices.data());
	glDisableClientState(GL_FOG_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
}

// Uploads image (one level, or the chain down to 1x1 with kQATexture_Mipmap) as GL_RGBA8, clamped,
// into *name, or a new texture when it is 0.
// ARGB32 images are RGBA8 bytes (Textures.cp); RGB32 ones are Pomme GWorld A,R,G,B bytes, alpha ignored.
static TQAError Texture_Upload(long flags, long pixelFormat, const TQAImage* image, GLuint* name)
{
	long			levels = 1;
	GLenum			format = GL_RGBA,
					type = GL_UNSIGNED_BYTE,
					internalFormat = GL_RGBA8;

	if(pixelFormat == kQAPixel_RGB32) {
		format = GL_BGRA;
		type = SDL_BYTEORDER == SDL_LIL_ENDIAN ? GL_UNSIGNED_INT_8_8_8_8 : GL_UNSIGNED_INT_8_8_8_8_REV;
		internalFormat = GL_RGB8;
	}
	else if(pixelFormat != kQAPixel_ARGB32)
	return paramErr; // Pomme GWorlds and the converted PNGs are always 32 bits
	if(flags & kQATexture_Mipmap)
	while((image[0].width >> levels) || (image[0].height >> levels))
	++levels;

	if(*name == 0)
	glGenTextures(1, name);
	glBindTexture(GL_TEXTURE_2D, *name);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, levels - 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	for(long i = 0; i < levels; ++i) {
		glPixelStorei(GL_UNPACK_ROW_LENGTH, image[i].rowBytes / 4);
		glTexImage2D(GL_TEXTURE_2D, i, internalFormat, image[i].width, image[i].height, 0, format, type, image[i].pixmap);
	}
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

	return kQANoErr;
}

TQAError QATextureNew(TQAEngine* engine, long flags, long pixelFormat, const TQAImage* image, TQATexture** texture)
{
	TQATexture*		newTexture = new TQATexture();
	TQAError		theError = Texture_Upload(flags, pixelFormat, image, &newTexture->name);

	if(theError != kQANoErr) {
		delete newTexture;
		newTexture = nil;
	}
	else
	newTexture->mipmap = flags & kQATexture_Mipmap;

	*texture = newTexture;
	return theError;
}

// GL copies the pixels in glTexImage2D: the caller may free them right away.
TQAError QATextureDetach(TQAEngine* engine, TQATexture* texture)
{
	return kQANoErr;
}

void QATextureDelete(TQAEngine* engine, TQATexture* texture)
{
	glDeleteTextures(1, &texture->name);
	delete texture;
}

TQAError QABitmapNew(TQAEngine* engine, long flags, long pixelFormat, const TQAImage* image, TQABitmap** bitmap)
{
	TQABitmap*		newBitmap = new TQABitmap();
	TQAError		theError = Texture_Upload(0, pixelFormat, image, &newBitmap->name);

	if(theError != kQANoErr) {
		delete newBitmap;
		newBitmap = nil;
	}
	else {
		newBitmap->width = image->width;
		newBitmap->height = image->height;
	}

	*bitmap = newBitmap;
	return theError;
}

TQAError QABitmapDetach(TQAEngine* engine, TQABitmap* bitmap)
{
	return kQANoErr;
}

void QABitmapDelete(TQAEngine* engine, TQABitmap* bitmap)
{
	glDeleteTextures(1, &bitmap->name);
	delete bitmap;
}

// Binds a texture with the context state plus its filter and texture op (PORTING.md sec. 4.3).
// The primary colour is kd,a and the secondary colour ks. Op None keeps the texel colour but still
// multiplies its alpha by a; Highlight adds ks after the texture stage.
static void Context_BindTexture(TQADrawContext* drawContext, GLuint name, bool mipmap, long filter, long textureOp)
{
	GLint			minFilter,
					magFilter = GL_LINEAR;

	Context_Bind(drawContext);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, name);

	switch(filter) {
		case kQATextureFilter_Fast:	minFilter = magFilter = GL_NEAREST;						break;
		case kQATextureFilter_Best:	minFilter = mipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;	break;
		default:					minFilter = mipmap ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR;	break;
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);

	if(textureOp & kQATextureOp_Modulate)
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	else {
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
	}
	if(textureOp & kQATextureOp_Highlight)
	glEnable(GL_COLOR_SUM);
}

// The bitmap is drawn unscaled with its top-left corner at v (BitmapScale is dropped), without fog.
void QADrawBitmap(TQADrawContext* drawContext, TQAVGouraud* v, TQABitmap* bitmap)
{
	float			w = 1.0f / v->invW,
					right = v->x + bitmap->width,
					bottom = v->y + bitmap->height;

	Context_BindTexture(drawContext, bitmap->name, false, kQATextureFilter_Fast, kQATextureOp_None);
	glDisable(GL_FOG);
	glColor4f(1, 1, 1, v->a);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);	glVertex4f(v->x * w, v->y * w, v->z * w, w);
	glTexCoord2f(1, 0);	glVertex4f(right * w, v->y * w, v->z * w, w);
	glTexCoord2f(1, 1);	glVertex4f(right * w, bottom * w, v->z * w, w);
	glTexCoord2f(0, 1);	glVertex4f(v->x * w, bottom * w, v->z * w, w);
	glEnd();
}

// GL interpolates s,t perspective correctly from the clip w, so they are uOverW·w and 1 - vOverW·w:
// RAVE's v runs up from the bottom of the image, GL's t down from its first (top) row.
void QASubmitVerticesTexture(TQADrawContext* drawContext, long nVertices, TQAVTexture* vertices)
{
	GLfloat*		dest;
	float			w;

	drawContext->textureVertices.resize(nVertices * 13);
	dest = drawContext->textureVertices.data();
	for(long i = 0; i < nVertices; ++i, ++vertices) {
		w = 1.0f / vertices->invW;
		*dest++ = vertices->x * w;
		*dest++ = vertices->y * w;
		*dest++ = vertices->z * w;
		*dest++ = w;
		*dest++ = vertices->uOverW * w;
		*dest++ = 1.0f - vertices->vOverW * w;
		*dest++ = vertices->kd_r;
		*dest++ = vertices->kd_g;
		*dest++ = vertices->kd_b;
		*dest++ = vertices->a;
		*dest++ = vertices->ks_r;
		*dest++ = vertices->ks_g;
		*dest++ = vertices->ks_b;
	}
}

void QADrawTriMeshTexture(TQADrawContext* drawContext, long nTriangles, TQAIndexedTriangle* triangles)
{
	GLfloat*		vertices = drawContext->textureVertices.data();
	TQATexture*		texture = (TQATexture*) drawContext->texture;

	if(texture == nil)
	return;

	drawContext->indices.resize(nTriangles * 3);
	for(long i = 0; i < nTriangles; ++i) {
		drawContext->indices[i * 3 + 0] = triangles[i].vertices[0];
		drawContext->indices[i * 3 + 1] = triangles[i].vertices[1];
		drawContext->indices[i * 3 + 2] = triangles[i].vertices[2];
	}

	Context_BindTexture(drawContext, texture->name, texture->mipmap, drawContext->intTags[kQATag_TextureFilter], drawContext->intTags[kQATag_TextureOp]);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_SECONDARY_COLOR_ARRAY);
	glEnableClientState(GL_FOG_COORD_ARRAY);
	glVertexPointer(4, GL_FLOAT, 13 * sizeof(GLfloat), vertices);
	glTexCoordPointer(2, GL_FLOAT, 13 * sizeof(GLfloat), vertices + 4);
	glColorPointer(4, GL_FLOAT, 13 * sizeof(GLfloat), vertices + 6);
	glSecondaryColorPointer(3, GL_FLOAT, 13 * sizeof(GLfloat), vertices + 10);
	glFogCoordPointer(GL_FLOAT, 13 * sizeof(GLfloat), vertices + 3);	// w
	glDrawElements(GL_TRIANGLES, nTriangles * 3, GL_UNSIGNED_INT, drawContext->indices.data());
	glDisableClientState(GL_FOG_COORD_ARRAY);
	glDisableClientState(GL_SECONDARY_COLOR_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
}
