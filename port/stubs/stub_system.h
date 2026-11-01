#pragma once
// P0 stub family: stub_system. Declarations only, no behaviour.
// File Manager, Folder Manager, Display Manager, Gestalt, Memory Manager error.
// Pomme.h already provides FSSpec, FSpOpenDF, FSpCreate, FSRead/FSWrite/FSClose, Sound Manager and Memory calls.

#include "Pomme.h"

// 'pascal' is a CodeWarrior keyword that Pomme does not define. On the port it is a no-op.
#ifndef pascal
#define pascal
#endif

//-----------------------------------------------------------------------------
// Gestalt (values not checked against Apple headers; Gestalt is unimplemented)

enum
{
	gestaltSystemVersion	= 'sysv',
	gestaltNativeCPUtype	= 'cput',
	gestaltCPU601			= 1,
	gestaltCPU603			= 2,
	gestaltCPU603e			= 3,
	gestaltCPU603ev			= 4,
	gestaltCPU604			= 5,
	gestaltCPU604e			= 6,
	gestaltCPU604ev			= 7,
	gestaltCPU750			= 8
};

#ifdef __cplusplus
extern "C" {
#endif

OSErr Gestalt(OSType selector, long* response);

//-----------------------------------------------------------------------------
// Display Manager and Video types

// Bit numbers in DMCheckDisplayMode flags. Value not verified against Apple headers.
enum { kNoSwitchConfirmBit = 6 };

typedef unsigned long DisplayIDType;
typedef unsigned long DMListIndexType;
typedef Handle DMListType;

typedef struct VDSwitchInfoRec
{
	unsigned short	csMode;
	unsigned long	csData;
	unsigned short	csPage;
	Ptr				csBaseAddr;
	unsigned long	csReserved;
} VDSwitchInfoRec;
typedef VDSwitchInfoRec* VDSwitchInfoPtr;

typedef struct VPBlock
{
	long			vpBaseOffset;
	Rect			vpBounds;
	short			vpVersion;
	short			vpPackType;
	long			vpPackSize;
	Fixed			vpHRes;
	Fixed			vpVRes;
	short			vpPixelType;
	short			vpPixelSize;
	short			vpCmpCount;
	short			vpCmpSize;
	long			vpPlaneBytes;
} VPBlock;
typedef VPBlock* VPBlockPtr;

typedef struct DMDepthBlock
{
	VPBlockPtr		depthVPBlock;
	VDSwitchInfoPtr	depthSwitchInfo;
} DMDepthBlock;

typedef struct DMDepthInfo
{
	unsigned long	depthBlockCount;
	DMDepthBlock	depthVPBlock[1];
} DMDepthInfo;
typedef DMDepthInfo* DMDepthInfoPtr;

typedef struct DMDisplayModeListEntry
{
	Str255			displayModeName;
	DMDepthInfoPtr	displayModeDepthBlockInfo;
} DMDisplayModeListEntry;
typedef DMDisplayModeListEntry* DMDisplayModeListEntryPtr;

typedef void (*DMDisplayModeListIteratorProcPtr)(void* userData, DMListIndexType itemIndex, DMDisplayModeListEntryPtr info);
typedef DMDisplayModeListIteratorProcPtr DMDisplayModeListIteratorUPP;

// UPPs are plain function pointers on the port. The game passes callbacks with differing
// userData pointer types, so the cast is done in the macro.
#define NewDMDisplayModeListIteratorProc(proc)	((DMDisplayModeListIteratorUPP)(proc))
#define DisposeRoutineDescriptor(upp)			((void)(upp))

OSErr DMBeginConfigureDisplays(Handle* displayState);
OSErr DMEndConfigureDisplays(Handle displayState);
OSErr DMGetDisplayMode(GDHandle device, VDSwitchInfoRec* switchInfo);
OSErr DMSetDisplayMode(GDHandle device, unsigned long displayMode, unsigned long* depthMode, unsigned long flags, Handle displayState);
OSErr DMCheckDisplayMode(GDHandle device, unsigned long displayMode, unsigned long depthMode, unsigned long* flags, unsigned long reserved, Boolean* modeOK);
OSErr DMNewDisplayModeList(DisplayIDType displayID, unsigned long options, unsigned long filter, UInt32* numModes, DMListType* theList);
OSErr DMGetIndexedDisplayModeFromList(DMListType theList, DMListIndexType index, unsigned long options, DMDisplayModeListIteratorUPP proc, void* userData);
OSErr DMDisposeList(DMListType theList);
OSErr DMGetDisplayIDByGDevice(GDHandle device, DisplayIDType* displayID, Boolean failToMain);
OSErr DMGetGDeviceByDisplayID(DisplayIDType displayID, GDHandle* device, Boolean failToMain);
GDHandle DMGetFirstScreenDevice(Boolean activeOnly);
GDHandle DMGetNextScreenDevice(GDHandle device, Boolean activeOnly);

//-----------------------------------------------------------------------------
// File Manager and Folder Manager

typedef struct FInfo
{
	OSType			fdType;
	OSType			fdCreator;
	unsigned short	fdFlags;
	Point			fdLocation;
	short			fdFldr;
} FInfo;

typedef struct DInfo
{
	Rect			frRect;
	unsigned short	frFlags;
	Point			frLocation;
	short			frView;
} DInfo;

// Parameter blocks. Each variant starts with the common header, as in the Mac headers.
#define PB_COMMON_HEADER \
	void*			qLink; \
	short			qType; \
	short			ioTrap; \
	Ptr				ioCmdAddr; \
	ProcPtr			ioCompletion; \
	OSErr			ioResult; \
	StringPtr		ioNamePtr; \
	short			ioVRefNum;

typedef struct HVolumeParam
{
	PB_COMMON_HEADER
	short			ioVolIndex;
	short			ioVDRefNum;
} HVolumeParam;

typedef struct CntrlParam
{
	PB_COMMON_HEADER
	short			ioCRefNum;
	short			csCode;
	short			csParam[11];
} CntrlParam;

typedef union ParamBlockRec
{
	HVolumeParam	volumeParam;
	CntrlParam		cntrlParam;
} ParamBlockRec;
typedef ParamBlockRec* ParamBlockRecPtr;
typedef ParamBlockRec HParamBlockRec;

typedef struct HFileInfo
{
	PB_COMMON_HEADER
	short			ioFRefNum;
	SInt8			ioFVersNum;
	SInt8			filler1;
	short			ioFDirIndex;
	SInt8			ioFlAttrib;
	UInt8			ioACUser;
	FInfo			ioFlFndrInfo;
	long			ioDirID;
} HFileInfo;

typedef struct DirInfo
{
	PB_COMMON_HEADER
	short			ioFRefNum;
	SInt8			ioFVersNum;
	SInt8			filler1;
	short			ioFDirIndex;
	SInt8			ioFlAttrib;
	UInt8			ioACUser;
	DInfo			ioDrUsrWds;
	long			ioDrDirID;
	unsigned short	ioDrNmFls;
} DirInfo;

// Catalog info record: the game casts it to HFileInfo* or DirInfo*.
typedef union CInfoPBRec
{
	HFileInfo		hFileInfo;
	DirInfo			dirInfo;
} CInfoPBRec;

OSErr PBGetCatInfo(CInfoPBRec* paramBlock, Boolean async);
OSErr PBHGetVInfoSync(HParamBlockRec* paramBlock);
OSErr PBGetVInfo(ParamBlockRec* paramBlock, Boolean async);
OSErr PBControlSync(ParamBlockRec* paramBlock);
OSErr ResolveAliasFile(FSSpec* theSpec, Boolean resolveChains, Boolean* targetIsFolder, Boolean* wasAliased);
OSErr FSpGetFInfo(const FSSpec* spec, FInfo* fileInfo);
Boolean EqualString(ConstStr255Param str1, ConstStr255Param str2, Boolean caseSensitive, Boolean diacSensitive);

//-----------------------------------------------------------------------------
// Sound Manager sound header types (Sound Manager calls live in Pomme.h)

typedef long double extended80;	// 80-bit float on the Mac; long double here

typedef struct SoundComponentData
{
	unsigned long	flags;
	OSType			format;
	short			numChannels;
	short			sampleSize;
	UnsignedFixed	sampleRate;
	unsigned long	sampleCount;
	unsigned char*	buffer;
	long			reserved;
} SoundComponentData;

typedef struct SoundHeader
{
	Ptr				samplePtr;
	unsigned long	numChannels;
	UnsignedFixed	sampleRate;
	unsigned long	loopStart;
	unsigned long	loopEnd;
	UInt8			encode;
	UInt8			baseFrequency;
	UInt8			sampleArea[1];
} SoundHeader;
typedef SoundHeader* SoundHeaderPtr;

typedef struct ExtSoundHeader
{
	Ptr				samplePtr;
	unsigned long	numChannels;
	UnsignedFixed	sampleRate;
	unsigned long	loopStart;
	unsigned long	loopEnd;
	UInt8			encode;
	UInt8			baseFrequency;
	unsigned long	numFrames;
	extended80		AIFFSampleRate;
	Ptr				markerChunk;
	OSType			format;
	Ptr				instrumentChunks;
	Ptr				AESRecording;
	unsigned short	sampleSize;
	unsigned short	futureUse1;
	unsigned long	futureUse2;
	unsigned long	futureUse3;
	unsigned long	futureUse4;
	UInt8			sampleArea[1];
} ExtSoundHeader;
typedef ExtSoundHeader* ExtSoundHeaderPtr;

// shortcut: CmpSoundHeader layout not modelled, it shares ExtSoundHeader's fields; upgrade if the compressed path reads other fields.
typedef ExtSoundHeader CmpSoundHeader;
typedef CmpSoundHeader* CmpSoundHeaderPtr;

enum
{
	stdSH	= 0x00,
	extSH	= 0xFF,
	cmpSH	= 0xFE
};

// Sound Manager constants: siDecompressionParams value not verified against Apple headers.
enum { siDecompressionParams = 'sdcp' };

OSErr ParseSndHeader(SndListHandle sndHandle, SoundComponentData* info, unsigned long* numFrames, long* dataOffset);

//-----------------------------------------------------------------------------
// Low-memory globals (MemError is declared in stub_toolbox.h)

void LMSetCurDirStore(long dirID);
void LMSetSFSaveDisk(short vRefNum);

#ifdef __cplusplus
}
#endif
