// Text Engine.cp test (#64): text lines drawn on 32-bit ARGB pixels in a 32-bit colour. Draws "My name" in
// red (New menu.cp's "name already in use" colour) centred, then right-aligned, over a dark Pomme GWorld and
// checks the ink colour, the blend of the anti-aliased edges, the alignment and that nothing else changed.
#include <algorithm>
#include <cstdio>

#include "Text Engine.h"

static int gFailures = 0;
#define CHECK(c) do { if(!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++gFailures; } } while(0)

static const int kWidth = 640, kHeight = 40;
static const unsigned char kBackground[4] = {0xFF, 0x10, 0x20, 0x30};	// A, R, G, B

// Draws one line and returns the horizontal extent of the changed pixels.
static void DrawAndCheck(GWorldPtr world, Boolean right, int* minX, int* maxX)
{
	PixMapHandle		pixMap = GetGWorldPixMap(world);
	unsigned char*		pixels = (unsigned char*) GetPixBaseAddr(pixMap);
	Point				destPoint = {5, 320};	// v, h
	int					ink = 0,
						edge = 0;

	for(int i = 0; i < kWidth * kHeight; ++i)
	std::copy(kBackground, kBackground + 4, pixels + i * 4);

	if(right)
	TextEngine_DrawLine_Right((Ptr) "My name", 7, TextEngine_Color32(255, 0, 0), *pixMap, destPoint);
	else
	TextEngine_DrawLine_Centered((Ptr) "My name", 7, TextEngine_Color32(255, 0, 0), *pixMap, destPoint);

	*minX = kWidth;
	*maxX = -1;
	for(int y = 0; y < kHeight; ++y)
	for(int x = 0; x < kWidth; ++x) {
		unsigned char* p = pixels + (y * kWidth + x) * 4;
		if(std::equal(p, p + 4, kBackground))
		continue;
		*minX = std::min(*minX, x);
		*maxX = std::max(*maxX, x);
		CHECK(y >= destPoint.v && y < destPoint.v + 20);
		CHECK(p[0] == 0xFF);	// alpha kept
		if(p[1] == 0xFF && p[2] == 0 && p[3] == 0)
		++ink;
		else {
			// Edge: background blended towards red
			CHECK(p[1] >= kBackground[1] && p[2] <= kBackground[2] && p[3] <= kBackground[3]);
			++edge;
		}
	}
	std::printf("%s: %d ink, %d edge pixels, x %d..%d\n", right ? "right" : "centred", ink, edge, *minX, *maxX);
	CHECK(ink > 100);
	CHECK(edge > 20);
}

int main()
{
	GWorldPtr			world;
	Rect				bounds = {0, 0, kHeight, kWidth};
	int					minX,
						maxX,
						width;

	CHECK(NewGWorld(&world, 32, &bounds, nil, nil, 0) == noErr);
	CHECK(TextEngine_Init(32, kWidth) == noErr);
	TextEngine_Settings_Clipping(0, kHeight);
	TextEngine_Settings_Text(0, 14, 0);	// FtDf 129 spec 1: Charcoal 14

	DrawAndCheck(world, false, &minX, &maxX);
	width = maxX - minX + 1;
	CHECK(width > 40 && width < 120);
	CHECK(std::abs((minX + maxX) / 2 - 320) <= 4);

	DrawAndCheck(world, true, &minX, &maxX);
	CHECK(maxX <= 320 && maxX >= 320 - 8);
	CHECK(maxX - minX + 1 == width);

	CHECK(TextEngine_Quit() == noErr);
	std::printf(gFailures ? "text engine test FAILED\n" : "text engine test passed\n");
	return gFailures ? 1 : 0;
}
