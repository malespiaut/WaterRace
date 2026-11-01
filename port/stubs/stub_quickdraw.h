#pragma once
// P0 stub family: stub_quickdraw. Declarations only, no behaviour.
// Types already in Pomme (Rect, Point, RGBColor, Picture, PixMap, GrafPort, GWorldPtr, GDHandle,
// CTabHandle, PicHandle, PixMapHandle...) come from Pomme.h; only the gaps are declared here.

#ifndef pascal
#define pascal
#endif

// Regions

typedef Handle RgnHandle;

RgnHandle NewRgn(void);
void DisposeRgn(RgnHandle rgn);
void SetEmptyRgn(RgnHandle rgn);
void CopyRgn(RgnHandle srcRgn, RgnHandle dstRgn);
void RectRgn(RgnHandle rgn, const Rect* r);
void OpenRgn(void);
void CloseRgn(RgnHandle dstRgn);
void UnionRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn);
void DiffRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn);
Boolean RectInRgn(const Rect* r, RgnHandle rgn);
void SetClip(RgnHandle rgn);
void GetClip(RgnHandle rgn);
void ClipRect(const Rect* r);

// Rectangles

Boolean PtInRect(Point pt, const Rect* r);
Boolean UnionRect(const Rect* src1, const Rect* src2, Rect* dstRect);
void InsetRect(Rect* r, short dh, short dv);
void FrameRoundRect(const Rect* r, short ovalWidth, short ovalHeight);

// Pictures

PicHandle OpenPicture(const Rect* picFrame);
void ClosePicture(void);

// Pixels

void UnlockPixels(PixMapHandle pm);

// Text

typedef struct FontInfo
{
	short ascent;
	short descent;
	short widMax;
	short leading;
} FontInfo;

void DrawString(ConstStr255Param s);
void TextFont(short font);
void TextSize(short size);
void TextFace(short face);
void GetFontInfo(FontInfo* info);
void GetFNum(ConstStr255Param name, short* numPtr);

// Ports and colour

void InitGraf(void* globalPtr);
CTabHandle GetCTable(short ctID);
