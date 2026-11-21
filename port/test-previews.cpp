// Ship and location previews (issue #74): Preload_Folder and Preload_Core's Preload_Ship/Preload_Terrain build every 'shds'/'trds'
// entry's previews from the PNGs of tools/convert_previews.py. Checks each is a decoded Pomme picture whose frame matches the
// picFrame of the original 'imag' PICT. The app's STR# fork is not in the repo, so the folder and file names are given here.
// Linked with every game file; Boot.cp (main() renamed) and Data Files.cp (static loaders) are included. No window is opened.
// Usage: test-previews [GAME_ROOT]   (GAME_ROOT holds "WaterRace Data/")

#include <unistd.h>

#define main WaterRace_Main
#include "Boot.cp"
#undef main

#include "Data Files.cp"

static int		gFailures = 0;

#define CHECK(cond) do { if(!(cond)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++gFailures; } } while(0)

// The preview against the picFrame (big-endian top, left, bottom, right at offset 2) of the MetaFile's 'imag' PICT
static void CheckPreview(MetaFile_ReferencePtr reference, const char* file, OSType previewID, PicHandle preview)
{
	OSType			dataType = kSubTag_Image,
					dataID = previewID;
	Ptr				pict;
	unsigned char*	p;
	short			width,
					height;

	CHECK(preview != nil && (**preview).__pomme_pixelsARGB32 != nil);
	if(preview == nil || MetaFile_Read_Data(reference, &dataType, kMetaFile_UseID, &pict, &dataID) != noErr) {
		fprintf(stderr, "FAIL %s: preview or 'imag' tag missing\n", file);
		++gFailures;
		return;
	}
	p = (unsigned char*) pict;
	width = (short) ((p[8] << 8) | p[9]) - (short) ((p[4] << 8) | p[5]);
	height = (short) ((p[6] << 8) | p[7]) - (short) ((p[2] << 8) | p[3]);
	printf("%s %c%c%c%c: %dx%d, PICT %dx%d\n", file, (char) (previewID >> 24), (char) (previewID >> 16), (char) (previewID >> 8), (char) previewID, (**preview).picFrame.right, (**preview).picFrame.bottom, width, height);
	CHECK((**preview).picFrame.left == 0 && (**preview).picFrame.top == 0);
	CHECK((**preview).picFrame.right == width && (**preview).picFrame.bottom == height);
	DisposePtr(pict);
}

template <class Entry> static void CheckEntry(const Entry& entry)
{
	FSSpec					spec = entry.fileSpec;
	MetaFile_ReferencePtr	reference;

	if(MetaFile_Read_OpenFile(&spec, &reference) != noErr) {
		fprintf(stderr, "FAIL %s: cannot reopen\n", spec.cName);
		++gFailures;
		return;
	}
	CheckPreview(reference, spec.cName, kEnablePreviewID, entry.enablePreview);
	CheckPreview(reference, spec.cName, kDisablePreviewID, entry.disablePreview);
	MetaFile_Read_CloseFile(reference);
}

int main(int argc, char** argv)
{
	const char*		root = argc > 1 ? argv[1] : WATERRACE_ROOT;

	if(chdir(root) != 0) {
		perror(root);
		return 1;
	}
	Pomme::Init();
	// Resolve_DataFolders and Preload_Core with the names of STR# folderFileNameResID
	auto folder = [](FSSpec* spec, const char* name) { *spec = dataFolder; snprintf(spec->cName, sizeof(spec->cName), "%s", name); CHECK(ScanForFolderAlias(spec) == noErr); };
	auto core = [](const char* name, auto loader, auto* entry) {
		FSSpec					spec = coreFolder;
		MetaFile_ReferencePtr	reference;

		snprintf(spec.cName, sizeof(spec.cName), "%s", name);
		CHECK(MetaFile_Read_OpenFile(&spec, &reference) == noErr);
		CHECK(loader(&spec, reference, entry) == noErr);
		MetaFile_Read_CloseFile(reference);
	};
	folder(&dataFolder, "WaterRace Data");
	folder(&locationsFolder, "Locations");
	folder(&shipsFolder, "Ships");
	folder(&coreFolder, "Core");
	CHECK(Preload_Folder(&locationsFolder, &coreData, true) == noErr);
	CHECK(Preload_Folder(&shipsFolder, &coreData, true) == noErr);
	core("Practice Zone", Preload_Terrain, &practiceTerrainData);
	core("G-Boat", Preload_Ship, &leahShipData);
	core("Cote d'azur", Preload_Terrain, &leahTerrainData);

	printf("%ld ships, %ld locations\n", coreData.shipCount, coreData.terrainCount);
	CHECK(coreData.shipCount == 8 && coreData.terrainCount == 8);
	for(long i = 0; i < coreData.shipCount; ++i)
	CheckEntry(coreData.shipList[i]);
	for(long i = 0; i < coreData.terrainCount; ++i)
	CheckEntry(coreData.terrainList[i]);
	CheckEntry(leahShipData);
	CheckEntry(leahTerrainData);
	CheckEntry(practiceTerrainData);

	Preload_ReleaseMemory();
	printf("%s\n", gFailures ? "FAILED" : "OK");
	return gFailures ? 1 : 0;
}
