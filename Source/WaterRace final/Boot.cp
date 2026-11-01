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

// Linux boot path (PORTING.md sec. 6.1, P1). Replaces the Mac boot in main.cp for now.
// The working directory is the game root: the folder that holds "WaterRace Data/" and the app's
// resource fork. Pomme resolves every FSSpec against it (HostVolume directory 0).

#include				<cstdio>
#include				<cstring>

#include				"PommeInit.h"
#include				"WaterRace.h"

//CONSTANTES:

#define					kAppResourceFile			"WaterRace 1.0.1 [English]"
#define					kDataFolderName				"WaterRace Data"
#define					kCheckStringListID			200

//VARIABLES:

extern short			mainResFileID;	// defined in main.cp

//ROUTINES:

// Opens the app's resource fork and makes it current. Returns noErr or fnfErr.
OSErr Boot_OpenAppResources()
{
	mainResFileID = OpenResFile(kAppResourceFile);
	if(mainResFileID < 0)
	return fnfErr;

	UseResFile(mainResFileID);
	return noErr;
}

// shortcut: searches every open resource file, not only the current one; upgrade if a second
// resource file is ever opened at the same time.
Handle Get1Resource(ResType theType, SInt16 theID)
{
	return GetResource(theType, theID);
}

// P1 check: the data-folder name comes out of STR# 129 through Pomme, and a resource read
// through Get1Resource returns data. Returns true when both hold.
Boolean Boot_CheckResources()
{
	Str255			name;
	Handle			list;

	GetIndString(name, folderFileNameResID, 1);
	if(strcmp((char*) name, kDataFolderName) != 0)
	return false;

	list = Get1Resource('STR#', kCheckStringListID);
	if(list == nil || GetHandleSize(list) == 0)
	return false;

	return true;
}

// Boot the game on Linux. Returns 0 when the P1 checks pass.
int Boot_Startup()
{
	Pomme::Init();

	if(Boot_OpenAppResources() != noErr) {
		fprintf(stderr, "Boot: cannot open resource file \"%s\" in the game root\n", kAppResourceFile);
		return 1;
	}

	if(!Boot_CheckResources()) {
		fprintf(stderr, "Boot: resource check failed (STR# %d or 'STR#' %d)\n", folderFileNameResID, kCheckStringListID);
		return 1;
	}

	fprintf(stderr, "Boot: resource check passed\n");
	return 0;
}
