#pragma once
// P0 stub: RAVE declarations for the subset the engine uses. Declarations only,
// no behaviour; the GL implementation comes in P3 (PORTING.md §4.2, §4.3).
// Not Apple's RAVE.h: names and signatures are taken from the game's call sites.
#include "Pomme.h"

typedef long TQAError;
#define kQANoErr 0

typedef struct TQAEngine TQAEngine;
typedef struct TQADrawContext TQADrawContext;
typedef struct TQATexture TQATexture;
typedef struct TQABitmap TQABitmap;

typedef struct TQADeviceMemory {
	Ptr				baseAddr;
	long			rowBytes;
	long			width;
	long			height;
	long			pixelSize;
} TQADeviceMemory;

typedef struct TQADevice {
	long			deviceType;
	union {
		GDHandle		gDevice;
		TQADeviceMemory	memoryDevice;
	} device;
} TQADevice;

typedef struct TQAImage {
	long			width;
	long			height;
	long			rowBytes;
	Ptr				pixmap;
} TQAImage;

typedef struct TQARect {
	long			left;
	long			top;
	long			right;
	long			bottom;
} TQARect;

typedef struct TQAVGouraud {
	float			x, y, z, invW;
	float			r, g, b, a;
	float			kd_r, kd_g, kd_b;
	float			ks_r, ks_g, ks_b;
} TQAVGouraud;

typedef struct TQAVTexture {
	float			x, y, z, invW;
	float			uOverW, vOverW;
	float			r, g, b, a;
	float			kd_r, kd_g, kd_b;
	float			ks_r, ks_g, ks_b;
} TQAVTexture;

typedef struct TQAIndexedTriangle {
	unsigned long	vertices[3];
	unsigned long	triangleFlags;
} TQAIndexedTriangle;

typedef void (*TQABufferNoticeMethod)(const TQADrawContext* drawContext, const TQADevice* buffer, const TQARect* dirtyRect, void* refCon);

typedef struct TQANoticeMethod {
	TQABufferNoticeMethod	bufferNoticeMethod;
} TQANoticeMethod;

enum {
	kQADeviceGDevice = 1,
	kQADeviceMemory
};

enum {
	kQAVendor_Apple = 1,
	kQAVendor_ATI
};

enum {
	kQAEngine_AppleSW = 1,
	kQAEngine_AppleHW
};

enum {
	kQAVersion_1_6 = 0x0160
};

enum {
	kQAGestalt_EngineID = 1,
	kQAGestalt_VendorID,
	kQAGestalt_Revision,
	kQAGestalt_ASCIINameLength,
	kQAGestalt_ASCIIName,
	kQAGestalt_OptionalFeatures,
	kQAGestalt_OptionalFeatures2,
	kQAGestalt_FastTextureMemory
};

enum {
	kQAOptional_DeepZ = 1 << 0
};

enum {
	kQAOptional2_BitmapScale = 1 << 0,
	kQAOptional2_DrawContextScale = 1 << 1
};

enum {
	kQAContext_DoubleBuffer = 1 << 0,
	kQAContext_NoZBuffer = 1 << 1,
	kQAContext_DeepZ = 1 << 2,
	kQAContext_Scale = 1 << 3
};

enum {
	kQAPixel_RGB16 = 1,
	kQAPixel_ARGB16,
	kQAPixel_RGB32,
	kQAPixel_ARGB32,
	kQAPixel_CL8
};

enum {
	kQATexture_None = 0,
	kQATexture_NoCompression,
	kQATexture_HighCompression,
	kQATexture_Mipmap = 1 << 8
};

enum {
	kQABitmap_None = 0,
	kQABitmap_NoCompression,
	kQABitmap_HighCompression
};

enum {
	kQATextureOp_None = 0,
	kQATextureOp_Modulate = 1 << 0,
	kQATextureOp_Highlight = 1 << 1
};

enum {
	kQATextureFilter_Fast = 1,
	kQATextureFilter_Mid,
	kQATextureFilter_Best
};

enum {
	kQAPerspectiveZ_Off = 0,
	kQAPerspectiveZ_On
};

enum {
	kQAZFunction_None = 0,
	kQAZFunction_LT,
	kQAZFunction_True
};

enum {
	kQABlend_Interpolate = 1,
	kQABlend_PreMultiply
};

enum {
	kQAFogMode_None = 0,
	kQAFogMode_Linear,
	kQAFogMode_Exponential,
	kQAFogMode_ExponentialSquared
};

enum {
	kQAMethod_ImageBufferInitialize = 1,
	kQAMethod_ImageBuffer2DComposite
};

enum {
	kQATag_ZFunction = 1,
	kQATag_TextureOp,
	kQATag_Blend,
	kQATag_Texture,
	kQATag_TextureFilter,
	kQATag_PerspectiveZ,
	kQATag_Antialias,
	kQATag_BitmapScale_x,
	kQATag_BitmapScale_y,
	kQATag_ColorBG_r,
	kQATag_ColorBG_g,
	kQATag_ColorBG_b,
	kQATag_ColorBG_a,
	kQATag_FogMode,
	kQATag_FogStart,
	kQATag_FogEnd,
	kQATag_FogDensity,
	kQATag_FogMaxDepth,
	kQATag_FogColor_r,
	kQATag_FogColor_g,
	kQATag_FogColor_b,
	kQATag_FogColor_a
};

// Engine and device
TQAEngine*	QADeviceGetFirstEngine(TQADevice* device);
TQAEngine*	QADeviceGetNextEngine(TQADevice* device, TQAEngine* engine);
TQAError	QAEngineEnable(long vendor, long engine);
TQAError	QAEngineGestalt(TQAEngine* engine, long selector, void* response);

// Draw contexts and bitmaps
TQAError	QADrawContextNew(TQADevice* device, const TQARect* bounds, void* notice, TQAEngine* engine, unsigned long flags, TQADrawContext** drawContext);
void		QADrawContextDelete(TQADrawContext* drawContext);
TQAError	QABitmapNew(TQAEngine* engine, long flags, long pixelFormat, const TQAImage* image, TQABitmap** bitmap);
void		QABitmapDelete(TQAEngine* engine, TQABitmap* bitmap);
TQAError	QABitmapDetach(TQAEngine* engine, TQABitmap* bitmap);
TQAError	QATextureNew(TQAEngine* engine, long flags, long pixelFormat, const TQAImage* image, TQATexture** texture);
void		QATextureDelete(TQAEngine* engine, TQATexture* texture);
TQAError	QATextureDetach(TQAEngine* engine, TQATexture* texture);

// State
TQAError	QASetInt(TQADrawContext* drawContext, long tag, long value);
TQAError	QASetFloat(TQADrawContext* drawContext, long tag, float value);
TQAError	QASetPtr(TQADrawContext* drawContext, long tag, void* value);
long		QAGetInt(TQADrawContext* drawContext, long tag);
TQAError	QAGetNoticeMethod(TQADrawContext* drawContext, long method, TQANoticeMethod* noticeMethod, void** refCon);
TQAError	QASetNoticeMethod(TQADrawContext* drawContext, long method, TQANoticeMethod noticeMethod, void* refCon);

// Rendering
TQAError	QARenderStart(TQADrawContext* drawContext, const TQARect* dirtyRect, void* initialContext);
TQAError	QARenderEnd(TQADrawContext* drawContext, void* finalContext);
void		QASubmitVerticesGouraud(TQADrawContext* drawContext, long nVertices, TQAVGouraud* vertices);
void		QASubmitVerticesTexture(TQADrawContext* drawContext, long nVertices, TQAVTexture* vertices);
void		QADrawTriMeshGouraud(TQADrawContext* drawContext, long nTriangles, TQAIndexedTriangle* triangles);
void		QADrawTriMeshTexture(TQADrawContext* drawContext, long nTriangles, TQAIndexedTriangle* triangles);
void		QADrawTriGouraud(TQADrawContext* drawContext, TQAVGouraud* v0, TQAVGouraud* v1, TQAVGouraud* v2, unsigned long flags);
void		QADrawLine(TQADrawContext* drawContext, TQAVGouraud* v0, TQAVGouraud* v1);
void		QADrawBitmap(TQADrawContext* drawContext, TQAVGouraud* v, TQABitmap* bitmap);
