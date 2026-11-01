#pragma once
// P0 stub family: stub_game_a. Declarations only, no behaviour.
// Mac types and globals used by the WaterRace final/ game files that Pomme and the other stub
// families do not declare.

// Style is the QuickDraw font-style bitfield (Mac: unsigned char).
typedef unsigned char Style;

// Volume lookup by Mac HFS name (Pomme only has the flat GetVol).
OSErr HGetVol(StringPtr volName, short* vRefNum, long* dirID);

// Random number generator (Mac Toolbox, 16-bit range).
short Random(void);

// QuickDraw globals: only the fields the game reads.
typedef struct QDGlobals
{
	GrafPort	thePort;
	long		randSeed;
} QDGlobals;
extern QDGlobals qd;

// Date and time (Mac Toolbox DateTimeRec; GetTime is the Toolbox call, not <ctime>).
typedef struct DateTimeRec
{
	short year, month, day, hour, minute, second, dayOfWeek;
} DateTimeRec;
void GetTime(DateTimeRec* dateTime);

// Startup volume and the root folder directory ID (Files Manager constants).
short GetStartUpVolume(void);
#define fsRtDirID 2

// Main monitor rectangle in global coordinates (the GDevice gdRect the game reads).
Rect GetMainGDRect(void);

// Resource fork open by Mac volume and directory (Pomme only has FSpOpenResFile).
short HOpenResFile(short vRefNum, long dirID, ConstStr255Param fileName, SInt8 permission);

// RAVE draw-context flush (Drawing.cp; the Infinity Engine calls are commented out).
TQAError QAFlush(TQADrawContext* drawContext);
