// Reference PICT decoder for tools/texture_sheets.py: Pomme's ReadPICT (independent of pict2png.py).
// stdin: raw PICT (no 512-byte header); stdout: "w h\n" then w*h*4 bytes ARGB.
// Build (Pomme's 4-plane Unpack4, PICT.cpp:230-233, indexes rows with w*3 instead of w*4 - fixed on a copy, the submodule stays untouched):
//   sed '226,236s/y\*w\*3/y*w*4/' Pomme/src/Graphics/PICT.cpp > /tmp/PICT.cpp
//   g++ -w -std=c++17 -IPomme/src -IPomme/src/Graphics tools/pomme_pict_ref.cpp /tmp/PICT.cpp Pomme/src/Graphics/{ARGBPixmap,Color}.cpp \
//       Pomme/src/Utilities/{bigendianstreams,IEEEExtended}.cpp -o pomme_pict_ref
#include "PommeGraphics.h"
#include <cstdio>
#include <iostream>
#include <sstream>

int main()
{
	std::ostringstream ss;
	ss << std::cin.rdbuf();
	std::istringstream in(ss.str());
	try {
		auto pm = Pomme::Graphics::ReadPICT(in, false);
		printf("%d %d\n", pm.width, pm.height);
		fwrite(pm.data.data(), 1, pm.data.size(), stdout);
	} catch (std::exception& e) {
		fprintf(stderr, "%s\n", e.what());
		return 1;
	}
	return 0;
}
