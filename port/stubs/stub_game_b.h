#pragma once
// P0 stub family: stub_game_b. Declarations only, no behaviour.
// Toolbox names used by WaterRace final/ (event, window, dialog, menu, text, file, gestalt)
// that no other stub family declares. Values of XTI options are unchecked (networking is P7).

// --- Event Manager ---------------------------------------------------------

enum {
	nullEvent = 0,
	mouseDown = 1,
	mouseUp = 2,
	keyDown = 3,
	keyUp = 4,
	autoKey = 5,
	updateEvt = 6,
	activateEvt = 8
};
enum {
	charCodeMask = 0x000000FF,
	keyCodeMask = 0x0000FF00,
	cmdKey = 0x0100
};

Boolean OSEventAvail(SInt16 mask, EventRecord* theEvent);

// --- Window, Region and Dialog Managers ------------------------------------

// Only the bounding box is read by the game.
typedef struct MacRegion
{
	SInt16		rgnSize;
	Rect		rgnBBox;
} MacRegion;
typedef MacRegion** MacRegionHandle;

enum {
	inDrag = 4
};

MacRegionHandle LMGetGrayRgn(void);
WindowPtr GetNewCWindow(SInt16 windowID, void* wStorage, WindowPtr behind);
void ShowHide(WindowPtr theWindow, Boolean showFlag);
void UpdateControls(WindowPtr theWindow, RgnHandle updateRgn);

enum {
	appendDITLRight = 0,
	appendDITLBottom = 1,
	appendDITLLeft = 2,
	appendDITLTop = 3,
	overlayDITL = 4
};
SInt16 CountDITL(DialogPtr theDialog);
void ShortenDITL(DialogPtr theDialog, SInt16 numberItems);
SInt16 Alert(SInt16 alertID, ModalFilterProcPtr filterProc);

// --- Menu Manager ----------------------------------------------------------

void InsertMenuItem(MenuHandle theMenu, ConstStr255Param itemString, SInt16 afterItem);

// --- Toolbox initialisation ------------------------------------------------

void InitFonts(void);
void InitWindows(void);
void InitMenus(void);
void TEInit(void);
void InitDialogs(void* continueProc);

// --- Text ------------------------------------------------------------------

short TextWidth(const void* textBuf, short firstByte, short byteCount);
void DrawText(const void* textBuf, short firstByte, short byteCount);
Boolean IsAntiAliasedTextEnabled(short* size);
void SetAntiAliasedTextEnabled(Boolean enable, short size);

// --- File Manager (HFS, non-FSSpec calls) ----------------------------------

OSErr HOpen(short vRefNum, long dirID, ConstStr255Param fileName, SInt8 permission, short* refNum);
OSErr HCreate(short vRefNum, long dirID, ConstStr255Param fileName, OSType creator, OSType fileType);

// CInfoPBRec and HFileInfo come from stub_system.h; the game calls the sync form.
OSErr PBGetCatInfoSync(CInfoPBRec* paramBlock);

// --- Apple Events (odoc handler in main.cp; replaced by a fixed replay folder, PORTING.md 6.2) ---

typedef UInt32 DescType;
typedef UInt32 AEKeyword;
typedef struct AEDesc
{
	DescType	descriptorType;
	Handle		dataHandle;
} AEDesc;
typedef AEDesc AEDescList;
typedef AEDesc AppleEvent;
typedef OSErr (*AEEventHandlerProcPtr)(const AppleEvent* theAppleEvent, AppleEvent* reply, long handlerRefCon);
enum {
	kCoreEventClass = 'aevt',
	kAEOpenDocuments = 'odoc',
	typeAppleEvent = 'aevt',
	typeAEList = 'list',
	typeFSS = 'fss ',
	keyDirectObject = '----',
	highLevelEventMask = 0x0400
};
UniversalProcPtr NewAEEventHandlerProc(AEEventHandlerProcPtr userRoutine);
OSErr AEInstallEventHandler(OSType eventClass, OSType eventID, UniversalProcPtr handler, long handlerRefCon, Boolean isSysHandler);
OSErr AERemoveEventHandler(OSType eventClass, OSType eventID, UniversalProcPtr handler, Boolean isSysHandler);
OSErr AEProcessAppleEvent(const EventRecord* theEventRecord);
OSErr AEGetParamDesc(const AppleEvent* theAppleEvent, AEKeyword theAEKeyword, DescType desiredType, AEDesc* result);
OSErr AECountItems(const AEDescList* theAEDescList, long* theCount);
OSErr AEGetNthPtr(const AEDescList* theAEDescList, long index, DescType desiredType, AEKeyword* theAEKeyword, DescType* typeCode, void* dataPtr, Size maximumSize, Size* actualSize);
OSErr AEDisposeDesc(AEDesc* theAEDesc);

// --- Gestalt, Appearance and version numbers -------------------------------

enum {
	gestaltAppearanceAttr = 'appr',
	gestaltAppearanceExists = 0,
	gestaltQuickTimeVersion = 'qtim',
	gestaltProcClkSpeed = 'pclk',
	gestaltPhysicalRAMSize = 'ram '
};
Boolean BitTst(const void* bytePtr, long bitNumber);
OSErr RegisterAppearanceClient(void);

typedef union NumVersionVariant
{
	NumVersion	parts;
	UInt32		whole;
} NumVersionVariant;

// --- Display Manager -------------------------------------------------------

#define kInvalidDisplayID		0xFFFFFFFFUL

// --- Open Transport XTI options (networking, P7) ---------------------------

enum {
	XTI_GENERIC = 0xFFFE,
	XTI_SNDLOWAT = 0x1003,
	XTI_RCVLOWAT = 0x1004
};

// --- Alert and RAVE draw context -------------------------------------------

enum {
	kAlertCautionAlert = 2
};

// RAVE only exposes the version field (drawContext->version in main.cp); full layout comes with P3.
struct TQADrawContext
{
	unsigned long	version;
};
