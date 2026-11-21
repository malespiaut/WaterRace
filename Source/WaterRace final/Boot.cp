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

// Linux boot path (PORTING.md sec. 6.1, P1). Replaces the Mac boot in main.cp for now.
// The working directory is the game root: the folder that holds "WaterRace Data/" and the app's
// resource fork. Pomme resolves every FSSpec against it (HostVolume directory 0).

#include				<cstdio>
#include				<csignal>
#include				<cstdlib>
#include				<cstring>
#include				<vector>

#include				<SDL3/SDL.h>
#include				<SDL3/SDL_opengl.h>

#include				"PommeInit.h"
#include				"PommeGraphics.h"
#include				"WaterRace.h"
#include				"Data Files.h"
#include				"Interface.h"

#include				"stb_image.h"	// implementation in port/TextureLoader.cpp
#define					STB_IMAGE_WRITE_IMPLEMENTATION
#include				"stb_image_write.h"

//CONSTANTES:

#define					kAppResourceFile			"WaterRace 1.0.1 [English]"
#define					kDataFolderName				"WaterRace Data"
#define					kCheckStringListID			200
#define					kWindowWidth				640
#define					kWindowHeight				480
#define					kPortTextureWidth			1024		// power of two: GL 1.4 has no NPOT textures
#define					kPortTextureHeight			512
#define					kFadeStepMicroseconds		10000		// one gamma step per 10 ms, as on the Mac
#define					kTestFadeFrame				10

//STRUCTURES:

// The "monitor state" is the fade overlay: premultiplied colour and alpha (PORTING.md sec. 6.3).
struct OpaqueGammaInfo
{
	float					red,
							green,
							blue,
							alpha;
};
typedef struct OpaqueGammaInfo GammaInfo;

//VARIABLES:

extern short			mainResFileID;	// defined in main.cp

SDL_Window*				gSDLWindow = nil;
static SDL_GLContext	gGLContext = nil;
static long				gFrameCount = 0;
static GLuint			gPortTexture = 0;
static GammaInfo		gOverlay = {0, 0, 0, 0};
static Boolean			gFrameBegun = false,
						gScreenshotRequested = false;
static volatile sig_atomic_t	gSnapshotRequested = 0;

//ROUTINES:

// Opens the app's resource fork and makes it current. Returns noErr or fnfErr.
OSErr Boot_OpenAppResources()
{
	mainResFileID = OpenResFile(kAppResourceFile);
	if(mainResFileID < 0)
	return fnfErr;

	UseResFile(mainResFileID);
	return noErr;
}

// Loads "WaterRace Data/Interface PNG/<id>.png" (tools/convert_interface.py) as a Pomme picture:
// a Picture followed by its ARGB32 pixels (A,R,G,B in memory), freed by DisposeHandle. Returns nil if missing.
static PicHandle Boot_LoadInterfacePNG(SInt16 theID)
{
	char			path[64];
	int				width,
					height,
					i;
	unsigned char*	rgba;
	PicHandle		thePic;
	unsigned char*	dst;

	snprintf(path, sizeof(path), kDataFolderName "/Interface PNG/%d.png", theID);
	rgba = stbi_load(path, &width, &height, nil, 4);
	if(rgba == nil)
	return nil;

	thePic = (PicHandle) NewHandle(sizeof(Picture) + (size_t) width * height * 4);
	(**thePic).picSize = -1;
	SetRect(&(**thePic).picFrame, 0, 0, width, height);
	(**thePic).__pomme_pixelsARGB32 = (Ptr) *thePic + sizeof(Picture);
	dst = (unsigned char*) (**thePic).__pomme_pixelsARGB32;
	for(i = 0; i < width * height; ++i) {
		dst[i * 4 + 0] = rgba[i * 4 + 3];
		dst[i * 4 + 1] = rgba[i * 4 + 0];
		dst[i * 4 + 2] = rgba[i * 4 + 1];
		dst[i * 4 + 3] = rgba[i * 4 + 2];
	}
	stbi_image_free(rgba);

	return thePic;
}

// shortcut: searches every open resource file, not only the current one; upgrade if a second
// resource file is ever opened at the same time.
// Interface PICTs come from their converted PNGs (PORTING.md sec. 5.3): Pomme can't decode all of them.
// 'Btns' (ButtonList, all shorts) is swapped from big-endian here: every caller detaches it right away,
// so each load is a fresh copy, swapped once.
Handle Get1Resource(ResType theType, SInt16 theID)
{
	PicHandle		thePic;
	Handle			theResource;
	SInt16*			data;
	long			i;

	if(theType == 'PICT' && CurResFile() == interfaceResFileID && (thePic = Boot_LoadInterfacePNG(theID)) != nil)
	return (Handle) thePic;

	theResource = GetResource(theType, theID);
	if(theType == kButtonsResType && theResource != nil) {
		data = (SInt16*) *theResource;
		for(i = 0; i < GetHandleSize(theResource) / 2; ++i)
		data[i] = SDL_Swap16BE(data[i]);
	}

	return theResource;
}

// The 'clut' resource as is: Pomme's NewGWorld ignores it (every GWorld is ARGB32), the callers only release it.
CTabHandle GetCTable(short ctID)
{
	return (CTabHandle) GetResource('clut', ctID);
}

// QuickDraw's Random: uniform in -32767..32767.
short Random(void)
{
	return (short) (SDL_rand(65535) - 32767);
}

// DrawPicture for Pomme pictures, scaled (nearest pixel) to dstRect and clipped to the current port.
void Port_DrawPicture(PicHandle myPicture, const Rect* dstRect)
{
	GrafPtr			port;
	PixMapHandle	pixMap;
	UInt32*			src;
	UInt32*			dst;
	int				portWidth,
					portHeight,
					srcWidth = (**myPicture).picFrame.right - (**myPicture).picFrame.left,
					srcHeight = (**myPicture).picFrame.bottom - (**myPicture).picFrame.top,
					dstWidth = dstRect->right - dstRect->left,
					dstHeight = dstRect->bottom - dstRect->top,
					x,
					y;

	if(dstWidth <= 0 || dstHeight <= 0)
	return;

	GetPort(&port);
	pixMap = GetGWorldPixMap((GWorldPtr) port);
	portWidth = (**pixMap).bounds.right - (**pixMap).bounds.left;
	portHeight = (**pixMap).bounds.bottom - (**pixMap).bounds.top;
	src = (UInt32*) (**myPicture).__pomme_pixelsARGB32;
	dst = (UInt32*) GetPixBaseAddr(pixMap);

	for(y = SDL_max(dstRect->top, 0); y < SDL_min(dstRect->bottom, portHeight); ++y)
	for(x = SDL_max(dstRect->left, 0); x < SDL_min(dstRect->right, portWidth); ++x)
	dst[y * portWidth + x] = src[(y - dstRect->top) * srcHeight / dstHeight * srcWidth + (x - dstRect->left) * srcWidth / dstWidth];

	DamagePortRegion(dstRect);
}

// Pomme's CopyBits damages the current port, not the destination: also damage the screen port when
// it is the destination, so the compositor uploads what was copied to the window.
#undef CopyBits	// Pomme's from here on
void Port_CopyBits(const PixMap* srcBits, PixMap* dstBits, const Rect* srcRect, const Rect* dstRect, short mode, void* maskRgn)
{
	CGrafPtr		screen = Pomme::Graphics::GetScreenPort();
	GrafPtr			savePort;

	CopyBits(srcBits, dstBits, srcRect, dstRect, mode, maskRgn);
	if(dstBits != GetPortBitMapForCopyBits(screen))
	return;

	GetPort(&savePort);
	SetPort(screen);
	DamagePortRegion(dstRect);
	SetPort(savePort);
}

// fileName is a C string: the callers get it from GetIndString, which is GetIndStringC here.
short HOpenResFile(short vRefNum, long dirID, ConstStr255Param fileName, SInt8 permission)
{
	FSSpec			spec;

	if(FSMakeFSSpec(vRefNum, dirID, (const char*) fileName, &spec) != noErr)
	return -1;

	return FSpOpenResFile(&spec, permission);
}

// Host files are never aliases: checks that the spec exists and tells a folder (no data fork) from a file.
OSErr ResolveAliasFile(FSSpec* theSpec, Boolean resolveChains, Boolean* targetIsFolder, Boolean* wasAliased)
{
	FSSpec			spec;
	short			refNum;

	*wasAliased = false;
	if(FSMakeFSSpec(theSpec->vRefNum, theSpec->parID, theSpec->cName, &spec) != noErr)
	return fnfErr;

	*targetIsFolder = FSpOpenDF(&spec, fsRdPerm, &refNum) != noErr;
	if(!*targetIsFolder)
	FSClose(refNum);

	return noErr;
}

// No catalog index on host folders yet: folder scans (Preload_Folder) find nothing.
//shortcut: Data Files.cp only loads the core MetaFiles; enumerate host directories here when Locations/Ships/Characters must be scanned
OSErr PBGetCatInfo(CInfoPBRec* paramBlock, Boolean async)
{
	return fnfErr;
}

// Clock.h's time base (OTGetTimeStamp on the Mac): time since start in 1/timeUnit seconds.
unsigned long GetCurrentAbsoluteTime(long timeUnit)
{
	return SDL_GetTicksNS() * timeUnit / SDL_NS_PER_SECOND;
}

// P1 check: the data-folder name comes out of STR# 129 through Pomme, and a resource read
// through Get1Resource returns data. Returns true when both hold.
Boolean Boot_CheckResources()
{
	Str255			name;
	Handle			list;

	GetIndString(name, folderFileNameResID, 1);
	if(strcmp((char*) name, kDataFolderName) != 0)
	return false;

	list = Get1Resource('STR#', kCheckStringListID);
	if(list == nil || GetHandleSize(list) == 0)
	return false;

	return true;
}

// Shows a fatal error to the user (and on stderr).
static void Boot_Error(const char* message)
{
	fprintf(stderr, "Boot: %s\n", message);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "WaterRace", message, gSDLWindow);
}

// Creates the window and a GL compatibility context, then checks for GL 1.4, or GL 1.2 with
// EXT_secondary_color and EXT_fog_coord (PORTING.md sec. 4.1). Returns true when usable.
Boolean Boot_InitGL()
{
	const char*		version;
	int				major = 0,
					minor = 0,
					width = kWindowWidth,
					height = kWindowHeight;
	const char*		size = getenv("WATERRACE_WINDOW_SIZE");	// debug: "WxH", e.g. 1280x720
	Boolean			ok;

	if(!SDL_Init(SDL_INIT_VIDEO)) {
		Boot_Error(SDL_GetError());
		return false;
	}

	if(size != nil)
	sscanf(size, "%dx%d", &width, &height);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	gSDLWindow = SDL_CreateWindow("WaterRace", width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if(gSDLWindow == nil || (gGLContext = SDL_GL_CreateContext(gSDLWindow)) == nil) {
		Boot_Error(SDL_GetError());
		return false;
	}
	SDL_GL_SetSwapInterval(1);

	version = (const char*) glGetString(GL_VERSION);
	if(version != nil)
	sscanf(version, "%d.%d", &major, &minor);
	fprintf(stderr, "Boot: GL_VERSION %s, GL_RENDERER %s\n", version ? version : "?", (const char*) glGetString(GL_RENDERER));

	ok = major > 1 || (major == 1 && minor >= 4)
		|| (major == 1 && minor >= 2 && SDL_GL_ExtensionSupported("GL_EXT_secondary_color") && SDL_GL_ExtensionSupported("GL_EXT_fog_coord"));
	fprintf(stderr, "Boot: GL feature check (1.4, or 1.2 + EXT_secondary_color + EXT_fog_coord) %s\n", ok ? "passed" : "FAILED");
	if(!ok)
	Boot_Error("OpenGL 1.4 (or 1.2 with EXT_secondary_color and EXT_fog_coord) is required.");

	return ok;
}

// Writes the back buffer to a PNG (top row first). Returns true on success.
static Boolean Boot_WriteScreenshot(const char* path)
{
	int							width,
								height;
	std::vector<unsigned char>	pixels;

	SDL_GetWindowSizeInPixels(gSDLWindow, &width, &height);
	pixels.resize((size_t) width * height * 4);
	glReadBuffer(GL_BACK);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
	stbi_flip_vertically_on_write(1);
	return stbi_write_png(path, width, height, 4, pixels.data(), width * 4) != 0;
}

// The window's drawable size in pixels: the size of a kInfinityRenderingFlag_FullWindow context.
void Boot_GetWindowSize(int* width, int* height)
{
	SDL_GetWindowSizeInPixels(gSDLWindow, width, height);
}

// Maps a rectangle of the 640x480 interface to the letterboxed window, in GL window pixels
// (origin bottom-left), ready for glViewport and glScissor.
void Boot_InterfaceToWindow(const Rect* r, int* x, int* y, int* width, int* height)
{
	int				windowWidth,
					windowHeight;
	float			scale,
					left,
					bottom;

	SDL_GetWindowSizeInPixels(gSDLWindow, &windowWidth, &windowHeight);
	scale = SDL_min((float) windowWidth / kWindowWidth, (float) windowHeight / kWindowHeight);
	left = (windowWidth - kWindowWidth * scale) / 2;
	bottom = (windowHeight - kWindowHeight * scale) / 2;

	*x = SDL_lroundf(left + r->left * scale);
	*y = SDL_lroundf(bottom + (kWindowHeight - r->bottom) * scale);
	*width = SDL_lroundf(left + r->right * scale) - *x;
	*height = SDL_lroundf(bottom + (kWindowHeight - r->top) * scale) - *y;
}

// GetMouse in 640x480 interface coordinates: the inverse of the letterbox above (PORTING.md sec. 6.4).
// Pumps SDL events, so Button() sees the current state too.
void Port_GetMouse(Point* mouseLoc)
{
	int				windowWidth,
					windowHeight;
	float			x,
					y,
					scale;

	SDL_PumpEvents();
	SDL_GetMouseState(&x, &y);
	SDL_GetWindowSize(gSDLWindow, &windowWidth, &windowHeight);
	scale = SDL_min((float) windowWidth / kWindowWidth, (float) windowHeight / kWindowHeight);
	mouseLoc->h = (short) SDL_floorf((x - (windowWidth - kWindowWidth * scale) / 2) / scale);
	mouseLoc->v = (short) SDL_floorf((y - (windowHeight - kWindowHeight * scale) / 2) / scale);
}

Boolean PtInRect(Point pt, const Rect* r)
{
	return pt.h >= r->left && pt.h < r->right && pt.v >= r->top && pt.v < r->bottom;
}

// Drains the SDL event queue and reports its first key press as a keyDown/autoKey event, with the
// key's character (SDL keycodes are ASCII for letters). The mask is ignored: the callers pass everyEvent.
// shortcut: removes the events (Pomme's FlushEvents is a no-op, the callers flush right after); peek if a caller ever relies on them staying
Boolean OSEventAvail(SInt16 mask, EventRecord* theEvent)
{
	SDL_Event		event;
	Boolean			found = false;

	while(SDL_PollEvent(&event))
	if(event.type == SDL_EVENT_KEY_DOWN && !found) {
		found = true;
		theEvent->what = event.key.repeat ? autoKey : keyDown;
		theEvent->message = (event.key.key & charCodeMask) | ((event.key.scancode << 8) & keyCodeMask);
		theEvent->when = TickCount();
		Port_GetMouse(&theEvent->where);
		theEvent->modifiers = 0;
	}

	return found;
}

// Uploads the damaged region of Pomme's screen port (ARGB32, A first in memory) to the port texture.
static void Boot_UploadPort()
{
	CGrafPtr		port = Pomme::Graphics::GetScreenPort();
	GrafPtr			savePort;
	Rect			r;

	GetPort(&savePort);
	SetPort(port);

	if(gPortTexture == 0) {
		glGenTextures(1, &gPortTexture);
		glBindTexture(GL_TEXTURE_2D, gPortTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, kPortTextureWidth, kPortTextureHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, nil);
		DamagePortRegion(&port->portRect);
	}

	if(IsPortDamaged()) {
		GetPortDamageRegion(&r);
		r.left = SDL_max(r.left, 0);
		r.top = SDL_max(r.top, 0);
		r.right = SDL_min(r.right, kWindowWidth);
		r.bottom = SDL_min(r.bottom, kWindowHeight);
		if(r.left < r.right && r.top < r.bottom) {
			glBindTexture(GL_TEXTURE_2D, gPortTexture);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
			glPixelStorei(GL_UNPACK_ROW_LENGTH, kWindowWidth);
			glPixelStorei(GL_UNPACK_SKIP_PIXELS, r.left);
			glPixelStorei(GL_UNPACK_SKIP_ROWS, r.top);
			// Bytes A,R,G,B read as a native 32-bit word: 8_8_8_8 on little endian, 8_8_8_8_REV on big endian.
			glTexSubImage2D(GL_TEXTURE_2D, 0, r.left, r.top, r.right - r.left, r.bottom - r.top, GL_BGRA,
				SDL_BYTEORDER == SDL_LIL_ENDIAN ? GL_UNSIGNED_INT_8_8_8_8 : GL_UNSIGNED_INT_8_8_8_8_REV,
				GetPixBaseAddr(GetGWorldPixMap(port)));
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
			glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
			glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
		}
		ClearPortDamage();
	}

	SetPort(savePort);
}

// Draws a quad over the current viewport, in 0..1 coordinates with y down, texture coordinates 0..s, 0..t.
static void Boot_DrawQuad(float s, float t)
{
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);	glVertex2f(0, 0);
	glTexCoord2f(s, 0);	glVertex2f(1, 0);
	glTexCoord2f(s, t);	glVertex2f(1, 1);
	glTexCoord2f(0, t);	glVertex2f(0, 1);
	glEnd();
}

// Whole window, 0..1 coordinates with y down, every 3D state off (RAVE contexts may have left it on).
static void Boot_Set2D()
{
	int				width,
					height;

	SDL_GetWindowSizeInPixels(gSDLWindow, &width, &height);
	glViewport(0, 0, width, height);
	glDisable(GL_SCISSOR_TEST);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 1, 1, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_FOG);
	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_COLOR_SUM);
	glColor4f(1, 1, 1, 1);
}

// Starts the frame (PORTING.md sec. 6.3): clears the window and draws the interface port letterboxed into it.
// Runs once per frame; QARenderStart calls it, so RAVE contexts draw over the interface.
void Boot_BeginFrame()
{
	Rect			interfaceRect = {0, 0, kWindowHeight, kWindowWidth};
	int				x,
					y,
					width,
					height;

	if(gFrameBegun)
	return;
	gFrameBegun = true;

	Boot_UploadPort();

	Boot_Set2D();
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	Boot_InterfaceToWindow(&interfaceRect, &x, &y, &width, &height);
	glViewport(x, y, width, height);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, gPortTexture);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	Boot_DrawQuad((float) kWindowWidth / kPortTextureWidth, (float) kWindowHeight / kPortTextureHeight);
	glDisable(GL_TEXTURE_2D);
}

// Debug hook: the time (1/100 s) of WATERRACE_SCREENSHOT_TIME, or -1. The player shows that time of its
// animation and calls Boot_RequestScreenshot, so the next presented frame is written (Infinity Player.cp).
long Boot_ScreenshotTime()
{
	const char*		time = getenv("WATERRACE_SCREENSHOT_TIME");

	return getenv("WATERRACE_SCREENSHOT") != nil && time != nil ? atol(time) : -1;
}

void Boot_RequestScreenshot()
{
	gScreenshotRequested = true;
}

// Ends the frame: the fade overlay over everything, then presents it (QARenderEnd calls it too).
// Debug hook: with WATERRACE_SCREENSHOT=path.png, frame WATERRACE_SCREENSHOT_FRAME (default 1), or the
// frame at WATERRACE_SCREENSHOT_TIME, is written to that PNG and the game exits.
// Debug hook: with WATERRACE_SNAPSHOT=path.png, each SIGUSR1 writes the next frame there and the game goes on.
void Boot_PresentFrame()
{
	const char*		path = getenv("WATERRACE_SCREENSHOT");
	const char*		frame = getenv("WATERRACE_SCREENSHOT_FRAME");

	Boot_BeginFrame();

	// The gamma fade covered the whole monitor, so the overlay covers the whole window.
	if(gOverlay.alpha > 0) {
		Boot_Set2D();
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		glColor4f(gOverlay.red, gOverlay.green, gOverlay.blue, gOverlay.alpha);
		Boot_DrawQuad(0, 0);
		glDisable(GL_BLEND);
	}

	++gFrameCount;
	if(path != nil && (Boot_ScreenshotTime() >= 0 ? gScreenshotRequested : gFrameCount == (frame != nil ? atol(frame) : 1))) {
		Boolean written = Boot_WriteScreenshot(path);
		fprintf(stderr, "Boot: screenshot of frame %ld %s %s\n", gFrameCount, written ? "written to" : "FAILED for", path);
		SDL_Quit();
		exit(written ? 0 : 1);
	}
	if(gSnapshotRequested) {
		gSnapshotRequested = 0;
		fprintf(stderr, "Boot: snapshot of frame %ld %s\n", gFrameCount, Boot_WriteScreenshot(getenv("WATERRACE_SNAPSHOT")) ? "written" : "FAILED");
	}

	SDL_GL_SwapWindow(gSDLWindow);
	gFrameBegun = false;
}

// Gamma fades (Local ColorGamma.h) as an animated overlay. GDHandle arguments are ignored: one window.

static void SetVolume(SndChannelPtr chan, short volume)
{
	SndCommand		theCommand;

	theCommand.cmd		= volumeCmd;
	theCommand.param1	= 0;
	theCommand.param2	= ((long) volume << 16) + volume;
	SndDoImmediate(chan, &theCommand);
}

static short GetVolume(SndChannelPtr chan)
{
	SndCommand		theCommand;
	long			volume = 0;

	theCommand.cmd		= getVolumeCmd;
	theCommand.param1	= 0;
	theCommand.param2	= (long) &volume;
	SndDoImmediate(chan, &theCommand);

	return (volume & 0x0000FFFF);
}

// Fades the overlay from its current state to "to" in numSteps + 1 presented frames, ramping the
// channel volume as the Mac version did. Same step curve as XHighLevelFade in Local ColorGamma.cp.
static OSErr Boot_Fade(const GammaInfo* to, UInt16 numSteps, FadeType typeOfFade, SndChannelPtr chan, short volume)
{
	GammaInfo		from = gOverlay;
	UInt32			i,
					which = 0,
					total;
	float			soundDiff = 0,
					t;
	Uint64			startTime,
					elapsed;

	if(typeOfFade & quadraticFade)
	while(numSteps > 256)
	numSteps >>= 1;

	switch(typeOfFade) {
		case linearFade:			total = numSteps;				break;
		case quadraticFade:
		case inverseQuadraticFade:	total = numSteps * numSteps;	break;
		default:					return invalidFadeType;
	}
	if(total == 0)
	total = 1;

	if(chan != nil)
	soundDiff = (float) (GetVolume(chan) - volume) / (float) (numSteps + 1);

	for(i = 0; i <= numSteps; ++i) {
		startTime = SDL_GetTicksNS();

		switch(typeOfFade) {
			case linearFade:			which = i;											break;
			case quadraticFade:			which = i * i;										break;
			case inverseQuadraticFade:	which = total - (numSteps - i) * (numSteps - i);	break;
		}
		t = SDL_min((float) which / total, 1.0f);
		gOverlay.red = from.red + (to->red - from.red) * t;
		gOverlay.green = from.green + (to->green - from.green) * t;
		gOverlay.blue = from.blue + (to->blue - from.blue) * t;
		gOverlay.alpha = from.alpha + (to->alpha - from.alpha) * t;

		if((chan != nil) && (soundDiff != 0.0))
		SetVolume(chan, volume + soundDiff * (numSteps - i));

		Boot_PresentFrame();

		elapsed = SDL_GetTicksNS() - startTime;
		if(elapsed < SDL_US_TO_NS(kFadeStepMicroseconds))
		SDL_DelayNS(SDL_US_TO_NS(kFadeStepMicroseconds) - elapsed);
	}
	gOverlay = *to;

	return noErr;
}

// A nil state is the unfaded screen.
static GammaInfo Boot_StateOf(GammaRef state)
{
	GammaInfo		clear = {0, 0, 0, 0};

	return state != nil ? **state : clear;
}

pascal OSErr StartFading(GammaRef* returnedInitialState)
{
	*returnedInitialState = (GammaRef) NewHandle(sizeof(GammaInfo));
	if(*returnedInitialState == nil)
	return memFullErr;

	***returnedInitialState = gOverlay;
	return noErr;
}

pascal void StopFading(GammaRef initialState, Boolean restore)
{
	if(restore)
	SetMonitorState(initialState);
	if(initialState != nil)
	DisposeHandle((Handle) initialState);
}

pascal OSErr SetMonitorState(GammaRef state)
{
	gOverlay = Boot_StateOf(state);
	return noErr;
}

pascal OSErr Set1MonitorState(GDHandle theDevice, GammaRef state)
{
	return SetMonitorState(state);
}

pascal OSErr Fade1ToColor(GDHandle theDevice, const RGBColor* color, UInt16 numSteps, FadeType typeOfFade, SndChannelPtr chan, short volume)
{
	GammaInfo		to = {color->red / 65535.0f, color->green / 65535.0f, color->blue / 65535.0f, 1};

	return Boot_Fade(&to, numSteps, typeOfFade, chan, volume);
}

pascal OSErr Fade1ToGamma(GDHandle theDevice, GammaRef to, UInt16 numSteps, FadeType typeOfFade, SndChannelPtr chan, short volume)
{
	GammaInfo		state = Boot_StateOf(to);

	return Boot_Fade(&state, numSteps, typeOfFade, chan, volume);
}

pascal OSErr Fade1ToBlack(GDHandle theDevice, UInt16 numSteps, FadeType typeOfFade, SndChannelPtr chan, short volume)
{
	RGBColor		black = {0, 0, 0};

	return Fade1ToColor(theDevice, &black, numSteps, typeOfFade, chan, volume);
}

// P3 placeholder screen: QuickDraw paints a known pattern into the screen port, checked by
// port/check-compositor.py: background, red top-left, green bottom-right and white centre squares.
static void Boot_DrawTestPattern()
{
	RGBColor		background = {32 << 8, 64 << 8, 128 << 8},
					red = {0xFFFF, 0, 0},
					green = {0, 0xFFFF, 0},
					white = {0xFFFF, 0xFFFF, 0xFFFF};
	Rect			r;

	SetPort(Pomme::Graphics::GetScreenPort());
	SetRect(&r, 0, 0, kWindowWidth, kWindowHeight);	RGBForeColor(&background);	PaintRect(&r);
	SetRect(&r, 0, 0, 40, 40);						RGBForeColor(&red);			PaintRect(&r);
	SetRect(&r, 600, 440, 640, 480);				RGBForeColor(&green);		PaintRect(&r);
	SetRect(&r, 300, 220, 340, 260);				RGBForeColor(&white);		PaintRect(&r);
}

// Boot the game on Linux: Pomme, the app's resources and the GL window. Returns 0 when ready for
// Boot_Sequence (main.cp). Debug: WATERRACE_TEST_PATTERN=1 runs the compositor test screen instead.
int Boot_Startup()
{
	SDL_Event		event;

	Pomme::Init();

	if(Boot_OpenAppResources() != noErr) {
		fprintf(stderr, "Boot: cannot open resource file \"%s\" in the game root\n", kAppResourceFile);
		return 1;
	}

	if(!Boot_CheckResources()) {
		fprintf(stderr, "Boot: resource check failed (STR# %d or 'STR#' %d)\n", folderFileNameResID, kCheckStringListID);
		return 1;
	}

	fprintf(stderr, "Boot: resource check passed\n");

	if(!Boot_InitGL())
	return 1;

	if(getenv("WATERRACE_SNAPSHOT") != nil)
	signal(SIGUSR1, [](int) { gSnapshotRequested = 1; });

	if(getenv("WATERRACE_TEST_PATTERN") == nil)
	return 0;

	// Shows the QuickDraw test pattern, fades it to black and back in at frame kTestFadeFrame,
	// until the window closes (port/check-compositor.py).
	Boot_DrawTestPattern();
	StartFading(&mainGamma);
	for(;;) {
		while(SDL_PollEvent(&event))
		if(event.type == SDL_EVENT_QUIT) {
			StopFading(mainGamma, true);
			SDL_Quit();
			exit(0);
		}

		Boot_PresentFrame();
		if(gFrameCount == kTestFadeFrame) {
			Fade1ToBlack(nil, kGammaSpeed, quadraticFade, nil, 0);
			Fade1ToGamma(nil, mainGamma, kGammaSpeed, quadraticFade, nil, 0);
		}
	}
}

int Game_Main(); // main.cp

int main()
{
	try {
		return Game_Main();
	}
	catch(const Pomme::QuitRequest&) {	// thrown by ExitToShell
		SDL_Quit();
		return 0;
	}
}
