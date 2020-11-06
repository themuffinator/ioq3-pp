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
#include "keycodes.h"

typedef struct {
	qboolean	down;
	int			repeats;		// if > 1, it is autorepeating
	char		*binding;
} qkey_t;

extern	qboolean	key_overstrikeMode;
extern	qkey_t		keys[MAX_KEYS];

// NOTE TTimo the declaration of field_t and Field_Clear is now in qcommon/qcommon.h
void Field_KeyDownEvent( field_t *edit, int key );
void Field_CharEvent( field_t *edit, int ch );
//nql
void Field_Draw( field_t *edit, const float x, const float y, const float w, const qboolean showCursor, const qboolean noColorEscape, const float scale, const float *color );

#define		CHATT_NORMAL	1
#define		CHATT_TEAM		2
#define		CHATT_TARGET	3
#define		CHATT_ATTACK	4
//-nql
#define		COMMAND_HISTORY		32
extern	field_t	historyEditLines[COMMAND_HISTORY];

extern	field_t	g_consoleField;
extern	field_t	chatField;
extern	int				anykeydown;
//nql
//extern	qboolean	chat_team;
extern	int			chat_type;		// 1 = say, 2 = say_team, 3 = tell_target, 4 = tell_attacker
//-nql
extern	int			chat_playerNum;

void Key_WriteBindings( fileHandle_t f );
void Key_SetBinding( int keynum, const char *binding );
char *Key_GetBinding( int keynum );
qboolean Key_IsDown( int keynum );
qboolean Key_GetOverstrikeMode( void );
void Key_SetOverstrikeMode( qboolean state );
void Key_ClearStates( void );
int Key_GetKey(const char *binding);
