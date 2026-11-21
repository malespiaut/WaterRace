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


#include		"WaterRace.h"
#include		"Text Engine.h"
#include		"Graphics/SysFont.h"

#include		<algorithm>
#include		<cstring>

//CONSTANTES:

#define			kZoomFactor			3
#define			kSpace				1
#define			kSysFontSize		12		// Pomme's SysFont (Chicago-like) is drawn at 12 points

enum {kLeft, kCentered, kRight};

//MACROS:

// Blends a 0x00RRGGBB color over an ARGB32 pixel (A,R,G,B in memory), keeping its alpha
#define Mask32(d, c, m) { (d)[1] = ((((c) >> 16) & 0xFF) * (m) + (d)[1] * (0xFF - (m))) / 255; \
						(d)[2] = ((((c) >> 8) & 0xFF) * (m) + (d)[2] * (0xFF - (m))) / 255; \
						(d)[3] = (((c) & 0xFF) * (m) + (d)[3] * (0xFF - (m))) / 255; }

//VARIABLES:

// Port: the text buffer is 8-bit coverage (0 = background, 0xFF = ink) drawn here with Pomme's SysFont
// scaled to the font size; the Mac drew Charcoal with QuickDraw into an 8-bit GWorld (PORTING.md 6.5).
//shortcut: one font for every font spec and no bold/italic, upgrade with a TTF rasterizer if Charcoal's look matters
static unsigned char*			fontBuffer = nil;
static long						bufferWidth,
								bufferHeight,
								zoomedSize;
static long						maxHeight,
								fontAscent,
								vSpace;
static long						correction = 0,
								localTopClip = 0,
								localBottomClip = 0,
								localLeftMarging = 0,
								localRightMarging = 0;

//ROUTINES:

OSErr TextEngine_Init(short maxFontSize, short maxBufferWidth)
{
	bufferWidth = maxBufferWidth * (kZoomFactor + 1);
	bufferHeight = maxFontSize * (kZoomFactor + 1);
	fontBuffer = (unsigned char*) NewPtrClear(bufferWidth * bufferHeight);
	if(fontBuffer == nil)
	return memFullErr;

	return noErr;
}

// Draws a line of text in the cleared text buffer, its top-left at (left, top) in buffer pixels. Returns its width.
static long Font_DrawText(const unsigned char* text, long length, long left, long top)
{
	long				pen = 0,
						x,
						y,
						glyphLeft;

	memset(fontBuffer, 0, bufferWidth * bufferHeight);
	for(long i = 0; i < length; ++i) {
		const Pomme::SysFont::Glyph& glyph = Pomme::SysFont::GetGlyph(text[i]);
		glyphLeft = left + (pen - Pomme::SysFont::leftMargin) * zoomedSize / kSysFontSize;
		for(y = std::max(top, 0L); y < std::min(top + Pomme::SysFont::rows * zoomedSize / kSysFontSize, bufferHeight); ++y)
		for(x = std::max(glyphLeft, 0L); x < std::min(glyphLeft + Pomme::SysFont::widthBits * zoomedSize / kSysFontSize, bufferWidth); ++x)
		if(glyph.bits[(y - top) * kSysFontSize / zoomedSize] & (1 << ((x - glyphLeft) * kSysFontSize / zoomedSize)))
		fontBuffer[y * bufferWidth + x] = 0xFF;
		pen += glyph.width + Pomme::SysFont::charSpacing;
	}

	return length ? (pen - Pomme::SysFont::charSpacing) * zoomedSize / kSysFontSize : 0;
}

void TextEngine_Settings_Text(short fontNum, short fontSize, Style fontStyle)
{
	FontInfo			info;

	zoomedSize = fontSize * kZoomFactor;
	info.ascent = Pomme::SysFont::ascend * zoomedSize / kSysFontSize;
	info.descent = Pomme::SysFont::descend * zoomedSize / kSysFontSize;
	info.leading = 0;
	info.widMax = Pomme::SysFont::widthBits * zoomedSize / kSysFontSize;
	maxHeight = (info.ascent + info.descent) / kZoomFactor;
	fontAscent = info.ascent;
	vSpace = (info.ascent + info.descent + info.leading) / kZoomFactor;
	
#if 0
	if(fontStyle & italic)
	correction = (float) info.widMax * 0.1;
	else
	correction = 0;
#else
	correction = (float) info.widMax * 0.1;
#endif
}

void TextEngine_Settings_Clipping(short topClip, short bottomClip)
{
	localTopClip = topClip;
	localBottomClip = bottomClip;
}

void TextEngine_Settings_Margings(short leftMargin, short rightMargin)
{
	localLeftMarging = leftMargin;
	localRightMarging = rightMargin;
}

short TextEngine_GetInterline()
{
	return vSpace;
}

inline void Draw_TextLine_Clipped(long textWidth, UInt32 color, PixMapPtr thePixMapPtr, Point destPoint)
{
	unsigned char		*dest;
	unsigned char		*source;
	Ptr					srcAddress,
						destAddress;
	long				srcRowBytes,
						destRowBytes;
	long				h,
						v,
						size;
						
	srcRowBytes = bufferWidth;
	srcAddress = (Ptr) fontBuffer + srcRowBytes + 1;
	destRowBytes = thePixMapPtr->rowBytes & 0x3FFF;
	destAddress = GetPixBaseAddr(&thePixMapPtr) + destRowBytes * destPoint.v + destPoint.h * 4;
	for(v = 0; v < maxHeight; ++v) {
		//Do we need to clip?
		if(destPoint.v + v < localTopClip) {
			srcAddress += kZoomFactor * srcRowBytes;
			destAddress += destRowBytes;
			continue;
		}
		if(destPoint.v + v > localBottomClip - 1)
		break;
		
		//Plot line
		source = (unsigned char*) srcAddress;
		dest = (unsigned char*) destAddress;
		for(h = 0; h < textWidth; ++h) {
			size = (*(source - srcRowBytes - 1) + *(source - srcRowBytes) + *(source - srcRowBytes + 1)
					+ *(source - 1) + *source + *(source + 1)
					+ *(source + srcRowBytes - 1) + *(source + srcRowBytes) + *(source + srcRowBytes + 1)
					) / 9;
			
			if(size)
			Mask32(dest, color, size);

			source += 3;
			dest += 4;
		}
		srcAddress += kZoomFactor * srcRowBytes;
		destAddress += destRowBytes;
	}
}

inline void Draw_TextLine_Clipped_WithFade(long textWidth, UInt32 color, PixMapPtr thePixMapPtr, Point destPoint, long fadeHeight)
{
	unsigned char		*dest;
	unsigned char		*source;
	Ptr					srcAddress,
						destAddress;
	long				srcRowBytes,
						destRowBytes;
	long				h,
						v,
						size;
						
	srcRowBytes = bufferWidth;
	srcAddress = (Ptr) fontBuffer + srcRowBytes + 1;
	destRowBytes = thePixMapPtr->rowBytes & 0x3FFF;
	destAddress = GetPixBaseAddr(&thePixMapPtr) + destRowBytes * destPoint.v + destPoint.h * 4;
	for(v = 0; v < maxHeight; ++v) {
		//Do we need to clip?
		if(destPoint.v + v < localTopClip) {
			srcAddress += kZoomFactor * srcRowBytes;
			destAddress += destRowBytes;
			continue;
		}
		if(destPoint.v + v > localBottomClip - 1)
		break;
		
		//Do we need to fade
		source = (unsigned char*) srcAddress;
		dest = (unsigned char*) destAddress;
		if(destPoint.v + v < localBottomClip - fadeHeight) {
			//Plot line
			for(h = 0; h < textWidth; ++h) {
				size = (*(source - srcRowBytes - 1) + *(source - srcRowBytes) + *(source - srcRowBytes + 1)
						+ *(source - 1) + *source + *(source + 1)
						+ *(source + srcRowBytes - 1) + *(source + srcRowBytes) + *(source + srcRowBytes + 1)
						) / 9;
				
				if(size)
				Mask32(dest, color, size);

				source += 3;
				dest += 4;
			}
		}
		else {
			//Plot line - with fade!
			float factor = 1.0 - (float) (destPoint.v + v - (localBottomClip - fadeHeight)) / (float) fadeHeight;
			for(h = 0; h < textWidth; ++h) {
				size = (*(source - srcRowBytes - 1) + *(source - srcRowBytes) + *(source - srcRowBytes + 1)
						+ *(source - 1) + *source + *(source + 1)
						+ *(source + srcRowBytes - 1) + *(source + srcRowBytes) + *(source + srcRowBytes + 1)
						) / 9;
				
				if(size)
				Mask32(dest, color, (long) (size * factor));

				source += 3;
				dest += 4;
			}
		}
		srcAddress += kZoomFactor * srcRowBytes;
		destAddress += destRowBytes;
	}
}

void TextEngine_DrawLine_Left(Ptr textPtr, short textLength, UInt32 color, PixMapPtr thePixMapPtr, Point destPoint)
{
	long				textWidth;

	//Draw text in text buffer
	textWidth = (Font_DrawText((unsigned char*) textPtr, textLength, kZoomFactor * kSpace, kZoomFactor * kSpace) + correction) / kZoomFactor;

	//Copy and anti-alias text to buffer
	Draw_TextLine_Clipped(textWidth, color, thePixMapPtr, destPoint);
}

void TextEngine_DrawLine_Centered(Ptr textPtr, short textLength, UInt32 color, PixMapPtr thePixMapPtr, Point destPoint)
{
	long				textWidth;

	//Draw text in text buffer
	textWidth = (Font_DrawText((unsigned char*) textPtr, textLength, kZoomFactor * kSpace, kZoomFactor * kSpace) + correction) / kZoomFactor;

	//Adjust text position
	destPoint.h -= textWidth / 2;
	
	//Copy and anti-alias text to buffer
	Draw_TextLine_Clipped(textWidth, color, thePixMapPtr, destPoint);
}

void TextEngine_DrawLine_Right(Ptr textPtr, short textLength, UInt32 color, PixMapPtr thePixMapPtr, Point destPoint)
{
	long				textWidth;

	//Draw text in text buffer
	textWidth = (Font_DrawText((unsigned char*) textPtr, textLength, kZoomFactor * kSpace, kZoomFactor * kSpace) + correction) / kZoomFactor;

	//Adjust text position
	destPoint.h -= textWidth;
	
	//Copy and anti-alias text to buffer
	Draw_TextLine_Clipped(textWidth, color, thePixMapPtr, destPoint);
}

void TextEngine_DrawText_Left(Ptr textPtr, short textLength, UInt32 color, PixMapPtr thePixMapPtr, Point destPoint)
{
	unsigned char			*thePtr = (unsigned char*) textPtr,
							*tempPtr;
	long					numCharsLeft = textLength,
							numChars;
							
	while(numCharsLeft > 0) {
		tempPtr = thePtr;
		numChars = 0;
		while((*tempPtr != '\r') && (numChars < numCharsLeft)) {
			++tempPtr;
			++numChars;
		}
		
		TextEngine_DrawLine_Left((Ptr) thePtr, numChars, color, thePixMapPtr, destPoint);
		
		destPoint.v += vSpace;
		thePtr += numChars + 1;
		numCharsLeft -= numChars + 1;
	}
}

void TextEngine_DrawFormattedText(Ptr textPtr, short textLength, TE_FontSpec fontSpecs[], PixMapPtr thePixMapPtr, Point destPoint)
{
	unsigned char			*thePtr = (unsigned char*) textPtr,
							*tempPtr;
	long					numCharsLeft = textLength,
							numChars;
	UInt32					curColor = 0xFFFFFF;
	long					curAlign = kLeft,
							specNum;
	long					textWidth;

	while(numCharsLeft > 0) {
		//Check for tag
		if(*thePtr == '<') {
			//Read alignement
			++thePtr;
			switch(*thePtr) {
				case 'L': curAlign = kLeft; break;
				case 'C': curAlign = kCentered; break;
				case 'R': curAlign = kRight; break;
			}
			//Read text spec
			++thePtr;
			specNum = *thePtr - '1';
			TextEngine_Settings_Text(fontSpecs[specNum].fontNum, fontSpecs[specNum].fontSize, fontSpecs[specNum].fontStyle);
			curColor = fontSpecs[specNum].fontColor;
			//Skip close tag
			thePtr += 2;
			
			numCharsLeft -= 4;
		}
		
		//Extract line of text
		tempPtr = thePtr;
		numChars = 0;
		while((*tempPtr != '\r') && (numChars < numCharsLeft)) {
			++tempPtr;
			++numChars;
		}
		
		//Draw line of text
		if((destPoint.v > -maxHeight) && (destPoint.v < localBottomClip)) {
			//Draw text in text buffer
			textWidth = (Font_DrawText(thePtr, numChars, kZoomFactor * kSpace, kZoomFactor * kSpace) + correction) / kZoomFactor;
			
			//Calculate text position
			switch(curAlign) {
				case kLeft: destPoint.h = localLeftMarging; break;
				case kCentered: destPoint.h = (localLeftMarging + localRightMarging) / 2 - textWidth / 2; break;
				case kRight: destPoint.h = localRightMarging - textWidth; break;
			}
			
			//Copy and anti-alias text to buffer
			Draw_TextLine_Clipped(textWidth, curColor, thePixMapPtr, destPoint);
		}
		
		destPoint.v += vSpace;
		thePtr += numChars + 1;
		numCharsLeft -= numChars + 1;
	}
}

void TextEngine_DrawFormattedText_WithFade(Ptr textPtr, short textLength, TE_FontSpec fontSpecs[], PixMapPtr thePixMapPtr, Point destPoint, long fadeHeight)
{
	unsigned char			*thePtr = (unsigned char*) textPtr,
							*tempPtr;
	long					numCharsLeft = textLength,
							numChars;
	UInt32					curColor = 0xFFFFFF;
	long					curAlign = kLeft,
							specNum;
	long					textWidth;

	while(numCharsLeft > 0) {
		//Check for tag
		if(*thePtr == '<') {
			//Read alignement
			++thePtr;
			switch(*thePtr) {
				case 'L': curAlign = kLeft; break;
				case 'C': curAlign = kCentered; break;
				case 'R': curAlign = kRight; break;
			}
			//Read text spec
			++thePtr;
			specNum = *thePtr - '1';
			TextEngine_Settings_Text(fontSpecs[specNum].fontNum, fontSpecs[specNum].fontSize, fontSpecs[specNum].fontStyle);
			curColor = fontSpecs[specNum].fontColor;
			//Skip close tag
			thePtr += 2;
			
			numCharsLeft -= 4;
		}
		
		//Extract line of text
		tempPtr = thePtr;
		numChars = 0;
		while((*tempPtr != '\r') && (numChars < numCharsLeft)) {
			++tempPtr;
			++numChars;
		}
		
		//Draw line of text
		if((destPoint.v > -maxHeight) && (destPoint.v < localBottomClip)) {
			//Draw text in text buffer
			textWidth = (Font_DrawText(thePtr, numChars, kZoomFactor * kSpace, kZoomFactor * kSpace) + correction) / kZoomFactor;
			
			//Calculate text position
			switch(curAlign) {
				case kLeft: destPoint.h = localLeftMarging; break;
				case kCentered: destPoint.h = (localLeftMarging + localRightMarging) / 2 - textWidth / 2; break;
				case kRight: destPoint.h = localRightMarging - textWidth; break;
			}
			
			//Copy and anti-alias text to buffer
			if(destPoint.v < localBottomClip - fadeHeight)
			Draw_TextLine_Clipped(textWidth, curColor, thePixMapPtr, destPoint);
			else
			Draw_TextLine_Clipped_WithFade(textWidth, curColor, thePixMapPtr, destPoint, fadeHeight);
		}
		
		destPoint.v += vSpace;
		thePtr += numChars + 1;
		numCharsLeft -= numChars + 1;
	}
}

OSErr TextEngine_Quit()
{
	if(fontBuffer == nil)
	return noErr;

	DisposePtr((Ptr) fontBuffer);
	fontBuffer = nil;

	return noErr;
}
