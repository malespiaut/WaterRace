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


#include			"Infinity Structures.h"
#include			"Infinity Terrain.h"
#include			"MetaFile Structures.h"
#include			"Infinity Error Codes.h"
#include			"Infinity Audio Engine.h"
#include			"Textures.h"
#include			"Infinity rendering.h"

//CONSTANTES:

#define				kFromStartTag				0
#define				kFromStartDepth				0

//Linux port: sizes of the big-endian PPC structures on disk (PORTING.md 5.2)
#define				kMetaFile_HeaderDiskSize	16
#define				kMetaFile_EntryDiskSize		24
#define				kShape_DiskSize				300
#define				kVertex_DiskSize			24
#define				kTriFace_DiskSize			12
#define				kVector_DiskSize			12
#define				kTextureSet_DiskSize		1680
#define				kScript_DiskSize			192
#define				kAnimation_HeaderDiskSize	24
#define				kEvent_DiskSize				32
#define				kCamera_DiskSize			136
#define				kObject_DiskSize			1036
#define				kMegaObject_DiskSize		3932
#define				kMegaObject0300_DiskSize	3004
#define				kCollisionBox_DiskSize		220
#define				kOldCollisionBox_DiskSize	52
#define				kParticle_DiskSize			80
#define				kShapeDataSprite_DiskSize	56
#define				kShapeDataLight_DiskSize	76
#define				kShapeDataLens_DiskSize		28
#define				kShapeDataParticle_DiskSize	104
#define				kView_DiskSize				120
#define				kTerrain_DiskSize			5636
#define				kTerrainMesh_DiskSize		12792
#define				kSeaMesh_DiskSize			12796
#define				kModelReference_DiskSize	96
#define				kLightning_DiskSize			24
#define				kModelAnimation_HeaderDiskSize	96
#define				kKeyFrame_DiskSize			32
#define				kRaceTrack_HeaderDiskSize	316
#define				kCheckPoint_DiskSize		68
#define				kItemReference_DiskSize		28
#define				kEnclosure_HeaderDiskSize	44
#define				kCollisionWall_DiskSize		92
#define				kSkeleton_HeaderDiskSize	25632	//16 + kSkeleton_MaxNodes nodes + shapePtr + unused[3]
#define				kNode_DiskSize				640
#define				kShapePoint_DiskSize		24

//ROUTINES:

//Utils:

static float UnpackFloatBE(const unsigned char* data)
{
	UInt32				bits = UnpackU32BE(data);
	float				value;
	
	BlockMove(&bits, &value, sizeof(float));
	
	return value;
}

static void UnpackFloatsBE(float* dest, const unsigned char* data, long count)
{
	for(long i = 0; i < count; ++i)
	dest[i] = UnpackFloatBE(data + 4 * i);
}

//Linux port: unpacks the 1036-byte big-endian Object at the start of a 'mega' payload; shapeList stays nil
static void UnpackObjectBE(ObjectPtr object, const unsigned char* data)
{
	object->shapeCount = UnpackU32BE(data + 0);
	object->flags = UnpackU32BE(data + 804);
	UnpackFloatsBE(&object->pos.x.x, data + 808, 12);
	UnpackFloatsBE(&object->rotateX, data + 856, 4); //rotateX, rotateY, rotateZ, scale
	UnpackFloatsBE(&object->boundingBox[0].x, data + 872, kBBSize * 3);
	BlockMove(data + 968, object->name, sizeof(Str31));
	BlockMove(data + 1000, object->unused, sizeof(object->unused));
	object->sphereRadius = UnpackFloatBE(data + 1016);
	UnpackFloatsBE(&object->sphereCenter.x, data + 1020, 3);
	object->id = UnpackU32BE(data + 1032);
}

//Linux port: reads the next tag entry, which must be a subtag of the given size, into a new buffer
static OSErr Read_NextSubTag(MetaFile_ReferencePtr reference, OSType subTag, long dataSize, Ptr* data)
{
	MetaFile_EntryPtr	tagEntry = &reference->tagTable.tagList[reference->currentTag + 1];
	long				bytes = dataSize;
	
	if((reference->currentTag + 1 >= reference->tagTable.tagCount) || (tagEntry->tag != kTag_IncompleteData) || (tagEntry->subTag != subTag))
	return kError_ItemNotFound;
	if(tagEntry->dataSize != dataSize)
	return kError_FatalError;
	reference->currentTag += 1;
	SetFPos(reference->fileRefNum, fsFromStart, tagEntry->dataPosition);
	
	*data = NewPtr(dataSize);
	if(*data == nil)
	return MemError();
	
	return FSRead(reference->fileRefNum, &bytes, *data);
}

//Linux port: reads the point, lightning and triangle subtags of a terrain or sea mesh (24-byte vertices and lightnings, 12-byte triangles on disk)
static OSErr Read_MeshLists(MetaFile_ReferencePtr reference, long pointCount, long triangleCount, VertexPtr* pointList, LightningPtr* lightningList, TriFacePtr* triangleList)
{
	OSErr				theError;
	Ptr					diskTriangles;
	long				i;
	
	if((pointCount < 0) || (triangleCount < 0))
	return kError_FatalError;
	
	theError = Read_NextSubTag(reference, kSubTag_PointList, pointCount * kVertex_DiskSize, (Ptr*) pointList);
	if(theError)
	return theError;
	UnpackIntsBE(4, pointCount * 6, *pointList);
	
	theError = Read_NextSubTag(reference, kSubTag_LightningList, pointCount * kLightning_DiskSize, (Ptr*) lightningList);
	if(theError)
	return theError;
	UnpackIntsBE(4, pointCount * 6, *lightningList);
	
	//TriFace holds unsigned longs, 8 bytes on LP64
	theError = Read_NextSubTag(reference, kSubTag_TriangleList, triangleCount * kTriFace_DiskSize, &diskTriangles);
	if(theError)
	return theError;
	*triangleList = (TriFacePtr) NewPtr(triangleCount * sizeof(TriFace));
	if(*triangleList == nil)
	return MemError();
	for(i = 0; i < triangleCount * 3; ++i)
	(*triangleList)[i / 3].corner[i % 3] = UnpackU32BE((unsigned char*) diskTriangles + 4 * i);
	DisposePtr(diskTriangles);
	
	return noErr;
}

//Linux port: unpacks the fields shared by the big-endian TerrainMesh and SeaMesh (name to diffuse); pointers stay nil
template <class Mesh> static void UnpackMeshBE(Mesh* mesh, const unsigned char* data)
{
	BlockMove(data + 0, mesh->name, sizeof(Str31));
	mesh->id = UnpackU32BE(data + 32);
	mesh->flags = UnpackU32BE(data + 36);
	UnpackFloatsBE(&mesh->pos.x.x, data + 40, 12);
	UnpackFloatsBE(&mesh->rotateX, data + 88, 6); //rotateX, rotateY, rotateZ, scaleX, scaleY, scaleZ
	mesh->pointCount = UnpackI32BE(data + 112);
	mesh->triangleCount = UnpackI32BE(data + 124);
	UnpackFloatsBE(&mesh->boundingBox[0].x, data + 132, kBBSize * 3);
	mesh->clipping = UnpackI32BE(data + 228);
	mesh->texture = UnpackU32BE(data + 232);
	mesh->diffuse = UnpackFloatBE(data + 236);
	BlockMove(data + 248, mesh->unusedArray, sizeof(mesh->unusedArray));
}

//Linux port: unpacks big-endian ModelReferences (96 bytes each on disk)
static OSErr UnpackModelReferencesBE(ModelReferencePtr list, unsigned long* count, const unsigned char* data)
{
	*count = UnpackU32BE(data);
	if(*count > kMaxReferences_Mesh)
	return kError_FatalError;
	for(unsigned long i = 0; i < *count; ++i) {
		const unsigned char*	ref = data + 4 + i * kModelReference_DiskSize;
	
		list[i].id = UnpackU32BE(ref + 0);
		list[i].flags = UnpackU32BE(ref + 4);
		list[i].visibility = UnpackI32BE(ref + 8);
		UnpackFloatsBE(&list[i].pos.x.x, ref + 12, 12);
		UnpackFloatsBE(&list[i].rotateX, ref + 60, 5); //rotateX, rotateY, rotateZ, scale, inertia
		for(long j = 0; j < 4; ++j)
		list[i].unused[j] = UnpackI32BE(ref + 80 + 4 * j);
	}
	
	return noErr;
}

//Linux port: fake tags (tgeo, ttxl, mtxl...) have no data, at position -1; seeking there would fail Pomme's stream for every later read
static OSErr SetPos_IndexTag(MetaFile_ReferencePtr reference, OSType tag, long index, long fromTag, long minDepth)
{
	long				num = 0,
						i,
						count = reference->tagTable.tagCount;
	MetaFile_EntryPtr	entry = &reference->tagTable.tagList[fromTag];
	
	for(i = fromTag; i < count; ++i, ++entry) {
		if(entry->depth < minDepth)
		return kError_ItemNotFound;
		if(entry->tag == tag) {
			++num;
			if(num == index) {
				reference->currentTag = i;
				if(entry->dataPosition != kMetaFile_NoData) SetFPos(reference->fileRefNum, fsFromStart, entry->dataPosition);
				return noErr;
			}
		}
	}
	
	return kError_ItemNotFound;
}

static OSErr SetPos_IDTag(MetaFile_ReferencePtr reference, OSType tag, OSType ID, long fromTag, long minDepth)
{
	long				i,
						count = reference->tagTable.tagCount;
	MetaFile_EntryPtr	entry = &reference->tagTable.tagList[fromTag];
	
	for(i = fromTag; i < count; ++i, ++entry) {
		if(entry->depth < minDepth)
		return kError_ItemNotFound;
		if((entry->tag == tag) && (entry->ID == ID)) {
			reference->currentTag = i;
			if(entry->dataPosition != kMetaFile_NoData) SetFPos(reference->fileRefNum, fsFromStart, entry->dataPosition);
			return noErr;
		}
	}
	
	return kError_ItemNotFound;
}

static OSErr SetPos_IndexSubTag(MetaFile_ReferencePtr reference, OSType tag, OSType subTag, long index, long fromTag, long minDepth)
{
	long				num = 0,
						i,
						count = reference->tagTable.tagCount;
	MetaFile_EntryPtr	entry = &reference->tagTable.tagList[fromTag];
	
	for(i = fromTag; i < count; ++i, ++entry) {
		if(entry->depth < minDepth)
		return kError_ItemNotFound;
		if((entry->tag == tag) && (entry->subTag == subTag)) {
			++num;
			if(num == index) {
				reference->currentTag = i;
				if(entry->dataPosition != kMetaFile_NoData) SetFPos(reference->fileRefNum, fsFromStart, entry->dataPosition);
				return noErr;
			}
		}
	}
	
	return kError_ItemNotFound;
}

static OSErr SetPos_IDSubTag(MetaFile_ReferencePtr reference, OSType tag, OSType subTag, OSType id, long fromTag, long minDepth)
{
	long				num = 0,
						i,
						count = reference->tagTable.tagCount;
	MetaFile_EntryPtr	entry = &reference->tagTable.tagList[fromTag];
	
	for(i = fromTag; i < count; ++i, ++entry) {
		if(entry->depth < minDepth)
		return kError_ItemNotFound;
		if((entry->tag == tag) && (entry->subTag == subTag) && (entry->ID == id)) {
			reference->currentTag = i;
			if(entry->dataPosition != kMetaFile_NoData) SetFPos(reference->fileRefNum, fsFromStart, entry->dataPosition);
			return noErr;
		}
	}
	
	return kError_ItemNotFound;
}

static OSErr CountTags(MetaFile_ReferencePtr reference, OSType tag, long fromTag, long minDepth)
{
	long				num = 0,
						i,
						count = reference->tagTable.tagCount;
	MetaFile_EntryPtr	entry = &reference->tagTable.tagList[fromTag];
	
	for(i = fromTag; i < count; ++i, ++entry) {
		if(entry->depth < minDepth)
		return num;
		if(entry->tag == tag)
		++num;
	}
	
	return num;
}

//Core:

long MetaFile_CountTags(MetaFile_ReferencePtr reference, OSType tag)
{
	return CountTags(reference, tag, kFromStartTag, kFromStartDepth);
}

long MetaFile_CountTags_InsideMegaObject(MetaFile_ReferencePtr reference, OSType modelID, OSType tag)
{
	//Find megaobject
	if(SetPos_IDTag(reference, kTag_MegaObject, modelID, kFromStartTag, kFromStartDepth))
	return 0; //-1
	
	return CountTags(reference, tag, reference->currentTag + 1, reference->tagTable.tagList[reference->currentTag].depth + 1);
}

long MetaFile_CountDataSubTags(MetaFile_ReferencePtr reference, OSType subTag)
{
	long				num = 0,
						i,
						count = reference->tagTable.tagCount;
	MetaFile_EntryPtr	entry = reference->tagTable.tagList;
	
	for(i = 0; i < count; ++i, ++entry)
	if((entry->tag == kTag_Data) && (entry->subTag == subTag))
	++num;
	
	return num;
}

OSErr MetaFile_Read_Data(MetaFile_ReferencePtr reference, OSType* dataType, long index, Ptr* dataPtr, OSType* dataID)
{
	short				destFileID = reference->fileRefNum;
	long				bytes;
	OSErr				theError;
	MetaFile_EntryPtr	tagEntry;
	
	//Find data
	if(*dataType == kSubTag_None) //Find any data
	theError = SetPos_IndexTag(reference, kTag_Data, index, kFromStartTag, kFromStartDepth);
	else { //Find data of a given type
		if(index == kMetaFile_UseID) //Find data of a given ID
		theError = SetPos_IDSubTag(reference, kTag_Data, *dataType, *dataID, kFromStartTag, kFromStartDepth);
		else //Any ID
		theError = SetPos_IndexSubTag(reference, kTag_Data, *dataType, index, kFromStartTag, kFromStartDepth);
	}
	if(theError)
	return theError;
	tagEntry = &reference->tagTable.tagList[reference->currentTag];
	
	//Allocate memory
	*dataPtr = NewPtr(tagEntry->dataSize);
	if(*dataPtr == nil)
	return MemError();
	
	//Read data
	bytes = tagEntry->dataSize;
	theError = FSRead(destFileID, &bytes, *dataPtr);
	if(theError)
	return theError;
	
	//Copy type & ID
	*dataType = tagEntry->subTag;
	*dataID = tagEntry->ID;
	
	return noErr;
}

static OSErr Load_Texture(MetaFile_ReferencePtr reference, CompressedTexturePtr* texture)
{
	short				destFileID = reference->fileRefNum;
	long				bytes;
	OSErr				theError;
	MetaFile_EntryPtr	tagEntry = &reference->tagTable.tagList[reference->currentTag];
	
	unsigned char		header[kCompressedTexture_HeaderSize];

	//Allocate memory - the alpha layer after the header is not loaded: the PNG holds it
	*texture = (CompressedTexturePtr) NewPtrClear(sizeof(CompressedTexture));
	if(*texture == nil)
	return MemError();

	//Read texture header
	bytes = sizeof(header);
	theError = FSRead(destFileID, &bytes, (Ptr) header);
	if(theError)
	return theError;
	Texture_UnpackHeader(*texture, header);

	//Find texture image - we suppose it is the next tag entry - and skip it: the converted PNG replaces the PICT
	++tagEntry;
	if((tagEntry->tag != kTag_IncompleteData) || (tagEntry->subTag != kSubTag_TextureImage))
	return kError_ItemNotFound;
	reference->currentTag += 1;
	SetFPos(destFileID, fsFromStart, tagEntry->dataPosition + tagEntry->dataSize);

	(*texture)->pngPath = Texture_NewPNGPath(reference->textureFolder, (*texture)->name);
	if((*texture)->pngPath == nil)
	return MemError();

	return noErr;
}

static OSErr Load_TextureSet(MetaFile_ReferencePtr reference, TextureSetPtr* set)
{
	short				destFileID = reference->fileRefNum;
	long				bytes;
	OSErr				theError;
	long				i,
						searchTag = reference->currentTag + 1;
	MetaFile_EntryPtr	tagEntry = &reference->tagTable.tagList[reference->currentTag];
	unsigned char		data[kTextureSet_DiskSize];
	
	//Check size
	if(tagEntry->dataSize != kTextureSet_DiskSize)
	return kError_FatalError;
	
	//Allocate memory - the texture pointers stored on disk stay nil
	*set = (TextureSetPtr) NewPtrClear(sizeof(TextureSet));
	if(*set == nil)
	return MemError();
	
	//Read texture set data - Linux port: copy the fields of the 32-bit big-endian TextureSet (PORTING.md 5.2)
	bytes = kTextureSet_DiskSize;
	theError = FSRead(destFileID, &bytes, (Ptr) data);
	if(theError)
	return theError;
	BlockMove(data + 0, (*set)->name, sizeof(Str63));
	(*set)->id = UnpackU32BE(data + 64);
	(*set)->flags = UnpackU32BE(data + 68);
	(*set)->size = UnpackI32BE(data + 72);
	for(i = 0; i < kMaxShapes; ++i)
	(*set)->textureTable[i] = UnpackU32BE(data + 76 + 4 * i);
	(*set)->textureCount = UnpackU32BE(data + 876);
	if((*set)->textureCount > kMaxShapes)
	return kError_FatalError;
	
	//Load textures
	for(i = 0; i < (*set)->textureCount; ++i) {
		//Find texture
		theError = SetPos_IndexTag(reference, kTag_CompressedTexture, 1 + i, searchTag, kFromStartDepth);
		if(theError)
		return theError;
		
		//Load texture
		theError = Load_Texture(reference, &((*set)->textureList[i]));
		if(theError)
		return theError;
	}
	
	return noErr;
}

OSErr MetaFile_Read_TextureSet_ByIndex(MetaFile_ReferencePtr reference, TextureSetPtr* set, OSType modelID, long index)
{
	OSErr				theError;
		
	//Find megaobject
	theError = SetPos_IDTag(reference, kTag_MegaObject, modelID, kFromStartTag, kFromStartDepth);
	if(theError)
	return theError;
	
	//Find texture set inside current megaobject
	theError = SetPos_IndexTag(reference, kTag_TextureSet, index, reference->currentTag + 1, reference->tagTable.tagList[reference->currentTag].depth + 1);
	if(theError)
	return theError;
	
	return Load_TextureSet(reference, set);
}

OSErr MetaFile_Read_TextureSet_ByID(MetaFile_ReferencePtr reference, TextureSetPtr* set, OSType modelID, OSType setID)
{
	OSErr				theError;
		
	//Find megaobject
	theError = SetPos_IDTag(reference, kTag_MegaObject, modelID, kFromStartTag, kFromStartDepth);
	if(theError)
	return theError;
	
	//Find texture set inside current megaobject
	theError = SetPos_IDTag(reference, kTag_TextureSet, setID, reference->currentTag + 1, reference->tagTable.tagList[reference->currentTag].depth + 1);
	if(theError)
	return theError;
	
	return Load_TextureSet(reference, set);
}

OSErr MetaFile_Read_ExtractTextureSet(MetaFile_ReferencePtr reference, TextureSetPtr* set, MegaObjectPtr mega)
{
	OSErr				theError;
	long				i,
						j,
						searchTag,
						minDepth;
	Boolean				found;
	OSType				name;
	
	//Find model texture list
	theError = SetPos_IndexTag(reference, kFakeTag_ModelTextureList, 1, kFromStartTag, kFromStartDepth);
	if(theError)
	return theError;
	searchTag = reference->currentTag + 1;
	minDepth = reference->tagTable.tagList[reference->currentTag].depth + 1;
	
	//Allocate memory
	*set = (TextureSetPtr) NewPtrClear(sizeof(TextureSet));
	if(*set == nil)
	return MemError();
	
	//Setup texture set
	BlockMove("\025Extracted texture set", (*set)->name, sizeof(Str63));
	(*set)->id = kNoID;
	(*set)->flags = 0;
	(*set)->textureCount = 0;
	
	for(i = 0; i < mega->object.shapeCount; ++i) {
		//Get texture name
		name = mega->object.shapeList[i]->texture;
		
		//Update set texture table
		(*set)->textureTable[i] = name;
		
		//Check if this shape has a texture
		if(name == kNoTexture)
		continue;
		
		//Check if this texture is already loaded
		found = false;
		for(j = 0; j < (*set)->textureCount; ++j)
			if((*set)->textureList[j]->name == name)
			found = true;
		if(found)
		continue;
		
		//Look for the texture
		theError = SetPos_IDTag(reference, kTag_CompressedTexture, name, searchTag, minDepth);
		if(theError)
		return theError;
		
		//Load texture
		theError = Load_Texture(reference, &((*set)->textureList[(*set)->textureCount]));
		if(theError)
		return theError;
		
		(*set)->textureCount += 1;
	}
	
	return noErr;
}

static OSErr Load_CameraPosition(MetaFile_ReferencePtr reference, CameraPositionPtr camera)
{
	short				destFileID = reference->fileRefNum;
	long				bytes;
	OSErr				theError;
	MetaFile_EntryPtr	tagEntry = &reference->tagTable.tagList[reference->currentTag];
	
	//Read camera position data
	bytes = tagEntry->dataSize;
	theError = FSRead(destFileID, &bytes, (Ptr) camera);
	if(theError)
	return theError;
	
	return noErr;
}

OSErr MetaFile_Read_CameraPosition_ByIndex(MetaFile_ReferencePtr reference, CameraPositionPtr camera, long index)
{
	OSErr				theError;
		
	//Find camera position
	theError = SetPos_IndexTag(reference, kTag_CameraPosition, index, kFromStartTag, kFromStartDepth);
	if(theError)
	return theError;
	
	return Load_CameraPosition(reference, camera);
}

static OSErr Load_View(MetaFile_ReferencePtr reference, ViewPtr* view)
{
	short				destFileID = reference->fileRefNum;
	long				bytes;
	OSErr				theError;
	MetaFile_EntryPtr	tagEntry = &reference->tagTable.tagList[reference->currentTag];
	unsigned char		data[kView_DiskSize];

	//Check size
	if(tagEntry->dataSize != kView_DiskSize)
	return kError_FatalError;

	//Allocate memory - the window, state and refCon pointers stored on disk stay nil
	*view = (ViewPtr) NewPtrClear(sizeof(View));
	if(*view == nil)
	return MemError();

	//Read camera position data - Linux port: copy the fields of the 32-bit big-endian View (PORTING.md 5.2)
	bytes = kView_DiskSize;
	theError = FSRead(destFileID, &bytes, (Ptr) data);
	if(theError)
	return theError;
	BlockMove(data + 0, (*view)->name, sizeof(Str31));
	(*view)->flags = UnpackU32BE(data + 32);
	(*view)->width = UnpackI16BE(data + 36);
	(*view)->height = UnpackI16BE(data + 38);
	(*view)->posH = UnpackI16BE(data + 40);
	(*view)->posV = UnpackI16BE(data + 42);
	UnpackFloatsBE(&(*view)->camera.x.x, data + 44, 12);
	UnpackFloatsBE(&(*view)->roll, data + 92, 4); //roll, pitch, yaw, zoom

	return noErr;
}

OSErr MetaFile_Read_View_ByIndex(MetaFile_ReferencePtr reference, ViewPtr* view, long index)
{
	OSErr				theError;
		
	//Find camera position
	theError = SetPos_IndexTag(reference, kTag_ViewSetup, index, kFromStartTag, kFromStartDepth);
	if(theError)
	return theError;
	
	return Load_View(reference, view);
}

OSErr MetaFile_Read_GetModelDefaultTextureSet(MetaFile_ReferencePtr reference, TextureSetPtr* set, MegaObjectPtr mega)
{
	long			setCount;
	OSErr			theError;
	
	//Count texture sets
	setCount = MetaFile_CountTags_InsideMegaObject(reference, mega->object.id, kTag_TextureSet);
	
	//Does the megaobject contains texture sets?
	if(setCount > 0) {
		//Load the first one
		theError = MetaFile_Read_TextureSet_ByIndex(reference, set, mega->object.id, 1);
	}
	else {
		//Create a new one
		theError = MetaFile_Read_ExtractTextureSet(reference, set, mega);
	}
	
	return theError;
}

//Linux port: unpacks the 300-byte 32-bit big-endian Shape (PORTING.md 5.2); the list pointers are left alone
void MetaFile_UnpackShapeBE(ShapePtr s, const unsigned char* data)
{
	s->pointCount = UnpackI32BE(data + 0);
	s->triangleCount = UnpackI32BE(data + 8);
	s->normalMode = UnpackI16BE(data + 16);
	s->normalCount = UnpackI32BE(data + 24);
	s->flags = UnpackU32BE(data + 32);
	UnpackFloatsBE(&s->pos.x.x, data + 36, 12);
	s->rotateX = UnpackFloatBE(data + 84);
	s->rotateY = UnpackFloatBE(data + 88);
	s->rotateZ = UnpackFloatBE(data + 92);
	s->scale = UnpackFloatBE(data + 96);
	s->link = UnpackI32BE(data + 100);
	UnpackFloatsBE(&s->boundingBox[0].x, data + 104, kBBSize * 3);
	s->texture = UnpackU32BE(data + 200);
	s->alpha = UnpackFloatBE(data + 204);
	s->difuse = UnpackFloatBE(data + 212);
	s->specular = UnpackFloatBE(data + 216);
	s->backfaceCulling = data[220] >> 7; //first bit of a big-endian 32-bit bitfield unit
	s->unused_1 = UnpackI16BE(data + 224);
	s->shading = UnpackU16BE(data + 226);
	s->unused_2 = UnpackI16BE(data + 228);
	BlockMove(data + 230, s->name, sizeof(Str31));
	BlockMove(data + 262, s->buffer, sizeof(s->buffer));
	s->sphereRadius = UnpackFloatBE(data + 280);
	UnpackFloatsBE(&s->sphereCenter.x, data + 284, 3);
	s->id = UnpackU32BE(data + 296);
}

static OSErr Load_Shape(MetaFile_ReferencePtr reference, ShapePtr* shape)
{
	short				destFileID = reference->fileRefNum;
	long				bytes;
	OSErr				theError;
	MetaFile_EntryPtr	tagEntry = &reference->tagTable.tagList[reference->currentTag];
	
	unsigned char		data[kShape_DiskSize];
	ShapePtr			s;
	TriFacePtr			diskTriangles;
	long				i;
	
	//Check size
	if(tagEntry->dataSize != kShape_DiskSize)
	return kError_FatalError;
	
	//Allocate memory - pointers stored on disk are meaningless and stay nil
	*shape = s = (ShapePtr) NewPtrClear(sizeof(Shape));
	if(*shape == nil)
	return MemError();
	
	//Read shape data - Linux port: copy the fields of the 32-bit big-endian Shape (PORTING.md 5.2)
	bytes = kShape_DiskSize;
	theError = FSRead(destFileID, &bytes, (Ptr) data);
	if(theError)
	return theError;
	MetaFile_UnpackShapeBE(s, data);
	
	//Find point list - we suppose it is the next tag entry
	++tagEntry;
	if((tagEntry->tag != kTag_IncompleteData) || (tagEntry->subTag != kSubTag_PointList)) {
		(*shape)->pointList = nil; //return kError_ItemNotFound;
		--tagEntry;
	}
	else {
		reference->currentTag += 1;
		SetFPos(destFileID, fsFromStart, tagEntry->dataPosition);
		
		//Allocate memory
		(*shape)->pointList = (VertexPtr) NewPtr(tagEntry->dataSize);
		if((*shape)->pointList == nil)
		return MemError();
		
		//Read points
		bytes = tagEntry->dataSize;
		theError = FSRead(destFileID, &bytes, (Ptr) (*shape)->pointList);
		if(theError)
		return theError;
		
		//Linux port: Vertex is 6 floats, same layout on disk - swap in place
		//shortcut: other FX shapes keep their point data raw; swap it by shading when one turns up
		if(IsGeometricShape(s)) {
			if(tagEntry->dataSize != s->pointCount * kVertex_DiskSize)
			return kError_FatalError;
			UnpackIntsBE(4, tagEntry->dataSize / 4, (*shape)->pointList);
		}
		else if(s->shading == kShader_Particle) {
			//InfinityParticle records: 19 floats then the Boolean alive byte, 80 bytes on disk and on LP64
			if(tagEntry->dataSize % kParticle_DiskSize || sizeof(InfinityParticle) != kParticle_DiskSize)
			return kError_FatalError;
			for(i = 0; i < tagEntry->dataSize / kParticle_DiskSize; ++i)
			UnpackIntsBE(4, 19, (Ptr) (*shape)->pointList + i * kParticle_DiskSize);
		}
	}
	
	//Find triangle list - we suppose it is the next tag entry
	++tagEntry;
	if((tagEntry->tag != kTag_IncompleteData) || (tagEntry->subTag != kSubTag_TriangleList)) {
		(*shape)->triangleList = nil; //return kError_ItemNotFound;
		--tagEntry;
	}
	else {
		reference->currentTag += 1;
		SetFPos(destFileID, fsFromStart, tagEntry->dataPosition);
		
		//Check size
		if(tagEntry->dataSize != s->triangleCount * kTriFace_DiskSize)
		return kError_FatalError;
		
		//Allocate memory - Linux port: TriFace holds unsigned longs, 8 bytes on LP64
		(*shape)->triangleList = (TriFacePtr) NewPtr(s->triangleCount * sizeof(TriFace));
		diskTriangles = (TriFacePtr) NewPtr(tagEntry->dataSize);
		if(((*shape)->triangleList == nil) || (diskTriangles == nil))
		return MemError();
		
		//Read triangles
		bytes = tagEntry->dataSize;
		theError = FSRead(destFileID, &bytes, (Ptr) diskTriangles);
		if(theError)
		return theError;
		for(i = 0; i < s->triangleCount * 3; ++i)
		(*shape)->triangleList[i / 3].corner[i % 3] = UnpackU32BE((unsigned char*) diskTriangles + 4 * i);
		DisposePtr((Ptr) diskTriangles);
	}
	
	//Check for normals
	if((*shape)->normalMode != kNoNormals) {
		//Find normal list - we suppose it is the next tag entry
		++tagEntry;
		if((tagEntry->tag != kTag_IncompleteData) || (tagEntry->subTag != kSubTag_NormalList))
		return kError_ItemNotFound;
		reference->currentTag += 1;
		SetFPos(destFileID, fsFromStart, tagEntry->dataPosition);
		
		//Check size
		if(tagEntry->dataSize != s->normalCount * kVector_DiskSize)
		return kError_FatalError;
		
		//Allocate memory
		(*shape)->normalList = (VectorPtr) NewPtr(tagEntry->dataSize);
		if((*shape)->normalList == nil)
		return MemError();
	
		//Read normals
		bytes = tagEntry->dataSize;
		theError = FSRead(destFileID, &bytes, (Ptr) (*shape)->normalList);
		if(theError)
		return theError;
		UnpackIntsBE(4, tagEntry->dataSize / 4, (*shape)->normalList);
	}
	else
	(*shape)->normalList = nil;
	
	//Find custom data - we suppose it is the next tag entry
	++tagEntry;
	if((tagEntry->tag != kTag_IncompleteData) || (tagEntry->subTag != kSubTag_ShapeData)) {
		(*shape)->dataPtr = nil; //return kError_ItemNotFound;
		--tagEntry;
	}
	else {
		reference->currentTag += 1;
		SetFPos(destFileID, fsFromStart, tagEntry->dataPosition);
		
		//Allocate memory
		(*shape)->dataPtr = NewPtr(tagEntry->dataSize);
		if((*shape)->dataPtr == nil)
		return MemError();
		
		//Read triangles
		bytes = tagEntry->dataSize;
		theError = FSRead(destFileID, &bytes, (*shape)->dataPtr);
		if(theError)
		return theError;
		
		//Linux port: unpack the 32-bit big-endian ShapeData_* into the native struct (longs are 8 bytes on LP64)
		//shortcut: Audio stays raw big-endian; unpack it with the audio work (PORTING.md 6.6)
		if(s->shading == kShader_Sprite) {
			ShapeData_SpritePtr		sprite;
			const unsigned char*	disk = (unsigned char*) (*shape)->dataPtr;
			
			if((tagEntry->dataSize != kShapeDataSprite_DiskSize) || ((sprite = (ShapeData_SpritePtr) NewPtr(sizeof(ShapeData_Sprite))) == nil))
			return kError_FatalError;
			UnpackFloatsBE(&sprite->sprite.position.x, disk + 0, 9); //sprite position ... hiColor_b
			UnpackFloatsBE(&sprite->localPosition.x, disk + 36, 4); //localPosition, frontOffset
			sprite->flags = UnpackU32BE(disk + 52);
			DisposePtr((*shape)->dataPtr);
			(*shape)->dataPtr = (Ptr) sprite;
		}
		else if(s->shading == kShader_Light) {
			//19 floats: same layout on disk and on LP64 - swap in place
			if((tagEntry->dataSize != kShapeDataLight_DiskSize) || (sizeof(ShapeData_Light) != kShapeDataLight_DiskSize))
			return kError_FatalError;
			UnpackIntsBE(4, kShapeDataLight_DiskSize / 4, (*shape)->dataPtr);
		}
		else if(s->shading == kShader_Lens) {
			ShapeData_LensPtr		lens;
			const unsigned char*	disk = (unsigned char*) (*shape)->dataPtr;
			
			if((tagEntry->dataSize != kShapeDataLens_DiskSize) || ((lens = (ShapeData_LensPtr) NewPtr(sizeof(ShapeData_Lens))) == nil))
			return kError_FatalError;
			UnpackFloatsBE(&lens->size, disk + 0, 3); //size, order, depth
			lens->flags = UnpackU32BE(disk + 12);
			UnpackFloatsBE(&lens->fakePosition.x, disk + 16, 3);
			DisposePtr((*shape)->dataPtr);
			(*shape)->dataPtr = (Ptr) lens;
		}
		else if(s->shading == kShader_Particle) {
			ShapeData_ParticlePtr	particle;
			const unsigned char*	disk = (unsigned char*) (*shape)->dataPtr;
			
			if((tagEntry->dataSize != kShapeDataParticle_DiskSize) || ((particle = (ShapeData_ParticlePtr) NewPtr(sizeof(ShapeData_Particle))) == nil))
			return kError_FatalError;
			particle->particleCount = UnpackI32BE(disk + 0);
			particle->lastTime = UnpackU32BE(disk + 4);
			particle->flags = UnpackU32BE(disk + 8);
			UnpackFloatsBE(&particle->gravity, disk + 12, 23); //gravity ... modulate_b
			DisposePtr((*shape)->dataPtr);
			(*shape)->dataPtr = (Ptr) particle;
		}
	}
	
	return noErr;
}

static OSErr Load_Script(MetaFile_ReferencePtr reference, ScriptPtr* script)
{
	short				destFileID = reference->fileRefNum;
	long				bytes;
	OSErr				theError;
	long				i,
						searchTag = reference->currentTag;
	MetaFile_EntryPtr	tagEntry = &reference->tagTable.tagList[reference->currentTag];
	
	unsigned char		data[kScript_DiskSize];
	unsigned char*		animData;
	AnimationPtr		animation;
	unsigned long		eventCount,
						j;
	
	//Check size
	if(tagEntry->dataSize != kScript_DiskSize)
	return kError_FatalError;
	
	//Allocate memory - the animation pointers stored on disk stay nil
	*script = (ScriptPtr) NewPtrClear(sizeof(Script));
	if(*script == nil)
	return MemError();
	
	//Read script data - Linux port: copy the fields of the 32-bit big-endian Script (PORTING.md 5.2)
	bytes = kScript_DiskSize;
	theError = FSRead(destFileID, &bytes, (Ptr) data);
	if(theError)
	return theError;
	BlockMove(data + 0, (*script)->name, sizeof(Str31));
	(*script)->flags = UnpackU32BE(data + 32);
	(*script)->id = UnpackU32BE(data + 36);
	(*script)->type = UnpackU32BE(data + 40);
	(*script)->skeletonID = UnpackU32BE(data + 44);
	(*script)->startTime = UnpackI32BE(data + 48);
	(*script)->length = UnpackI32BE(data + 52);
	(*script)->loopTime = UnpackI32BE(data + 56);
	(*script)->animationCount = UnpackU32BE(data + 60);
	if((*script)->animationCount > kMaxAnimations)
	return kError_FatalError;
	
	//Read animations
	for(i = 0; i < (*script)->animationCount; ++i) {
		//Find animation - we suppose it is the next tag entry
		++tagEntry;
		if((tagEntry->tag != kTag_IncompleteData) || (tagEntry->subTag != kSubTag_Animation))
		return kError_ItemNotFound;
		reference->currentTag += 1;
		SetFPos(destFileID, fsFromStart, tagEntry->dataPosition);
		
		//Check size
		if(tagEntry->dataSize < kAnimation_HeaderDiskSize)
		return kError_FatalError;
		
		//Read animation
		animData = (unsigned char*) NewPtr(tagEntry->dataSize);
		if(animData == nil)
		return MemError();
		bytes = tagEntry->dataSize;
		theError = FSRead(destFileID, &bytes, (Ptr) animData);
		if(theError)
		return theError;
		eventCount = UnpackU32BE(animData + 20);
		if(tagEntry->dataSize != kAnimation_HeaderDiskSize + eventCount * kEvent_DiskSize)
		return kError_FatalError;
		
		//Allocate memory - Linux port: Event holds a long, 40 bytes on LP64 against 32 on disk
		(*script)->animationList[i] = animation = (AnimationPtr) NewPtrClear(sizeof(Animation) + eventCount * sizeof(Event));
		if(animation == nil)
		return MemError();
		
		//Copy the fields of the 32-bit big-endian Animation
		animation->shapeID = UnpackU32BE(animData + 0);
		animation->flags = UnpackU32BE(animData + 4);
		animation->type = UnpackU32BE(animData + 8);
		animation->endTime = UnpackI32BE(animData + 12);
		animation->running = animData[16];
		animation->eventCount = eventCount;
		for(j = 0; j < eventCount; ++j) {
			const unsigned char*	event = animData + kAnimation_HeaderDiskSize + j * kEvent_DiskSize;
			
			animation->eventList[j].time = UnpackI32BE(event);
			//Seven 32-bit words after time: rotation and position, or a SoundEvent's soundID (copied bit for bit)
			UnpackFloatsBE(&animation->eventList[j].rotateX, event + 4, 7);
		}
		DisposePtr((Ptr) animData);
	}
	
	return noErr;
}

static OSErr Load_Skeleton(MetaFile_ReferencePtr reference, SkeletonPtr* skeleton)
{
	short				destFileID = reference->fileRefNum;
	long				bytes;
	OSErr				theError;
	MetaFile_EntryPtr	tagEntry = &reference->tagTable.tagList[reference->currentTag];
	
	unsigned char*		data;
	const unsigned char*	diskNode;
	NodePtr				node;
	long				pointCount,
						i,
						j;
	
	//Check size
	if((tagEntry->dataSize < kSkeleton_HeaderDiskSize) || ((tagEntry->dataSize - kSkeleton_HeaderDiskSize) % kShapePoint_DiskSize))
	return kError_FatalError;
	pointCount = (tagEntry->dataSize - kSkeleton_HeaderDiskSize) / kShapePoint_DiskSize;
	
	//Allocate memory
	*skeleton = (SkeletonPtr) NewPtrClear(sizeof(Skeleton) + pointCount * sizeof(ShapePoint));
	data = (unsigned char*) NewPtr(tagEntry->dataSize);
	if((*skeleton == nil) || (data == nil))
	return MemError();
	
	//Read skeleton data
	bytes = tagEntry->dataSize;
	theError = FSRead(destFileID, &bytes, (Ptr) data);
	if(theError) {
		DisposePtr((Ptr) data);
		return theError;
	}
	
	//Linux port: unpack the 32-bit big-endian Skeleton, its unsigned longs are 8 bytes on LP64 (PORTING.md 5.2); shapePtr stays nil
	(*skeleton)->id = UnpackU32BE(data + 0);
	(*skeleton)->flags = UnpackU32BE(data + 4);
	(*skeleton)->shapeID = UnpackU32BE(data + 8);
	(*skeleton)->nodeCount = UnpackU32BE(data + 12);
	for(i = 0; i < kSkeleton_MaxNodes; ++i) {
		diskNode = data + 16 + i * kNode_DiskSize;
		node = &(*skeleton)->nodeList[i];
		node->parentNode = UnpackU32BE(diskNode + 0);
		node->childNodeCount = UnpackU32BE(diskNode + 4);
		for(j = 0; j < kSkeleton_MaxChildNodes; ++j)
		node->childNodeList[j] = UnpackU32BE(diskNode + 8 + 4 * j);
		UnpackFloatsBE(&node->pos.x.x, diskNode + 32, 12);
		UnpackFloatsBE(&node->rotateX, diskNode + 80, 3);
		node->verticeCount = UnpackU32BE(diskNode + 92);
		for(j = 0; j < kSkeleton_MaxVertices; ++j)
		node->verticeList[j] = UnpackU32BE(diskNode + 96 + 4 * j);
		BlockMove(diskNode + 608, node->name, sizeof(Str31));
		if((node->childNodeCount > kSkeleton_MaxChildNodes) || (node->verticeCount > kSkeleton_MaxVertices)) {
			DisposePtr((Ptr) data);
			return kError_FatalError;
		}
	}
	UnpackFloatsBE(&(*skeleton)->points[0].vector.x, data + kSkeleton_HeaderDiskSize, pointCount * 6);
	DisposePtr((Ptr) data);
	if((*skeleton)->nodeCount > kSkeleton_MaxNodes)
	return kError_FatalError;
	
	return noErr;
}

static OSErr Load_Camera(MetaFile_ReferencePtr reference, CameraPtr* camera)
{
	short				destFileID = reference->fileRefNum;
	long				bytes;
	OSErr				theError;
	MetaFile_EntryPtr	tagEntry = &reference->tagTable.tagList[reference->currentTag];
	
	unsigned char		data[kCamera_DiskSize];
	
	//Check size
	if(tagEntry->dataSize != kCamera_DiskSize)
	return kError_FatalError;
	
	//Allocate memory
	*camera = (CameraPtr) NewPtr(sizeof(Camera));
	if(*camera == nil)
	return MemError();
	
	//Read camera data - Linux port: copy the fields of the 32-bit big-endian Camera (PORTING.md 5.2)
	bytes = kCamera_DiskSize;
	theError = FSRead(destFileID, &bytes, (Ptr) data);
	if(theError)
	return theError;
	BlockMove(data + 0, (*camera)->name, sizeof(Str31));
	BlockMove(data + 32, (*camera)->unused, sizeof((*camera)->unused));
	(*camera)->visibility = UnpackI32BE(data + 60);
	(*camera)->flags = UnpackU32BE(data + 64);
	(*camera)->id = UnpackU32BE(data + 68);
	UnpackFloatsBE(&(*camera)->pos.x.x, data + 72, 12);
	UnpackFloatsBE(&(*camera)->roll, data + 120, 4); //roll, pitch, yaw, zoom
	
	//Check version
	if(tagEntry->version == 0x0100)
	(*camera)->visibility = kVisible_Easy | kVisible_Medium | kVisible_Hard;
	
	return noErr;
}

static OSErr Load_MegaObject(MetaFile_ReferencePtr reference, MegaObjectPtr mega, long skipFlags)
{
	OSErr				theError;
	long				i,
						searchTag = reference->currentTag + 1;
	MetaFile_EntryPtr	tagEntry = &reference->tagTable.tagList[reference->currentTag];
	long				bytes;
	short				destFileID = reference->fileRefNum;
	unsigned char		data[kMegaObject_DiskSize];
	long				boxSize;
	
	//Check version and size - Linux port: only the 0x0300 and 0x0310 layouts occur in the data (PORTING.md 5.2)
	//shortcut: the 0x0100 and 0x0200 (MetaFile_3DModel) layouts fail here; unpack them if an older file turns up
	if((tagEntry->version == 0x0300) && (tagEntry->dataSize == kMegaObject0300_DiskSize))
	boxSize = kOldCollisionBox_DiskSize;
	else if((tagEntry->version == 0x0310) && (tagEntry->dataSize == kMegaObject_DiskSize))
	boxSize = kCollisionBox_DiskSize;
	else
	return kError_FatalError;
	
	//Read megaobject data - copy the fields of the 32-bit big-endian MegaObject; the pointers stored on disk are not used
	bytes = tagEntry->dataSize;
	theError = FSRead(destFileID, &bytes, (Ptr) data);
	if(theError)
	return theError;
	UnpackObjectBE(&mega->object, data);
	mega->skeletonCount = UnpackI32BE(data + 1036);
	mega->scriptCount = UnpackI32BE(data + 1840);
	mega->cameraCount = UnpackI32BE(data + 2100);
	mega->boxCount = UnpackU32BE(data + 2168);
	if((mega->object.shapeCount > kMaxShapes) || (mega->skeletonCount < 0) || (mega->skeletonCount > kMega_MaxSkeletons)
		|| (mega->scriptCount < 0) || (mega->scriptCount > kMega_MaxScripts) || (mega->cameraCount < 0) || (mega->cameraCount > kMega_MaxCameras))
	return kError_FatalError;
	
	//Convert collision boxes - version 0x0300 stores OLD_CollisionBox (center and axes only)
	if(mega->boxCount > kMega_MaxCollisionBoxes)
	mega->boxCount = kMega_MaxCollisionBoxes;
	for(i = 0; i < mega->boxCount; ++i) {
		CollisionBoxPtr		box = &mega->boxList[i];
		
		if(boxSize == kOldCollisionBox_DiskSize)
		UnpackFloatsBE(&box->center.x, data + 2172 + i * boxSize, 12);
		else
		UnpackFloatsBE(&box->center.x, data + 2172 + i * boxSize, 51); //axes, half sizes, matrix and corners
	}
	
	//Read shapes
	for(i = 0; i < mega->object.shapeCount; ++i) {
		//Find shape
		theError = SetPos_IndexTag(reference, kTag_Shape, 1 + i, searchTag, kFromStartDepth);
		if(theError)
		return theError;
		
		//Read shape
		theError = Load_Shape(reference, &mega->object.shapeList[i]);
		if(theError)
		return theError;
	}
	
	//Read skeletons
	if(skipFlags & kMetaFile_SkipSkeletons)
	mega->skeletonCount = 0;
	else
	for(i = 0; i < mega->skeletonCount; ++i) {
		//Find skeleton
		theError = SetPos_IndexTag(reference, kTag_Skeleton, 1 + i, searchTag, kFromStartDepth);
		if(theError)
		return theError;
		
		//Read skeleton
		theError = Load_Skeleton(reference, &mega->skeletonList[i]);
		if(theError)
		return theError;
	}
	
	//Read scripts
	if(skipFlags & kMetaFile_SkipScripts)
	mega->scriptCount = 0;
	else
	for(i = 0; i < mega->scriptCount; ++i) {
		//Find script
		theError = SetPos_IndexTag(reference, kTag_Script, 1 + i, searchTag, kFromStartDepth);
		if(theError)
		return theError;
		
		//Read script
		theError = Load_Script(reference, &mega->scriptList[i]);
		if(theError)
		return theError;
	}
	
	//Read cameras
	if(skipFlags & kMetaFile_SkipCameras)
	mega->cameraCount = 0;
	else
	for(i = 0; i < mega->cameraCount; ++i) {
		//Find camera
		theError = SetPos_IndexTag(reference, kTag_Camera, 1 + i, searchTag, kFromStartDepth);
		if(theError)
		return theError;
		
		//Read camera
		theError = Load_Camera(reference, &mega->cameraList[i]);
		if(theError)
		return theError;
	}
	
	return noErr;
}

OSErr MetaFile_Read_MegaObject_ByIndex(MetaFile_ReferencePtr reference, MegaObjectPtr mega, long index, long skipFlags)
{
	OSErr				theError;
	
	//Find data
	theError = SetPos_IndexTag(reference, kTag_MegaObject, index, kFromStartTag, kFromStartDepth);
	if(theError)
	return theError;
	
	return Load_MegaObject(reference, mega, skipFlags);
}

OSErr MetaFile_Read_TextureList(MetaFile_ReferencePtr reference, CompressedTexturePtr textureList[], long* textureCount, OSType tag)
{
	OSErr				theError;
	long				i,
						searchTag,
						minDepth;
	short				destFileID = reference->fileRefNum;
	
	//Find texture list
	theError = SetPos_IndexTag(reference, tag, 1, kFromStartTag, kFromStartDepth);
	if(theError)
	return theError;
	searchTag = reference->currentTag + 1;
	minDepth = reference->tagTable.tagList[reference->currentTag].depth + 1;
	
	//Count textures
	*textureCount = CountTags(reference, kTag_CompressedTexture, searchTag, minDepth);
	
	//Load textures
	for(i = 0; i < *textureCount; ++i) {
		//Find texture
		theError = SetPos_IndexTag(reference, kTag_CompressedTexture, 1 + i, searchTag, minDepth);
		if(theError)
		return theError;
		
		//Read texture
		theError = Load_Texture(reference, &textureList[i]);
		if(theError)
		return theError;
	}
	
	return noErr;
}

static OSErr Load_TerrainMesh(MetaFile_ReferencePtr reference, TerrainMeshPtr* mesh)
{
	short				destFileID = reference->fileRefNum;
	long				bytes;
	OSErr				theError;
	MetaFile_EntryPtr	tagEntry = &reference->tagTable.tagList[reference->currentTag];
	unsigned char		data[kTerrainMesh_DiskSize];
	
	//Check size
	if(tagEntry->dataSize != kTerrainMesh_DiskSize)
	return kError_FatalError;
	
	//Allocate memory - pointers stored on disk stay nil
	*mesh = (TerrainMeshPtr) NewPtrClear(sizeof(TerrainMesh));
	if(*mesh == nil)
	return MemError();
	
	//Read mesh data - Linux port: copy the fields of the 32-bit big-endian TerrainMesh (PORTING.md 5.2)
	bytes = kTerrainMesh_DiskSize;
	theError = FSRead(destFileID, &bytes, (Ptr) data);
	if(theError)
	return theError;
	UnpackMeshBE(*mesh, data);
	theError = UnpackModelReferencesBE((*mesh)->referenceList, &(*mesh)->referenceCount, data + 500);
	if(theError)
	return theError;
	
	//Read point, lightning and triangle lists - we suppose they are the next tag entries
	theError = Read_MeshLists(reference, (*mesh)->pointCount, (*mesh)->triangleCount, &(*mesh)->pointList, &(*mesh)->lightningList, &(*mesh)->triangleList);
	if(theError)
	return theError;
	
	//Read normal list (one per point) - we suppose it is the next tag entry
	theError = Read_NextSubTag(reference, kSubTag_NormalList, (*mesh)->pointCount * kVector_DiskSize, (Ptr*) &(*mesh)->normalList);
	if(theError)
	return theError;
	UnpackIntsBE(4, (*mesh)->pointCount * 3, (*mesh)->normalList);
	
	return noErr;
}

static OSErr Load_SeaMesh(MetaFile_ReferencePtr reference, SeaMeshPtr* mesh)
{
	short				destFileID = reference->fileRefNum;
	long				bytes;
	OSErr				theError;
	MetaFile_EntryPtr	tagEntry = &reference->tagTable.tagList[reference->currentTag];
	unsigned char		data[kSeaMesh_DiskSize];
	
	//Check size
	if(tagEntry->dataSize != kSeaMesh_DiskSize)
	return kError_FatalError;
	
	//Allocate memory - pointers stored on disk stay nil
	*mesh = (SeaMeshPtr) NewPtrClear(sizeof(SeaMesh));
	if(*mesh == nil)
	return MemError();
	
	//Read mesh data - Linux port: copy the fields of the 32-bit big-endian SeaMesh (PORTING.md 5.2)
	bytes = kSeaMesh_DiskSize;
	theError = FSRead(destFileID, &bytes, (Ptr) data);
	if(theError)
	return theError;
	UnpackMeshBE(*mesh, data);
	(*mesh)->alpha = UnpackFloatBE(data + 240);
	(*mesh)->lastUpdateTime = UnpackU32BE(data + 500);
	theError = UnpackModelReferencesBE((*mesh)->referenceList, &(*mesh)->referenceCount, data + 504);
	if(theError)
	return theError;
	
	//Read point, lightning and triangle lists - we suppose they are the next tag entries
	return Read_MeshLists(reference, (*mesh)->pointCount, (*mesh)->triangleCount, &(*mesh)->pointList, &(*mesh)->lightningList, &(*mesh)->triangleList);
}

static OSErr Load_AmbientSound(MetaFile_ReferencePtr reference, AmbientSoundPtr* sound)
{
	short				destFileID = reference->fileRefNum;
	long				bytes;
	OSErr				theError;
	MetaFile_EntryPtr	tagEntry = &reference->tagTable.tagList[reference->currentTag];
	
	//Allocate memory
	*sound = (AmbientSoundPtr) NewPtr(tagEntry->dataSize);
	if(*sound == nil)
	return MemError();
	
	//Read sound data
	//shortcut: AmbientSound and its ExtSoundHeader/PCM stay raw big-endian; unpack them with the audio work (PORTING.md 6.6)
	bytes = tagEntry->dataSize;
	theError = FSRead(destFileID, &bytes, (Ptr) *sound);
	if(theError)
	return theError;
	
	return noErr;
} 

static OSErr Load_ModelAnimation(MetaFile_ReferencePtr reference, ModelAnimationPtr* path)
{
	short				destFileID = reference->fileRefNum;
	long				bytes;
	OSErr				theError;
	MetaFile_EntryPtr	tagEntry = &reference->tagTable.tagList[reference->currentTag];
	unsigned char*		data;
	unsigned long		i,
						frameCount;
	
	//Check size
	if(tagEntry->dataSize < kModelAnimation_HeaderDiskSize)
	return kError_FatalError;
	
	//Read path data
	data = (unsigned char*) NewPtr(tagEntry->dataSize);
	if(data == nil)
	return MemError();
	bytes = tagEntry->dataSize;
	theError = FSRead(destFileID, &bytes, (Ptr) data);
	if(theError)
	return theError;
	frameCount = UnpackU32BE(data + 92);
	if(tagEntry->dataSize != kModelAnimation_HeaderDiskSize + frameCount * kKeyFrame_DiskSize)
	return kError_FatalError;
	
	//Allocate memory - Linux port: KeyFrame holds a long, 40 bytes on LP64 against 32 on disk
	*path = (ModelAnimationPtr) NewPtrClear(sizeof(ModelAnimation) + frameCount * sizeof(KeyFrame));
	if(*path == nil)
	return MemError();
	
	//Copy the fields of the 32-bit big-endian ModelAnimation (PORTING.md 5.2)
	(*path)->modelID = UnpackU32BE(data + 0);
	(*path)->flyID = UnpackU32BE(data + 4);
	(*path)->flags = UnpackU32BE(data + 8);
	UnpackFloatsBE(&(*path)->pos.x.x, data + 12, 12);
	UnpackFloatsBE(&(*path)->rotateX, data + 60, 3);
	(*path)->visibility = UnpackI32BE(data + 72);
	(*path)->unused1 = UnpackFloatBE(data + 76);
	(*path)->loopTime = UnpackI32BE(data + 80);
	(*path)->endTime = UnpackI32BE(data + 84);
	(*path)->running = data[88];
	(*path)->frameCount = frameCount;
	for(i = 0; i < frameCount; ++i) {
		const unsigned char*	frame = data + kModelAnimation_HeaderDiskSize + i * kKeyFrame_DiskSize;
	
		(*path)->frameList[i].time = UnpackI32BE(frame);
		UnpackFloatsBE(&(*path)->frameList[i].rotateX, frame + 4, 7); //rotateX, rotateY, rotateZ, rotateW, position
	}
	DisposePtr((Ptr) data);
	
	return noErr;
}

static OSErr Load_Track(MetaFile_ReferencePtr reference, RaceTrackPtr* track)
{
	short				destFileID = reference->fileRefNum;
	long				bytes;
	OSErr				theError;
	MetaFile_EntryPtr	tagEntry = &reference->tagTable.tagList[reference->currentTag];
	unsigned char*		data;
	unsigned long		i,
						checkCount;
	
	//Check size
	if(tagEntry->dataSize < kRaceTrack_HeaderDiskSize)
	return kError_FatalError;
	
	//Read track data
	data = (unsigned char*) NewPtr(tagEntry->dataSize);
	if(data == nil)
	return MemError();
	bytes = tagEntry->dataSize;
	theError = FSRead(destFileID, &bytes, (Ptr) data);
	if(theError)
	return theError;
	checkCount = UnpackU32BE(data + 312);
	if(tagEntry->dataSize != kRaceTrack_HeaderDiskSize + checkCount * kCheckPoint_DiskSize)
	return kError_FatalError;
	
	//Allocate memory
	*track = (RaceTrackPtr) NewPtrClear(sizeof(RaceTrack) + checkCount * sizeof(Check));
	if(*track == nil)
	return MemError();
	
	//Copy the fields of the 32-bit big-endian RaceTrack (PORTING.md 5.2)
	BlockMove(data + 0, (*track)->name, sizeof(Str31));
	(*track)->id = UnpackU32BE(data + 32);
	(*track)->flags = UnpackU32BE(data + 36);
	UnpackFloatsBE(&(*track)->color_r, data + 40, 3);
	(*track)->lapNumber = UnpackI32BE(data + 52);
	BlockMove(data + 56, (*track)->unused, sizeof((*track)->unused));
	(*track)->checkCount = checkCount;
	for(i = 0; i < checkCount; ++i) {
		const unsigned char*	check = data + kRaceTrack_HeaderDiskSize + i * kCheckPoint_DiskSize;
	
		(*track)->checkList[i].flags = UnpackU32BE(check);
		UnpackFloatsBE(&(*track)->checkList[i].rightMark.x, check + 4, 6); //rightMark, leftMark
		BlockMove(check + 28, (*track)->checkList[i].unused, sizeof((*track)->checkList[i].unused));
	}
	DisposePtr((Ptr) data);
	
	return noErr;
}

static OSErr Load_Enclosure(MetaFile_ReferencePtr reference, CollisionEnclosurePtr* enclosure)
{
	short				destFileID = reference->fileRefNum;
	long				bytes;
	OSErr				theError;
	MetaFile_EntryPtr	tagEntry = &reference->tagTable.tagList[reference->currentTag];
	unsigned char*		data;
	long				i,
						j,
						wallCount;
	
	//Check size
	if(tagEntry->dataSize < kEnclosure_HeaderDiskSize)
	return kError_FatalError;
	
	//Read enclosure data
	data = (unsigned char*) NewPtr(tagEntry->dataSize);
	if(data == nil)
	return MemError();
	bytes = tagEntry->dataSize;
	theError = FSRead(destFileID, &bytes, (Ptr) data);
	if(theError)
	return theError;
	wallCount = UnpackI32BE(data + 40);
	if((wallCount < 0) || (tagEntry->dataSize != kEnclosure_HeaderDiskSize + wallCount * kCollisionWall_DiskSize))
	return kError_FatalError;
	
	//Allocate memory
	*enclosure = (CollisionEnclosurePtr) NewPtrClear(sizeof(CollisionEnclosure) + wallCount * sizeof(CollisionWall));
	if(*enclosure == nil)
	return MemError();
	
	//Copy the fields of the 32-bit big-endian CollisionEnclosure (PORTING.md 5.2)
	(*enclosure)->flags = UnpackU32BE(data + 0);
	(*enclosure)->closed = data[4];
	UnpackFloatsBE(&(*enclosure)->bounding_min_x, data + 8, 4);
	for(j = 0; j < 4; ++j)
	(*enclosure)->unused[j] = UnpackI32BE(data + 24 + 4 * j);
	(*enclosure)->wallCount = wallCount;
	for(i = 0; i < wallCount; ++i) {
		const unsigned char*	wall = data + kEnclosure_HeaderDiskSize + i * kCollisionWall_DiskSize;
	
		UnpackFloatsBE(&(*enclosure)->wallList[i].left.x, wall, 19); //left, right, inverseMatrix, width
		for(j = 0; j < 4; ++j)
		(*enclosure)->wallList[i].unused[j] = UnpackI32BE(wall + 76 + 4 * j);
	}
	DisposePtr((Ptr) data);
	
	return noErr;
}

static OSErr Load_ItemList(MetaFile_ReferencePtr reference, SpecialItemReferencePtr* itemList)
{
	short				destFileID = reference->fileRefNum;
	long				bytes;
	OSErr				theError;
	MetaFile_EntryPtr	tagEntry = &reference->tagTable.tagList[reference->currentTag];
	unsigned char*		data;
	long				i,
						count = tagEntry->dataSize / kItemReference_DiskSize;
	
	//Check size
	if(tagEntry->dataSize % kItemReference_DiskSize)
	return kError_FatalError;
	
	//Read item list data
	data = (unsigned char*) NewPtr(tagEntry->dataSize);
	*itemList = (SpecialItemReferencePtr) NewPtrClear(count * sizeof(SpecialItemReference));
	if((data == nil) || (*itemList == nil))
	return MemError();
	bytes = tagEntry->dataSize;
	theError = FSRead(destFileID, &bytes, (Ptr) data);
	if(theError)
	return theError;
	
	//Copy the fields of the 32-bit big-endian SpecialItemReferences (PORTING.md 5.2)
	for(i = 0; i < count; ++i) {
		const unsigned char*	item = data + i * kItemReference_DiskSize;
	
		(*itemList)[i].itemID = UnpackU32BE(item + 0);
		(*itemList)[i].flags = UnpackU32BE(item + 4);
		(*itemList)[i].visibility = UnpackI32BE(item + 8);
		UnpackFloatsBE(&(*itemList)[i].position.x, item + 12, 3);
		(*itemList)[i].unused = UnpackI32BE(item + 24);
	}
	DisposePtr((Ptr) data);
	
	return noErr;
}

//Linux port: copies the fields of the 32-bit big-endian Terrain into the native one; the pointer lists stay nil (PORTING.md 5.2)
static OSErr UnpackTerrainBE(TerrainPtr terrain, const unsigned char* data)
{
	long				i;
	
	BlockMove(data + 0, terrain->name, sizeof(Str63));
	terrain->id = UnpackU32BE(data + 64);
	terrain->flags = UnpackU32BE(data + 68);
	BlockMove(data + 72, terrain->unused, sizeof(terrain->unused));
	terrain->enclosureCount = UnpackI32BE(data + 196);
	UnpackFloatsBE(&terrain->timeScale, data + 328, 10); //timeScale ... sunAngle_Z
	terrain->skyFlags = UnpackU32BE(data + 368);
	terrain->numTiles = UnpackI16BE(data + 372);
	UnpackFloatsBE(&terrain->skyScale, data + 376, 5); //skyScale, skyRotation, unused_1 ... unused_3
	terrain->fogMode = UnpackI16BE(data + 396);
	UnpackFloatsBE(&terrain->fogColor_r, data + 400, 9); //fogColor_r ... fogEnd, clippingDistance, skyRadius, skyHeight
	terrain->itemReferenceCount = UnpackU32BE(data + 436);
	terrain->specialItemCount = UnpackU32BE(data + 444);
	for(i = 0; i < kMaxItems; ++i) {
		const unsigned char*	item = data + 448 + i * 24;
	
		terrain->specialItemList[i].id = UnpackU32BE(item + 0);
		terrain->specialItemList[i].flags = UnpackU32BE(item + 4);
		terrain->specialItemList[i].itemType = UnpackU32BE(item + 8);
		terrain->specialItemList[i].modelID = UnpackU32BE(item + 12);
		terrain->specialItemList[i].itemValue = UnpackI32BE(item + 16);
		terrain->specialItemList[i].unused = UnpackI32BE(item + 20);
	}
	BlockMove(data + 640, terrain->unused2, sizeof(terrain->unused2));
	terrain->trackCount = UnpackU32BE(data + 672);
	terrain->meshCount = UnpackU32BE(data + 708);
	terrain->seaCount = UnpackU32BE(data + 1224);
	terrain->terrainTextureSize_NoMipMap = UnpackU32BE(data + 1740);
	terrain->terrainTextureSize_MipMap = UnpackU32BE(data + 1744);
	terrain->terrainTextureSize_FullMipMap = UnpackU32BE(data + 1748);
	terrain->modelTextureSize_NoMipMap = UnpackU32BE(data + 1752);
	terrain->modelTextureSize_MipMap = UnpackU32BE(data + 1756);
	terrain->modelTextureSize_FullMipMap = UnpackU32BE(data + 1760);
	terrain->sourceCount = UnpackI32BE(data + 1764);
	for(i = 0; i < kMaxSources; ++i) {
		const unsigned char*	source = data + 1768 + i * 48;
	
		terrain->sourceList[i].sourceType = UnpackU32BE(source + 0);
		terrain->sourceList[i].soundID = UnpackU32BE(source + 4);
		UnpackFloatsBE(&terrain->sourceList[i].position.x, source + 8, 5); //position, maxDistance, minDistance
		terrain->sourceList[i].visibility = UnpackI32BE(source + 28);
		UnpackFloatsBE(&terrain->sourceList[i].volume, source + 32, 2); //volume, pitch
		terrain->sourceList[i].unused[0] = UnpackI32BE(source + 40);
		terrain->sourceList[i].unused[1] = UnpackI32BE(source + 44);
	}
	terrain->cameraCount = UnpackI32BE(data + 4840);
	terrain->soundCount = UnpackI32BE(data + 5100);
	terrain->unused3 = UnpackU32BE(data + 5360);
	terrain->animStartTime = UnpackI32BE(data + 5364);
	terrain->animLength = UnpackI32BE(data + 5368);
	terrain->unused4 = UnpackI32BE(data + 5372);
	terrain->animCount = UnpackU32BE(data + 5376);
	
	if((terrain->enclosureCount < 0) || (terrain->enclosureCount > kMaxEnclosures) || (terrain->specialItemCount > kMaxItems) || (terrain->trackCount > kMaxTracks)
		|| (terrain->meshCount > kMaxMesh) || (terrain->seaCount > kMaxSea) || (terrain->sourceCount < 0) || (terrain->sourceCount > kMaxSources)
		|| (terrain->cameraCount < 0) || (terrain->cameraCount > kMaxCameras) || (terrain->soundCount < 0) || (terrain->soundCount > kMaxAmbientSounds)
		|| (terrain->animCount > kMaxModelAnimations))
	return kError_FatalError;
	
	return noErr;
}

static OSErr Load_Terrain(MetaFile_ReferencePtr reference, TerrainPtr terrain, long skipFlags)
{
	OSErr				theError;
	long				bytes,
						i,
						searchTag = reference->currentTag + 1;
	short				destFileID = reference->fileRefNum;
	MetaFile_EntryPtr	tagEntry = &reference->tagTable.tagList[reference->currentTag];
	unsigned char		data[kTerrain_DiskSize];
	
	//Check size - Linux port: only the 0x0104 layout occurs in the data
	//shortcut: older (smaller) terrain layouts fail here; unpack them if such a file turns up
	if(tagEntry->dataSize != kTerrain_DiskSize)
	return kError_FatalError;
	
	//Read terrain data - Linux port: copy the fields of the 32-bit big-endian Terrain (PORTING.md 5.2)
	bytes = kTerrain_DiskSize;
	theError = FSRead(destFileID, &bytes, (Ptr) data);
	if(theError)
	return theError;
	*terrain = {}; //pointer lists stay nil
	theError = UnpackTerrainBE(terrain, data);
	if(theError)
	return theError;
	
	//Check version
	if(tagEntry->version > kVersion_Terrain)
	return kError_BadVersion;
	if(tagEntry->version <= 0x0100) {
		terrain->trackCount = 0;
	}
	if(tagEntry->version <= 0x0101) {
		terrain->itemReferenceCount = 0;
		terrain->specialItemCount = 0;
	}
	if(tagEntry->version <= 0x0102) {
		for(i = 0; i < terrain->sourceCount; ++i)
		if(terrain->sourceList[i].sourceType == kSource_Sound) {
			terrain->sourceList[i].volume = kInfinityAudio_MaxVolume;
			terrain->sourceList[i].pitch = kInfinityAudio_NormalPitch;
		}
	}
	if(tagEntry->version <= 0x0103) {
		terrain->enclosureCount = 0;
	}
	
	//Read terrain meshes
	for(i = 0; i < terrain->meshCount; ++i) {
		//Find mesh
		theError = SetPos_IndexTag(reference, kTag_TerrainMesh, 1 + i, searchTag, kFromStartDepth);
		if(theError)
		return theError;
		
		//Read mesh
		theError = Load_TerrainMesh(reference, &terrain->meshList[i]);
		if(theError)
		return theError;
	}
	for(i = 0; i < terrain->seaCount; ++i) {
		//Find mesh
		theError = SetPos_IndexTag(reference, kTag_SeaMesh, 1 + i, searchTag, kFromStartDepth);
		if(theError)
		return theError;
		
		//Read sea mesh
		theError = Load_SeaMesh(reference, &terrain->seaList[i]);
		if(theError)
		return theError;
	}
	
	//Read camera list
	if(skipFlags & kMetaFile_SkipCameras)
	terrain->cameraCount = 0;
	else
	for(i = 0; i < terrain->cameraCount; ++i) {
		//Find camera
		theError = SetPos_IndexTag(reference, kTag_Camera, 1 + i, searchTag, kFromStartDepth);
		if(theError)
		return theError;
		
		//Read camera
		theError = Load_Camera(reference, &terrain->cameraList[i]);
		if(theError)
		return theError;
	}
	
	//Read ambient sound list
	if(skipFlags & kMetaFile_SkipAmbientSounds)
	terrain->soundCount = 0;
	else
	for(i = 0; i < terrain->soundCount; ++i) {
		//Find sound
		theError = SetPos_IndexTag(reference, kTag_AmbientSound, 1 + i, searchTag, kFromStartDepth);
		if(theError)
		return theError;
		
		//Read sound
		theError = Load_AmbientSound(reference, &terrain->soundList[i]);
		if(theError)
		return theError;
	}
	
	//Read animation list
	if(skipFlags & kMetaFile_SkipAnimations)
	terrain->animCount = 0;
	else
	for(i = 0; i < terrain->animCount; ++i) {
		//Find path
		theError = SetPos_IndexTag(reference, kTag_ModelAnimation, 1 + i, searchTag, kFromStartDepth);
		if(theError)
		return theError;
		
		//Read path
		theError = Load_ModelAnimation(reference, &terrain->animList[i]);
		if(theError)
		return theError;
	}
	
	//Read track list
	if(skipFlags & kMetaFile_SkipTracks)
	terrain->trackCount = 0;
	else
	for(i = 0; i < terrain->trackCount; ++i) {
		//Find track
		theError = SetPos_IndexTag(reference, kTag_Track, 1 + i, searchTag, kFromStartDepth);
		if(theError)
		return theError;
		
		//Read track
		theError = Load_Track(reference, &terrain->trackList[i]);
		if(theError)
		return theError;
	}
	
	//Read special items list
	if(skipFlags & kMetaFile_SkipSpecialItems)
	terrain->itemReferenceCount = 0;
	else if(terrain->itemReferenceCount) {
		//Find item list
		theError = SetPos_IndexTag(reference, kTag_ItemList, 1, searchTag, kFromStartDepth);
		if(theError)
		return theError;
		if(reference->tagTable.tagList[reference->currentTag].dataSize != terrain->itemReferenceCount * kItemReference_DiskSize)
		return kError_FatalError;

		//Read item list
		theError = Load_ItemList(reference, &terrain->itemReferenceList);
		if(theError)
		return theError;
	}
	
	//Read track list
	if(skipFlags & kMetaFile_SkipEnclosures)
	terrain->enclosureCount = 0;
	else
	for(i = 0; i < terrain->enclosureCount; ++i) {
		//Find track
		theError = SetPos_IndexTag(reference, kTag_Enclosure, 1 + i, searchTag, kFromStartDepth);
		if(theError)
		return theError;
		
		//Read track
		theError = Load_Enclosure(reference, &terrain->enclosureList[i]);
		if(theError)
		return theError;
	}
	
	return noErr;
}

OSErr MetaFile_Read_Terrain(MetaFile_ReferencePtr reference, TerrainPtr terrain, long skipFlags)
{
	OSErr				theError;
	
	//Find data
	theError = SetPos_IndexTag(reference, kTag_Terrain, 1, kFromStartTag, kFromStartDepth);
	if(theError)
	return theError;
	
	return Load_Terrain(reference, terrain, skipFlags);
}

OSErr MetaFile_Read_MegaObjectNameID_ByIndex(MetaFile_ReferencePtr reference, Str63 name, OSType* id, long index)
{
	OSErr				theError;
	unsigned char		data[kObject_DiskSize];
	long				bytes;
	MetaFile_EntryPtr	tagEntry;
	short				destFileID = reference->fileRefNum;
	
	//Find data
	theError = SetPos_IndexTag(reference, kTag_MegaObject, index, kFromStartTag, kFromStartDepth);
	if(theError)
	return theError;
	
	//Read megaobject data - Linux port: only the big-endian Object at its start is needed (PORTING.md 5.2)
	tagEntry = &reference->tagTable.tagList[reference->currentTag];
	if(tagEntry->dataSize < kObject_DiskSize)
	return kError_FatalError;
	bytes = kObject_DiskSize;
	theError = FSRead(destFileID, &bytes, (Ptr) data);
	if(theError)
	return theError;
	
	BlockMove(data + 968, name, sizeof(Str31));
	*id = UnpackU32BE(data + 1032);
	
	return noErr;
}

OSErr MetaFile_Read_TerrainNameID_ByIndex(MetaFile_ReferencePtr reference, Str63 name, OSType* id, long index)
{
	OSErr				theError;
	unsigned char		data[sizeof(Str63) + 4]; //name, id
	long				bytes;
	MetaFile_EntryPtr	tagEntry;
	short				destFileID = reference->fileRefNum;

	//Find data
	theError = SetPos_IndexTag(reference, kTag_Terrain, index, kFromStartTag, kFromStartDepth);
	if(theError)
	return theError;

	//Read terrain data - Linux port: only the name and big-endian id at its start are needed (PORTING.md 5.2)
	tagEntry = &reference->tagTable.tagList[reference->currentTag];
	if(tagEntry->dataSize < sizeof(data))
	return kError_FatalError;
	bytes = sizeof(data);
	theError = FSRead(destFileID, &bytes, (Ptr) data);
	if(theError)
	return theError;

	BlockMove(data + 0, name, sizeof(Str63));
	*id = UnpackU32BE(data + 64);
	
	return noErr;
}

OSErr MetaFile_Read_TextureSetName_ByIndex(MetaFile_ReferencePtr reference, OSType modelID, Str63 name, long index)
{
	OSErr				theError;
	TextureSet			tempSet;
	long				bytes;
	MetaFile_EntryPtr	tagEntry;
	short				destFileID = reference->fileRefNum;
	
	//Find megaobject
	theError = SetPos_IDTag(reference, kTag_MegaObject, modelID, kFromStartTag, kFromStartDepth);
	if(theError)
	return theError;
	
	//Find texture set inside current megaobject
	theError = SetPos_IndexTag(reference, kTag_TextureSet, index, reference->currentTag + 1, reference->tagTable.tagList[reference->currentTag].depth + 1);
	if(theError)
	return theError;
	
	//Read texture set data
	tagEntry = &reference->tagTable.tagList[reference->currentTag];
	bytes = tagEntry->dataSize;
	theError = FSRead(destFileID, &bytes, (Ptr) &tempSet);
	if(theError)
	return theError;
	
	BlockMove(tempSet.name, name, sizeof(Str63));
	
	return noErr;
}

OSErr MetaFile_Read_Preview(MetaFile_ReferencePtr reference, PicHandle* previewPic)
{
	short				destFileID = reference->fileRefNum;
	long				bytes;
	OSErr				theError;
	MetaFile_EntryPtr	tagEntry;
	
	//Find data
	theError = SetPos_IndexTag(reference, kTag_Preview, 1, kFromStartTag, kFromStartDepth);
	if(theError)
	return theError;
	tagEntry = &reference->tagTable.tagList[reference->currentTag];
	
	//Allocate memory
	*previewPic = (PicHandle) NewHandle(tagEntry->dataSize);
	if(*previewPic == nil)
	return MemError();
	
	//Read preview data
	bytes = tagEntry->dataSize;
	HLock((Handle) *previewPic);
	theError = FSRead(destFileID, &bytes, (Ptr) **previewPic);
	HUnlock((Handle) *previewPic);
	if(theError)
	return theError;
	
	return noErr;
}

OSErr MetaFile_Read_CameraLayout(MetaFile_ReferencePtr reference, CameraStatePtr localCamera, CameraStatePtr topCamera, CameraStatePtr frontCamera, CameraStatePtr rightCamera, CameraStatePtr worldState)
{
	short				destFileID = reference->fileRefNum;
	long				bytes;
	OSErr				theError;
	MetaFile_EntryPtr	tagEntry;
	
	//Find camera layout data
	theError = SetPos_IndexTag(reference, kTag_CameraLayout, 1, kFromStartTag, kFromStartDepth);
	if(theError)
	return theError;
	tagEntry = &reference->tagTable.tagList[reference->currentTag];
	
	//Check size
	if(tagEntry->dataSize != 5 * sizeof(CameraState))
	return kError_BadVersion;
	
	//Read camera data
	bytes = sizeof(CameraState);
	theError = FSRead(destFileID, &bytes, (Ptr) localCamera);
	if(theError)
	return theError;
	
	bytes = sizeof(CameraState);
	theError = FSRead(destFileID, &bytes, (Ptr) topCamera);
	if(theError)
	return theError;
	
	bytes = sizeof(CameraState);
	theError = FSRead(destFileID, &bytes, (Ptr) frontCamera);
	if(theError)
	return theError;
	
	bytes = sizeof(CameraState);
	theError = FSRead(destFileID, &bytes, (Ptr) rightCamera);
	if(theError)
	return theError;
	
	bytes = sizeof(CameraState);
	theError = FSRead(destFileID, &bytes, (Ptr) worldState);
	if(theError)
	return theError;
	
	return noErr;
}

#if 0
OSErr MetaFile_LowLevelRead_DataPtr(MetaFile_ReferencePtr reference, long tagNum, Ptr data)
{
	short				destFileID = reference->fileRefNum;
	long				bytes;
	OSErr				theError;
	MetaFile_EntryPtr	entry = &reference->tagTable.tagList[tagNum];
	
	//Find data
	reference->currentTag = tagNum;
	if(entry->dataPosition != kMetaFile_NoData) SetFPos(reference->fileRefNum, fsFromStart, entry->dataPosition);
	
	//Read data
	bytes = entry->dataSize;
	theError = FSRead(destFileID, &bytes, data);
	if(theError)
	return theError;
	
	return noErr;
}
#else
OSErr MetaFile_LowLevelRead_DataPtr(MetaFile_ReferencePtr reference, long tagNum, Ptr* data)
{
	short				destFileID = reference->fileRefNum;
	long				bytes;
	OSErr				theError;
	MetaFile_EntryPtr	entry = &reference->tagTable.tagList[tagNum];
	
	//Find data
	reference->currentTag = tagNum;
	if(entry->dataPosition != kMetaFile_NoData) SetFPos(reference->fileRefNum, fsFromStart, entry->dataPosition);
	
	//Allocate memory
	*data = NewPtr(entry->dataSize);
	if(*data == nil)
	return MemError();
	
	//Read data
	bytes = entry->dataSize;
	theError = FSRead(destFileID, &bytes, *data);
	if(theError)
	return theError;
	
	return noErr;
}
#endif

OSErr MetaFile_LowLevelRead_DataHandle(MetaFile_ReferencePtr reference, long tagNum, Handle* data)
{
	short				destFileID = reference->fileRefNum;
	long				bytes;
	OSErr				theError;
	MetaFile_EntryPtr	entry = &reference->tagTable.tagList[tagNum];
	
	//Find data
	reference->currentTag = tagNum;
	if(entry->dataPosition != kMetaFile_NoData) SetFPos(reference->fileRefNum, fsFromStart, entry->dataPosition);
	
	//Allocate memory
	*data = NewHandle(entry->dataSize);
	if(*data == nil)
	return MemError();
	
	//Read data
	bytes = entry->dataSize;
	HLock(*data);
	theError = FSRead(destFileID, &bytes, **data);
	HUnlock(*data);
	if(theError)
	return theError;
	
	return noErr;
}

OSErr MetaFile_Read_OpenFile(FSSpec* destFile, MetaFile_ReferencePtr* newReference)
{
	OSErr					theError;
	short					destFileID;
	MetaFile_ReferencePtr	reference;
	MetaFile_Header			header;
	long					bytes;
	unsigned long			tagCount,
							i;
	unsigned char			data[kMetaFile_EntryDiskSize];
	Boolean					targetIsFolder,
							wasAliased;
	FSSpec					realFile;
	
	//Resolve alias
	realFile = *destFile;
	theError = ResolveAliasFile(&realFile, true, &targetIsFolder, &wasAliased);
	if(theError)
	return theError;
	
	//Open file
	theError = FSpOpenDF(&realFile, fsRdPerm, &destFileID);
	if(theError)
	return theError;
	
	//Read header - Linux port: unpacked from the 16 big-endian bytes on disk (PORTING.md 5.2)
	bytes = kMetaFile_HeaderDiskSize;
	theError = FSRead(destFileID, &bytes, (Ptr) data);
	if(theError)
	return theError;
	header.version = UnpackI16BE(data + 0);
	header.flags = UnpackU16BE(data + 2);
	header.creatorCode = UnpackU32BE(data + 4);
	header.tagTablePosition = UnpackI32BE(data + 8);
	header.tagTableSize = UnpackI32BE(data + 12);
	
	//Check version
	if(header.version > kMetaFile_Version) {
		FSClose(destFileID);
		return kError_BadVersion;
	}
	
	//Read tag count and check it against the table size
	SetFPos(destFileID, fsFromStart, header.tagTablePosition);
	bytes = 4;
	theError = FSRead(destFileID, &bytes, (Ptr) data);
	if(theError)
	return theError;
	tagCount = UnpackU32BE(data);
	if((tagCount > kMetaFile_MaxTagEntries) || (header.tagTableSize != 4 + tagCount * kMetaFile_EntryDiskSize)) {
		FSClose(destFileID);
		return kError_FatalError;
	}
	
	//Create reference
	reference = (MetaFile_ReferencePtr) NewPtr(sizeof(MetaFile_Reference) + tagCount * sizeof(MetaFile_Entry));
	if(reference == nil)
	return MemError();
	
	//Read tag table - Linux port: each 24-byte big-endian entry is unpacked into the native one
	reference->tagTable.tagCount = tagCount;
	for(i = 0; i < tagCount; ++i) {
		MetaFile_EntryPtr	entry = &reference->tagTable.tagList[i];
		
		bytes = kMetaFile_EntryDiskSize;
		theError = FSRead(destFileID, &bytes, (Ptr) data);
		if(theError)
		return theError;
		entry->depth = data[0];
		entry->flags = data[1];
		entry->version = UnpackI16BE(data + 2);
		entry->tag = UnpackU32BE(data + 4);
		entry->subTag = UnpackU32BE(data + 8);
		entry->ID = UnpackU32BE(data + 12);
		entry->dataSize = UnpackI32BE(data + 16);
		entry->dataPosition = UnpackI32BE(data + 20);
	}
	
	//Setup reference
	reference->fileRefNum = destFileID;
	reference->currentDepth = 0;
	reference->currentTag = 0;
	Texture_GetMetaFileFolder(&realFile, reference->textureFolder, sizeof(reference->textureFolder));
	*newReference = reference;
	
	//Reset to beginning of data
	SetFPos(destFileID, fsFromStart, kMetaFile_HeaderDiskSize);
	
	return noErr;
}

OSErr MetaFile_Read_CloseFile(MetaFile_ReferencePtr reference)
{
	short				destFileID = reference->fileRefNum;
	
	//Clean up
	FSClose(destFileID);
	DisposePtr((Ptr) reference);
	
	return noErr;
}

PicHandle MetaFile_ExtractPreview(FSSpec* theFile)
{
	MetaFile_ReferencePtr	reference;
	OSErr					theError;
	PicHandle				thePic;
	
	theError = MetaFile_Read_OpenFile(theFile, &reference);
	if(theError)
	return nil;
	
	theError = MetaFile_Read_Preview(reference, &thePic);
	MetaFile_Read_CloseFile(reference);
	if(theError)
	return nil;
	
	return thePic;
}
