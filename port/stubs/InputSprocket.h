#pragma once
// P0 stub: InputSprocket subset used by ISpRoutines.cp, main.cp and Preferences.cp.
// Declarations only; ISpRoutines.cp is rewritten over SDL in P5 (PORTING.md §6.4).
#include "Pomme.h"

typedef unsigned long ISpElementReference;
typedef unsigned long ISpElementListReference;
typedef unsigned long ISpDeviceReference;
typedef unsigned long ISpElementLabel;
typedef unsigned long ISpElementKind;
typedef void (*ISpEventProcPtr)(unsigned long refCon);

typedef struct ISpNeed {
	Str255			name;
	SInt16			iconSuiteResourceId;
	SInt16			playerNum;
	SInt16			group;
	ISpElementKind		theKind;
	ISpElementLabel		theLabel;
	unsigned long		flags;
	unsigned long		reserved1;
	unsigned long		reserved2;
	unsigned long		reserved3;
} ISpNeed;

typedef struct ISpElementEvent {
	ISpElementReference	element;
	unsigned long		refCon;
	unsigned long		data;
} ISpElementEvent;

enum {
	kISpElementKind_Button = 1,
	kISpElementKind_Axis,
	kISpElementKind_DPad
};

enum {
	kISpElementLabel_Btn_Fire = 1,
	kISpElementLabel_Btn_Quit,
	kISpElementLabel_Axis_XAxis,
	kISpElementLabel_Axis_YAxis,
	kISpElementLabel_Axis_Rx,
	kISpElementLabel_Axis_Ry,
	kISpElementLabel_Axis_Rz,
	kISpElementLabel_Pad_POV
};

enum {
	kISpNeedFlag_PolledOnly = 1 << 0,
	kISpNeedFlag_NoMultiConfig = 1 << 1,
	kISpNeedFlag_Utility = 1 << 2,
	kISpNeedFlag_EventsOnly = 1 << 3,
	kISpNeedFlag_NoConfig = 1 << 4
};

enum {
	kISpDeviceClass_Keyboard = 1,
	kISpDeviceClass_Mouse
};

enum {
	kISpKeyboardID_Apple = 1
};

enum {
	kISpAxisMinimum = 0,
	kISpAxisMiddle = 0x8000,
	kISpAxisMaximum = 0xFFFF
};

enum {
	kISpButtonUp = 0,
	kISpButtonDown = 1
};

enum {
	kISpPadIdle = 0,
	kISpPadUp,
	kISpPadUpRight,
	kISpPadRight,
	kISpPadDownRight,
	kISpPadDown,
	kISpPadDownLeft,
	kISpPadLeft,
	kISpPadUpLeft
};

OSStatus	ISpStartup(void);
OSStatus	ISpShutdown(void);
OSStatus	ISpSuspend(void);
OSStatus	ISpResume(void);
OSStatus	ISpStop(void);
OSStatus	ISpConfigure(ISpEventProcPtr eventProc);
NumVersion	ISpGetVersion(void);
OSStatus	ISpInit(unsigned long needCount, ISpNeed* needs, ISpElementReference* elements, OSType creator, OSType version, unsigned long flags, short setListResID, unsigned long reserved);

OSStatus	ISpDevices_ExtractByClass(unsigned long deviceClass, unsigned long maxDevices, UInt32* count, ISpDeviceReference* devices);
OSStatus	ISpDevices_Activate(unsigned long count, ISpDeviceReference* devices);
OSStatus	ISpDevices_Deactivate(unsigned long count, ISpDeviceReference* devices);

OSStatus	ISpElement_NewVirtualFromNeeds(unsigned long needCount, ISpNeed* needs, ISpElementReference* elements, unsigned long flags);
OSStatus	ISpElement_DisposeVirtual(unsigned long count, ISpElementReference* elements);
OSStatus	ISpElement_GetSimpleState(ISpElementReference element, UInt32* data);
OSStatus	ISpElement_Flush(ISpElementReference element);

OSStatus	ISpElementList_New(unsigned long count, ISpElementReference* elements, ISpElementListReference* list, unsigned long flags);
OSStatus	ISpElementList_AddElements(ISpElementListReference list, unsigned long refCon, unsigned long count, ISpElementReference* elements);
OSStatus	ISpElementList_Dispose(ISpElementListReference list);
OSStatus	ISpElementList_GetNextEvent(ISpElementListReference list, unsigned long bufferSize, ISpElementEvent* event, Boolean* wasEvent);
