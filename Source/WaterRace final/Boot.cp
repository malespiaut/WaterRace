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
#include				<cstdlib>
#include				<cstring>
#include				<vector>

#include				<SDL3/SDL.h>
#include				<SDL3/SDL_opengl.h>

#include				"PommeInit.h"
#include				"WaterRace.h"

#define					STB_IMAGE_WRITE_IMPLEMENTATION
#include				"stb_image_write.h"

//CONSTANTES:

#define					kAppResourceFile			"WaterRace 1.0.1 [English]"
#define					kDataFolderName				"WaterRace Data"
#define					kCheckStringListID			200
#define					kWindowWidth				640
#define					kWindowHeight				480

//VARIABLES:

extern short			mainResFileID;	// defined in main.cp

SDL_Window*				gSDLWindow = nil;
static SDL_GLContext	gGLContext = nil;
static long				gFrameCount = 0;

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

// shortcut: searches every open resource file, not only the current one; upgrade if a second
// resource file is ever opened at the same time.
Handle Get1Resource(ResType theType, SInt16 theID)
{
	return GetResource(theType, theID);
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
					minor = 0;
	Boolean			ok;

	if(!SDL_Init(SDL_INIT_VIDEO)) {
		Boot_Error(SDL_GetError());
		return false;
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	gSDLWindow = SDL_CreateWindow("WaterRace", kWindowWidth, kWindowHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
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

// Presents the frame. Debug hook: with WATERRACE_SCREENSHOT=path.png, frame
// WATERRACE_SCREENSHOT_FRAME (default 1) is written to that PNG and the game exits.
void Boot_PresentFrame()
{
	const char*		path = getenv("WATERRACE_SCREENSHOT");
	const char*		frame = getenv("WATERRACE_SCREENSHOT_FRAME");

	++gFrameCount;
	if(path != nil && gFrameCount == (frame != nil ? atol(frame) : 1)) {
		Boolean written = Boot_WriteScreenshot(path);
		fprintf(stderr, "Boot: screenshot of frame %ld %s %s\n", gFrameCount, written ? "written to" : "FAILED for", path);
		SDL_Quit();
		exit(written ? 0 : 1);
	}

	SDL_GL_SwapWindow(gSDLWindow);
}

// Boot the game on Linux. Returns 0 when the P1 checks pass and the window closes.
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

	// shortcut: presents cleared frames until the window closes; P3 screens replace this loop.
	for(;;) {
		while(SDL_PollEvent(&event))
		if(event.type == SDL_EVENT_QUIT) {
			SDL_Quit();
			return 0;
		}

		glClearColor(32 / 255.0f, 64 / 255.0f, 128 / 255.0f, 1.0f);	// distinct from black so the screenshot proves the clear
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		Boot_PresentFrame();
	}
}
