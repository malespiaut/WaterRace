// Headless test for the MetaFile Read.cp Terrain, TerrainMesh, SeaMesh and View read sites (issue #45, PORTING.md 5.2):
// loads "WaterRace Data/Core/Practice Zone" the way World.cp Terrain_Load does (terrain, terrain and model texture
// lists, megaobjects) and checks counts, finite bounding boxes, in-range indices, unit normals and that the texture
// sizes match "WaterRace Data/Textures/manifest.json".
// Usage: test-terrain [GAME_ROOT]   (GAME_ROOT holds "WaterRace Data/")

#include <Rave.h>

#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <vector>

#include "PommeFiles.h"

// The loaders are static: test them in place
#include "MetaFile Read.cp"

static int		gFailures = 0;

#define CHECK(cond) do { if(!(cond)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++gFailures; } } while(0)

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

struct Entry {
	std::string		source,
					png;
	int				width,
					height;
};

// manifest.json is written one key per line by convert_textures.py, entries in MetaFile table order
static std::vector<Entry> ReadManifest(const std::string& path, const std::string& source)
{
	std::ifstream		f(path);
	std::stringstream	ss;
	std::vector<Entry>	entries;
	size_t				pos = 0;

	ss << f.rdbuf();
	std::string			s = ss.str();
	while((pos = s.find('{', pos)) != std::string::npos) {
		std::string		obj = s.substr(pos, s.find('}', pos) - pos);
		auto			field = [&](const char* key) { return atoi(obj.c_str() + obj.find(key) + strlen(key)); };
		auto			text = [&](const char* key) { size_t p = obj.find(key) + strlen(key); return obj.substr(p, obj.find('"', p) - p); };
		Entry			e = {text("\"source\": \""), text("\"png\": \""), field("\"width\": "), field("\"height\": ")};

		if(e.source == source)
		entries.push_back(e);
		++pos;
	}
	return entries;
}

static bool Finite(const Vector& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

// Shared TerrainMesh/SeaMesh checks: finite bounding box holding the points, indices in range, finite lightning, sane references
template <class Mesh> static void CheckMesh(const Mesh* mesh, const char* kind)
{
	Vector			lo = {1e30f, 1e30f, 1e30f},
					hi = {-1e30f, -1e30f, -1e30f};
	const float		eps = 1e-2f;

	printf("  %s %.*s: %ld points, %ld triangles, texture %c%c%c%c, %lu references, flags 0x%lx\n", kind, mesh->name[0], (char*) mesh->name + 1, mesh->pointCount, mesh->triangleCount,
		(char) (mesh->texture >> 24), (char) (mesh->texture >> 16), (char) (mesh->texture >> 8), (char) mesh->texture, mesh->referenceCount, mesh->flags);
	CHECK(mesh->name[0] > 0 && mesh->name[0] < sizeof(Str31) && mesh->flags < 0x10000);
	CHECK(mesh->pointCount > 0 && mesh->triangleCount > 0 && mesh->pointList != nil && mesh->lightningList != nil && mesh->triangleList != nil);
	CHECK(mesh->texturePtr == nil && std::isfinite(mesh->diffuse) && std::isfinite(mesh->scaleX) && mesh->scaleX > 0);
	for(int i = 0; i < kBBSize; ++i) {
		CHECK(Finite(mesh->boundingBox[i]));
		lo.x = fMin(lo.x, mesh->boundingBox[i].x); hi.x = fMax(hi.x, mesh->boundingBox[i].x);
		lo.y = fMin(lo.y, mesh->boundingBox[i].y); hi.y = fMax(hi.y, mesh->boundingBox[i].y);
		lo.z = fMin(lo.z, mesh->boundingBox[i].z); hi.z = fMax(hi.z, mesh->boundingBox[i].z);
	}
	CHECK(lo.x <= hi.x && lo.y <= hi.y && lo.z <= hi.z && hi.x - lo.x < 1000 && hi.z - lo.z < 1000);
	if(mesh->pointList == nil || mesh->lightningList == nil || mesh->triangleList == nil)
	return;
	for(long i = 0; i < mesh->pointCount; ++i) {
		const Vector&		p = mesh->pointList[i].point;
		const Lightning&	l = mesh->lightningList[i];

		CHECK(Finite(p) && std::isfinite(mesh->pointList[i].u) && std::isfinite(mesh->pointList[i].v));
		CHECK(p.x >= lo.x - eps && p.x <= hi.x + eps && p.y >= lo.y - eps && p.y <= hi.y + eps && p.z >= lo.z - eps && p.z <= hi.z + eps);
		CHECK(l.kd_r >= 0 && l.kd_r <= 10 && l.kd_g >= 0 && l.kd_g <= 10 && l.kd_b >= 0 && l.kd_b <= 10 && std::isfinite(l.ks_r) && std::isfinite(l.ks_g) && std::isfinite(l.ks_b));
	}
	for(long i = 0; i < mesh->triangleCount; ++i)
	for(int j = 0; j < 3; ++j)
	CHECK(mesh->triangleList[i].corner[j] < (unsigned long) mesh->pointCount);
	for(unsigned long i = 0; i < mesh->referenceCount; ++i) {
		const ModelReference&	r = mesh->referenceList[i];

		CHECK(r.id != 0 &&(r.visibility & (kVisible_Easy | kVisible_Medium | kVisible_Hard)) && std::isfinite(r.scale) && r.scale > 0 && Finite(r.pos.w));
	}
}

int main(int argc, char** argv)
{
	const char*				root = argc > 1 ? argv[1] : WATERRACE_ROOT;
	FSSpec					spec;
	MetaFile_ReferencePtr	reference;
	Terrain					terrain;
	CompressedTexturePtr	terrainTextures[kMaxTextures],
							modelTextures[kMaxTextures];
	long					terrainTextureCount = 0,
							modelTextureCount = 0;
	std::vector<Entry>		manifest;
	ViewPtr					view = nil;
	Str63					name;
	OSType					id;
	OSErr					err;

	if(chdir(root) != 0) {
		perror(root);
		return 1;
	}
	Pomme::Files::Init();
	if(FSMakeFSSpec(0, 0, ":WaterRace Data:Core:Practice Zone", &spec) != noErr || MetaFile_Read_OpenFile(&spec, &reference) != noErr) {
		fprintf(stderr, "FAIL: Practice Zone not found or not opened\n");
		return 1;
	}

	// Terrain: the counts the file holds, sky and fog settings
	CHECK(MetaFile_CountTags(reference, kTag_Terrain) == 1);
	CHECK(MetaFile_Read_TerrainNameID_ByIndex(reference, name, &id, 1) == noErr && name[0] == 12 && memcmp(name + 1, "Pratice Zone", 12) == 0);
	err = MetaFile_Read_Terrain(reference, &terrain, kMetaFile_SkipNone);
	printf("MetaFile_Read_Terrain: %d\n", err);
	CHECK(err == noErr);
	if(err != noErr)
	return 1;
	CHECK(terrain.id == id);
	printf("terrain %.*s: %lu meshes, %lu seas, %lu tracks, %lu animations, %ld enclosures, %ld sounds, %ld sources, %lu items, fog %d %g..%g, sky radius %g\n",
		terrain.name[0], (char*) terrain.name + 1, terrain.meshCount, terrain.seaCount, terrain.trackCount, terrain.animCount, terrain.enclosureCount, terrain.soundCount,
		terrain.sourceCount, terrain.itemReferenceCount, terrain.fogMode, terrain.fogStart, terrain.fogEnd, terrain.skyRadius);
	CHECK(terrain.meshCount == 14 && terrain.seaCount == 9 && terrain.trackCount == 1 && terrain.animCount == 3 && terrain.enclosureCount == 6);
	CHECK(terrain.soundCount == 6 && terrain.sourceCount == 4 && terrain.cameraCount == 0 && terrain.itemReferenceCount == 8 && terrain.specialItemCount == 1);
	CHECK(terrain.fogMode == kFog_Linear && terrain.fogStart == 50 && terrain.fogEnd == 70 && terrain.clippingDistance == 70 && terrain.skyRadius == 70);
	CHECK(terrain.fogColor_r >= 0 && terrain.fogColor_r <= 1 && terrain.fogColor_g >= 0 && terrain.fogColor_g <= 1 && terrain.fogColor_b >= 0 && terrain.fogColor_b <= 1);
	CHECK(std::isfinite(terrain.waveAmplitude) && std::isfinite(terrain.ambientLight) && terrain.ambientLight >= 0 && terrain.ambientLight <= 1);
	for(long i = 0; i < terrain.sourceCount; ++i)
	CHECK((terrain.sourceList[i].sourceType == kSource_Sound || terrain.sourceList[i].sourceType == kSource_Fog) && Finite(terrain.sourceList[i].position)
		&& terrain.sourceList[i].minDistance >= 0 && terrain.sourceList[i].minDistance <= terrain.sourceList[i].maxDistance);

	// Terrain and sea meshes; terrain normals are one unit vector per point
	for(unsigned long i = 0; i < terrain.meshCount; ++i) {
		const TerrainMesh*	mesh = terrain.meshList[i];

		CheckMesh(mesh, "mesh");
		CHECK(mesh->normalList != nil);
		if(mesh->normalList != nil)
		for(long j = 0; j < mesh->pointCount; ++j) {
			const Vector&	v = mesh->normalList[j];

			CHECK(std::fabs(std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z) - 1.0f) < 1e-2f);
		}
	}
	for(unsigned long i = 0; i < terrain.seaCount; ++i) {
		CheckMesh(terrain.seaList[i], "sea");
		CHECK(terrain.seaList[i]->texture == 'sea ' && terrain.seaList[i]->alpha >= 0 && terrain.seaList[i]->alpha <= 1);
	}

	// Track, model animations, enclosures and item list
	for(unsigned long i = 0; i < terrain.trackCount; ++i) {
		const RaceTrack*	track = terrain.trackList[i];

		printf("  track %.*s: %lu checks, %ld laps\n", track->name[0], (char*) track->name + 1, track->checkCount, track->lapNumber);
		CHECK(track->checkCount >= 3 && track->lapNumber > 0 && track->lapNumber < 100);
		for(unsigned long j = 0; j < track->checkCount; ++j)
		CHECK(Finite(track->checkList[j].leftMark) && Finite(track->checkList[j].rightMark) && track->checkList[j].flags < 0x100);
	}
	for(unsigned long i = 0; i < terrain.animCount; ++i) {
		const ModelAnimation*	anim = terrain.animList[i];

		printf("  animation %c%c%c%c: %lu frames, end %ld\n", (char) (anim->modelID >> 24), (char) (anim->modelID >> 16), (char) (anim->modelID >> 8), (char) anim->modelID, anim->frameCount, anim->endTime);
		CHECK(anim->frameCount > 0 && anim->endTime > 0 && anim->frameList[anim->frameCount - 1].time == anim->endTime);
		for(unsigned long j = 0; j < anim->frameCount; ++j)
		CHECK(anim->frameList[j].time >= 0 && (j == 0 || anim->frameList[j].time > anim->frameList[j - 1].time) && Finite(anim->frameList[j].position) && std::isfinite(anim->frameList[j].rotateW));
	}
	for(long i = 0; i < terrain.enclosureCount; ++i) {
		const CollisionEnclosure*	enclosure = terrain.enclosureList[i];

		CHECK(enclosure->wallCount > 0); //bounds and wall widths are zero on disk: Enclosure_PreProcess computes them
		for(long j = 0; j < enclosure->wallCount; ++j)
		CHECK(Finite(enclosure->wallList[j].left) && Finite(enclosure->wallList[j].right) && enclosure->wallList[j].width == 0);
	}
	for(unsigned long i = 0; i < terrain.itemReferenceCount; ++i)
	CHECK(terrain.itemReferenceList[i].itemID == terrain.specialItemList[0].id && Finite(terrain.itemReferenceList[i].position));

	// Texture lists, in the order of the manifest, with its sizes and PNG paths; every mesh texture is in the terrain list
	CHECK(MetaFile_Read_TextureList(reference, terrainTextures, &terrainTextureCount, kFakeTag_TerrainTextureList) == noErr);
	CHECK(MetaFile_Read_TextureList(reference, modelTextures, &modelTextureCount, kFakeTag_ModelTextureList) == noErr);
	manifest = ReadManifest("WaterRace Data/Textures/manifest.json", "Core/Practice Zone");
	printf("textures: %ld terrain, %ld model, %zu in manifest\n", terrainTextureCount, modelTextureCount, manifest.size());
	CHECK(terrainTextureCount > 0 && modelTextureCount > 0 && (size_t) (terrainTextureCount + modelTextureCount) == manifest.size());
	for(long i = 0; i < terrainTextureCount + modelTextureCount && (size_t) i < manifest.size(); ++i) {
		CompressedTexturePtr	texture = i < terrainTextureCount ? terrainTextures[i] : modelTextures[i - terrainTextureCount];
		const Entry&			e = manifest[i];
		std::string				path = texture->pngPath != nil ? texture->pngPath : "";

		CHECK(texture->width == e.width && texture->height == e.height);
		CHECK(path.size() >= e.png.size() && path.compare(path.size() - e.png.size(), e.png.size(), e.png) == 0 && access(path.c_str(), R_OK) == 0);
	}
	for(unsigned long i = 0; i < terrain.meshCount; ++i) {
		bool			found = false;

		for(long j = 0; j < terrainTextureCount; ++j)
		found |= terrainTextures[j]->name == terrain.meshList[i]->texture;
		CHECK(found);
	}

	// Megaobjects, as Terrain_Load reads them
	long					modelCount = MetaFile_CountTags(reference, kTag_MegaObject);
	CHECK(modelCount == 37);
	for(long i = 0; i < modelCount; ++i) {
		MegaObject		mega = {};

		err = MetaFile_Read_MegaObject_ByIndex(reference, &mega, i + 1, kMetaFile_SkipNone);
		CHECK(err == noErr && mega.object.shapeCount > 0);
	}

	// View
	CHECK(MetaFile_Read_View_ByIndex(reference, &view, 1) == noErr);
	if(view != nil) {
		printf("view %.*s: %dx%d at %d,%d, zoom %g\n", view->name[0], (char*) view->name + 1, view->width, view->height, view->posH, view->posV, view->zoom);
		CHECK(view->width == 320 && view->height == 200 && std::fabs(view->zoom - 0.8f) < 1e-6f && Finite(view->camera.w) && view->window == nil);
	}

	MetaFile_Read_CloseFile(reference);
	if(gFailures)
	fprintf(stderr, "%d failure(s)\n", gFailures);
	else
	printf("OK\n");
	return gFailures ? 1 : 0;
}
