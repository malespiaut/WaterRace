/*****************************************************************************
 * Copyright (c) 1998-2001, French Touch, SARL
 * http://www.french-touch.net
 * info@french-touch.net
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/


#include				<Appearance.h>
#include				<QD3DAcceleration.h>
#include				<InputSprocket.h>
#include				"PommeGraphics.h"

#include				"WaterRace.h"
#include				"Strings.h"
#include				"Data Files.h"
#include				"Interface.h"
#include				"Preferences.h"
#include				"ISpRoutines.h"
#include				"Network Engine.h"

#include				"Infinity Rendering.h"
#include				"Infinity Player.h"
#include				"Infinity Audio Engine.h"
#include				"Textures.h"

#include				"Keys.h"
#include				"Dialog Utils.h"

#if __USE_AUDIO_CD_TRACKS__
#include				"Audio CD.h"
#elif __USE_AUDIO_FILES__
#include				"QDesign Player.h"
#endif

#if	__CD_PROTECTION__
#include				"CD Protection.h"
#endif

//CONSTANTES:

#if __PATCH_PREFERENCES__
#if __PATCH_PREFERENCES__ == 1
#define	__render_size_h__	1024
#define	__render_size_v__	768
#define	__render_depth__	32
#define	__low_res__			0
#define	__mipmap__			0
#elif __PATCH_PREFERENCES__ == 2
#define	__render_size_h__	640
#define	__render_size_v__	480
#define	__render_depth__	32
#define	__low_res__			0
#define	__mipmap__			0
#elif __PATCH_PREFERENCES__ == 3
#define	__render_size_h__	512
#define	__render_size_v__	384
#define	__render_depth__	16
#define	__low_res__			0
#define	__mipmap__			0
#else
#define	__render_size_h__	600
#define	__render_size_v__	400
#define	__render_depth__	16
#define	__low_res__			1
#define	__mipmap__			0
#endif
#endif

//MACROS:

#if __SECURE_CRASH__
#define ExitToShell() { properlyQuit = true; ExitToShell(); }
#endif

//VARIABLES:

Point					screenOffset,
						screenResolution,
						mainWinOffset;
GDeviceInfoPtr			screenInfo = nil;
GDHandle				mainGDHandle;
VDSwitchInfoRec			oldScreenSwitchInfo;
Handle					theDisplayState = nil;
#if __SCREEN_FADES__
GammaRef				mainGamma = nil;
#endif
WindowPtr				mainWin,
						backgroundWin;
GWorldPtr				bufferGWorld,
						bufferGWorld2;

Boolean					redefineScreens;
#if !__DEMO_MODE__
Boolean					networkAvailable;
#endif
short					mainResFileID;
#if __USE_AUDIO_CD_TRACKS__
short					audioCD;
#endif
#if __CD_PROTECTION__ && __NETWORK_ONLY_IF_CD_FAILURE__
Boolean					networkOnly = false;
#endif

#if __SECURE_CRASH__
static Boolean			properlyQuit = false,
						screenInited = false,
						fontAliased = false;
#endif

//ROUTINES:

//Linux port: Boot_InitGL (Boot.cp) opened the window; the interface draws into Pomme's 640x480 screen port,
//which the compositor presents. No display picker, resolution switch, menu bar or background window (PORTING.md 6.1, 6.2).
static OSErr Screen_Init()
{
	OSErr				theError;
	Rect				winBounds = {0,0,kInterface_Height,kInterface_Width};
	PixMapHandle		pictPix = NULL;
	CGrafPtr			savePort = NULL;
	GDHandle			saveDevice = NULL;
	
	SpinningCursor_Stop();
	HideCursor();
	
#if __SCREEN_FADES__
	//Init fading
	StartFading(&mainGamma);
	
	//Fade in...
	Fade1ToBlack(mainGDHandle, 2 * kGammaSpeed, quadraticFade, nil, 0);
#endif
	
#if __SECURE_CRASH__
	screenInited = true;
#endif

	//Create GWorlds
	GetGWorld(&savePort, &saveDevice);
	theError = NewGWorld(&bufferGWorld, kInterface_Depth, &winBounds, NULL, NULL, NULL);
	if(theError) {
		Wrapper_Error_Display(133, theError, nil, false);
		return theError;
	}
	pictPix = GetGWorldPixMap(bufferGWorld);
	LockPixels(pictPix);
	SetGWorld(bufferGWorld, NULL);
	BackColor(whiteColor);
	ForeColor(blackColor);
	
	theError = NewGWorld(&bufferGWorld2, kInterface_Depth, &winBounds, NULL, NULL, NULL);
	if(theError) {
		Wrapper_Error_Display(133, theError, nil, false);
		return theError;
	}
	pictPix = GetGWorldPixMap(bufferGWorld2);
	LockPixels(pictPix);
	SetGWorld(bufferGWorld2, NULL);
	BackColor(whiteColor);
	ForeColor(blackColor);
	SetGWorld(savePort, saveDevice);
	
	//Set up interface window
	mainWin = (WindowPtr) Pomme::Graphics::GetScreenPort();
	mainWinOffset.h = 0;
	mainWinOffset.v = 0;
	SetPort(mainWin);
	BackColor(whiteColor);
	ForeColor(blackColor);
	
	return noErr;
}

static OSErr Screen_Quit()
{
	//Kill GWorlds
	UnlockPixels(GetGWorldPixMap(bufferGWorld));
	DisposeGWorld(bufferGWorld);
	UnlockPixels(GetGWorldPixMap(bufferGWorld2));
	DisposeGWorld(bufferGWorld2);
	
#if __SCREEN_FADES__
	//Fade out...
	Fade1ToGamma(mainGDHandle, mainGamma, kGammaSpeed, quadraticFade, nil, 0);
	
	//Clean up
	StopFading(mainGamma, true);
#endif
	
	return noErr;
}

static void Boot_Sequence()
{
	OSErr				theError;
	
	//Linux port: Boot_Startup (Boot.cp) replaces Init_Toolbox and the Appearance, RAVE, QuickTime,
	//Sound Manager and InputSprocket checks; the CD protection is gone (PORTING.md 6.1)
	fprintf(stderr, "Boot: Boot_Sequence\n");
	redefineScreens = false;
	
#if __USE_AUDIO_CD_TRACKS__
	//Init audio CD driver
	audioCD = 0;
#if	__CD_PROTECTION__
	theError = AudioCD_Init_Manually(driverRefNum);
#else
	theError = AudioCD_Init_UseAppleDriver();
#endif
	if(theError)
	Wrapper_Error_Display(174, theError, nil, false);
	else {
#if	__CD_PROTECTION__ && __NETWORK_ONLY_IF_CD_FAILURE__
		if(AudioCD_IsCDInserted()) {
			if(networkOnly)
			audioCD = -1;
			else
			audioCD = 1;
		}
#else
		if(AudioCD_IsCDInserted())
		audioCD = 1;
#endif
		else
		Wrapper_Error_Display(175, kError_AudioCDNotFound, nil, false);
	}
#endif
	
	//Read preferences
	fprintf(stderr, "Boot: Preference_Read\n");
	if(Preference_Read())
	ExitToShell();
#if __PATCH_PREFERENCES__
	thePrefs.renderWidth = __render_size_h__;
	thePrefs.renderHeight = __render_size_v__;
	if(__render_depth__ == 32)
	thePrefs.engineFlags |= kPref_EngineFlag32BitRendering;
	else
	thePrefs.engineFlags &= ~kPref_EngineFlag32BitRendering;
	if(__low_res__)
	thePrefs.engineFlags |= kPref_EngineFlagLowResolution;
	else
	thePrefs.engineFlags &= ~kPref_EngineFlagLowResolution;
	if(__mipmap__) {
		thePrefs.textureFlags &= ~kTextureFlag_MipMap;
		thePrefs.textureFlags |= kTextureFlag_ForceMipMap;
	}
	else {
		thePrefs.textureFlags |= kTextureFlag_MipMap;
		thePrefs.textureFlags &= ~kTextureFlag_ForceMipMap;
	}
	thePrefs.gameScreen.csMode = 0;
#endif

#if __MOVIE__
	thePrefs.renderWidth = 640;
	thePrefs.renderHeight = 480;
	thePrefs.engineFlags = kPref_EngineFlagFiltering + kPref_EngineFlagVideoFX + kPref_EngineFlag32BitRendering 
		+ kPref_EngineFlag32BitZBuffer + kPref_EngineFlagFurtherClipping + kPref_EngineFlagTriFiltering;
	thePrefs.textureFlags = kTextureFlag_MipMap;
	thePrefs.gameScreen.csMode = 0;
#endif

	//Start spinning cursor
	SpinningCursor_Init();
	SpinningCursor_Start();
	
	//Find data folders
	fprintf(stderr, "Boot: Resolve_DataFolders\n");
	theError = Resolve_DataFolders();
	if(theError) {
		SpinningCursor_Quit();
		properlyQuit = true;
		Wrapper_Error_Display(134, theError, nil, true);
	}
	
	//Scan for ships & terrains
	fprintf(stderr, "Boot: Preload_DataFolders\n");
	theError = Preload_DataFolders();
	//shortcut: not fatal until every MetaFile read site is swapped (P4); make it fatal again then
	if(theError)
	Wrapper_Error_Display(135, theError, nil, false);
	
	//Open data files
	fprintf(stderr, "Boot: Open_DataFiles\n");
	if(Open_DataFiles()) {
		SpinningCursor_Quit();
		ExitToShell();
	}
	
#if !__DEMO_MODE__
	//Init network - Linux port: no sockets backend before P7
	networkAvailable = false;
#endif
	
	//Init ISp - Linux port: SDL input replaces InputSprocket in P5 (PORTING.md 6.4)
	UseResFile(mainResFileID);
	
	//One window: no screen to pick, no driver to check
	mainGDHandle = nil;
	
	//Set screen... - this hides the cursor and stops spinning
	fprintf(stderr, "Boot: Screen_Init\n");
	if(Screen_Init() != noErr) {
		SpinningCursor_Quit();
		ExitToShell();
	}
	
	//Init Text Engine
	fprintf(stderr, "Boot: TextEngine_Init\n");
	theError = TextEngine_Init(32, kInterface_Width);
	if(theError) {
		SpinningCursor_Quit();
		Wrapper_Error_Display(138, theError, nil, true);
	}
	
	//Create 3D rendering buffers & Init audio engine - Linux port: no audio engine before Pomme's mixer in P5 (PORTING.md 6.6)
	fprintf(stderr, "Boot: InfinityPlayer_Init\n");
	theError = InfinityPlayer_Init(false);
	if(theError) {
		SpinningCursor_Quit();
		Wrapper_Error_Display(139, theError, nil, true);
	}
	InfinityAudio_SetMatrix(&worldCamera.camera);
	InfinityAudio_Volume(thePrefs.soundFXVolume);
	
	//Load interface sfx - Linux port: sound comes with Pomme's mixer in P5 (PORTING.md 6.6)
	UseResFile(mainResFileID);
	fprintf(stderr, "Boot: Boot_Sequence done\n");
}

static void Shut_Down()
{
	//Fade in...
#if __SCREEN_FADES__
#if __USE_AUDIO_CD_TRACKS__
	Fade1ToBlack(mainGDHandle, 2 * kGammaSpeed, quadraticFade, nil, 0);
	AudioCD_SetVolume(0, 0);
#elif __USE_AUDIO_FILES__
	Fade1ToBlack(mainGDHandle, 2 * kGammaSpeed, quadraticFade, QDChannel, 0);
#else
	Fade1ToBlack(mainGDHandle, 2 * kGammaSpeed, quadraticFade, nil, 0);
#endif
#endif
	
	//Do clean up
	SpinningCursor_Quit();
	TextEngine_Quit();
	Sound_QuitSFx();
	InfinityPlayer_Quit();
#if !__DEMO_MODE__
	if(networkAvailable)
	Network_Quit();
#endif
	InputSprocket_Quit();
	Screen_Quit();
#if __USE_AUDIO_CD_TRACKS__
	if(audioCD)
	AudioCD_Stop();
#elif __USE_AUDIO_FILES__
	QDPlayer_Stop();
#endif
	InitCursor();
	
	Close_DataFiles();
#if !__PATCH_PREFERENCES__ && !__MOVIE__
	Preference_Write();
#endif
	
	Preload_ReleaseMemory();

	//Quit!
	FlushEvents(everyEvent, 0);
	ExitToShell();
}

#if __USE_INI__
#include	"Numeric Utils.h"

#define	kInitFileName				"\015WaterRace.ini"
#define	kCommand_GameMode			"\010GameMode"
#define	kCommand_TerrainID			"\011TerrainID"
#define	kCommand_ShipID				"\006ShipID"
#define	kCommand_PilotID			"\007PilotID"
#define	kCommand_PlayerName			"\012PlayerName"
#define	kCommand_TerrainDifficulty	"\021TerrainDifficulty"
#define	kEndOfLineChar				'\r'
#define	kOrderChar					'='
#define	kOrderIsDigitChar			'#'

static Ptr InitFile_Load(long* bufferSize)
{
	Ptr			buffer = nil;
	FSSpec		initFile;
	short		destFileID;
	long		bytes;
	
	BlockMove(kInitFileName, initFile.name, sizeof(Str31));
	HGetVol(nil, &initFile.vRefNum, &initFile.parID);
	
	if(FSpOpenDF(&initFile, fsRdPerm, &destFileID))
	return nil;
	GetEOF(destFileID, &bytes);
	
	buffer = NewPtrClear(bytes);
	if(buffer == nil)
	return nil;
	
	if(FSRead(destFileID, &bytes, buffer))
	return nil;
	
	FSClose(destFileID);
	
	*bufferSize = bytes;
	
	return buffer;
}

static long InitFile_ReadCommand(Ptr buffer, long size, Str31 command, void* result)
{
	unsigned char*		theChar = (unsigned char*) buffer;
	long				matchCount = 0,
						position = 1;
	
	//Scan for command
	while(size) {
		if(*theChar == command[position]) {
			++matchCount;
			++position;
			if((matchCount == command[0]) && (*(theChar + 1) == kOrderChar)) {
				theChar += 2;
				if(*theChar == kOrderIsDigitChar) {
					++theChar;
					*((long*) result) = Text2Integer(theChar, size);
					return sizeof(long);
				}
				else {
					matchCount = 0;
					while(*theChar != kEndOfLineChar) {
						*((unsigned char*) result) = *theChar;
						++((unsigned char*) result);
						++matchCount;
						++theChar;
					}
					return matchCount;
				}
			}
		}
		else {
			matchCount = 0;
			position = 1;
		}
		
		++theChar;
		--size;
	}
	
	return 0;
}

#endif

#if __RECORD_RACE__ && __REPLAY_AWARE__
//Linux port: no Apple Events, so no replay file opened from the Finder; a replay argument can come with P6 (PORTING.md 6.2)
static Boolean Check_ForReplay()
{
	return false;
}
#endif

#if __MOVIE__

FSSpec			destFolder;

static Boolean Request_Replay()
{
	StandardFileReply		theReply,
							theReply2;
	SFTypeList				list = {kReplayFileType};
	
	StandardGetFile(nil, 1, list, &theReply);
	if(!theReply.sfGood)
	return false;
	
	StandardPutFile("\044Select where to save the screenshots", "\004Temp", &theReply2);
	if(!theReply.sfGood)
	return false;
	destFolder = theReply2.sfFile;
	
	HideCursor();
	Replay_SetupGame(&theReply.sfFile);
	Game_New();
	
	return true;
}
#endif

int Boot_Startup(); // Boot.cp

int Game_Main() // the Mac main(); main() in Boot.cp calls it and catches ExitToShell
{
	OSErr			theError;

	//Pomme, resources and the GL window (Boot.cp)
	if(Boot_Startup())
	return 1;

	//Boot...
	Boot_Sequence();

#if __MOVIE__
	Request_Replay();
	Shut_Down();
#endif

#if __RECORD_RACE__ && __REPLAY_AWARE__
	if(!Check_ForReplay()) {
#endif

#if __NETWORK_CLIENT_MODE__
	if(networkAvailable) {
		theError = NetworkMenu_Display();
		if(theError) {
			if(theError != kError_UserCancel)
			Wrapper_Error_Display(140, theError, nil, false);
		}
		else
		Game_New();
	}
	else
	Wrapper_Error_Display(141, kNetError_NoNetworkAvailable, nil, false);
#else

#if __CD_PROTECTION__ && __NETWORK_ONLY_IF_CD_FAILURE__
	if(networkOnly) {
		if(networkAvailable) {
			theError = NetworkMenu_Display();
			if(theError) {
				if(theError != kError_UserCancel)
				Wrapper_Error_Display(140, theError, nil, false);
			}
			else
			Game_New();
		}
		else
		Wrapper_Error_Display(141, kNetError_NoNetworkAvailable, nil, false);
	}
	else {
#endif

#if __USE_INI__
	Ptr					initBuffer;
	long				initBufferSize;
	long				gameMode = 0;
	OSType				terrainID = 'Jamc',
						shipID = 'SpdB',
						pilotID = 'WgLi';
	Str31				playerName;
	long				difficulty = kDifficulty_Easy;
	
	initBuffer = InitFile_Load(&initBufferSize);
	if(initBuffer != nil) {
		InitFile_ReadCommand(initBuffer, initBufferSize, kCommand_GameMode, &gameMode);
		InitFile_ReadCommand(initBuffer, initBufferSize, kCommand_TerrainID, &terrainID);
		InitFile_ReadCommand(initBuffer, initBufferSize, kCommand_TerrainDifficulty, &difficulty);
		InitFile_ReadCommand(initBuffer, initBufferSize, kCommand_ShipID, &shipID);
		InitFile_ReadCommand(initBuffer, initBufferSize, kCommand_PilotID, &pilotID);
		playerName[0] = InitFile_ReadCommand(initBuffer, initBufferSize, kCommand_PlayerName, &playerName[1]);
		
		if(gameMode == 1) {
			if(networkAvailable) {
				theError = NetworkMenu_Display();
				if(theError) {
					if(theError != kError_UserCancel)
					Wrapper_Error_Display(140, theError, nil, false);
				}
				else
				Game_New();
			}
			else
			Wrapper_Error_Display(141, kNetError_NoNetworkAvailable, nil, false);
		}
		else {
			Pilot		tempPilot;
			
			Pilot_New(&tempPilot, playerName, pilotID, kPilotType_FreePlay, difficulty);
			theError = Game_SetupLocal(kGameMode_Local, &tempPilot, terrainID, shipID);
			if(theError)
			Wrapper_Error_Display(142, theError, nil, false);
			else
			Game_New();
		}
		
		DisposePtr(initBuffer);
	}
	else {
#endif
#if __EXPO_MODE__
	{
		Pilot		thePilot;
		
		Pilot_New(&thePilot, "\006Player", kExpo_PilotSet, kExpo_PilotMode, kExpo_PilotDifficulty);
		Interface_RunLocalGame(&thePilot);
	}
#else

#if __PLAY_DEMO__
	//Play intro
	fprintf(stderr, "Boot: Demo_Play\n");
	//shortcut: WATERRACE_BOOT_ONLY=1 quits here for port/check-boot.py; keep it as a boot check once Demo_Play runs (#38, #44)
	if(getenv("WATERRACE_BOOT_ONLY") != nil)
	ExitToShell();
	theError = Demo_Play();
	fprintf(stderr, "Boot: Demo_Play done (%d)\n", theError);
	if(theError) {
		Shut_Down();
		ExitToShell();
	}
	//shortcut: WATERRACE_INTRO_ONLY=1 quits after the intro for port/check-intro.py, until the interface runs (#48)
	if(getenv("WATERRACE_INTRO_ONLY") != nil)
	ExitToShell();
#else
#if __USE_AUDIO_CD_TRACKS__ || __USE_AUDIO_FILES__
	GetIndString(musicsFolder.name, MusicFileNameResID, interfaceMusic);
#if __USE_AUDIO_CD_TRACKS__
	theError = AudioCD_PlayAudioTrack_Wrapper(&musicsFolder);
#elif __USE_AUDIO_FILES__
	theError = QDPlayer_Start(&musicsFolder);
#endif //__USE_AUDIO_CD_TRACKS__
	if(theError)
	Wrapper_Error_Display(143, theError, nil, false);
			
#if __USE_AUDIO_CD_TRACKS__
	AudioCD_SetVolume(thePrefs.musicVolume, thePrefs.musicVolume);
#elif __USE_AUDIO_FILES__
	QDPlayer_Volume(thePrefs.musicVolume);
#endif //__USE_AUDIO_CD_TRACKS__
#endif //__USE_AUDIO_CD_TRACKS__ || __USE_AUDIO_FILES__
#endif //__PLAY_DEMO__

	//Interface
	Interface_Display();

#endif //__EXPO_MODE__

#if __SHOW_PUB__
	//Show pubs
	theError = PubWaterRace_Display();
	if(theError)
	Wrapper_Error_Display(144, theError, nil, false);
	theError = PubFusion_Display();
	if(theError)
	Wrapper_Error_Display(144, theError, nil, false);
#endif

#if __USE_INI__
	}
#endif
#if __CD_PROTECTION__ && __NETWORK_ONLY_IF_CD_FAILURE__
	}
#endif
#endif
#if __RECORD_RACE__ && __REPLAY_AWARE__
	}
#endif

	//Terminate...
	Shut_Down();
	return 0;
}

#if __SECURE_CRASH__
extern "C" void terminate();

void terminate()
{
	//Restore screen res, gamma & show cursor
	if(!properlyQuit) {
		SpinningCursor_Quit();
		TextEngine_Quit();
		Sound_QuitSFx();
		InfinityPlayer_Quit();
#if !__DEMO_MODE__
		if(networkAvailable) {
			Server_ShutDown();
			Client_Disconnect();
			Network_Quit();
		}
#endif
		InputSprocket_Quit();
		if(screenInited)
		Screen_Quit();
#if __USE_AUDIO_CD_TRACKS__
		if(audioCD)
		AudioCD_Stop();
#elif __USE_AUDIO_FILES__
		QDPlayer_Stop();
#endif
		InitCursor();
		Close_DataFiles();
		
		Preload_ReleaseMemory();
		if(fontAliased)
		MacOSTextAliasing_Restore();
	}
}

#endif
