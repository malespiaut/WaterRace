#pragma once
// P0 stub family: stub_common. Declarations only, no behaviour.
// Toolbox and QuickTime names used by Source/Common code/ that Pomme and the other stub
// families do not declare: trap dispatch, Cursor and Time Manager, TextEdit, the Mac
// device record accessors, the AIFF chunk layouts, the Sound Converter, the QuickTime movie
// calls used by the audio players, and Video gamma tables. Behaviour comes later (TODOFATAL).

#include "stub_quickdraw.h"
#include "stub_toolbox.h"
#include "stub_system.h"

// --- MacTypes helpers ------------------------------------------------------

#define LoWord(x)	((SInt16)((unsigned long)(x) & 0xFFFF))
#define HiWord(x)	((SInt16)(((unsigned long)(x) >> 16) & 0xFFFF))

// --- Trap dispatch (ControlStrip, Gamma and Dialog checks) -----------------
// Dead on Linux: the code only calls these after an unresolved-address check.

typedef UInt32 ProcInfoType;
#define kD0DispatchedPascalStackBased	0
#define kTwoByteCode					0
#define SIZE_CODE(x)					(x)
#define RESULT_SIZE(x)					(x)
#define DISPATCHED_STACK_ROUTINE_SELECTOR_SIZE(x)		(x)
#define DISPATCHED_STACK_ROUTINE_PARAMETER(n, size)		(size)
enum
{
	_Unimplemented			= 0xA89F,
	_ControlStripDispatch	= 0xAAF2
};

UniversalProcPtr GetToolboxTrapAddress(UInt16 trapNum);
UniversalProcPtr GetToolTrapAddress(UInt16 trapNum);
UniversalProcPtr NGetTrapAddress(UInt16 trapNum, UInt16 trapType);
SInt32 CallUniversalProc(UniversalProcPtr theProcPtr, ProcInfoType procInfo, ...);

// --- Menu Bar, Gestalt and Control Strip -----------------------------------

enum
{
	gestaltControlStripAttr		= 'cstr',
	gestaltControlStripExists	= 0
};
void DrawMenuBar(void);
SInt16 GetMBarHeight(void);
void LMSetMBarHeight(SInt16 height);
void CalcVisBehind(WindowPtr firstWindow, RgnHandle clobberedRgn);
void PaintBehind(WindowPtr startWindow, RgnHandle clobberedRgn);

// --- Quickdraw and Control extras ------------------------------------------

enum
{
	kAlignNone		= 0,
	kTransformNone	= 0
};
void PlotIconID(const Rect* theRect, SInt16 align, SInt16 transform, SInt16 theResID);
void PaintRoundRect(const Rect* r, SInt16 ovalWidth, SInt16 ovalHeight);
void DrawThemeFocusRect(const Rect* r, Boolean hasFocus);
Boolean MapRect(Rect* r, const Rect* srcRect, const Rect* dstRect);
void GlobalToLocal(Point* pt);
void LocalToGlobal(Point* pt);
SInt16 TestControl(ControlHandle theControl, Point thePoint);
void SetPtrSize(Ptr p, Size newSize);
void StringToNum(ConstStr255Param theString, long* theNum);

// --- Window and Event Manager extras ----------------------------------------

typedef WindowPtr WindowRef;
WindowPtr LMGetWindowList(void);
enum
{
	inContent		= 3
};
enum
{
	useTempMem		= 4
};
enum
{
	OSTrap			= 0,
	ToolTrap		= 1
};
// Trap number for GetDeviceList, not verified against Apple headers.
enum
{
	_GetDeviceList	= 0xA954
};

// --- Cursors ---------------------------------------------------------------

typedef struct Cursor
{
	UInt16		data[16];
	UInt16		mask[16];
	Point		hotSpot;
} Cursor;
typedef Cursor** CursHandle;
CursHandle GetCursor(SInt16 cursorID);
void SetCursor(const Cursor* crsr);

// --- Time Manager (spinning cursor) ----------------------------------------

struct TMTask;
typedef struct TMTask* TMTaskPtr;
typedef void (*TimerProcPtr)(TMTaskPtr tmTaskPtr);
typedef TimerProcPtr TimerUPP;

typedef struct QElem
{
	struct QElem*	qLink;
	unsigned short	qType;
	short			qData[1];
} QElem, *QElemPtr;

typedef struct TMTask
{
	QElemPtr		qLink;
	short			qType;
	TimerUPP		tmAddr;
	long			tmCount;
	long			tmWakeUp;
	long			tmReserved;
} TMTask;

TimerUPP NewTimerProc(TimerProcPtr userRoutine);
void DisposeTimerUPP(TimerUPP userUPP);
void InsXTime(QElemPtr qElemPtr);
void PrimeTime(QElemPtr qElemPtr, long time);
void RmvTime(QElemPtr qElemPtr);

// --- TextEdit (Agreement, Dialog Boxes) ------------------------------------

typedef struct TERec TERec;
typedef TERec** TEHandle;

typedef struct StScrpRec* StScrpPtr;
typedef StScrpPtr* StScrpHandle;

TEHandle TENew(const Rect* destRect, const Rect* viewRect);
TEHandle TEStyleNew(const Rect* destRect, const Rect* viewRect);
void TEDispose(TEHandle hTE);
void TESetText(const void* text, long length, TEHandle hTE);
void TEStyleInsert(const void* text, long length, StScrpHandle hST, TEHandle hTE);
void TEPinScroll(SInt16 dh, SInt16 dv, TEHandle hTE);
void TEScroll(SInt16 dh, SInt16 dv, TEHandle hTE);
void TEUpdate(const Rect* rUpdate, TEHandle hTE);

// --- Device manager --------------------------------------------------------
// Pomme's GDHandle is a generic Handle. The game reads these fields through GDeviceHandle casts.

typedef struct GDevice
{
	short			gdRefNum;
	short			gdType;
	PixMapHandle	gdPMap;
	Rect			gdRect;
} GDevice;
typedef GDevice** GDeviceHandle;

GDHandle GetMainDevice(void);
GDHandle GetDeviceList(void);
GDHandle GetNextDevice(GDHandle curDevice);
GDHandle GetGDevice(void);
void SetGDevice(GDHandle gd);
Boolean TestDeviceAttribute(GDHandle gdh, SInt16 attribute);
enum
{
	screenDevice	= 0,		// bit numbers, not verified against Apple headers
	noDriver		= 13,
	fixedType		= 1
};
enum
{
	cDevErr			= -1,		// value not verified against Apple headers
	cscGetGamma		= 0,		// Control call codes, not verified
	cscSetGamma		= 0
};

// --- Intl and Text Utils ---------------------------------------------------

Handle GetIntlResource(SInt16 theID);

typedef struct Intl0Rec
{
	unsigned char	decimalPt;		// only the field the game reads
} Intl0Rec;
typedef Intl0Rec** Intl0Hndl;

// --- Sound Manager and AIFF ------------------------------------------------

typedef struct SoundDescription
{
	long			descSize;
	OSType			dataFormat;
	long			resvd1;
	short			resvd2;
	short			dataRefIndex;
	short			version;
	short			revlevel;
	OSType			vendor;
	short			numChannels;
	short			sampleSize;
	short			compressionID;
	short			packetSize;
	UnsignedFixed	sampleRate;
} SoundDescription;
typedef SoundDescription** SoundDescriptionHandle;

typedef struct SoundDescriptionV1
{
	SoundDescription	desc;
	UInt32				samplesPerPacket;
	UInt32				bytesPerPacket;
	UInt32				bytesPerFrame;
	UInt32				bytesPerSample;
} SoundDescriptionV1;
typedef SoundDescriptionV1** SoundDescriptionV1Handle;

typedef struct SampleDescription SampleDescription;
typedef SampleDescription** SampleDescriptionHandle;

enum
{
	kSoundNotCompressed	= 0
};

// AIFF chunk layouts (Apple AIFF.h). Only the fields the QDesign player reads are named;
// the other chunks are declared with the common header so the union compiles.
enum
{
	FORMID				= 'FORM',
	FormatVersionID		= 'FVER',
	CommonID			= 'COMM',
	SoundDataID			= 'SSND',
	AIFCID				= 'AIFC',
	AIFCVersion1		= 0xA2805140UL
};
typedef struct ChunkHeader
{
	OSType			ckID;
	long			ckSize;
} ChunkHeader;
typedef struct ContainerChunk
{
	OSType			ckID;
	long			ckSize;
	OSType			formType;
} ContainerChunk;
typedef struct FormatVersionChunk
{
	OSType			ckID;
	long			ckSize;
	UInt32			timestamp;
} FormatVersionChunk;
typedef struct CommonChunk
{
	OSType			ckID;
	long			ckSize;
	SInt16			numChannels;
	UInt32			numSampleFrames;
	SInt16			sampleSize;
	extended80		sampleRate;
} CommonChunk;
typedef struct ExtCommonChunk
{
	OSType			ckID;
	long			ckSize;
	SInt16			numChannels;
	UInt32			numSampleFrames;
	SInt16			sampleSize;
	extended80		sampleRate;
	OSType			compressionType;
	Str255			compressionName;
} ExtCommonChunk;
typedef struct SoundDataChunk
{
	OSType			ckID;
	long			ckSize;
	UInt32			offset;
	UInt32			blockSize;
} SoundDataChunk;
typedef ChunkHeader MarkerChunk;
typedef ChunkHeader InstrumentChunk;
typedef ChunkHeader MIDIDataChunk;
typedef ChunkHeader AudioRecordingChunk;
typedef ChunkHeader CommentsChunk;
typedef ChunkHeader TextChunk;

void x80told(const extended80* x80, long double* ld);

// QuickTime atoms that the players copy as bytes. The layout is not declared.
typedef struct AudioFormatAtom
{
	long			atomSize;
	long			atomType;
	char			atomData[16];
} AudioFormatAtom, *AudioFormatAtomPtr;
typedef struct AudioTerminatorAtom
{
	long			atomSize;
	long			atomType;
} AudioTerminatorAtom;

typedef ComponentInstance SoundConverter;
OSErr SoundConverterOpen(const SoundComponentData* inputFormat, const SoundComponentData* outputFormat, SoundConverter* sc);
OSErr SoundConverterSetInfo(SoundConverter sc, OSType selector, void* infoPtr);
OSErr SoundConverterGetBufferSizes(SoundConverter sc, unsigned long inputBufferSize, unsigned long* inputFrames,
								   unsigned long* inputBytes, unsigned long* outputBytes);
OSErr SoundConverterBeginConversion(SoundConverter sc);
OSErr SoundConverterConvertBuffer(SoundConverter sc, const void* inputPtr, unsigned long inputFrames, void* outputPtr,
								  unsigned long* outputFrames, unsigned long* outputBytes);
OSErr SoundConverterClose(SoundConverter sc);

// --- Movie Toolbox (MP3 Player) --------------------------------------------

typedef struct OpaqueMovie* Movie;
typedef struct OpaqueTrack* Track;
typedef struct OpaqueMedia* Media;

enum
{
	newMovieActive			= 1,
	movieTrackMediaType		= 1,
	SoundMediaType			= 'soun'
};
OSErr OpenMovieFile(const FSSpec* fileSpec, short* resRefNum, SInt8 permission);
OSErr CloseMovieFile(short resRefNum);
OSErr NewMovieFromFile(Movie* theMovie, short resRefNum, short* resId, StringPtr resName, short newMovieFlags, Boolean* dataRefWasChanged);
Track GetMovieIndTrackType(Movie theMovie, long index, OSType trackType, long flags);
Media GetTrackMedia(Track theTrack);
void GetMediaSampleDescription(Media theMedia, long index, SampleDescriptionHandle descH);
OSErr GetMoviesError(void);
OSErr GetSoundDescriptionExtension(SoundDescriptionHandle desc, Handle* extension, OSType idType);

// --- Sound Driver (Audio CD) -----------------------------------------------

typedef ParamBlockRec* ParmBlkPtr;
OSErr MacOpenDriver(ConstStr255Param driverName, short* drvrRefNum);
OSErr PBStatusSync(ParamBlockRec* paramBlock);

// --- Video (ColorGamma, gamma fades) ---------------------------------------

OSErr HandToHand(Handle* theHndl);

typedef struct GammaTbl
{
	short			gVersion;
	short			gType;
	short			gFormulaSize;
	short			gChanCnt;
	short			gDataCnt;
	short			gDataWidth;
	char			gFormulaData[1];
} GammaTbl;
typedef GammaTbl* GammaTblPtr;
typedef GammaTblPtr* GammaTblHandle;
