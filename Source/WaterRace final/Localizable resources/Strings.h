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


#ifndef __WATERRACE_STRINGS__
#define __WATERRACE_STRINGS__

/********************************************************************/
/*						TRANSLATION NOTES							*/
/*	• only strings in uppercase should be translated in uppercase	*/
/*	• translated strings must be - if possible - same size of		*/
/*		shorter	than the original string							*/
/*	• If any, preserve ABSOLUTELY the "\000" at the beginning of the	*/
/*		string														*/
/*	• Some strings need their length computed manually.	If this is	*/
/*		the case, replace kString_xxx_L with the length of the		*/
/*		string ("\000" = 1 character, not 2) */
/********************************************************************/


//CONSTANTES:

/****************************************************************/
/*					ENGLISH VERSION								*/
/****************************************************************/

#if __LANGUAGE__ == kLanguage_English

#define	kString_Drop_OffLimit			"\012OFF LIMIT!"
#define	kString_Drop_UpsideDown			"\014UPSIDE-DOWN!"
#define	kString_Drop_UnderWater			"\014UNDER WATER!"
#define	kString_Drop_Collision			"\012COLLISION!"
#define	kString_Drop_Manual				"\014PLAYER DROP!"

#define	kString_Network_PlayerOffLine	"\017PLAYER OFFLINE!"
#define	kString_Network_BotLocation		"\007Botland"
#define	kString_Network_ServerOffLine	"\017SERVER OFFLINE!"

#if __DISPLAY_CHECK_POINT__
#define	kString_Driver_CheckPoint		"\014CHECK POINT!"
#define	kString_Driver_CheckPoint_L		13
#endif
#if __DISPLAY_MISSED_CHECK_POINT__
#define	kString_Driver_MissedCheckPoint	"\023CHECK POINT MISSED!"
#define	kString_Driver_MissedCheckPoint_L	20
#endif
#define	kString_Driver_BestLapTime		"\017BEST LAP TIME: "
#define	kString_Driver_BestLapTime_L	16
#define	kString_Driver_LapTime			"\012LAP TIME: "
#define	kString_Driver_LapTime_L		11
#define	kString_Driver_TimeExtended		"\020TIME EXTENDED: +"
#define	kString_Driver_TimeExtended_L	17
#define	kString_Driver_FirstPlace		"\014FIRST PLACE!"
#define	kString_Driver_FirstPlace_L		13
#define	kString_Driver_Finished			" HAS FINISHED!"
#define	kString_Driver_Finished_L		14
#if __BOTS_FINISH_RACE__
#define	kString_Driver_LocalFinish		"\022YOU HAVE FINISHED!"
#define	kString_Driver_LocalWon			"\023YOU ARE THE WINNER!"
#endif
#define	kString_Driver_Ready			"READY: x"
#define	kString_Driver_Ready_L			8
#define	kString_Driver_Left				"LEFT: x"
#define	kString_Driver_Left_L			7
#if __DISPLAY_WRONG_DIRECTION__
#define	kString_Driver_WrongDirection	"\020WRONG DIRECTION!"
#define	kString_Driver_WrongDirection_L	17
#endif
#if __AUTOPILOT__
#define	kString_Driver_AutoPilot		"\013AUTO PILOT!"
#define	kString_Driver_Finish			"\025PRESS ESC TO END RACE"
#endif

#define	kString_WhiteScreen_Escape_1	"PRESS ESC TO"
#define	kString_WhiteScreen_Escape_1_L	12
#define	kString_WhiteScreen_Escape_2	"RETURN TO MENU"
#define	kString_WhiteScreen_Escape_2_L	14
#define	kString_WhiteScreen_Replay_1	"PRESS SPACEBAR"
#define	kString_WhiteScreen_Replay_1_L	14
#define	kString_WhiteScreen_Replay_2	"TO REPLAY RACE"
#define	kString_WhiteScreen_Replay_2_L	14
#define	kString_WhiteScreen_RaceTime	"RACE TIME:"
#define	kString_WhiteScreen_RaceTime_L	10
#define	kString_WhiteScreen_Results		"PLAYER RESULTS: "
#define	kString_WhiteScreen_Results_L	16
#define	kString_WhiteScreen_Rank		"RANK:"
#define	kString_WhiteScreen_Rank_L		5
#define	kString_WhiteScreen_Bonus		"BONUS:"
#define	kString_WhiteScreen_Bonus_L		6
#define	kString_WhiteScreen_BestLap		"BEST LAP:"
#define	kString_WhiteScreen_BestLap_L	9
#define	kString_WhiteScreen_TimeLeft	"TIME LEFT:"
#define	kString_WhiteScreen_TimeLeft_L	10
#define	kString_WhiteScreen_MaxSpeed	"TOP SPEED:"
#define	kString_WhiteScreen_MaxSpeed_L	10
#define	kString_WhiteScreen_Distance	"DISTANCE:"
#define	kString_WhiteScreen_Distance_L	9
#define	kString_WhiteScreen_AvSpeed		"AV. SPEED:"
#define	kString_WhiteScreen_AvSpeed_L	10

#define	kString_Interface_ManualResolution	"\100Press the command key during start up to show this dialog again."
#define	kString_Interface_NA			"\003N/A"
#define	kString_Interface_RequiredP		"\020Required points:"
#define	kString_Interface_JokerAccess	"JOKER ACCESS!"
#define	kString_Interface_JokerAccess_L	13
#define	kString_Interface_JokerRedo		"You can use a joker to redo\rthis race."
#define	kString_Interface_JokerRedo_L	38
#define	kString_Interface_AccessDenied	"ACCESS DENIED!"
#define	kString_Interface_AccessDenied_L	14
#define	kString_Interface_JokerNeed		"You need a joker to redo\rthis race."
#define	kString_Interface_JokerNeed_L	35
#define	kString_Interface_RaceLocation	"\015Race location"
#define	kString_Interface_Rank			"\004Rank"
#define	kString_Interface_Score			"\005Score"
#define	kString_Interface_TimeLeft		"\011Time left"
#define	kString_Interface_Level			"\006Level "
#define	kString_Interface_GameOver		"\011Game over"
#define	kString_Interface_Champion		"\010Champion"
#define	kString_Interface_Challenge		"\011Challenge"

#define	kString_Camera_Free				"\013FREE CAMERA"
#define	kString_Camera_Follow			"\015FOLLOW CAMERA"
#define	kString_Camera_Head				"\013HEAD CAMERA"
#define	kString_Camera_Onboard			"\016ONBOARD CAMERA"
#define	kString_Camera_TV				"\014WATERRACE TV"
#define	kString_Camera_Chase			"\014CHASE CAMERA"
#define	kString_Camera_From				"\006 FROM "
#define	kString_Camera_Of				"\004 OF "
#define	kString_Camera_On				"\004 ON "
#define	kString_Camera_Replay			"\006REPLAY"
#define	kString_Camera_Separator		"\003 / "
#define	kString_Camera_Track			"\014TRACK CAMERA"

#define	kString_Preferences_Name		"\007My name"
#define	kString_Preferences_Location	"\013My location"
#define	kString_Preferences_Computer	"\017Computer's name"
#define	kString_Preferences_Welcome		"\023Welcome to my game!"

#define	kString_Misc_ReplayFileName		"\014Race Replay "
#define	kString_Misc_Continue			"\010Continue"
#define	kString_Misc_Quit				"\004Quit"
#define	kString_Misc_ScreenshotFileName	"\016WR Screenshot "

#define	kString_Network_Disconnected	"\040Disconnected from host!\rReason: "
#define	kString_Network_Reason1			"\030Incorrect game version.\r"
#define	kString_Network_Reason2			"\020Wrong password.\r"
#define	kString_Network_Reason3			"\026Server was shut down.\r"
#define	kString_Network_Reason4			"\065Game has already started on server.\rTry again later.\r"
#define	kString_Network_Reason5			"\010Unknow.\r"
#define	kString_Network_Client1			"\032Server info received:\r    "
#define	kString_Network_Client2			"\005\r    "
#define	kString_Network_Client3			"\024\r    Server running "
#define	kString_Network_Client4			"\004 on "
#define	kString_Network_Client5			"\016Terrain name: "
#define	kString_Network_Client6			"\032\r\rSending player info...\r\r"
#define	kString_Network_Client7			"\036Waiting for game to begin...\r\r"
#define	kString_Network_Client8			"\022Game has started!\r"
#define	kString_Network_Server1			"\020Server offline!\r"
#define	kString_Network_Server2			"\023Player has joined: "
#define	kString_Network_Server3			"\006 from "
#define	kString_Network_Server4			"\023 was disconnected!\r"
#define	kString_Network_Connecting1		"\016Connecting to "
#define	kString_Network_Connecting2		"\051... (please wait - 10 seconds max delay)\r"
#define	kString_Network_Connecting3		"\043Could not connect to host!\rReason: "
#define	kString_Network_Connecting4		"\121Server unreachable (wrong IP address, server might be down or too many players).\r"
#define	kString_Network_Connecting5		"\031Connection established.\r\r"
#define	kString_Network_Connecting6		"\033Waiting for server info...\r"
#define	kString_Network_Connecting7		"\007(Error "
#define	kString_Network_Hosting1		"\023Starting server at "
#define	kString_Network_Hosting2		"\037Could not start server! (Error "
#define	kString_Network_Hosting3		"\017Server online.\r"
#define	kString_Network_Hosting4		"\030Waiting for clients...\r\r"
#define	kString_Network_Hosting5		"\035Cannot connect local player!\r"


/****************************************************************/
/*					FRENCH VERSION								*/
/****************************************************************/

#elif __LANGUAGE__ == kLanguage_French

#define	kString_Drop_OffLimit			"\014HORS LIMITE!"
#define	kString_Drop_UpsideDown			"\011RENVERSE!"
#define	kString_Drop_UnderWater			"\013SOUS L'EAU!"
#define	kString_Drop_Collision			"\012COLLISION!"
#define	kString_Drop_Manual				"\017RELANCE JOUEUR!"

#define	kString_Network_PlayerOffLine	"\022JOUEUR DECONNECTE!"
#define	kString_Network_BotLocation		"\007Botland"
#define	kString_Network_ServerOffLine	"\023SERVEUR DECONNECTE!"

#if __DISPLAY_CHECK_POINT__
#define	kString_Driver_CheckPoint		"\014CHECK POINT!"
#define	kString_Driver_CheckPoint_L		13
#endif
#if __DISPLAY_MISSED_CHECK_POINT__
#define	kString_Driver_MissedCheckPoint	"\023CHECK POINT MANQUE!"
#define	kString_Driver_MissedCheckPoint_L	20
#endif
#define	kString_Driver_BestLapTime		"\020MEILLEUR TEMPS: "
#define	kString_Driver_BestLapTime_L	17
#define	kString_Driver_LapTime			"\017TEMPS DU TOUR: "
#define	kString_Driver_LapTime_L		16
#define	kString_Driver_TimeExtended		"\017PROLONGATION: +"
#define	kString_Driver_TimeExtended_L	16
#define	kString_Driver_FirstPlace		"\017PREMIERE PLACE!"
#define	kString_Driver_FirstPlace_L		16
#define	kString_Driver_Finished			" A TERMINE!"
#define	kString_Driver_Finished_L		11
#if __BOTS_FINISH_RACE__
#define	kString_Driver_LocalFinish		"\022VOUS AVEZ TERMINE!"
#define	kString_Driver_LocalWon			"\027VOUS ETES LE VAINQUEUR!"
#endif
#define	kString_Driver_Ready			"PRET: x"
#define	kString_Driver_Ready_L			7
#define	kString_Driver_Left				"RESTANT: x"
#define	kString_Driver_Left_L			10
#if __DISPLAY_WRONG_DIRECTION__
#define	kString_Driver_WrongDirection	"\023MAUVAISE DIRECTION!"
#define	kString_Driver_WrongDirection_L	20
#endif
#if __AUTOPILOT__
#define	kString_Driver_AutoPilot		"\023PILOTE AUTOMATIQUE!"
#define	kString_Driver_Finish			"\035APPUYEZ SUR ESC POUR TERMINER"
#endif

#define	kString_WhiteScreen_Escape_1	"APPUYEZ SUR ESC POUR"
#define	kString_WhiteScreen_Escape_1_L	20
#define	kString_WhiteScreen_Escape_2	"RETOURNER AU MENU"
#define	kString_WhiteScreen_Escape_2_L	17
#define	kString_WhiteScreen_Replay_1	"APPUYEZ SUR ESPACE"
#define	kString_WhiteScreen_Replay_1_L	18
#define	kString_WhiteScreen_Replay_2	"POUR REVOIR LA COURSE"
#define	kString_WhiteScreen_Replay_2_L	21
#define	kString_WhiteScreen_RaceTime	"TOTAL:"
#define	kString_WhiteScreen_RaceTime_L	6
#define	kString_WhiteScreen_Results		"RESULTAT DU JOUEUR: "
#define	kString_WhiteScreen_Results_L	20
#define	kString_WhiteScreen_Rank		"RANG:"
#define	kString_WhiteScreen_Rank_L		5
#define	kString_WhiteScreen_Bonus		"BONUS:"
#define	kString_WhiteScreen_Bonus_L		6
#define	kString_WhiteScreen_BestLap		"MEILLEUR TOUR:"
#define	kString_WhiteScreen_BestLap_L	14
#define	kString_WhiteScreen_TimeLeft	"TEMPS RESTANT:"
#define	kString_WhiteScreen_TimeLeft_L	14
#define	kString_WhiteScreen_MaxSpeed	"VITESSE MAX:"
#define	kString_WhiteScreen_MaxSpeed_L	12
#define	kString_WhiteScreen_Distance	"DISTANCE:"
#define	kString_WhiteScreen_Distance_L	9
#define	kString_WhiteScreen_AvSpeed		"V. MOYENNE:"
#define	kString_WhiteScreen_AvSpeed_L	11

#define	kString_Interface_ManualResolution	"\106Appuyez sur commande au démarrage pour afficher à nouveau ce dialogue."
#define	kString_Interface_NA			"\003N/D"
#define	kString_Interface_RequiredP		"\023Points nécessaires:"
#define	kString_Interface_JokerAccess	"NECESSITE UN JOKER"
#define	kString_Interface_JokerAccess_L	18
#define	kString_Interface_JokerRedo		"Vous pouvez utiliser un joker pour refaire\rcette course."
#define	kString_Interface_JokerRedo_L	57
#define	kString_Interface_AccessDenied	"ACCES REFUSE!"
#define	kString_Interface_AccessDenied_L	13
#define	kString_Interface_JokerNeed		"Vous avez besoin d'un joker\rpour refaire cette course."
#define	kString_Interface_JokerNeed_L	55
#define	kString_Interface_RaceLocation	"\021Lieu de la course"
#define	kString_Interface_Rank			"\004Rang"
#define	kString_Interface_Score			"\005Score"
#define	kString_Interface_TimeLeft		"\015Temps restant"
#define	kString_Interface_Level			"\007Niveau "
#define	kString_Interface_GameOver		"\005Perdu"
#define	kString_Interface_Champion		"\010Champion"
#define	kString_Interface_Challenge		"\011Challenge"

#define	kString_Camera_Free				"\014CAMERA LIBRE"
#define	kString_Camera_Follow			"\017CAMERA SUIVANTE"
#define	kString_Camera_Head				"\016CAMERA COCKPIT"
#define	kString_Camera_Onboard			"\015CAMERA A BORD"
#define	kString_Camera_TV				"\014WATERRACE TV"
#define	kString_Camera_Chase			"\020CAMERA DE CHASSE"
#define	kString_Camera_From				"\004 DE "
#define	kString_Camera_Of				"\004 DE "
#define	kString_Camera_On				"\005 SUR "
#define	kString_Camera_Replay			"\006REVOIR"
#define	kString_Camera_Separator		"\003 / "
#define	kString_Camera_Track			"\020CAMERA DE COURSE"

#define	kString_Preferences_Name		"\007Mon nom"
#define	kString_Preferences_Location	"\010Mon lieu"
#define	kString_Preferences_Computer	"\023Nom de l'ordinateur"
#define	kString_Preferences_Welcome		"\021Bienvenue au jeu!"

#define	kString_Misc_ReplayFileName		"\014Race Replay "
#define	kString_Misc_Continue			"\011Continuer"
#define	kString_Misc_Quit				"\007Quitter"
#define	kString_Misc_ScreenshotFileName	"\016WR Screenshot "

#define	kString_Network_Disconnected	"\041Déconnection du serveur!\rRaison: "
#define	kString_Network_Reason1			"\033Version du jeu incorrecte.\r"
#define	kString_Network_Reason2			"\026Mauvais mot de passe.\r"
#define	kString_Network_Reason3			"\031Le serveur a été arrêté.\r"
#define	kString_Network_Reason4			"\071Le jeu a déjà démarré sur le serveur.\rEssayez plus tard.\r"
#define	kString_Network_Reason5			"\012Inconnue.\r"
#define	kString_Network_Client1			"\032Infos serveur reçues:\r    "
#define	kString_Network_Client2			"\005\r    "
#define	kString_Network_Client3			"\026\r    Serveur en place "
#define	kString_Network_Client4			"\005 sur "
#define	kString_Network_Client5			"\020Nom du terrain: "
#define	kString_Network_Client6			"\040\r\rEnvoi des infos du joueur...\r\r"
#define	kString_Network_Client7			"\034Attente du début du jeu...\r\r"
#define	kString_Network_Client8			"\023Le jeu a commencé!\r"
#define	kString_Network_Server1			"\024Serveur déconnecté!\r"
#define	kString_Network_Server2			"\030Connection d'un joueur: "
#define	kString_Network_Server3			"\004 de "
#define	kString_Network_Server4			"\023 a été déconnecté!\r"
#define	kString_Network_Connecting1		"\031En cours de connection à "
#define	kString_Network_Connecting2		"\052... (patientez SVP - 10 secondes maximum)\r"
#define	kString_Network_Connecting3		"\057Impossible de ce connecter au serveur!\rRaison: "
#define	kString_Network_Connecting4		"\126Serveur inatteignable (adresse IP incorrecte, serveur déconnecté ou trop de joueurs).\r"
#define	kString_Network_Connecting5		"\025Connection établie.\r\r"
#define	kString_Network_Connecting6		"\040Attente des infos du serveur...\r"
#define	kString_Network_Connecting7		"\010(Erreur "
#define	kString_Network_Hosting1		"\027Démarrage du serveur à "
#define	kString_Network_Hosting2		"\053Impossible de démarrer le serveur! (Erreur "
#define	kString_Network_Hosting3		"\022Serveur en place.\r"
#define	kString_Network_Hosting4		"\030Attente des joueurs...\r\r"
#define	kString_Network_Hosting5		"\051Impossible de connecter le joueur local!\r"


/****************************************************************/
/*					GERMAN VERSION								*/
/****************************************************************/

#elif __LANGUAGE__ == kLanguage_German

#define	kString_Drop_OffLimit			"\015AUSSER LIMIT!"
#define	kString_Drop_UpsideDown			"\012UMGEKIPPT!"
#define	kString_Drop_UnderWater			"\015UNTER WASSER!"
#define	kString_Drop_Collision			"\016ZUSAMMENSTOSS!"
#define	kString_Drop_Manual				"\021NEUSTART SPIELER!"

#define	kString_Network_PlayerOffLine	"\020SPIELER OFFLINE!"
#define	kString_Network_BotLocation		"\007Botland"
#define	kString_Network_ServerOffLine	"\017SERVER OFFLINE!"

#if __DISPLAY_CHECK_POINT__
#define	kString_Driver_CheckPoint		"\014CHECK POINT!"
#define	kString_Driver_CheckPoint_L		13
#endif
#if __DISPLAY_MISSED_CHECK_POINT__
#define	kString_Driver_MissedCheckPoint	"\025CHECK POINT VERFEHLT!"
#define	kString_Driver_MissedCheckPoint_L	23
#endif
#define	kString_Driver_BestLapTime		"\012BESTZEIT: "
#define	kString_Driver_BestLapTime_L	11
#define	kString_Driver_LapTime			"\006ZEIT: "
#define	kString_Driver_LapTime_L		7
#define	kString_Driver_TimeExtended		"\020TIME EXTENDED: +"
#define	kString_Driver_TimeExtended_L	17
#define	kString_Driver_FirstPlace		"\014ERSTER RANG!"
#define	kString_Driver_FirstPlace_L		13
#define	kString_Driver_Finished			" IST AM ZIEL!"
#define	kString_Driver_Finished_L		13
#if __BOTS_FINISH_RACE__
#define	kString_Driver_LocalFinish		"\021SIE SIND AM ZIEL!"
#define	kString_Driver_LocalWon			"\026SIE SIND DER GEWINNER!"
#endif
#define	kString_Driver_Ready			"BEREIT: x"
#define	kString_Driver_Ready_L			9
#define	kString_Driver_Left				"NOCH: x"
#define	kString_Driver_Left_L			7
#if __DISPLAY_WRONG_DIRECTION__
#define	kString_Driver_WrongDirection	"\021FALSCHE RICHTUNG!"
#define	kString_Driver_WrongDirection_L	18
#endif
#if __AUTOPILOT__
#define	kString_Driver_AutoPilot		"\012AUTOPILOT!"
#define	kString_Driver_Finish			"\034DRUECKEN SIE ESC ZUM BEENDEN"
#endif

#define	kString_WhiteScreen_Escape_1	"DRUECKEN SIE ESC UM"
#define	kString_WhiteScreen_Escape_1_L	19
#define	kString_WhiteScreen_Escape_2	"ZUM MENU ZU GELANGEN"
#define	kString_WhiteScreen_Escape_2_L	20
#define	kString_WhiteScreen_Replay_1	"DRUECKEN SIE LEERSCHLAG"
#define	kString_WhiteScreen_Replay_1_L	23
#define	kString_WhiteScreen_Replay_2	"FUER DIE WIEDERHOLUNG."
#define	kString_WhiteScreen_Replay_2_L	22
#define	kString_WhiteScreen_RaceTime	"GESAMTZEIT:"
#define	kString_WhiteScreen_RaceTime_L	11
#define	kString_WhiteScreen_Results		"ERGEBNIS:"
#define	kString_WhiteScreen_Results_L	9
#define	kString_WhiteScreen_Rank		"RANG:"
#define	kString_WhiteScreen_Rank_L		5
#define	kString_WhiteScreen_Bonus		"BONUS:"
#define	kString_WhiteScreen_Bonus_L		6
#define	kString_WhiteScreen_BestLap		"BESTE RUNDE:"
#define	kString_WhiteScreen_BestLap_L	12
#define	kString_WhiteScreen_TimeLeft	"RESTZEIT:"
#define	kString_WhiteScreen_TimeLeft_L	9
#define	kString_WhiteScreen_MaxSpeed	"HOECHSTG.:"
#define	kString_WhiteScreen_MaxSpeed_L	10
#define	kString_WhiteScreen_Distance	"STRECKE:"
#define	kString_WhiteScreen_Distance_L	8
#define	kString_WhiteScreen_AvSpeed		"SCHNITT:"
#define	kString_WhiteScreen_AvSpeed_L	8

#define	kString_Interface_ManualResolution	"\124Drücken Sie auf die Befehlstaste beim Starten um dieses Fenster wiederum anzuzeigen."
#define	kString_Interface_NA			"\003N/V"
#define	kString_Interface_RequiredP		"\021Benötigte Punkte:"
#define	kString_Interface_JokerAccess	"JOCKER BENOETIGT!"
#define	kString_Interface_JokerAccess_L	17
#define	kString_Interface_JokerRedo		"Sie können ein Jocker verwenden\rum dieses Rennen zu wiederholen."
#define	kString_Interface_JokerRedo_L	65
#define	kString_Interface_AccessDenied	"ZUGRIFF VERWEIGERT!"
#define	kString_Interface_AccessDenied_L	19
#define	kString_Interface_JokerNeed		"Sie benötigen einen Jocker \rum dieses Rennen zu wiederholen."
#define	kString_Interface_JokerNeed_L	59
#define	kString_Interface_RaceLocation	"\007Rennort"
#define	kString_Interface_Rank			"\004Rang"
#define	kString_Interface_Score			"\015Anzahl punkte"
#define	kString_Interface_TimeLeft		"\010Restzeit"
#define	kString_Interface_Level			"\006Level "
#define	kString_Interface_GameOver		"\003Aus"
#define	kString_Interface_Champion		"\010Champion"
#define	kString_Interface_Challenge		"\011Challenge"

#define	kString_Camera_Free				"\014FREIE KAMERA"
#define	kString_Camera_Follow			"\022VERFOLGUNGS-KAMERA"
#define	kString_Camera_Head				"\016COCKPIT-KAMERA"
#define	kString_Camera_Onboard			"\016ONBOARD KAMERA"
#define	kString_Camera_TV				"\014WATERRACE TV"
#define	kString_Camera_Chase			"\023VERFOLGUNGS-KAMERA2"
#define	kString_Camera_From				"\005 VON "
#define	kString_Camera_Of				"\004 OF "
#define	kString_Camera_On				"\005 AUF "
#define	kString_Camera_Replay			"\014WIEDERHOLUNG"
#define	kString_Camera_Separator		"\003 / "
#define	kString_Camera_Track			"\016STRECKENKAMERA"

#define	kString_Preferences_Name		"\011Mein Name"
#define	kString_Preferences_Location	"\010Mein Ort"
#define	kString_Preferences_Computer	"\022Name des Computers"
#define	kString_Preferences_Welcome		"\032Wilkommen in meinem Spiel!"

#define	kString_Misc_ReplayFileName		"\014Race Replay "
#define	kString_Misc_Continue			"\006Weiter"
#define	kString_Misc_Quit				"\007Beenden"
#define	kString_Misc_ScreenshotFileName	"\016WR Screenshot "

#define	kString_Network_Disconnected	"\053Verbindung mit Server unterbrochen!\rGrund: "
#define	kString_Network_Reason1			"\026Falsche Spielversion.\r"
#define	kString_Network_Reason2			"\023Falsches Passwort.\r"
#define	kString_Network_Reason3			"\026Server ausgeschaltet.\r"
#define	kString_Network_Reason4			"\112Spiel auf dem Server bereits gestartet.\rVersuchen Sie es später nochmals.\r"
#define	kString_Network_Reason5			"\013Unbekannt.\r"
#define	kString_Network_Client1			"\036Informationen vom Server:\r    "
#define	kString_Network_Client2			"\005\r    "
#define	kString_Network_Client3			"\026\r    Server gestartet "
#define	kString_Network_Client4			"\005 auf "
#define	kString_Network_Client5			"\026Name der Rennstrecke: "
#define	kString_Network_Client6			"\030\r\rSende Spielerinfo...\r\r"
#define	kString_Network_Client7			"\035Warte den Spielbeginn ab...\r\r"
#define	kString_Network_Client8			"\030Spiel bereits begonnen!\r"
#define	kString_Network_Server1			"\026Server nicht im Netz!\r"
#define	kString_Network_Server2			"\021Spielereintritt: "
#define	kString_Network_Server3			"\005 von "
#define	kString_Network_Server4			"\027 wurde ausgeschlossen!\r"
#define	kString_Network_Connecting1		"\017Verbindung mit "
#define	kString_Network_Connecting2		"\066... (Bitte warten - maximale Verzögerung 10 Sekunden)\r"
#define	kString_Network_Connecting3		"\050Verbindung mit Server unmöglich!\rGrund: "
#define	kString_Network_Connecting4		"\130Server nicht verfügbar (falsche IP Adresse, Server nicht im Netz oder zuviele Spieler).\r"
#define	kString_Network_Connecting5		"\014Verbunden.\r\r"
#define	kString_Network_Connecting6		"\044Erwarte Informationen vom Server...\r"
#define	kString_Network_Connecting7		"\010(Fehler "
#define	kString_Network_Hosting1		"\026Starte den Server mit "
#define	kString_Network_Hosting2		"\056Server konnte nicht gestartet werden! (Fehler "
#define	kString_Network_Hosting3		"\022Server verbunden.\r"
#define	kString_Network_Hosting4		"\031Warte auf Mitspieler...\r\r"
#define	kString_Network_Hosting5		"\052Verbindung mit lokalem Spieler unmöglich!\r"

/****************************************************************/
/*					ITALIAN VERSION								*/
/****************************************************************/

#elif __LANGUAGE__ == kLanguage_Italian

#define	kString_Drop_OffLimit			"\021FUORI DAI LIMITI!"
#define	kString_Drop_UpsideDown			"\014UPSIDE-DOWN!"
#define	kString_Drop_UnderWater			"\013SOTT'ACQUA!"
#define	kString_Drop_Collision			"\013COLLISIONE!"
#define	kString_Drop_Manual				"\021GIOCATORE CADUTO!"

#define	kString_Network_PlayerOffLine	"\022GIOCATORE OFFLINE!"
#define	kString_Network_BotLocation		"\007Botland"
#define	kString_Network_ServerOffLine	"\017SERVER OFFLINE!"

#if __DISPLAY_CHECK_POINT__
#define	kString_Driver_CheckPoint		"\014CHECK POINT!"
#define	kString_Driver_CheckPoint_L		13
#endif
#if __DISPLAY_MISSED_CHECK_POINT__
#define	kString_Driver_MissedCheckPoint	"\024CHECK POINT MANCATO!"
#define	kString_Driver_MissedCheckPoint_L	21
#endif
#define	kString_Driver_BestLapTime		"\027MIGLIOR TEMPO SU GIRO: "
#define	kString_Driver_BestLapTime_L	24
#define	kString_Driver_LapTime			"\017TEMPO SU GIRO: "
#define	kString_Driver_LapTime_L		16
#define	kString_Driver_TimeExtended		"\023ESTENSIONE TEMPO: +"
#define	kString_Driver_TimeExtended_L	20
#define	kString_Driver_FirstPlace		"\014PRIMO POSTO!"
#define	kString_Driver_FirstPlace_L		13
#define	kString_Driver_Finished			" HA TERMINATO!"
#define	kString_Driver_Finished_L		14
#if __BOTS_FINISH_RACE__
#define	kString_Driver_LocalFinish		"\016HAI TERMINATO!"
#define	kString_Driver_LocalWon			"\021SEI IL VINCITORE!"
#endif
#define	kString_Driver_Ready			"PRONTO: x"
#define	kString_Driver_Ready_L			9
#define	kString_Driver_Left				"RIMANE: x"
#define	kString_Driver_Left_L			9
#if __DISPLAY_WRONG_DIRECTION__
#define	kString_Driver_WrongDirection	"\024DIREZIONE SBAGLIATA!"
#define	kString_Driver_WrongDirection_L	21
#endif
#if __AUTOPILOT__
#define	kString_Driver_AutoPilot		"\022PILOTA AUTOMATICO!"
#define	kString_Driver_Finish			"\027PREMI ESC PER TERMINARE"
#endif

#define	kString_WhiteScreen_Escape_1	"PREMI ESC PER"
#define	kString_WhiteScreen_Escape_1_L	13
#define	kString_WhiteScreen_Escape_2	"RITORNARE AL MENU"
#define	kString_WhiteScreen_Escape_2_L	17
#define	kString_WhiteScreen_Replay_1	"PREMI SPAZIO"
#define	kString_WhiteScreen_Replay_1_L	12
#define	kString_WhiteScreen_Replay_2	"PER RIVEDERE LA CORSA"
#define	kString_WhiteScreen_Replay_2_L	21
#define	kString_WhiteScreen_RaceTime	"TEMPO DELLA CORSA:"
#define	kString_WhiteScreen_RaceTime_L	18
#define	kString_WhiteScreen_Results		"RISULTATI GIOCATORE: "
#define	kString_WhiteScreen_Results_L	21
#define	kString_WhiteScreen_Rank		"POSIZIONE:"
#define	kString_WhiteScreen_Rank_L		10
#define	kString_WhiteScreen_Bonus		"BONUS:"
#define	kString_WhiteScreen_Bonus_L		6
#define	kString_WhiteScreen_BestLap		"MIGLIOR GIRO:"
#define	kString_WhiteScreen_BestLap_L	13
#define	kString_WhiteScreen_TimeLeft	"TEMPO RIMANENTE:"
#define	kString_WhiteScreen_TimeLeft_L	16
#define	kString_WhiteScreen_MaxSpeed	"VELOCITA' MASSIMA:"
#define	kString_WhiteScreen_MaxSpeed_L	18
#define	kString_WhiteScreen_Distance	"DISTANZA:"
#define	kString_WhiteScreen_Distance_L	9
#define	kString_WhiteScreen_AvSpeed		"VELOCITA' MEDIA:"
#define	kString_WhiteScreen_AvSpeed_L	16

#define	kString_Interface_ManualResolution	"\117Premi il tasto comando durante l'avvio per mostrare nuovamente questa finestra."
#define	kString_Interface_NA			"\003N/A"
#define	kString_Interface_RequiredP		"\020Punti richiesti:"
#define	kString_Interface_JokerAccess	"ACCESSO JOLLY!"
#define	kString_Interface_JokerAccess_L	14
#define	kString_Interface_JokerRedo		"Puoi usare un jolly per rifare\rquesta corsa."
#define	kString_Interface_JokerRedo_L	45
#define	kString_Interface_AccessDenied	"ACCESSO NEGATO!"
#define	kString_Interface_AccessDenied_L	15
#define	kString_Interface_JokerNeed		"Hai bisogno di un jolly per rifare\rquesta corsa."
#define	kString_Interface_JokerNeed_L	49
#define	kString_Interface_RaceLocation	"\010Località"
#define	kString_Interface_Rank			"\011Posizione"
#define	kString_Interface_Score			"\011Punteggio"
#define	kString_Interface_TimeLeft		"\017Tempo rimanente"
#define	kString_Interface_Level			"\010Livello "
#define	kString_Interface_GameOver		"\011Game over"
#define	kString_Interface_Champion		"\010Campione"
#define	kString_Interface_Challenge		"\005Sfida"

#define	kString_Camera_Free				"\016RIPRESA LIBERA"
#define	kString_Camera_Follow			"\021RIPRESA A SEGUIRE"
#define	kString_Camera_Head				"\017RIPRESA INTERNA"
#define	kString_Camera_Onboard			"\013CAMERA BOAT"
#define	kString_Camera_TV				"\014TV WATERRACE"
#define	kString_Camera_Chase			"\014CHASE CAMERA"
#define	kString_Camera_From				"\004 DA "
#define	kString_Camera_Of				"\004 DI "
#define	kString_Camera_On				"\004 ON "
#define	kString_Camera_Replay			"\006REPLAY"
#define	kString_Camera_Separator		"\003 / "
#define	kString_Camera_Track			"\015RIPRESE FERME"

#define	kString_Preferences_Name		"\010Nome mio"
#define	kString_Preferences_Location	"\014Mia località"
#define	kString_Preferences_Computer	"\015Nome Computer"
#define	kString_Preferences_Welcome		"\030Benvenuto nel mio gioco!"

#define	kString_Misc_ReplayFileName		"\015Replay Corsa "
#define	kString_Misc_Continue			"\010Continua"
#define	kString_Misc_Quit				"\004Esci"
#define	kString_Misc_ScreenshotFileName	"\016WR Screenshot "

#define	kString_Network_Disconnected	"\040Disconnesso dall'host!\rRagione: "
#define	kString_Network_Reason1			"\035Versione gioco non corretta.\r"
#define	kString_Network_Reason2			"\021Password errata.\r"
#define	kString_Network_Reason3			"\034Il Server è stato staccato.\r"
#define	kString_Network_Reason4			"\065Il gioco sul Server è già iniziato.\rProva più tardi.\r"
#define	kString_Network_Reason5			"\015Sconosciuta.\r"
#define	kString_Network_Client1			"\036Ricevute info sul Server:\r    "
#define	kString_Network_Client2			"\005\r    "
#define	kString_Network_Client3			"\023\r    Server attivo "
#define	kString_Network_Client4			"\010 attivo "
#define	kString_Network_Client5			"\016Nome terreno: "
#define	kString_Network_Client6			"\036\r\rSpedendo info giocatore...\r\r"
#define	kString_Network_Client7			"\041Aspettando l'avvio del gioco...\r\r"
#define	kString_Network_Client8			"\024Il gioco ha inizio!\r"
#define	kString_Network_Server1			"\020Server offline!\r"
#define	kString_Network_Server2			"\030Un giocatore è entrato: "
#define	kString_Network_Server3			"\004 da "
#define	kString_Network_Server4			"\026 è stato disconnesso!\r"
#define	kString_Network_Connecting1		"\015Collegando a "
#define	kString_Network_Connecting2		"\062... (attendere prego - 10 secondi max di ritardo)\r"
#define	kString_Network_Connecting3		"\051Non posso connettermi all'host!\rRagione: "
#define	kString_Network_Connecting4		"\151Server non raggiungibile (indirizzo IP errato, il server potrebbe essere caduto o con troppi giocatori).\r"
#define	kString_Network_Connecting5		"\030Stabilita Connessione.\r\r"
#define	kString_Network_Connecting6		"\036Aspettando info dal server...\r"
#define	kString_Network_Connecting7		"\010(Errore "
#define	kString_Network_Hosting1		"\020Avviando server "
#define	kString_Network_Hosting2		"\047Impossibile avviare il server! (Errore "
#define	kString_Network_Hosting3		"\017Server online.\r"
#define	kString_Network_Hosting4		"\031Aspettando i clients...\r\r"
#define	kString_Network_Hosting5		"\051Impossibile connettere giocatore locale!\r"

#endif

#endif

