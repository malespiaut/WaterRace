// P0 stubs for the Infinity Engine: Sound Manager deferred tasks and output info,
// the QuickTime graphics importer (editor-only path), and Universal Interfaces names
// that Pomme lacks. Declarations only; behaviour comes later (TODOFATAL).
#pragma once

// Universal Interfaces: UniversalProcPtr is ProcPtr. RoutineDescriptorPtr and
// kUnresolvedCFragSymbolAddress are used for CFM "is this routine present" checks.
typedef ProcPtr UniversalProcPtr;
typedef struct RoutineDescriptor* RoutineDescriptorPtr;
#define kUnresolvedCFragSymbolAddress 0

// Sound Manager: callback UPP and output-device selectors.
SndCallbackUPP NewSndCallBackUPP(SndCallBackProcPtr userRoutine);
#define siSampleRate		'srat'
#define siHardwareFormat	'hwfm'
#define rate44khz			0xAC440000UL
#define rate22050hz			0x56220000UL
#define rate22khz			0x56EE8BA3UL
OSErr SetSoundOutputInfo(void* outputDevice, OSType selector, void* infoPtr);
OSErr GetSoundOutputInfo(void* outputDevice, OSType selector, void* infoPtr);

// Deferred Task Manager (used by the audio refill task).
typedef void (*DeferredTaskProcPtr)(long dtParam);
typedef DeferredTaskProcPtr DeferredTaskUPP;
typedef struct DeferredTask {
	struct DeferredTask*	qLink;
	short					qType;
	short					dtFlags;
	DeferredTaskUPP			dtAddr;
	long					dtParam;
	long					dtReserved;
} DeferredTask;
typedef DeferredTask* DeferredTaskPtr;
#define dtQType				8
DeferredTaskUPP NewDeferredTaskProc(DeferredTaskProcPtr userRoutine);
OSErr DTInstall(DeferredTaskPtr task);

// Component Manager and QuickTime graphics importer (editor-only path in Texture Utils.cp).
typedef struct ComponentInstanceRecord* ComponentInstance;
typedef ComponentInstance GraphicsImportComponent;
OSErr CloseComponent(ComponentInstance ci);
OSErr GetGraphicsImporterForFile(const FSSpec* file, GraphicsImportComponent* importer);
OSErr GraphicsImportGetAsPicture(GraphicsImportComponent importer, PicHandle* picture);

// Clock.h only defines this inline under __OPENTRANSPORT__; the Mac build reads it from the OS.
unsigned long GetCurrentAbsoluteTime(long timeUnit);
