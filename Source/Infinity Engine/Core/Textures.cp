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


#include			<Rave.h>

#include			<cstring>
#include			<vector>

#include			"PommeFiles.h"
#include			"Infinity Structures.h"
#include			"Infinity Error Codes.h"
#include			"Textures.h"
#include			"TextureLoader.h"

//CONSTANTES:

#define				kDataFolder				"WaterRace Data"
#define				kTexturesFolder			kDataFolder "/Textures"
#define				kPictPNGFolder			kDataFolder "/Post FX PNG"
#define				kMaxTQAImages			16

//ROUTINES:

//Linux port: textures come from the PNGs written by tools/convert_textures.py (PORTING.md 5.3), not from the PICT blob

//The CompressedTexture header on disk: 28 big-endian bytes (name, flags, width, height, depth, PICT handle, size, alphaDepth)
void Texture_UnpackHeader(CompressedTexturePtr texturePtr, const unsigned char* header)
{
	auto					be16 = [header](int p) { return (UInt16) ((header[p] << 8) | header[p + 1]); };
	auto					be32 = [header](int p) { return ((UInt32) header[p] << 24) | (header[p + 1] << 16) | (header[p + 2] << 8) | header[p + 3]; };

	texturePtr->name = be32(0);
	texturePtr->flags = be32(4);
	texturePtr->width = be16(8);
	texturePtr->height = be16(10);
	texturePtr->depth = be16(12);
	texturePtr->pngPath = nil;
	texturePtr->uncompressedSize = be32(20);
	texturePtr->alphaDepth = (SInt16) be16(24);
}

//Folder of a MetaFile relative to the data folder (e.g. "Locations/Venice"), as used under kTexturesFolder; "" if not found
void Texture_GetMetaFileFolder(const FSSpec* metaFile, char* folder, long size)
{
	fs::path			dataFolder = fs::current_path() / kDataFolder;
	std::error_code		err;
	
	folder[0] = 0;
	for(fs::recursive_directory_iterator it(dataFolder, err), end; it != end; it.increment(err)) {
		fs::path			path = (it->path() / metaFile->cName).lexically_normal();
		
		if(!it->is_directory() || !fs::is_regular_file(path))
		continue;
		//Pomme gives the same parID to every file of a host directory
		//shortcut: assumes the game's FSSpec came from Pomme's FSMakeFSSpec path handling; revisit when PBGetCatInfo stops being a stub
		if(Pomme::Files::HostPathToFSSpec(path).parID == metaFile->parID) {
			snprintf(folder, size, "%s", path.lexically_relative(dataFolder).generic_string().c_str());
			return;
		}
	}
}

char* Texture_NewPNGPath(const char* metaFileFolder, OSType name)
{
	std::string			path = Texture_PNGPath(kTexturesFolder, metaFileFolder, name);
	char*				pngPath = NewPtr(path.size() + 1);
	
	if(pngPath != nil)
	strcpy(pngPath, path.c_str());
	
	return pngPath;
}

//Load a PNG and its mip chain into TQAImages pointing into levels
static OSErr Texture_LoadImages(const char* path, unsigned long textureFlags, short alphaDepth, long flags, std::vector<TextureLevel>& levels, TQAImage image[kMaxTQAImages])
{
	if(!Texture_LoadPNG(path, textureFlags, alphaDepth, flags, levels) || (levels.size() > kMaxTQAImages))
	return kError_FatalError;
	
	for(long i = 0; i < levels.size(); ++i) {
		image[i].width = levels[i].width;
		image[i].height = levels[i].height;
		image[i].rowBytes = levels[i].width * 4;
		image[i].pixmap = (Ptr) levels[i].pixels.data();
	}
	
	return noErr;
}

OSErr Texture_NewFromCompressedTexture(TQAEngine* engine, TQATexture** RAVETexture, CompressedTexturePtr texturePtr, long flags)
{
	std::vector<TextureLevel>	levels;
	TQAImage				image[kMaxTQAImages];
	unsigned long			raveFlags;
	TQAError				theQAErr = kQANoErr;
	
	if((texturePtr == nil) || (texturePtr->pngPath == nil))
	return kError_FatalError;
	if((texturePtr->width > kMaxTextureSizeH) || (texturePtr->height > kMaxTextureSizeV))
	return kError_FatalError;
	
	//Load picture, reduce it and build its mipmaps (Reduce4X, Reduce16X, 3DfxMode, MipMap, ForceMipMap)
	if(Texture_LoadImages(texturePtr->pngPath, texturePtr->flags, texturePtr->alphaDepth, flags, levels, image))
	return kError_FatalError;
	
	//Setup Rave flags
	raveFlags = kQATexture_None;
	if(flags & kTextureFlag_Compress)
	raveFlags |= kQATexture_HighCompression;
	else if(!(flags & kTextureFlag_LowQuality))
	raveFlags |= kQATexture_NoCompression;
	if(levels.size() > 1)
	raveFlags |= kQATexture_Mipmap;
	if(flags & kTextureFlag_ForceAGP) //ATI only
	raveFlags |= 3 << 28;
	
	//Create Rave texture - pixels are always RGBA8 bytes now, whatever the alpha depth (PORTING.md 4)
	theQAErr = QATextureNew(engine, raveFlags, kQAPixel_ARGB32, image, RAVETexture);
	
	//Force load into VRAM
	if(theQAErr == kQANoErr) {
		theQAErr = QATextureDetach(engine, *RAVETexture); 
		if(theQAErr != kQANoErr) {		
			if(*RAVETexture != nil)
			QATextureDelete(engine, *RAVETexture);
			*RAVETexture = nil;
		}
	}
	
	return theQAErr;
}

//PICT resources of the app fork (Post FX textures, World.cp turbo/joker/wake, tools/convert_interface.py --argb) are read from kPictPNGFolder/<pictID>.png:
//not "Interface PNG", whose 7000/7001 are other pictures (Champion.cp backgrounds)
static std::string Texture_PictPNGPath(short pictID)
{
	return std::string(kPictPNGFolder "/") + std::to_string(pictID) + ".png";
}

OSErr Texture_NewARGB32BitmapFromPictResource(TQAEngine* engine, TQABitmap** RAVEBitmap, short pictID, long flags)
{
	TQAError		theQAErr = kQANoErr;
	std::vector<TextureLevel>	levels;
	TQAImage		image[kMaxTQAImages];
	
	//Get picture - bitmaps are never reduced
	if(Texture_LoadImages(Texture_PictPNGPath(pictID).c_str(), 0, kAlpha_8Bits, 0, levels, image))
	return resNotFound;
	
	//Create Rave texture
	if(flags & kTextureFlag_Compress)
	theQAErr = QABitmapNew(engine, kQABitmap_HighCompression, kQAPixel_ARGB32, image, RAVEBitmap);
	else if(!(flags & kTextureFlag_LowQuality))
	theQAErr = QABitmapNew(engine, kQABitmap_NoCompression, kQAPixel_ARGB32, image, RAVEBitmap);
	else
	theQAErr = QABitmapNew(engine, kQABitmap_None, kQAPixel_ARGB32, image, RAVEBitmap);
	if(theQAErr == kQANoErr) {
		theQAErr = QABitmapDetach(engine, *RAVEBitmap);
		if(theQAErr != kQANoErr) {	
			if(*RAVEBitmap != nil)
			QABitmapDelete(engine, *RAVEBitmap);
			*RAVEBitmap = nil;
		}
	}
	
	return theQAErr;
}

OSErr Texture_NewARGB32TextureFromPictResource(TQAEngine* engine, TQATexture** RAVETexture, short pictID, long flags)
{
	TQAError		theQAErr = kQANoErr;
	std::vector<TextureLevel>	levels;
	TQAImage		image[kMaxTQAImages];
	unsigned long	raveFlags;
	
	//Get picture, reduce it if required - no mipmaps
	if(Texture_LoadImages(Texture_PictPNGPath(pictID).c_str(), 0, kAlpha_8Bits, flags & (kTextureFlag_Reduce16X | kTextureFlag_Reduce4X | kTextureFlag_3DfxMode), levels, image))
	return resNotFound;
	
	//Setup Rave flags
	raveFlags = kQATexture_None;
	if(flags & kTextureFlag_Compress)
	raveFlags |= kQATexture_HighCompression;
	else if(!(flags & kTextureFlag_LowQuality))
	raveFlags |= kQATexture_NoCompression;
	if(flags & kTextureFlag_ForceAGP) //ATI only
	raveFlags |= 3 << 28;
	
	//Create Rave texture
	theQAErr = QATextureNew(engine, raveFlags, kQAPixel_ARGB32, image, RAVETexture);
	if(theQAErr == kQANoErr) {
		theQAErr = QATextureDetach(engine, *RAVETexture); 
		if(theQAErr != kQANoErr) {	
			if(*RAVETexture != nil)
			QATextureDelete(engine, *RAVETexture);
			*RAVETexture = nil;
		}
	}
	
	return theQAErr;
}
