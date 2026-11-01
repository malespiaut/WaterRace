#pragma once
// P0 stub: Navigation Services subset used by "Navigation Services.cp" only.
// Declarations only; the feature is replaced by in-game screens in P6 (PORTING.md §6.2).
#include "Pomme.h"

typedef long NavEventCallbackMessage;
enum { kNavCBEvent = 1 };

typedef void* NavCallBackUserData;

typedef struct NavEventDataParms {
	EventRecord*		event;
	void*				param;
} NavEventDataParms;

typedef struct NavCBRec {
	short				version;
	struct {
		NavEventDataParms	eventDataParms;
	} eventData;
	Rect				previewRect;
} NavCBRec, *NavCBRecPtr;

typedef pascal void (*NavEventProcPtr)(NavEventCallbackMessage callBackSelector, NavCBRecPtr callBackParms, NavCallBackUserData callBackUD);
typedef NavEventProcPtr NavEventUPP;
typedef pascal Boolean (*NavPreviewProcPtr)(NavCBRecPtr callBackParms, NavCallBackUserData callBackUD);
typedef NavPreviewProcPtr NavPreviewUPP;
NavEventUPP		NewNavEventProc(NavEventProcPtr userRoutine);
NavPreviewUPP	NewNavPreviewProc(NavPreviewProcPtr userRoutine);

enum {
	kNavDontAddTranslateItems = 1 << 0,
	kNavNoTypePopup = 1 << 1,
	kNavAllowPreviews = 1 << 2
};

typedef struct NavDialogOptions {
	unsigned long		dialogOptionFlags;
	Str255				message;
	Str63				clientName;
	Str255				savedFileName;
} NavDialogOptions;

typedef struct NavReplyRecord {
	Boolean				validRecord;
	Boolean				replacing;
	ScriptCode			keyScript;
	AEDesc				selection;
} NavReplyRecord;

typedef struct NavTypeList {
	OSType				componentSignature;
	long				reserved;
	short				osTypeCount;
	OSType				osType[1];
} NavTypeList, **NavTypeListHandle;

OSErr	NavGetDefaultDialogOptions(NavDialogOptions* dialogOptions);
OSErr	NavGetFile(AEDesc* defaultLocation, NavReplyRecord* reply, NavDialogOptions* dialogOptions, NavEventUPP eventProc, NavPreviewUPP previewProc, void* filterProc, NavTypeListHandle typeList, NavCallBackUserData callBackUD);
OSErr	NavPutFile(AEDesc* defaultLocation, NavReplyRecord* reply, NavDialogOptions* dialogOptions, NavEventUPP eventProc, OSType fileType, OSType fileCreator, NavCallBackUserData callBackUD);
OSErr	NavDisposeReply(NavReplyRecord* reply);
