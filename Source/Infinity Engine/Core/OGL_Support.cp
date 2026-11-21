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
// viewport + scissor rectangles, state tags, Gouraud triangles, meshes and lines.
// The types, tags and prototypes are in port/stubs/RAVE.h.

#include				<cstring>
#include				<vector>

#include				<SDL3/SDL_opengl.h>

#include				"RAVE.h"

//CONSTANTES:

#define					kTagCount					(kQATag_FogColor_a + 1)
#define					kEngineName					"OpenGL"
#define					kFastTextureMemory			(256L * 1024 * 1024)

//STRUCTURES:

struct TQAEngine
{
	long					unused;
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
	std::vector<GLuint>		indices;
};

//VARIABLES:

static TQAEngine		gEngine;

//ROUTINES:

void Boot_InterfaceToWindow(const Rect* r, int* x, int* y, int* width, int* height); // compositor, Boot.cp

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
	context->floatTags[kQATag_ColorBG_a] = 1.0;

	*drawContext = context;
	return kQANoErr;
}

void QADrawContextDelete(TQADrawContext* drawContext)
{
	delete drawContext;
}

// Antialias, PerspectiveZ and the fog tags are only stored. Depth always comes from z, which
// the engine computes for either PerspectiveZ setting.
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
	glDisable(GL_FOG);
	glDisable(GL_CULL_FACE);

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

// Runs the ImageBufferInitialize notice method; without one, clears to ColorBG. The depth buffer is always cleared.
TQAError QARenderStart(TQADrawContext* drawContext, const TQARect* dirtyRect, void* initialContext)
{
	TQADevice		device;

	Context_Bind(drawContext);
	glDepthMask(GL_TRUE);
	if(drawContext->initializeMethod.bufferNoticeMethod != nil) {
		device.deviceType = kQADeviceGDevice;	// Buffer_InitializeMethod_BufferCopy only copies to memory devices
		device.device.gDevice = nil;
		drawContext->initializeMethod.bufferNoticeMethod(drawContext, &device, dirtyRect, drawContext->initializeRefCon);
		glClear(GL_DEPTH_BUFFER_BIT);
	}
	else {
		glClearColor(drawContext->floatTags[kQATag_ColorBG_r], drawContext->floatTags[kQATag_ColorBG_g],
			drawContext->floatTags[kQATag_ColorBG_b], drawContext->floatTags[kQATag_ColorBG_a]);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	return kQANoErr;
}

// Presenting happens once per frame in the compositor (PORTING.md sec. 6.3).
TQAError QARenderEnd(TQADrawContext* drawContext, void* finalContext)
{
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
	glVertexPointer(4, GL_FLOAT, 8 * sizeof(GLfloat), vertices);
	glColorPointer(4, GL_FLOAT, 8 * sizeof(GLfloat), vertices + 4);
	glDrawElements(GL_TRIANGLES, nTriangles * 3, GL_UNSIGNED_INT, drawContext->indices.data());
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
}
