// P4 data check (issue #70, PORTING.md 8): loads every MetaFile in "WaterRace Data/" through the game's own loaders
// (megaobjects, terrain, texture sets and lists, views, camera positions and layout, preview, the 'data' subtags read in
// Data Files.cp) and prints the lines of tools/metadump.py from the loaded structures. ctest diffs the two dumps.
// Usage: test-datacheck [GAME_ROOT]   (GAME_ROOT holds "WaterRace Data/")

#include <Rave.h>

#include <algorithm>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <map>
#include <string>
#include <unistd.h>
#include <vector>

#include "PommeFiles.h"

// SetPos_IndexTag is static: build the loaders in place, as test-terrain does
#include "MetaFile Read.cp"
#include "MetaFile Data Tags.h"

namespace fs = std::filesystem;

// Textures.cp is linked for Texture_UnpackHeader and the PNG paths; its RAVE calls are not used here
TQAError QATextureNew(TQAEngine*, long, long, const TQAImage*, TQATexture**) { return kQANoErr; }
TQAError QATextureDetach(TQAEngine*, TQATexture*) { return kQANoErr; }
void QATextureDelete(TQAEngine*, TQATexture*) {}
TQAError QABitmapNew(TQAEngine*, long, long, const TQAImage*, TQABitmap**) { return kQANoErr; }
TQAError QABitmapDetach(TQAEngine*, TQABitmap*) { return kQANoErr; }
void QABitmapDelete(TQAEngine*, TQABitmap*) {}
// Game-side definitions (Boot.cp, link-stubs.cpp) not linked here
OSErr ResolveAliasFile(FSSpec*, Boolean, Boolean* targetIsFolder, Boolean* wasAliased) { *targetIsFolder = *wasAliased = false; return noErr; }
OSErr MemError(void) { return memFullErr; }

static int			gFailures = 0;

static std::string FourCC(OSType t) { return std::string{(char) (t >> 24), (char) (t >> 16), (char) (t >> 8), (char) t}; }
static std::string PName(const void* p) { const unsigned char* s = (const unsigned char*) p; return std::string((const char*) s + 1, std::min<int>(s[0], 31)); }

// Min and max over the 8 corners, as metadump.py prints them
static std::string BBox(const Vector* box)
{
	float		lo[3] = {box[0].x, box[0].y, box[0].z},
				hi[3] = {box[0].x, box[0].y, box[0].z};
	char		s[256];

	for(int i = 1; i < kBBSize; ++i) {
		const float	v[3] = {box[i].x, box[i].y, box[i].z};

		for(int j = 0; j < 3; ++j) {
			lo[j] = std::min(lo[j], v[j]);
			hi[j] = std::max(hi[j], v[j]);
		}
	}
	snprintf(s, sizeof(s), "%g,%g,%g,%g,%g,%g", lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]);
	return s;
}

// TerrainMesh and SeaMesh share their leading fields
template <class Mesh> static std::string MeshLine(const std::string& rel, OSType tag, const Mesh* m)
{
	return "mesh " + rel + " " + FourCC(tag) + " " + FourCC(m->id) + " points=" + std::to_string(m->pointCount) + " triangles=" + std::to_string(m->triangleCount)
		+ " bbox=" + BBox(m->boundingBox) + " name=" + PName(m->name);
}

static void Fail(const std::string& rel, const char* what, long index, OSErr err)
{
	printf("FAIL %s %s %ld err=%d\n", rel.c_str(), what, index, err);
	++gFailures;
}

static void CheckFile(const std::string& rel)
{
	FSSpec						spec;
	MetaFile_ReferencePtr		reference;
	std::string					macPath = ":WaterRace Data:" + rel;
	std::deque<ShapePtr>		shapes;
	std::deque<TerrainMeshPtr>	meshes;
	std::deque<SeaMeshPtr>		seas;
	std::deque<CompressedTexturePtr>	textures;
	std::map<std::string, long>	counts;
	std::vector<std::string>	records;
	unsigned char				header[8];
	long						bytes = sizeof(header),
								megaIndex = 0,
								setIndex = 0;
	OSType						megaID = kNoID;
	OSErr						err;

	if(fs::file_size("WaterRace Data/" + rel) == 0) { // resource-fork-only file
		printf("file %s empty\n", rel.c_str());
		return;
	}
	std::replace(macPath.begin(), macPath.end(), '/', ':');
	if((err = FSMakeFSSpec(0, 0, macPath.c_str(), &spec)) != noErr || (err = MetaFile_Read_OpenFile(&spec, &reference)) != noErr) {
		Fail(rel, "open", 0, err);
		return;
	}
	MetaFile_Entry* const		tags = reference->tagTable.tagList;
	const long					tagCount = reference->tagTable.tagCount;

	// The header fields MetaFile_Read_OpenFile checks but does not keep
	SetFPos(reference->fileRefNum, fsFromStart, 0);
	FSRead(reference->fileRefNum, &bytes, (Ptr) header);
	printf("file %s version=0x%04x creator=%s tags=%ld\n", rel.c_str(), (unsigned short) UnpackI16BE(header), FourCC(UnpackU32BE(header + 4)).c_str(), tagCount);

	// Load pass: every loadable tag, in table order, through the game's loaders
	for(long i = 0; i < tagCount; ++i) {
		const MetaFile_Entry&	e = tags[i];

		if(e.tag == kTag_MegaObject) {
			MegaObject		mega = {};

			megaID = e.ID;
			setIndex = 0;
			if((err = MetaFile_Read_MegaObject_ByIndex(reference, &mega, ++megaIndex, kMetaFile_SkipNone)) != noErr)
			Fail(rel, "mega", megaIndex, err);
			else
			shapes.insert(shapes.end(), mega.object.shapeList, mega.object.shapeList + mega.object.shapeCount);
		}
		else if(e.tag == kTag_TextureSet) {
			TextureSetPtr	set = nil;

			if((err = MetaFile_Read_TextureSet_ByIndex(reference, &set, megaID, ++setIndex)) != noErr)
			Fail(rel, "txst", setIndex, err);
			else
			textures.insert(textures.end(), set->textureList, set->textureList + set->textureCount);
		}
		else if(e.tag == kFakeTag_TerrainTextureList || e.tag == kFakeTag_ModelTextureList) {
			CompressedTexturePtr	list[kMaxTextures];
			long					count = 0;

			if((err = MetaFile_Read_TextureList(reference, list, &count, e.tag)) != noErr)
			Fail(rel, "texture list", i, err);
			else
			textures.insert(textures.end(), list, list + count);
		}
		else if(e.tag == kTag_Terrain) {
			Terrain			terrain;

			if((err = MetaFile_Read_Terrain(reference, &terrain, kMetaFile_SkipNone)) != noErr)
			Fail(rel, "terr", i, err);
			else {
				meshes.insert(meshes.end(), terrain.meshList, terrain.meshList + terrain.meshCount);
				seas.insert(seas.end(), terrain.seaList, terrain.seaList + terrain.seaCount);
			}
		}
	}
	for(long n = 1; n <= MetaFile_CountTags(reference, kTag_ViewSetup); ++n) {
		ViewPtr			view = nil;

		if((err = MetaFile_Read_View_ByIndex(reference, &view, n)) != noErr)
		Fail(rel, "view", n, err);
	}
	for(long n = 1; n <= MetaFile_CountTags(reference, kTag_CameraPosition); ++n) {
		CameraPosition	camera;

		if((err = MetaFile_Read_CameraPosition_ByIndex(reference, &camera, n)) != noErr)
		Fail(rel, "cmps", n, err);
	}
	if(MetaFile_CountTags(reference, kTag_CameraLayout) > 0) {
		CameraState		c[5];

		if((err = MetaFile_Read_CameraLayout(reference, &c[0], &c[1], &c[2], &c[3], &c[4])) != noErr)
		Fail(rel, "cmly", 1, err);
	}
	if(MetaFile_CountTags(reference, kTag_Preview) > 0) {
		PicHandle		pic = nil;

		if((err = MetaFile_Read_Preview(reference, &pic)) != noErr)
		Fail(rel, "prev", 1, err);
	}

	// Data subtags read in Data Files.cp, with its size checks
	for(OSType subTag : {kSubTag_TerrainDescription, kSubTag_ShipDescription, kSubTag_CharacterDescription, kSubTag_Image, kSubTag_Version})
	for(long n = 1; n <= MetaFile_CountDataSubTags(reference, subTag); ++n) {
		OSType			type = subTag,
						id;
		Ptr				data = nil;
		long			size;

		if((err = MetaFile_Read_Data(reference, &type, n, &data, &id)) != noErr) {
			Fail(rel, ("data/" + FourCC(subTag)).c_str(), n, err);
			continue;
		}
		size = GetPtrSize(data);
		if((subTag == kSubTag_TerrainDescription && (size < (long) offsetof(Data_TerrainDescription, unused2) || size > (long) sizeof(Data_TerrainDescription)))
			|| (subTag == kSubTag_ShipDescription && size != sizeof(Data_ShipDescription))
			|| (subTag == kSubTag_CharacterDescription && size != sizeof(Data_CharacterDescription))
			|| size <= 0)
		Fail(rel, ("size data/" + FourCC(subTag)).c_str(), n, (OSErr) size);
	}

	// Dump pass: the tag counts, then the loaded objects in the order of their tags
	for(long i = 0; i < tagCount; ++i) {
		const MetaFile_Entry&	e = tags[i];

		++counts[FourCC(e.tag) + (e.tag == kTag_Data || e.tag == kTag_IncompleteData ? "/" + FourCC(e.subTag) : "")];
		if(e.tag == kTag_Shape && !shapes.empty()) {
			ShapePtr		sh = shapes.front();

			shapes.pop_front();
			records.push_back("shape " + rel + " " + FourCC(sh->id) + " shading=" + std::to_string(sh->shading) + " points=" + std::to_string(sh->pointCount)
				+ " triangles=" + std::to_string(sh->triangleCount) + " normals=" + std::to_string(sh->normalCount) + " bbox=" + BBox(sh->boundingBox) + " name=" + PName(sh->name));
		}
		else if(e.tag == kTag_TerrainMesh && !meshes.empty()) {
			records.push_back(MeshLine(rel, e.tag, meshes.front()));
			meshes.pop_front();
		}
		else if(e.tag == kTag_SeaMesh && !seas.empty()) {
			records.push_back(MeshLine(rel, e.tag, seas.front()));
			seas.pop_front();
		}
		else if(e.tag == kTag_CompressedTexture && !textures.empty()) {
			CompressedTexturePtr	t = textures.front();

			textures.pop_front();
			records.push_back("cptx " + rel + " " + FourCC(t->name) + " " + std::to_string(t->width) + " " + std::to_string(t->height));
		}
	}
	for(const auto& c : counts)
	printf("count %s %s %ld\n", rel.c_str(), c.first.c_str(), c.second);
	for(const std::string& r : records) { //IDs may hold NUL bytes, written as-is like metadump.py
		fwrite(r.data(), 1, r.size(), stdout);
		putchar('\n');
	}
	if(!shapes.empty() || !meshes.empty() || !seas.empty() || !textures.empty())
	Fail(rel, "loaded objects without a tag", (long) (shapes.size() + meshes.size() + seas.size() + textures.size()), 0);

	MetaFile_Read_CloseFile(reference);
}

// metadump.py order: a folder's files, then its subfolders, each sorted by name
static void Walk(const fs::path& dir, const fs::path& root)
{
	std::vector<fs::path>	files,
							dirs;

	for(const fs::directory_entry& d : fs::directory_iterator(dir))
	(d.is_directory() ? dirs : files).push_back(d.path());
	std::sort(files.begin(), files.end());
	std::sort(dirs.begin(), dirs.end());
	for(const fs::path& p : files)
	if(p.extension() != ".rsrc_fork" && p.extension() != ".png" && p.extension() != ".json")
	CheckFile(fs::relative(p, root).string());
	for(const fs::path& p : dirs)
	Walk(p, root);
}

int main(int argc, char** argv)
{
	const char*			root = argc > 1 ? argv[1] : WATERRACE_ROOT;

	if(chdir(root) != 0) {
		perror(root);
		return 1;
	}
	Pomme::Files::Init();
	Walk("WaterRace Data", "WaterRace Data");
	if(gFailures)
	fprintf(stderr, "%d failure(s)\n", gFailures);
	return gFailures ? 1 : 0;
}
