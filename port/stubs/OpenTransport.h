#pragma once
// P0 stub: Open Transport / XTI subset used by the network code. Declarations only; real
// sockets come in P7 (PORTING.md §7). Names and signatures follow the game's call sites.
#include "Pomme.h"

typedef SInt32 OTResult;
typedef UInt32 OTFlags;
typedef UInt32 OTEventCode;
typedef UInt32 OTXTILevel;
typedef UInt32 OTXTIName;
typedef UInt32 InetHost;
typedef SInt32 EndpointRef;
typedef void* OTConfigurationRef;
typedef struct OTTimeStamp { UInt32 hi; UInt32 lo; } OTTimeStamp;
typedef pascal void (*OTNotifyProcPtr)(void* contextPtr, OTEventCode code, OTResult result, void* cookie);

typedef struct TNetbuf {
	UInt32				maxlen;
	UInt32				len;
	UInt8*				buf;
} TNetbuf;

typedef struct TOption {
	UInt32				len;
	OTXTILevel			level;
	OTXTIName			name;
	UInt32				status;
	UInt32				value[1];
} TOption;

typedef struct TOptMgmt {
	TNetbuf				opt;
	UInt32				flags;
} TOptMgmt;

typedef struct TEndpointInfo {
	UInt32				options;
} TEndpointInfo;

typedef struct TBind {
	TNetbuf				addr;
	UInt32				qlen;
} TBind;

typedef struct TCall {
	TNetbuf				addr;
	TNetbuf				opt;
	TNetbuf				udata;
	SInt32				sequence;
} TCall;

typedef struct TDiscon {
	TNetbuf				udata;
	SInt32				reason;
	SInt32				sequence;
} TDiscon;

typedef struct InetAddress {
	UInt16				fAddressType;
	UInt16				fPort;
	InetHost			fHost;
	UInt8				fUnused[8];
} InetAddress;

typedef struct InetInterfaceInfo {
	InetHost			fAddress;
} InetInterfaceInfo;

enum {
	kOTInvalidEndpointRef = -1,
	kOTLookErr,
	kOTNoDataErr,
	kOTNotFoundErr,
	kOTProviderWillClose,
	kOTProviderIsClosed,
	kOTConnectionRefusedError,
	kOTFourByteOptionSize,
	kOTAnyInetAddress,
	kDefaultInetInterface
};
#define kTCPName	"tcp"

// XTI option and event codes
enum {
	T_CURRENT = 1,
	T_SUCCESS,
	T_READONLY,
	T_DATA = 0x0100,	// was 4, which duplicated kOTProviderWillClose in a switch; value unchecked
	T_DISCONNECT
};

// Configuration, endpoints and modes
OTConfigurationRef	OTCreateConfiguration(const char* name);
OTConfigurationRef	OTCloneConfiguration(OTConfigurationRef config);
OTResult			OTDestroyConfiguration(OTConfigurationRef config);
EndpointRef			OTOpenEndpoint(OTConfigurationRef config, OTFlags oflag, TEndpointInfo* info, OTResult* err);
OTResult			OTCloseProvider(EndpointRef ep);
OTResult			OTUnbind(EndpointRef ep);
OTResult			OTBind(EndpointRef ep, TBind* reqAddr, TBind* retAddr);
OTResult			OTSetSynchronous(EndpointRef ep);
OTResult			OTSetAsynchronous(EndpointRef ep);
OTResult			OTSetBlocking(EndpointRef ep);
OTResult			OTSetNonBlocking(EndpointRef ep);
OTResult			OTOptionManagement(EndpointRef ep, TOptMgmt* req, TOptMgmt* ret);
OTResult			OTInstallNotifier(EndpointRef ep, OTNotifyProcPtr proc, void* contextPtr);
OTResult			OTRemoveNotifier(EndpointRef ep);
void				OTEnterNotifier(EndpointRef ep);
void				OTLeaveNotifier(EndpointRef ep);
OTResult			OTLook(EndpointRef ep);

// Connection
OTResult			OTConnect(EndpointRef ep, TCall* sndCall, TCall* rcvCall);
OTResult			OTListen(EndpointRef ep, TCall* call);
OTResult			OTAccept(EndpointRef ep, EndpointRef resEP, TCall* call);
OTResult			OTSndDisconnect(EndpointRef ep, TCall* call);
OTResult			OTRcvDisconnect(EndpointRef ep, TDiscon* discon);

// Data
SInt32				OTSnd(EndpointRef ep, void* buf, UInt32 nbytes, OTFlags flags);
SInt32				OTRcv(EndpointRef ep, void* buf, UInt32 nbytes, OTFlags* flags);

// Time
OTResult			OTGetTimeStamp(OTTimeStamp* stamp);
UInt32				OTElapsedMilliseconds(OTTimeStamp* stamp);

// Internet addresses
void				OTInitInetAddress(InetAddress* addr, UInt16 port, InetHost host);
OTResult			OTInetStringToHost(Ptr str, InetHost* host);
OTResult			OTInetHostToString(InetHost host, char* str);
OTResult			OTInetGetInterfaceInfo(InetInterfaceInfo* info, UInt32 interfaceIndex);

// Remaining XTI levels, options and states used by the network code
enum {
	INET_TCP = 1,
	T_NEGOTIATE,
	T_YES,
	T_NO,
	T_LISTEN,
	T_PASSCON,
	TCP_NODELAY,
	TCP_MAXSEG,
	TCP_CONN_ABORT_THRESHOLD
};

OSErr				InitOpenTransport(void);
void				CloseOpenTransport(void);
