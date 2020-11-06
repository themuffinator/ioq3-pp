/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// console.c

#include "client.h"

#define	DEFAULT_CONSOLE_WIDTH	78
#define	NUM_CON_TIMES			16
#define CON_TEXTSIZE			65536	//32768

int		g_console_field_width = DEFAULT_CONSOLE_WIDTH;
float	g_console_char_width = SMALLCHAR_WIDTH;
float	g_console_char_height = SMALLCHAR_HEIGHT;
float	g_console_font_scale = 1.0;

typedef struct {
	qboolean	initialized;

	short	text[CON_TEXTSIZE];
	int		current;		// line where next message will be printed
	int		x;				// offset in current line for next print
	int		display;		// bottom of console displays this line

	int 	linewidth;		// characters across screen
	int		totallines;		// total lines in console scrollback

	float	xadjust;		// for wide aspect screens

	float	displayFrac;	// aproaches finalFrac at scr_conspeed
	float	finalFrac;		// 0.0 to 1.0 lines of console to display

	int		vislines;		// in scanlines

	int		times[NUM_CON_TIMES];	// cls.realtime time the line was generated
								// for transparent notify lines
	int		skip[NUM_CON_TIMES];	// for timestamps
	vec4_t	color;
} console_t;

console_t	con;

cvar_t		*con_speed;
cvar_t		*con_notifytime;

//nql
cvar_t		*con_noprint;
cvar_t		*con_backcolor;
cvar_t		*con_background;
cvar_t		*con_breakerwidth;
cvar_t		*con_clock;
cvar_t		*con_color;
cvar_t		*con_fadefrac;
cvar_t		*con_fading;
cvar_t		*con_height;
cvar_t		*con_linescroll;
cvar_t		*con_notifyfadetime;
cvar_t		*con_notifylines;
cvar_t		*con_notifyscale;
cvar_t		*con_notifyY;
cvar_t		*con_opacity;
cvar_t		*con_scale;
cvar_t		*con_timestamps;

cvar_t		*msgmode_opacity;
cvar_t		*msgmode_scale;
cvar_t		*msgmode_Y;
//-nql


vec4_t conback_colors[] = {
	{ 0.10000f, 0.03000f, 0.03000f, 0.10000f },      // A - red			00
	{ 0.10000f, 0.05000f, 0.03000f, 0.10000f },      // B -				01
	{ 0.10000f, 0.06000f, 0.03000f, 0.10000f },      // C -				02
	{ 0.10000f, 0.08000f, 0.03000f, 0.10000f },      // D -				03
	{ 0.10000f, 0.10000f, 0.03000f, 0.10000f },      // E -	yellow		04
	{ 0.08000f, 0.10000f, 0.03000f, 0.10000f },      // F -				05
	{ 0.06000f, 0.10000f, 0.03000f, 0.10000f },      // G -				06
	{ 0.05000f, 0.10000f, 0.03000f, 0.10000f },      // H -				07
	{ 0.03000f, 0.10000f, 0.03000f, 0.10000f },      // I -	green		08
	{ 0.03000f, 0.10000f, 0.05000f, 0.10000f },      // J -				09
	{ 0.03000f, 0.10000f, 0.06000f, 0.10000f },      // K -				10
	{ 0.03000f, 0.10000f, 0.08000f, 0.10000f },      // L -				11
	{ 0.03000f, 0.10000f, 0.10000f, 0.10000f },      // M -	cyan		12
	{ 0.03000f, 0.08000f, 0.10000f, 0.10000f },      // N -				13
	{ 0.03000f, 0.06000f, 0.10000f, 0.10000f },      // O -				14
	{ 0.03000f, 0.05000f, 0.10000f, 0.10000f },      // P -				15
	{ 0.03000f, 0.03000f, 0.10000f, 0.10000f },      // Q -	blue		16
	{ 0.05000f, 0.03000f, 0.10000f, 0.10000f },      // R -				17
	{ 0.06000f, 0.03000f, 0.10000f, 0.10000f },      // S -				18
	{ 0.08000f, 0.03000f, 0.10000f, 0.10000f },      // T -				19
	{ 0.10000f, 0.03000f, 0.10000f, 0.10000f },      // U -	magenta		20
	{ 0.10000f, 0.03000f, 0.08000f, 0.10000f },      // V -				21
	{ 0.10000f, 0.03000f, 0.06000f, 0.10000f },      // W -				22
	{ 0.10000f, 0.03000f, 0.05000f, 0.10000f },      // X -				23
	{ 0.06000f, 0.06000f, 0.06000f, 1.00000f },		 // Y -	coal		24
	{ 0.00000f, 0.00000f, 0.00000f, 1.00000f }		 // Z -	black		25
};
static int  conbackColorSize = ARRAY_LEN( conback_colors );

/*
===================
Con_GetScale
===================
*/
float Con_GetScale( void ) {
	float	scale;

	scale = con_scale->value;
	if ( scale < 0.2 ) scale = 0.2;
	else if ( scale > 1.0 ) scale = 1.0;

	return scale;
}


/*
===================
Con_SetCharSizeAdjusted
===================
*/
void Con_SetCharSizeAdjusted( const qboolean init ) {
	float scale = /*init ? 1.0 : */Con_GetScale();

	SCR_AdjustFrom640(NULL, NULL, NULL, &scale);
	if ( g_console_font_scale == scale ) return;

	if ( !init ) con_scale->modificationCount = 0;

	g_console_font_scale = scale;
	g_console_char_width = (float)SMALLCHAR_WIDTH * g_console_font_scale;
	g_console_char_height = (float)SMALLCHAR_HEIGHT * g_console_font_scale;
}


/*
===================
Con_SetConLineWidth
===================
*/
static void Con_SetLineWidth( const qboolean init ) {
	g_console_field_width = init ? DEFAULT_CONSOLE_WIDTH : ( (float)cls.glconfig.vidWidth / g_console_char_width ) - 2;
}


/*
================
Con_Refresh
================
*/
void Con_Refresh(const qboolean init) {
	Con_SetCharSizeAdjusted(init);
	Con_SetLineWidth(init);
}


/*
================
Con_ClearField
================
*/
void Con_ClearField( const qboolean refresh ) {
	Field_Clear( &g_consoleField );
	if ( refresh ) Con_Refresh( qfalse );
	g_consoleField.widthInChars = g_console_field_width;
}


/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f( void ) {
	// Can't toggle the console when it's the only thing available
	if ( clc.state == CA_DISCONNECTED && Key_GetCatcher() == KEYCATCH_CONSOLE ) {
		return;
	}

	Con_ClearField( qtrue );
	Con_ClearNotify();
	Key_SetCatcher( Key_GetCatcher() ^ KEYCATCH_CONSOLE );
}


/*
===================
Con_ToggleMenu_f
===================
*/
void Con_ToggleMenu_f( void ) {
	CL_KeyEvent( K_ESCAPE, qtrue, Sys_Milliseconds() );
	CL_KeyEvent( K_ESCAPE, qfalse, Sys_Milliseconds() );
}


/*
================
Con_MessageMode_f
================
*/
void Con_MessageMode_f( void ) {
	chat_playerNum = -1;
//nql
	//chat_team = qfalse;
	chat_type = CHATT_NORMAL;
//-nql
	Field_Clear( &chatField );
//nql
	//chatField.widthInChars = 30;
	chatField.widthInChars = g_console_field_width - strlen("say: ");	//85;
//-nql

	Key_SetCatcher( Key_GetCatcher() ^ KEYCATCH_MESSAGE );
}

/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f (void) {
	chat_playerNum = -1;
//nql
	//chat_team = qtrue;
	chat_type = CHATT_TEAM;
//-nql
	Field_Clear( &chatField );
//nql
	//chatField.widthInChars = 25;
	chatField.widthInChars = g_console_field_width - strlen("say_team: ");	//80;
//-nql
	Key_SetCatcher( Key_GetCatcher() ^ KEYCATCH_MESSAGE );
}

/*
================
Con_MessageMode3_f
================
*/
void Con_MessageMode3_f (void) {
	chat_playerNum = VM_Call( cgvm, CG_CROSSHAIR_PLAYER );
	if ( chat_playerNum < 0 || chat_playerNum >= MAX_CLIENTS ) {
		chat_playerNum = -1;
		return;
	}
//nql
	//chat_team = qfalse;
	chat_type = CHATT_TARGET;
//-nql
	Field_Clear( &chatField );
//nql
	//chatField.widthInChars = 30;
	chatField.widthInChars = g_console_field_width - strlen("tell_target: ");	//77;
//-nql
	Key_SetCatcher( Key_GetCatcher() ^ KEYCATCH_MESSAGE );
}

/*
================
Con_MessageMode4_f
================
*/
void Con_MessageMode4_f (void) {
	chat_playerNum = VM_Call( cgvm, CG_LAST_ATTACKER );
	if ( chat_playerNum < 0 || chat_playerNum >= MAX_CLIENTS ) {
		chat_playerNum = -1;
		return;
	}
//nql
	//chat_team = qfalse;
	chat_type = CHATT_ATTACK;
//-nql
	Field_Clear( &chatField );
//nql
	//chatField.widthInChars = 30;
	chatField.widthInChars = g_console_field_width - strlen("tell_attacker: ");	//74;
//-nql
	Key_SetCatcher( Key_GetCatcher() ^ KEYCATCH_MESSAGE );
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f( void ) {
	int		i;

	for ( i = 0 ; i < CON_TEXTSIZE ; i++ ) {
		con.text[i] = (ColorIndex(COLOR_WHITE)<<8) | ' ';
	}

	Con_Bottom();		// go to end
}


/*
================
Con_Find_f

Find substrings within the console history
================
*/
void Con_Find_f( void ) {
#if 0
	char	*subs, *str;
	short	*line;
	int		bufferlen;
	char	*buffer;
	int		i, l;

	if ( Cmd_Argc() != 2 ) {
		Com_xPrintf( "Usage", "find", "[search term]" );
		Com_xPrintf( "Description", "Searches for a returns lines in console history containing the search term", "" );
		return;
	}

	for ( l = con.current - con.totallines + 1 ; l <= con.current ; l++ ) {
		
	}
#endif
}
						
/*
================
Con_Dump_f

Save the console contents out to a file
================
*/
void Con_Dump_f( void ) {
	int		l, x, i;
	short	*line;
	fileHandle_t	f;
	int		bufferlen;
	char	*buffer;
	char	filename[MAX_QPATH];

	if ( Cmd_Argc() != 2 ) {
		Com_xPrintf( "Usage", "condump", "<filename>" );
		return;
	}

	Q_strncpyz( filename, Cmd_Argv( 1 ), sizeof( filename ) );
	COM_DefaultExtension( filename, sizeof( filename ), ".txt" );

	f = FS_FOpenFileWrite( filename );
	if ( !f ) {
		Com_Printf ("ERROR: couldn't open %s.\n", filename);
		return;
	}

	// skip empty lines
	for ( l = con.current - con.totallines + 1 ; l <= con.current ; l++ ) {
		line = con.text + (l%con.totallines)*con.linewidth;
		for ( x=0 ; x<con.linewidth ; x++ )
			if ( (line[x] & 0xff) != ' ' )
				break;
		if ( x != con.linewidth )
			break;
	}

#ifdef _WIN32
	bufferlen = con.linewidth + 3 * sizeof ( char );
#else
	bufferlen = con.linewidth + 2 * sizeof ( char );
#endif

	buffer = Hunk_AllocateTempMemory( bufferlen );

	// write the remaining lines
	buffer[bufferlen-1] = 0;
	for ( ; l <= con.current ; l++ ) {
		line = con.text + (l%con.totallines)*con.linewidth;
		for( i = 0 ; i < con.linewidth ; i++ )
			buffer[i] = line[i] & 0xff;
		for ( x = con.linewidth-1 ; x >= 0 ; x-- ) {
			if ( buffer[x] == ' ' )
				buffer[x] = 0;
			else
				break;
		}
#ifdef _WIN32
		Q_strcat( buffer, bufferlen, "\r\n" );
#else
		Q_strcat( buffer, bufferlen, "\n" );
#endif
		FS_Write( buffer, strlen(buffer), f );
	}

	Com_Printf( "Dumped console text to %s.\n", filename );

	Hunk_FreeTempMemory( buffer );
	FS_FCloseFile( f );
}

						
/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify( void ) {
	int		i;
	
	for ( i = 0 ; i < NUM_CON_TIMES ; i++ ) {
		con.times[i] = 0;
	}
}


/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize( void ) {
	int		i, j, width, oldwidth, oldtotallines, numlines, numchars;
	short	tbuf[CON_TEXTSIZE];
	
	//width = ( cls.glconfig.vidWidth / SMALLCHAR_WIDTH ) - 2;
	width = g_console_field_width;

	if ( width == con.linewidth ) return;

	if ( width < 1 ) {			// video hasn't been initialized yet
		width = DEFAULT_CONSOLE_WIDTH;
		con.linewidth = width;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		for ( i = 0 ; i < CON_TEXTSIZE ; i++ )
			con.text[i] = ( ColorIndex(COLOR_WHITE)<<8 ) | ' ';
	} else {
		oldwidth = con.linewidth;
		con.linewidth = width;
		oldtotallines = con.totallines;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		numlines = oldtotallines;

		if ( con.totallines < numlines )
			numlines = con.totallines;

		numchars = oldwidth;
	
		if ( con.linewidth < numchars )
			numchars = con.linewidth;

		Com_Memcpy( tbuf, con.text, CON_TEXTSIZE * sizeof(short) );
		for( i = 0 ; i < CON_TEXTSIZE ; i++ )
			con.text[i] = ( ColorIndex(COLOR_WHITE)<<8 ) | ' ';

		for ( i = 0 ; i < numlines ; i++ ) {
			for ( j = 0 ; j < numchars ; j++ ) {
				con.text[(con.totallines - 1 - i) * con.linewidth + j] =
						tbuf[((con.current - i + oldtotallines) %
							  oldtotallines) * oldwidth + j];
			}
		}

		Con_ClearNotify();
	}

	con.current = con.totallines - 1;
	con.display = con.current;
}


/*
==================
Cmd_CompleteTxtName
==================
*/
void Cmd_CompleteTxtName( char *args, int argNum ) {
	if ( argNum == 2 ) {
		Field_CompleteFilename( "", "txt", qfalse, qtrue );
	}
}


/*
================
Con_Init
================
*/
void Con_Init (void) {
	int		i;

//nql: cvars
	con_notifytime = Cvar_Get( "con_notifytime", "3", 0 );
	Cvar_CheckRange( con_notifytime, 0, 10, qtrue );
	//Cvar_Description( con_speed, "Sets the maximum number of seconds console notifications appear for." );
	con_speed = Cvar_Get( "con_speed", "3", CVAR_ARCHIVE );
	Cvar_CheckRange( con_speed, 0.1f, 1000.0f, qfalse );
	//Cvar_Description( con_speed, "Sets the console opening and closing speed." );

	con_noprint = Cvar_Get( "con_noprint", "0", 0 );
	Cvar_CheckRange( con_noprint, 0, 1, qtrue );
	con_backcolor = Cvar_Get( "con_backcolor", "25", CVAR_ARCHIVE );
	Cvar_CheckRange( con_backcolor, 1, conbackColorSize, qtrue );
	con_background = Cvar_Get( "con_background", "1", CVAR_ARCHIVE );
	Cvar_CheckRange( con_background, 0, 1, qtrue );
	con_breakerwidth = Cvar_Get( "con_breakerwidth", "2", CVAR_ARCHIVE );
	Cvar_CheckRange( con_breakerwidth, 0, 8, qtrue );
	con_clock = Cvar_Get( "con_clock", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( con_clock, 0, 2, qtrue );
	con_color = Cvar_Get( "con_color", "1", CVAR_ARCHIVE );
	Cvar_CheckRange( con_color, 1, 26, qtrue );
	con_fadefrac = Cvar_Get( "con_fadefrac", "1.0", CVAR_ARCHIVE );
	Cvar_CheckRange( con_fadefrac, 0.0, 1.0, qfalse );
	con_fading = Cvar_Get( "con_fading", "1", CVAR_ARCHIVE );
	Cvar_CheckRange( con_fading, 0, 2, qtrue );
	con_height = Cvar_Get( "con_height", "0.5", CVAR_ARCHIVE );
	Cvar_CheckRange( con_height, 0.1, 1.0, qfalse );
	con_linescroll = Cvar_Get( "con_linescroll", "4", CVAR_ARCHIVE );
	Cvar_CheckRange( con_linescroll, 1, 32, qtrue );
	con_notifyfadetime = Cvar_Get( "con_notifyfadetime", "200", CVAR_ARCHIVE );
	Cvar_CheckRange( con_notifyfadetime, 0, 10000, qtrue );
	con_notifylines = Cvar_Get( "con_notifylines", "4", CVAR_ARCHIVE );
	Cvar_CheckRange( con_notifylines, 1, 16, qtrue );
	con_notifyscale = Cvar_Get( "con_notifyscale", "1.0", CVAR_ARCHIVE );
	Cvar_CheckRange( con_notifyscale, 0.1, 1.0, qfalse );
	con_notifyY = Cvar_Get( "con_notifyY", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( con_notifyY, 0, 480, qtrue );
	con_opacity = Cvar_Get( "con_opacity", "0.9", CVAR_ARCHIVE );
	Cvar_CheckRange( con_opacity, 0.0, 1.0, qfalse );
	con_scale = Cvar_Get( "con_scale", "1.0", CVAR_ARCHIVE );
	Cvar_CheckRange( con_scale, 0.2, 1.0, qfalse );
	con_timestamps = Cvar_Get( "con_timestamps", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( con_timestamps, 0, 4, qtrue );
	
	msgmode_opacity = Cvar_Get( "msgmode_opacity", "0.9", CVAR_ARCHIVE );
	Cvar_CheckRange( msgmode_opacity, 0.0, 1.0, qfalse );
	msgmode_scale = Cvar_Get( "msgmode_scale", "1.0", CVAR_ARCHIVE );
	Cvar_CheckRange( msgmode_scale, 0.1, 1.0, qfalse );
	msgmode_Y = Cvar_Get( "msgmode_Y", "410", CVAR_ARCHIVE );
	Cvar_CheckRange( msgmode_Y, 0, SCREEN_HEIGHT, qtrue );
//-nql

	//Con_Refresh(qtrue);

	Field_Clear( &g_consoleField );

	g_consoleField.widthInChars = g_console_field_width;
	
	for ( i = 0 ; i < COMMAND_HISTORY ; i++ ) {
		Field_Clear( &historyEditLines[i] );
		historyEditLines[i].widthInChars = g_console_field_width;
	}
	CL_LoadConsoleHistory();

	Cmd_AddCommand( "toggleconsole", Con_ToggleConsole_f );
	Cmd_AddCommand( "togglemenu", Con_ToggleMenu_f );
	Cmd_AddCommand( "messagemode", Con_MessageMode_f );
	Cmd_AddCommand( "messagemode2", Con_MessageMode2_f );
	Cmd_AddCommand( "messagemode3", Con_MessageMode3_f );
	Cmd_AddCommand( "messagemode4", Con_MessageMode4_f );
	Cmd_AddCommand( "clear", Con_Clear_f );
	Cmd_AddCommand( "condump", Con_Dump_f );
	Cmd_SetCommandCompletionFunc( "condump", Cmd_CompleteTxtName );
	Cmd_AddCommand( "find", Con_Find_f );
}

/*
================
Con_Shutdown
================
*/
void Con_Shutdown( void ) {
	Cmd_RemoveCommand( "toggleconsole" );
	Cmd_RemoveCommand( "togglemenu" );
	Cmd_RemoveCommand( "messagemode" );
	Cmd_RemoveCommand( "messagemode2" );
	Cmd_RemoveCommand( "messagemode3" );
	Cmd_RemoveCommand( "messagemode4" );
	Cmd_RemoveCommand( "clear" );
	Cmd_RemoveCommand( "condump" );
	Cmd_RemoveCommand( "find" );
}

/*
===============
Con_Linefeed
===============
*/
static void Con_Linefeed( qboolean skipnotify ) {
	int		i;

	// mark time for transparent overlay
	if ( con.current >= 0 ) {
		if ( skipnotify )
			con.times[con.current % NUM_CON_TIMES] = 0;
		else
			con.times[con.current % NUM_CON_TIMES] = cls.realtime;
	}

	con.x = 0;
	if ( con.display == con.current )
		con.display++;
	con.current++;
	for( i = 0 ; i < con.linewidth ; i++ )
		con.text[(con.current%con.totallines)*con.linewidth+i] = (ColorIndex(COLOR_WHITE)<<8) | ' ';
}

/*
================
CL_ConsolePrint

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the text will appear at the top of the game window
================
*/
void CL_ConsolePrint( char *txt ) {
	int		y, l;
	unsigned char	c;
	unsigned short	color;
	qboolean skipnotify = qfalse;		// NERVE - SMF
	int prev;							// NERVE - SMF

	// TTimo - prefix for text that shows up in console but not in notify
	// backported from RTCW
	if ( !Q_strncmp( txt, "[skipnotify]", 12 ) ) {
		skipnotify = qtrue;
		txt += 12;
	}
	
	// for some demos we don't want to ever show anything on the console
	if ( con_noprint && con_noprint->integer ) {
		return;
	}

	if ( con_timestamps && con_timestamps->integer && !con.x ) {
		char	timeString[MAXPRINTMSG];

		switch ( con_timestamps->integer ) {
		case 1:	// time of day
			{
				qtime_t	time;

				Com_RealTime( &time );
			
				Com_sprintf( timeString, sizeof(timeString),
					S_COLOR_GREY70 "[" S_COLOR_DIRTYWH "%02d:%02d:%02d" S_COLOR_GREY70 "]: " S_COLOR_WHITE "%s",
					time.tm_hour, time.tm_min, time.tm_sec, txt );

				strcpy( txt, timeString );
			}
			break;
		case 2: // unformatted date+time stamp
			{
				qtime_t	time;

				Com_RealTime( &time );
			
				Com_sprintf( timeString, sizeof(timeString),
					S_COLOR_GREY70 "[" S_COLOR_DIRTYWH "%04d%02d%02d%02d%02d%02d" S_COLOR_GREY70 "]: " S_COLOR_WHITE "%s",
					time.tm_year+1900, time.tm_mon+1, time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec, txt );

				strcpy( txt, timeString );
			}
			break;
		case 3:		// server time
		default:	// match time w/ forced hours
			{
				int		msec = cl.serverTime;	//cls.realtime;
				int		mins, seconds, tens;

				seconds = msec / 1000;
				mins = seconds / 60;
				seconds -= mins * 60;
				tens = seconds / 10;
				seconds -= tens * 10;

				if ( ((mins / 60) > 0) || con_timestamps->integer > 3 ) {
					int hours, tenm = 0;

					hours = mins / 60;
					if ( hours ) {
						mins -= mins * 60;
						tenm = mins / 10;
						mins -= tenm * 10;
					}

					Com_sprintf( timeString, sizeof(timeString),
						S_COLOR_GREY70 "[" S_COLOR_DIRTYWH "%02d:%i%i:%i%i" S_COLOR_GREY70 "]: " S_COLOR_WHITE "%s",
						hours, tenm, mins, tens, seconds, txt );
				} else {
					Com_sprintf( timeString, sizeof(timeString),
						S_COLOR_GREY70 "[" S_COLOR_DIRTYWH "%i:%i%i" S_COLOR_GREY70 "]: " S_COLOR_WHITE "%s",
						mins, tens, seconds, txt );
				}

				strcpy( txt, timeString );
			}
			break;
		}

		//con.skip[con.current % NUM_CON_TIMES] += strlen( timeString );
		//txt += strlen( timeString );
	}

	if ( !con.initialized ) {
		con.color[0] = 
		con.color[1] = 
		con.color[2] =
		con.color[3] = 1.0f;
		con.linewidth = -1;
		//Con_Refresh(qfalse);
		Con_CheckResize();
		con.initialized = qtrue;
	}

	color = ColorIndex(COLOR_WHITE);

	while ( (c = *((unsigned char *) txt)) != 0 ) {
		if ( Q_IsColorString( txt ) ) {
			color = ColorIndex( *(txt+1) );
			txt += 2;
			continue;
		}

		// count word length
		for (l=0 ; l< con.linewidth ; l++) {
			if ( txt[l] <= ' ') {
				break;
			}

		}

		// word wrap
		if (l != con.linewidth && (con.x + l >= con.linewidth) ) {
			Con_Linefeed(skipnotify);

		}

		txt++;

		switch (c)
		{
		case '\n':
			Con_Linefeed (skipnotify);
			break;
		case '\r':
			con.x = 0;
			break;
		default:	// display character and advance
			y = con.current % con.totallines;
			con.text[y*con.linewidth+con.x] = (color << 8) | c;
			con.x++;
			if(con.x >= con.linewidth)
				Con_Linefeed(skipnotify);
			break;
		}
	}


	// mark time for transparent overlay
	if (con.current >= 0) {
		// NERVE - SMF
		if ( skipnotify ) {
			prev = con.current % NUM_CON_TIMES - 1;
			if ( prev < 0 )
				prev = NUM_CON_TIMES - 1;
			con.times[prev] = 0;
		}
		else
		// -NERVE - SMF
			con.times[con.current % NUM_CON_TIMES] = cls.realtime;
	}
}


/*
==============================================================================

DRAWING

==============================================================================
*/


/*
================
CL_FadeColor
================
*/
float *CL_FadeColor( const int startMsec, const int totalMsec, const int fadeTime ) {
	static vec4_t		color;
	int			t;

	if ( !startMsec ) {
		return NULL;
	}

	t = cls.realtime - startMsec;

	if ( t >= totalMsec ) {
		return NULL;
	}

	// fade out
	if ( totalMsec - t < fadeTime ) {
		color[3] = ( totalMsec - t ) * 1.0/fadeTime;
	} else {
		color[3] = 1.0;
	}
	color[0] = color[1] = color[2] = 1;

	return color;
}


/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
static void Con_DrawNotify( void ) {
	int		x;
	float	y;
	short	*text;
	int		i;
	int		time;
	int		currentColor;
	//float	*rgba;
	float	*alpha;
	vec4_t	hcolor;
	float	scale = con_notifyscale->value;
	
	if ( con_notifytime->value <= 0 ) return;

	currentColor = COLOR_WHITE;
	memcpy( hcolor, g_color_table[currentColor], sizeof(hcolor) );

	y = con_notifyY->integer;
	SCR_AdjustFrom640( NULL, &y, NULL, &scale );

	if ( y + SMALLCHAR_HEIGHT*scale*con_notifylines->integer > cls.glconfig.vidHeight ) {
		y = cls.glconfig.vidHeight - SMALLCHAR_HEIGHT*scale*con_notifylines->integer;
	}
	if ( y < 0 ) y = 0;

	for ( i = con.current-con_notifylines->integer ; i <= con.current ; i++ ) {
		if ( i < 0 )
			continue;
		time = con.times[i % NUM_CON_TIMES];
		if ( time == 0 )
			continue;
		//time = cls.realtime - time;
		if ( time > con_notifytime->value*1000 + cls.realtime )
			continue;
		text = con.text + (i % con.totallines)*con.linewidth;

		if ( cl.snap.ps.pm_type != PM_INTERMISSION && Key_GetCatcher() & (KEYCATCH_UI | KEYCATCH_CGAME) ) {
			continue;
		}
		
		alpha = CL_FadeColor(time, con_notifytime->integer*1000, con_notifyfadetime->integer);
		if ( !alpha ) continue;
		hcolor[3] = alpha[3];
		re.SetColor( hcolor );

		for ( x = 0 ; x < con.linewidth ; x++ ) {
			if ( (text[x] & 0xff) == ' ' ) continue;

			if ( (text[x] >> 8) != currentColor ) {
				currentColor = ( text[x] >> 8 );

				memcpy( hcolor, g_color_table[currentColor], sizeof(hcolor) );
				alpha = CL_FadeColor(time, con_notifytime->integer*1000, con_notifyfadetime->integer);
				if ( !alpha ) continue;
				hcolor[3] = alpha[3];
				re.SetColor( hcolor );
			}
			SCR_DrawScaledChar( cl_conXOffset->integer + con.xadjust + (float)(x+1)*SMALLCHAR_WIDTH*scale, y, text[x]& 0xff, scale, qfalse );
		}

		y += SMALLCHAR_HEIGHT*scale;
		re.SetColor( NULL );
	}
}


/*
================
Con_DrawChatLine

Draws the messagemode text boxes
================
*/
//nql
static void Con_DrawChatLine( void ) {
	const char	*s = "";
	float		scale = 1.0;

	if ( Key_GetCatcher() & (KEYCATCH_UI|KEYCATCH_CGAME) ) return;

	scale = msgmode_scale->value;
	SCR_AdjustFrom640( NULL, NULL, NULL, &scale );

	if ( Key_GetCatcher() & KEYCATCH_MESSAGE ) {
		vec4_t	color;
		float	x, y, h, w;
		float	sw, sh;
		int		skip = 0;
		float	spacer = 2;

		x = 6;
		y = msgmode_Y->integer;	// ? 58 : 410;
		w = cls.glconfig.vidWidth - ( x * 2 );
		h = 22 * msgmode_scale->value;
		if ( y + h > SCREEN_HEIGHT )
			y = SCREEN_HEIGHT - h;

		// draw the box
		//VectorCopy( colorCoal, color );
		memcpy( color, conback_colors[con_backcolor->integer-1], sizeof(color) );
		color[3] = msgmode_opacity->value;	//0.75f;
		SCR_FillRect( x, y, w, h, color );

		SCR_AdjustFrom640( &x, &y, &w, &h );

		SCR_AdjustFrom640( NULL, NULL, &spacer, NULL );

		// draw the chat line

		// chat type header
		switch ( chat_type ) {
		case CHATT_TEAM:	s = "say_team: "; break;
		case CHATT_TARGET:	s = "tell_target: "; break;
		case CHATT_ATTACK:	s = "tell_attacker: "; break;
		default:			s = "say: "; break;	//CHATT_NORMAL
		}
		skip = strlen(s);

		sw = SMALLCHAR_WIDTH*scale;
		sh = SMALLCHAR_HEIGHT*scale;

		//x += sw;
		x += spacer;
		y += (h - sh)/2;

		//SCR_DrawSmallStringExt( x, y, s, colorGold, qfalse, qfalse );
		SCR_DrawScaledString( x, y, s, colorGold, qfalse, qfalse, scale, qfalse );
		
		// input text
		Field_Draw( &chatField, x + (skip * sw), y,
			w - (skip * sw) - spacer, qtrue, qtrue, scale, colorGold );
	}
}
//-nql


/*
================
Con_DrawInput

Draw the editline after a ] prompt
================
*/
static void Con_DrawInput( const float scale, const float sw, const float sh ) {
	float y;

	if ( clc.state != CA_DISCONNECTED && !(Key_GetCatcher() & KEYCATCH_CONSOLE) )
		return;

	y = con.vislines - ( sh * 2.0f );

	re.SetColor( con.color );

	//SCR_DrawSmallChar( con.xadjust + 1 * SMALLCHAR_WIDTH, y, ']' );
	SCR_DrawScaledChar( con.xadjust + sw, y, ']', scale, qfalse );

	Field_Draw( &g_consoleField, con.xadjust + (2.0f * sw), y,
		/*SCREEN_WIDTH*/ cls.glconfig.vidWidth - (3.0f * sw), qtrue, qtrue, scale, colorWhite );
}


/*
================
Con_DrawSolidConsole

Draws the console with the solid background
================
*/
static void Con_DrawSolidConsole( const float frac ) {
	int		i, j, rows, row, lines;
	int		currentColor;
	float	y;
	float	scale, sh, sw, versionh, versionw, adjust;
	short	*text;
	vec4_t	hcolor;
	vec4_t	cvarColor;
	float	fade = 0;

	lines = cls.glconfig.vidHeight * frac;
	if ( lines <= 0 )
		return;

	if ( lines > cls.glconfig.vidHeight )
		lines = cls.glconfig.vidHeight;

	scale = Con_GetScale();
	SCR_AdjustFrom640( NULL, NULL, NULL, &scale );
	sw = SMALLCHAR_WIDTH * scale;
	sh = SMALLCHAR_HEIGHT * scale;

	adjust = 1.0f;
	SCR_AdjustFrom640( NULL, NULL, NULL, &adjust );

	// on wide screens, we will center the text
	con.xadjust = 0;
	SCR_AdjustFrom640( &con.xadjust, NULL, NULL, NULL );

	// draw the background
	y = frac * SCREEN_HEIGHT;
	if ( y < 1 ) {
		y = 0;
	} else {
//nql
		float	opacity = con_opacity->value;

		if ( opacity > 0 && con_fadefrac->value > 0 ) {
			float	fopac;

			fopac = (frac / con_height->value) / con_fadefrac->value;
			if ( fopac > 1.0 ) fopac = 1.0;

			switch ( con_fading->integer ) {
			case 1:	// fade in to opacity
				fade = opacity * fopac;

				if ( fade > opacity )
					fade = opacity;
				break;
			case 2:	// fade out to opacity
				fade = opacity * ( 1.0 - fopac );

				if ( fade < opacity )
					fade = opacity;
				break;
			default:	// no fading
				fade = opacity;
				break;
			}
		} else {
			fade = opacity;
		}

		if ( con_background->value ) {
			vec4_t	backCol;
			float	dfrac = con.displayFrac;

			if ( !dfrac ) dfrac = 0.1;

			VectorSet( backCol, 1, 1, 1 );
			backCol[3] = fade;

			// set a solid background underneath if no transparency
			if ( con_opacity->value >= 1.0 ) {
				SCR_FillRect( 0, 0, SCREEN_WIDTH, y, colorBlack );
				SCR_DrawPic( 0, 0, SCREEN_WIDTH, y, cls.consoleShader );
			} else if ( con_opacity->value > 0 ) {
				re.SetColor( backCol );
				SCR_DrawPicExt( 0, 0, SCREEN_WIDTH, y, 0, 1.0f - (frac / 0.5f), 1, 1, cls.consoleShader, qtrue );
				re.SetColor( NULL );
			}
		} else {
			vec4_t	backCol;

			
			memcpy( backCol, conback_colors[con_backcolor->integer-1], sizeof(backCol) );
			backCol[3] = fade;

			SCR_FillRect( 0, 0, SCREEN_WIDTH, y, backCol );
		}
//-nql
	}

	Vector4Copy( g_color_table[ColorIndex('A') + con_color->integer - 1], cvarColor );
	cvarColor[3] = fade / con_opacity->value;

	// draw horizontal line
	if ( con_breakerwidth->integer )
		SCR_FillRect( 0, y, SCREEN_WIDTH, con_breakerwidth->integer, cvarColor );

	// draw the version number
	re.SetColor( cvarColor );
	j = strlen( Q3_VERSION );
	versionw = SMALLCHAR_WIDTH;
	versionh = SMALLCHAR_HEIGHT;
	SCR_AdjustFrom640( NULL, NULL, NULL, &versionw );
	SCR_AdjustFrom640( NULL, NULL, NULL, &versionh );

	for ( i = 0 ; i < j ; i++ ) {
		/*
		SCR_DrawSmallChar( cls.glconfig.vidWidth - ( j - i + 1 ) * SMALLCHAR_WIDTH,
			lines - SMALLCHAR_HEIGHT, Q3_VERSION[x] );
			*/
		SCR_DrawScaledChar( cls.glconfig.vidWidth - (float)(j - i + 1) * versionw,
			lines - versionh - 2*adjust, Q3_VERSION[i], adjust, qfalse );
	}

	// draw the time
	if ( con_clock->integer ) {
		qtime_t		time;
		static char timeString[16];

		Com_RealTime( &time );

		switch ( con_clock->integer ) {
		case 1:		// 24-hour
			Com_sprintf( timeString, sizeof(timeString), "%02d:%02d%c%02d", time.tm_hour, time.tm_min, time.tm_sec%2 ? ':' : ' ', time.tm_sec );
			break;
		default:	// AM/PM
			Com_sprintf( timeString, sizeof(timeString),
				"%2d:%02d%c%02d %s", time.tm_hour - ((time.tm_hour <= 12) ? 0 : 12),
				time.tm_min, time.tm_sec%2 ? ':' : ' ', time.tm_sec, (time.tm_hour < 12) ? "AM" : "PM" );
			break;
		}

		j = strlen( timeString );

		for ( i = 0 ; i < j ; i++ ) {
			re.SetColor( cvarColor );
			SCR_DrawScaledChar( cls.glconfig.vidWidth - (float)(j - i + 1) * versionw,
				lines - versionh - 2*adjust - versionh, timeString[i], adjust, qfalse );
			re.SetColor( NULL );
		}
	}

	// draw the text
	con.vislines = lines;
	rows = ( lines-sw )/sw;		// rows of text to draw

	y = lines - ( sh*3 );

	// draw from the bottom up
	if ( con.display != con.current ) {
		// draw arrows to show the buffer is backscrolled
		re.SetColor( cvarColor );
		for ( i = 0 ; i < con.linewidth ; i += 4 )
			//SCR_DrawSmallChar( con.xadjust + (i+1)*SMALLCHAR_WIDTH, y, '^' );
			SCR_DrawScaledChar( con.xadjust + (float)(i+1)*sw, y, '^', scale, qfalse );
		y -= sh;
		rows--;
	}
	
	row = con.display;

	if ( con.x == 0 ) {
		row--;
	}

	currentColor = COLOR_WHITE;
	memcpy( hcolor, g_color_table[currentColor], sizeof(hcolor) );
	hcolor[3] = (float)(fade / con_opacity->value) * (float)(con_background->integer ? 1.0f : 0.8f);
	re.SetColor( hcolor );

	for ( i = 0 ; i < rows ; i++, y -= sh, row-- ) {
		if ( row < 0 )
			break;
		if ( con.current - row >= con.totallines ) {
			// past scrollback wrap point
			continue;	
		}

		text = con.text + ( row % con.totallines )*con.linewidth;

		for ( j = 0 ; j < con.linewidth ; j++ ) {
			if ( (text[(int)(j)] & 0xff) == ' ' ) {
				continue;
			}

			if ( (text[j] >> 8) != currentColor ) {
				currentColor = ( text[j] >> 8 );
				memcpy( hcolor, g_color_table[currentColor], sizeof(hcolor) );
				hcolor[3] = (float)(fade / con_opacity->value) * (float)(con_background->integer ? 1.0f : 0.8f);
				re.SetColor( hcolor );
			}

			//SCR_DrawSmallChar(  con.xadjust + (x+1)*SMALLCHAR_WIDTH, y, text[x] & 0xff );
			SCR_DrawScaledChar( con.xadjust + (float)(j+1)*sw, y, text[j] & 0xff, scale, qfalse );
		}
	}

	// draw the input prompt, user text, and cursor if desired
	Con_DrawInput( scale, sw, sh );

	re.SetColor( NULL );
}


/*
==================
Con_DrawConsole
==================
*/
void Con_DrawConsole( void ) {
	// check for console width changes from a vid mode change
	Con_Refresh(qfalse);
	Con_CheckResize();

	// if disconnected, render console full screen
	if ( clc.state == CA_DISCONNECTED ) {
		if ( !(Key_GetCatcher() & (KEYCATCH_UI | KEYCATCH_CGAME)) ) {
			Con_DrawSolidConsole( 1.0 );
			return;
		}
	}

	if ( con.displayFrac ) {
		Con_DrawSolidConsole( con.displayFrac );
	} else {
		// draw notify lines
		if ( clc.state == CA_ACTIVE ) {
			Con_DrawNotify();
//nql
			Con_DrawChatLine();
//-nql
		}
	}
}

//================================================================

/*
==================
Con_RunConsole

Scroll it up or down
==================
*/
void Con_RunConsole( void ) {
//nql
	float	height = con_height->value;

	if ( height < 0.1 ) height = 0.1;
	else if ( height > 1.0 ) height = 1.0;
//-nql

	// decide on the destination height of the console
	if ( Key_GetCatcher() & KEYCATCH_CONSOLE )
//nql
		con.finalFrac = height;
//-nql
	else
		con.finalFrac = 0;				// none visible

	if (con.finalFrac == con.displayFrac) return;
	
	// scroll towards the destination height
	if ( con.finalFrac < con.displayFrac ) {
//nql
		if ( con_speed->integer == 1000 ) {
			con.displayFrac = con.finalFrac;
		} else {
			con.displayFrac -= con_speed->value * cls.realFrametime * 0.001;
			if ( con.finalFrac > con.displayFrac )
				con.displayFrac = con.finalFrac;
		}
//-nql
	} else if ( con.finalFrac > con.displayFrac ) {
//nql
		if ( con_speed->integer == 1000 ) {
			con.displayFrac = con.finalFrac;
		} else {
			con.displayFrac += con_speed->value * cls.realFrametime * 0.001;
			if ( con.finalFrac < con.displayFrac )
				con.displayFrac = con.finalFrac;
		}
//-nql
	}

}


void Con_PageUp( void ) {
//nql
	int lines = con_linescroll->integer;
	if ( lines < 1 ) lines = 1;
	else if ( lines > 32 ) lines = 32;

	con.display -= lines;
//-nql
	if ( con.current - con.display >= con.totallines ) {
		con.display = con.current - con.totallines + 1;
	}
}

void Con_PageDown( void ) {
//nql
	int lines = con_linescroll->integer;
	if ( lines < 1 ) lines = 1;
	else if ( lines > 32 ) lines = 32;

	con.display += lines;
//-nql
	if ( con.display > con.current ) {
		con.display = con.current;
	}
}

void Con_Top( void ) {
	con.display = con.totallines;
	if ( con.current - con.display >= con.totallines ) {
		con.display = con.current - con.totallines + 1;
	}
}

void Con_Bottom( void ) {
	con.display = con.current;
}


void Con_Close( void ) {
	if ( !com_cl_running->integer ) return;

	Field_Clear( &g_consoleField );
	Con_ClearNotify();
	Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_CONSOLE );
	con.finalFrac = 0;				// none visible
	con.displayFrac = 0;
}
