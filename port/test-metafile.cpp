// Headless test for the MetaFile Read.cp header, tag table and Shape read sites (PORTING.md 5.2): opens
// "WaterRace Data/Core/Logo Intro", checks the tag table, then loads its 16 shapes through Load_Shape and checks
// counts, indices, bounding boxes and normals.
// Usage: test-metafile [GAME_ROOT]   (GAME_ROOT holds "WaterRace Data/")

#include <cmath>
#include <filesystem>
#include <unistd.h>
#include <vector>

#include "PommeFiles.h"

// Load_Shape and SetPos_IndexTag are static: test them in place
#include "MetaFile Read.cp"

static int		gFailures = 0;

#define CHECK(cond) do { if(!(cond)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++gFailures; } } while(0)

// Textures.cp is not linked: Load_Texture is not exercised here
void Texture_UnpackHeader(CompressedTexturePtr, const unsigned char*) {}
void Texture_GetMetaFileFolder(const FSSpec*, char* folder, long) { folder[0] = 0; }
char* Texture_NewPNGPath(const char*, OSType) { return nil; }
// Game-side definitions (Boot.cp, link-stubs.cpp) not linked here
OSErr ResolveAliasFile(FSSpec*, Boolean, Boolean* targetIsFolder, Boolean* wasAliased) { *targetIsFolder = *wasAliased = false; return noErr; }
OSErr MemError(void) { return memFullErr; }

static bool Finite(const Vector& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

int main(int argc, char** argv)
{
	const char*				root = argc > 1 ? argv[1] : WATERRACE_ROOT;
	FSSpec					spec;
	MetaFile_ReferencePtr	reference;
	long					fileSize,
							geometric = 0;
	std::vector<ShapePtr>	shapes;

	if(chdir(root) != 0) {
		perror(root);
		return 1;
	}
	Pomme::Files::Init();
	fileSize = std::filesystem::file_size("WaterRace Data/Core/Logo Intro");
	if(FSMakeFSSpec(0, 0, ":WaterRace Data:Core:Logo Intro", &spec) != noErr) {
		fprintf(stderr, "FAIL: Logo Intro not found\n");
		return 1;
	}
	if(MetaFile_Read_OpenFile(&spec, &reference) != noErr) {
		fprintf(stderr, "FAIL: MetaFile_Read_OpenFile\n");
		return 1;
	}

	// Tag table: 86 entries, children one level below their parent, payloads inside the file
	CHECK(reference->tagTable.tagCount == 86);
	CHECK(reference->tagTable.tagList[0].tag == kTag_MegaObject && reference->tagTable.tagList[0].depth == 0);
	for(unsigned long i = 0; i < reference->tagTable.tagCount; ++i) {
		const MetaFile_Entry&	e = reference->tagTable.tagList[i];

		if(i > 0)
		CHECK(e.depth <= reference->tagTable.tagList[i - 1].depth + 1);
		if(e.dataSize != kMetaFile_NoData)
		CHECK(e.dataPosition >= kMetaFile_HeaderDiskSize && e.dataSize >= 0 && e.dataPosition + e.dataSize <= fileSize);
	}
	CHECK(MetaFile_CountTags(reference, kTag_Shape) == 16);

	for(long n = 1; n <= MetaFile_CountTags(reference, kTag_Shape); ++n) {
		ShapePtr		shape = nil;
		Vector			lo = {1e30f, 1e30f, 1e30f},
						hi = {-1e30f, -1e30f, -1e30f};
		const float		eps = 1e-3f;

		CHECK(SetPos_IndexTag(reference, kTag_Shape, n, kFromStartTag, kFromStartDepth) == noErr);
		OSType			tagID = reference->tagTable.tagList[reference->currentTag].ID;
		OSErr			err = Load_Shape(reference, &shape);
		if(shape != nil)
		shapes.push_back(shape);
		if(err != noErr) {
			fprintf(stderr, "FAIL: Load_Shape %ld\n", n);
			++gFailures;
			continue;
		}
		char			id[5] = {(char) (shape->id >> 24), (char) (shape->id >> 16), (char) (shape->id >> 8), (char) shape->id, 0};
		printf("  %s shading=%d points=%ld triangles=%ld normals=%ld name=%.*s\n", id, shape->shading, shape->pointCount, shape->triangleCount, shape->normalCount, shape->name[0], (char*) shape->name + 1);
		CHECK(shape->id == tagID);
		CHECK(shape->pointCount >= 0 && shape->triangleCount >= 0 && shape->name[0] < sizeof(Str31));
		CHECK(std::isfinite(shape->sphereRadius) && shape->sphereRadius >= 0 && Finite(shape->sphereCenter));
		for(int i = 0; i < kBBSize; ++i) {
			CHECK(Finite(shape->boundingBox[i]));
			lo.x = fMin(lo.x, shape->boundingBox[i].x); hi.x = fMax(hi.x, shape->boundingBox[i].x);
			lo.y = fMin(lo.y, shape->boundingBox[i].y); hi.y = fMax(hi.y, shape->boundingBox[i].y);
			lo.z = fMin(lo.z, shape->boundingBox[i].z); hi.z = fMax(hi.z, shape->boundingBox[i].z);
		}
		if(!IsGeometricShape(shape))
		continue;

		// Geometry: all three lists, points inside the bounding box and sphere, indices in range, unit normals
		++geometric;
		CHECK(shape->pointList != nil && shape->triangleList != nil && shape->normalList != nil);
		CHECK(shape->normalMode == kPerVertexNormals && shape->normalCount == shape->pointCount);
		if(shape->pointList == nil || shape->triangleList == nil || shape->normalList == nil)
		continue;
		for(long i = 0; i < shape->pointCount; ++i) {
			const Vector&	p = shape->pointList[i].point;
			float			dx = p.x - shape->sphereCenter.x,
							dy = p.y - shape->sphereCenter.y,
							dz = p.z - shape->sphereCenter.z;

			CHECK(Finite(p) && std::isfinite(shape->pointList[i].u) && std::isfinite(shape->pointList[i].v) && std::isfinite(shape->pointList[i].c));
			CHECK(p.x >= lo.x - eps && p.x <= hi.x + eps && p.y >= lo.y - eps && p.y <= hi.y + eps && p.z >= lo.z - eps && p.z <= hi.z + eps);
			CHECK(std::sqrt(dx * dx + dy * dy + dz * dz) <= shape->sphereRadius + eps);
		}
		for(long i = 0; i < shape->triangleCount; ++i)
		for(int j = 0; j < 3; ++j)
		CHECK(shape->triangleList[i].corner[j] < (unsigned long) shape->pointCount);
		for(long i = 0; i < shape->normalCount; ++i) {
			const Vector&	v = shape->normalList[i];

			CHECK(std::fabs(std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z) - 1.0f) < eps);
		}
	}
	CHECK(geometric == 11);
	MetaFile_Read_CloseFile(reference);
	for(ShapePtr shape : shapes) {
		for(Ptr p : {(Ptr) shape->pointList, (Ptr) shape->triangleList, (Ptr) shape->normalList, shape->dataPtr})
		if(p != nil)
		DisposePtr(p);
		DisposePtr((Ptr) shape);
	}

	if(gFailures)
	fprintf(stderr, "%d failure(s)\n", gFailures);
	else
	printf("OK\n");
	return gFailures ? 1 : 0;
}
