#pragma once
// P0 stub family: stub_toolbox. Declarations only, no behaviour.
// Dialog, Control, Menu, Window, Event, Alert and Resource Manager calls the game uses.
// Names the game defines itself (Dialog utils.h, GetWindowPosition, MenuSafeString,
// AppendDialogItemList, GetListBoxListHandle, SetDialogItemText in Network menu.cp) are not declared here.

// --- Event Manager ---------------------------------------------------------

typedef struct EventRecord
{
	SInt16		what;
	UInt32		message;
	UInt32		when;
	Point		where;
	SInt16		modifiers;
} EventRecord;

typedef UInt16 EventModifiers;

Boolean WaitNextEvent(SInt16 eventMask, EventRecord* theEvent, UInt32 sleep, RgnHandle mouseRgn);
Boolean Button(void);
void GetMouse(Point* mouseLoc);
UInt32 GetDblTime(void);
void Delay(long numTicks, unsigned long* finalTicks);

// --- Window Manager --------------------------------------------------------

SInt16 FindWindow(Point thePoint, WindowPtr* theWindow);
void MoveWindow(WindowPtr theWindow, SInt16 hGlobal, SInt16 vGlobal, Boolean front);
void SizeWindow(WindowPtr theWindow, SInt16 w, SInt16 h, Boolean fUpdate);
void ShowWindow(WindowPtr theWindow);
void SelectWindow(WindowPtr theWindow);
void DragWindow(WindowPtr theWindow, Point startPt, const Rect* boundsRect);
void DisposeWindow(WindowPtr theWindow);
void BeginUpdate(WindowPtr theWindow);
void EndUpdate(WindowPtr theWindow);

// --- Menu Manager ----------------------------------------------------------

typedef struct MenuInfo** MenuHandle;

MenuHandle GetMenu(SInt16 resourceID);
void DisableItem(MenuHandle theMenu, SInt16 item);

// --- Control Manager -------------------------------------------------------

// Only the fields the game reads. contrlRfCon is `long` (pointer-sized on LP64)
// because the game stores pointers in it.
typedef struct ControlRecord
{
	Rect		contrlRect;
	long		contrlRfCon;
} ControlRecord;
typedef ControlRecord** ControlHandle;

typedef SInt16 ControlPartCode;

typedef void (*ControlActionProcPtr)(ControlHandle theControl, SInt16 partCode);
typedef ControlActionProcPtr ControlActionUPP;

typedef void (*ControlUserPaneDrawProcPtr)(ControlHandle theControl, SInt16 partCode);
typedef ControlPartCode (*ControlUserPaneHitTestProcPtr)(ControlHandle theControl, Point where);
typedef ControlPartCode (*ControlUserPaneTrackingProcPtr)(ControlHandle theControl, Point startPt, ControlActionUPP actionProc);
typedef ControlUserPaneDrawProcPtr ControlUserPaneDrawUPP;
typedef ControlUserPaneHitTestProcPtr ControlUserPaneHitTestUPP;
typedef ControlUserPaneTrackingProcPtr ControlUserPaneTrackingUPP;

#define kControlUserPaneDrawProcTag			'draw'
#define kControlUserPaneHitTestProcTag		'hit?'
#define kControlUserPaneTrackingProcTag		'trak'
#define kControlScrollBarLiveProc			18
#define kControlListBoxListHandleTag		'lhan'	// value from memory of Mac headers, unverified
enum
{
	kControlNoPart					= 0,
	kControlButtonPart				= 10,
	kControlDownButtonPart			= 21,
	kControlUpButtonPart			= 20,
	kControlPageDownPart			= 23,
	kControlPageUpPart				= 22,
	kControlIndicatorPart			= 129
};

ControlActionUPP NewControlActionProc(ControlActionProcPtr userRoutine);
ControlUserPaneDrawUPP NewControlUserPaneDrawProc(ControlUserPaneDrawProcPtr userRoutine);
ControlUserPaneHitTestUPP NewControlUserPaneHitTestProc(ControlUserPaneHitTestProcPtr userRoutine);
ControlUserPaneTrackingUPP NewControlUserPaneTrackingProc(ControlUserPaneTrackingProcPtr userRoutine);

ControlHandle NewControl(WindowPtr theWindow, const Rect* boundsRect, ConstStr255Param title, Boolean visible,
						 SInt16 value, SInt16 min, SInt16 max, SInt16 procID, long refCon);
void DisposeControl(ControlHandle theControl);
void ShowControl(ControlHandle theControl);
void HideControl(ControlHandle theControl);
void Draw1Control(ControlHandle theControl);

void SetControlValue(ControlHandle theControl, SInt16 newValue);
void SetControlMinimum(ControlHandle theControl, SInt16 newMinimum);
void SetControlMaximum(ControlHandle theControl, SInt16 newMaximum);
SInt16 GetControlValue(ControlHandle theControl);
SInt16 GetControlMinimum(ControlHandle theControl);
SInt16 GetControlMaximum(ControlHandle theControl);
void SetControlAction(ControlHandle theControl, ControlActionUPP actionProc);
void SetControlReference(ControlHandle theControl, long data);
long GetControlReference(ControlHandle theControl);
void SetControlTitle(ControlHandle theControl, ConstStr255Param title);
OSErr SetControlData(ControlHandle theControl, ControlPartCode part, ResType tagName, Size bufferSize, Ptr buffer);
OSErr GetControlData(ControlHandle theControl, ControlPartCode part, ResType tagName, Size bufferSize, Ptr buffer, Size* actualSize);

void HiliteControl(ControlHandle theControl, ControlPartCode hiliteState);
ControlPartCode HandleControlClick(ControlHandle theControl, Point where, EventModifiers modifiers, ControlActionUPP actionProc);
ControlPartCode TrackControl(ControlHandle theControl, Point startPt, ControlActionUPP actionProc);
ControlPartCode FindControl(Point thePoint, WindowPtr theWindow, ControlHandle* theControl);

// --- Dialog Manager --------------------------------------------------------

typedef WindowPtr DialogPtr;
typedef SInt16 DialogItemIndex;
typedef SInt16 DITLMethod;

typedef Boolean (*ModalFilterProcPtr)(DialogPtr theDialog, EventRecord* theEvent, SInt16* itemHit);
typedef ModalFilterProcPtr ModalFilterUPP;
ModalFilterUPP NewModalFilterProc(ModalFilterProcPtr userRoutine);

DialogPtr GetNewDialog(SInt16 dialogID, void* dStorage, WindowPtr behind);
void DisposeDialog(DialogPtr theDialog);
void DrawDialog(DialogPtr theDialog);
void ModalDialog(ModalFilterUPP modalFilter, SInt16* itemHit);
Boolean IsDialogEvent(const EventRecord* theEvent);
Boolean DialogSelect(const EventRecord* theEvent, DialogPtr* theDialog, SInt16* itemHit);
void GetDialogItem(DialogPtr theDialog, SInt16 itemNo, SInt16* itemType, Handle* item, Rect* box);
OSErr GetDialogItemAsControl(DialogPtr theDialog, SInt16 itemNo, ControlHandle* theControl);
void SetDialogItemText(Handle item, ConstStr255Param text);
void GetDialogItemText(Handle item, Str255 text);
void SetDialogDefaultItem(DialogPtr theDialog, SInt16 newItem);
void SelectDialogItemText(DialogPtr theDialog, SInt16 itemNo, SInt16 strtSel, SInt16 endSel);
void ParamText(ConstStr255Param param0, ConstStr255Param param1, ConstStr255Param param2, ConstStr255Param param3);

// --- Alert Manager ---------------------------------------------------------

typedef SInt16 AlertType;
enum
{
	kAlertStopAlert					= 0,
	kAlertStdAlertOKButton			= 1
};
#define kAlertDefaultOKText			((ConstStr255Param) -1)
#define kWindowDefaultPosition		0

typedef struct AlertStdAlertParamRec
{
	Boolean				movable;
	Boolean				helpButton;
	ModalFilterUPP		filterProc;
	StringPtr			defaultText;
	StringPtr			cancelText;
	StringPtr			otherText;
	SInt16				defaultButton;
	SInt16				cancelButton;
	UInt16				position;
} AlertStdAlertParamRec;

OSErr StandardAlert(AlertType alertType, ConstStr255Param error, ConstStr255Param explanation,
					const AlertStdAlertParamRec* alertParam, SInt16* itemHit);

// --- List Manager (only ScreenLib.cp uses it) ------------------------------

typedef struct ListRec** ListHandle;
typedef struct Cell
{
	SInt16		v;
	SInt16		h;
} Cell;

void LSetDrawingMode(Boolean drawIt, ListHandle theList);
void LAddRow(SInt16 howMany, SInt16 where, ListHandle theList);
void LSetCell(const void* dataPtr, SInt16 dataLen, Cell theCell, ListHandle theList);
void LSetSelect(Boolean setIt, Cell theCell, ListHandle theList);
Boolean LGetSelect(Boolean next, Cell* theCell, ListHandle theList);

// --- Resource Manager ------------------------------------------------------

Handle Get1Resource(ResType theType, SInt16 theID);
void GetIndString(Str255 theString, SInt16 strListID, SInt16 index);
OSErr MemError(void);
