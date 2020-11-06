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
// cmd.c -- Quake script command processing module

#include "q_shared.h"
#include "qcommon.h"

#define	MAX_CMD_BUFFER	32768	//16384
#define	MAX_CMD_LINE	1024

typedef struct {
	byte	*data;
	int		maxsize;
	int		cursize;
} cmd_t;

//alias
#define	MAX_ALIAS_NAME		32
#define	ALIAS_LOOP_COUNT	16

typedef struct cmdalias_s {
	struct	cmdalias_s *next;
	char	name[MAX_ALIAS_NAME];
	char	*value;
	char	*listval;
} cmdalias_t;

int			alias_count;		// for detecting runaway loops
cmdalias_t	*cmd_alias;
//-alias

int			cmd_wait;
cmd_t		cmd_text;
byte		cmd_text_buf[MAX_CMD_BUFFER];


//=============================================================================

/*
============
Cmd_Wait_f

Causes execution of the remainder of the command buffer to be delayed until
next frame.  This allows commands like:
bind g "cmd use rocket ; +attack ; wait ; -attack ; cmd use blaster"
============
*/
void Cmd_Wait_f( void ) {
	if ( Cmd_Argc() == 2 ) {
		cmd_wait = atoi( Cmd_Argv( 1 ) );
		if ( cmd_wait < 0 )
			cmd_wait = 1; // ignore the argument
	} else {
		cmd_wait = 1;
	}
}


/*
=============================================================================

						COMMAND BUFFER

=============================================================================
*/

/*
============
Cbuf_Init
============
*/
void Cbuf_Init (void)
{
	cmd_text.data = cmd_text_buf;
	cmd_text.maxsize = MAX_CMD_BUFFER;
	cmd_text.cursize = 0;
}

/*
============
Cbuf_AddText

Adds command text at the end of the buffer, does NOT add a final \n
============
*/
void Cbuf_AddText( const char *text ) {
	int		l;
	
	l = strlen (text);

	if (cmd_text.cursize + l >= cmd_text.maxsize)
	{
		Com_Printf ("Cbuf_AddText: overflow\n");
		return;
	}
	Com_Memcpy(&cmd_text.data[cmd_text.cursize], text, l);
	cmd_text.cursize += l;
}


/*
============
Cbuf_InsertText

Adds command text immediately after the current command
Adds a \n to the text
============
*/
void Cbuf_InsertText( const char *text ) {
	int		len;
	int		i;

	len = strlen( text ) + 1;
	if ( len + cmd_text.cursize > cmd_text.maxsize ) {
		Com_Printf( "Cbuf_InsertText overflowed\n" );
		return;
	}

	// move the existing command text
	for ( i = cmd_text.cursize - 1 ; i >= 0 ; i-- ) {
		cmd_text.data[ i + len ] = cmd_text.data[ i ];
	}

	// copy the new text in
	Com_Memcpy( cmd_text.data, text, len - 1 );

	// add a \n
	cmd_text.data[ len - 1 ] = '\n';

	cmd_text.cursize += len;
}


/*
============
Cbuf_ExecuteText
============
*/
void Cbuf_ExecuteText (int exec_when, const char *text)
{
	switch (exec_when)
	{
	case EXEC_NOW:
		if (text && strlen(text) > 0) {
			Com_DPrintf(S_COLOR_YELLOW "EXEC_NOW %s\n", text);
			Cmd_ExecuteString (text);
		} else {
			Cbuf_Execute();
			Com_DPrintf(S_COLOR_YELLOW "EXEC_NOW %s\n", cmd_text.data);
		}
		break;
	case EXEC_INSERT:
		Cbuf_InsertText (text);
		break;
	case EXEC_APPEND:
		Cbuf_AddText (text);
		break;
	default:
		Com_Error (ERR_FATAL, "Cbuf_ExecuteText: bad exec_when");
	}
}

/*
============
Cbuf_Execute
============
*/
void Cbuf_Execute (void)
{
	int		i;
	char	*text;
	char	line[MAX_CMD_LINE];
	int		quotes;

//alias
	alias_count = 0;		// don't allow infinite alias loops
//-alias

	// This will keep // style comments all on one line by not breaking on
	// a semicolon.  It will keep /* ... */ style comments all on one line by not
	// breaking it for semicolon or newline.
	qboolean in_star_comment = qfalse;
	qboolean in_slash_comment = qfalse;
	while (cmd_text.cursize)
	{
		if ( cmd_wait > 0 ) {
			// skip out while text still remains in buffer, leaving it
			// for next frame
			cmd_wait--;
			break;
		}

		// find a \n or ; line break or comment: // or /* */
		text = (char *)cmd_text.data;

		quotes = 0;
		for (i=0 ; i< cmd_text.cursize ; i++)
		{
			if (text[i] == '"')
				quotes++;

			if ( !(quotes&1)) {
				if (i < cmd_text.cursize - 1) {
					if (! in_star_comment && text[i] == '/' && text[i+1] == '/')
						in_slash_comment = qtrue;
					else if (! in_slash_comment && text[i] == '/' && text[i+1] == '*')
						in_star_comment = qtrue;
					else if (in_star_comment && text[i] == '*' && text[i+1] == '/') {
						in_star_comment = qfalse;
						// If we are in a star comment, then the part after it is valid
						// Note: This will cause it to NUL out the terminating '/'
						// but ExecuteString doesn't require it anyway.
						i++;
						break;
					}
				}
				if (! in_slash_comment && ! in_star_comment && text[i] == ';')
					break;
			}
			if (! in_star_comment && (text[i] == '\n' || text[i] == '\r')) {
				in_slash_comment = qfalse;
				break;
			}
		}

		if( i >= (MAX_CMD_LINE - 1)) {
			i = MAX_CMD_LINE - 1;
		}
				
		Com_Memcpy (line, text, i);
		line[i] = 0;
		
// delete the text from the command buffer and move remaining commands down
// this is necessary because commands (exec) can insert data at the
// beginning of the text buffer

		if (i == cmd_text.cursize)
			cmd_text.cursize = 0;
		else
		{
			i++;
			cmd_text.cursize -= i;
			memmove (text, text+i, cmd_text.cursize);
		}

// execute the command line

		Cmd_ExecuteString (line);		
	}
}


/*
==============================================================================

						SCRIPT COMMANDS

==============================================================================
*/


/*
===============
Cmd_Exec_f
===============
*/
void Cmd_Exec_f( void ) {
	qboolean quiet, cfg;
	union {
		char	*c;
		void	*v;
	} f;
	char	filename[MAX_QPATH];

	quiet = !Q_stricmp(Cmd_Argv(0), "execq");
	cfg = !Q_stricmp(Cmd_Argv(0), "execc");

	if ( Cmd_Argc () != 2 ) {
		Com_xPrintf( "Description", "Load a settings file%s", "", quiet ? " without notification" : "" );
		Com_xPrintf( "Usage", "exec%s", "<filename>", quiet ? "q" : cfg ? "c" : "" );
		return;
	}

	Q_strncpyz( filename, va("%s%s", CFGPATH, Cmd_Argv(1)), sizeof(filename) );
	COM_DefaultExtension( filename, sizeof(filename), ".cfg" );
	FS_ReadFile( filename, &f.v );
	if ( !f.c ) {
		if ( cfg && Q_stricmp(filename, CFGPATH Q3CONFIG_CFG) ) {
			Q_strncpyz( filename, Q3CONFIG_CFG, sizeof(filename) );
			COM_DefaultExtension( filename, sizeof(filename), ".cfg" );
			FS_ReadFile( filename, &f.v );

			if ( !f.c ) {
				Com_Printf( "Couldn't load settings file: %s\n", filename );
				return;
			}
		} else {
			Com_Printf( "Couldn't load settings file: %s\n", filename );
			return;
		}
	}

	if ( !quiet )
		Com_Printf( S_COLOR_GREY70 "Loading " S_COLOR_DIRTYWH "%s\n", filename );
	
	Cbuf_InsertText( f.c );

	FS_FreeFile( f.v );
}


/*
===============
Cmd_Vstr_f

Inserts the current value of a variable as command text
===============
*/
void Cmd_Vstr_f( void ) {
	char	*v;

	if (Cmd_Argc () != 2) {
		Com_Printf ("vstr <variablename> : execute a variable command\n");
		return;
	}

	v = Cvar_VariableString( Cmd_Argv( 1 ) );
	Cbuf_InsertText( va("%s\n", v ) );
}


/*
===============
Cmd_Echo_f

Just prints the rest of the line to the console
===============
*/
void Cmd_Echo_f (void)
{
	Com_Printf ("%s\n", Cmd_Args());
}

//alias
/*
============
Cmd_WriteAlias

Write aliases to config
============
*/
void Cmd_WriteAlias( fileHandle_t f ) {
	cmdalias_t	*a;

	FS_Printf( f, "unaliasall\n" );
	for( a = cmd_alias ; a ; a = a->next )
		FS_Printf( f, "alias %s \"%s\"\n", a->name, a->value );
}

/*
===============
Cmd_Alias_f

Creates a new command that executes a command string (possibly ; seperated)
===============
*/
qboolean alias_modified;
void Cmd_Alias_f (void) {
	cmdalias_t	*a;
	char		cmd[ 1024 ];
	int			i = 0, c;
	char		*s;

	if( Cmd_Argc() == 1 ) {
		Com_xPrintf( "Usage", "alias", "[name] \"command\"" );
		Com_Printf( S_COLOR_DIRTYWH "Alias List:\n" S_COLOR_WHITE );
		for( a = cmd_alias ; a ; a = a->next, i++ )
			Com_Printf( S_COLOR_WHITE "  %s" S_COLOR_GREY70 "\"%s\"\n", a->name, a->value );
		Com_Printf( S_COLOR_GREY70 "\n--------------------------------\n" );
		Com_Printf( S_COLOR_WHITE "%i " S_COLOR_GREY70 "alias%s listed\n\n", i, i != 1 ? "es" : "" );
		return;
	}

	s = Cmd_Argv( 1 );
	if( strlen(s) >= MAX_ALIAS_NAME ) {
		Com_xPrintf( "Error", "Alias name is too long", "" );
		return;
	}

	// if the alias already exists, reuse it
	for( a = cmd_alias ; a ; a = a->next ) {
		if( !strcmp( s, a->name ) ) {
			Z_Free( a->value );
			break;
		}
	}

	if( !a ) {
		a = Z_Malloc( sizeof( cmdalias_t ) );
		a->next = cmd_alias;
		cmd_alias = a;
	}
	strcpy( a->name, s );

// copy the rest of the command line
	cmd[ 0 ] = 0;		// start out with a null string
	c = Cmd_Argc();
	for( i=2 ; i< c ; i++ ) {
		strcat ( cmd, Cmd_Argv(i) );
		if( i != ( c - 1 ) )
			strcat( cmd, " " );
	}
	//a->listval = CopyString( cmd );

	//strcat( cmd, "\n" );
	a->value = CopyString( cmd );
	alias_modified = qtrue;
}



/*
===============
Cmd_UnAlias_f

Removes the specified alias
===============
*/
void Cmd_UnAlias_f (void) {
	cmdalias_t	*a;
	char		*s;

	if( Cmd_Argc() == 1 ) {
		Com_xPrintf( "Usage", "unalias", "\"command\"" );
		return;
	}

	s = Cmd_Argv( 1 );

	// if the alias exists, remove it
	for( a = cmd_alias ; a ; a = a->next ) {
		if( !strcmp( s, a->name ) ) {
			Z_Free( a->value );
			Z_Free( a->name );
			break;
		}
	}
}


/*
===============
Cmd_UnAliasAll_f

Removes all aliases
===============
*/
void Cmd_UnAliasAll_f (void) {
	cmdalias_t	*a;

	// if an alias exists, remove it
	for( a = cmd_alias ; a ; a = a->next ) {
		Z_Free( a->value );
		Z_Free( a->name );
		break;
	}
}
//-alias

/*
=============================================================================

					COMMAND EXECUTION

=============================================================================
*/

typedef struct cmd_function_s
{
	struct cmd_function_s	*next;
	char					*name;
	xcommand_t				function;
	completionFunc_t	complete;
} cmd_function_t;


static	int			cmd_argc;
static	char		*cmd_argv[MAX_STRING_TOKENS];		// points into cmd_tokenized
static	char		cmd_tokenized[BIG_INFO_STRING+MAX_STRING_TOKENS];	// will have 0 bytes inserted
static	char		cmd_cmd[BIG_INFO_STRING]; // the original command we received (no token processing)

static	cmd_function_t	*cmd_functions;		// possible commands to execute

/*
============
Cmd_Argc
============
*/
int		Cmd_Argc( void ) {
	return cmd_argc;
}

/*
============
Cmd_Argv
============
*/
char	*Cmd_Argv( int arg ) {
	if ( (unsigned)arg >= cmd_argc ) {
		return "";
	}
	return cmd_argv[arg];	
}

/*
============
Cmd_ArgvBuffer

The interpreted versions use this because
they can't have pointers returned to them
============
*/
void	Cmd_ArgvBuffer( int arg, char *buffer, int bufferLength ) {
	Q_strncpyz( buffer, Cmd_Argv( arg ), bufferLength );
}


/*
============
Cmd_Args

Returns a single string containing argv(1) to argv(argc()-1)
============
*/
char	*Cmd_Args( void ) {
	static	char		cmd_args[MAX_STRING_CHARS];
	int		i;

	cmd_args[0] = 0;
	for ( i = 1 ; i < cmd_argc ; i++ ) {
		strcat( cmd_args, cmd_argv[i] );
		if ( i != cmd_argc-1 ) {
			strcat( cmd_args, " " );
		}
	}

	return cmd_args;
}

/*
============
Cmd_Args

Returns a single string containing argv(arg) to argv(argc()-1)
============
*/
char *Cmd_ArgsFrom( int arg ) {
	static	char		cmd_args[BIG_INFO_STRING];
	int		i;

	cmd_args[0] = 0;
	if (arg < 0)
		arg = 0;
	for ( i = arg ; i < cmd_argc ; i++ ) {
		strcat( cmd_args, cmd_argv[i] );
		if ( i != cmd_argc-1 ) {
			strcat( cmd_args, " " );
		}
	}

	return cmd_args;
}

/*
============
Cmd_ArgsBuffer

The interpreted versions use this because
they can't have pointers returned to them
============
*/
void	Cmd_ArgsBuffer( char *buffer, int bufferLength ) {
	Q_strncpyz( buffer, Cmd_Args(), bufferLength );
}

/*
============
Cmd_Cmd

Retrieve the unmodified command string
For rcon use when you want to transmit without altering quoting
https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=543
============
*/
char *Cmd_Cmd(void)
{
	return cmd_cmd;
}

/*
   Replace command separators with space to prevent interpretation
   This is a hack to protect buggy qvms
   https://bugzilla.icculus.org/show_bug.cgi?id=3593
   https://bugzilla.icculus.org/show_bug.cgi?id=4769
*/

void Cmd_Args_Sanitize(void)
{
	int i;

	for(i = 1; i < cmd_argc; i++)
	{
		char *c = cmd_argv[i];
		
		if(strlen(c) > MAX_CVAR_VALUE_STRING - 1)
			c[MAX_CVAR_VALUE_STRING - 1] = '\0';
		
		while ((c = strpbrk(c, "\n\r;"))) {
			*c = ' ';
			++c;
		}
	}
}


/*
======================
Com_MacroExpandString

From Quake II
======================
*/
char *Cmd_MacroExpandString( char *text ) {
	int		i, j, count, len;
	qboolean	inquote;
	char	*scan;
	static	char	expanded[MAX_STRING_CHARS];
	char	temporary[MAX_STRING_CHARS];
	char	*token, *start;

	inquote = qfalse;
	scan = text;

	len = strlen (scan);
	if (len >= MAX_STRING_CHARS)
	{
		Com_Printf ("Line exceeded %i chars, discarded.\n", MAX_STRING_CHARS);
		return NULL;
	}

	count = 0;

	for (i=0 ; i<len ; i++)
	{
		if (scan[i] == '"')
			inquote ^= 1;
		if (inquote)
			continue;	// don't expand inside quotes
		if (scan[i] != '$')
			continue;
		// scan out the complete macro
		start = scan+i+1;
		token = COM_Parse (&start);
		if (!start)
			continue;
	
		token = Cvar_VariableString (token);

		j = strlen(token);
		len += j;
		if (len >= MAX_STRING_CHARS)
		{
			Com_Printf ("Expanded line exceeded %i chars, discarded.\n", MAX_STRING_CHARS);
			return NULL;
		}

		strncpy (temporary, scan, i);
		strcpy (temporary+i, token);
		strcpy (temporary+i+j, start);

		strcpy (expanded, temporary);
		scan = expanded;
		i--;

		if (++count == 100)
		{
			Com_Printf ("Macro expansion loop, discarded.\n");
			return NULL;
		}
	}

	if (inquote)
	{
		Com_Printf ("Line has unmatched quote, discarded.\n");
		return NULL;
	}

	return scan;
}

/*
============
Cmd_TokenizeString

Parses the given string into command line tokens.
The text is copied to a seperate buffer and 0 characters
are inserted in the apropriate place, The argv array
will point into this temporary buffer.
============
*/
// NOTE TTimo define that to track tokenization issues
//#define TKN_DBG
static void Cmd_TokenizeString2( const char *text_in, qboolean ignoreQuotes, qboolean macroExpand ) {
	char	*text;
	char	*textOut;

#ifdef TKN_DBG
  // FIXME TTimo blunt hook to try to find the tokenization of userinfo
  Com_DPrintf("Cmd_TokenizeString: %s\n", text_in);
#endif

	// clear previous args
	cmd_argc = 0;

	if ( !text_in ) {
		return;
	}
	
	Q_strncpyz( cmd_cmd, text_in, sizeof(cmd_cmd) );
	text = (char *)text_in;

	// macro expand the text
	if ( macroExpand )
		text = Cmd_MacroExpandString( text );
	if ( !text )
		return;

	textOut = cmd_tokenized;

	while ( 1 ) {
		if ( cmd_argc == MAX_STRING_TOKENS ) {
			return;			// this is usually something malicious
		}

		while ( 1 ) {
			// skip whitespace
			while ( *text && *text <= ' ' ) {
				text++;
			}
			if ( !*text ) {
				return;			// all tokens parsed
			}

			// skip // comments
			if ( text[0] == '/' && text[1] == '/' ) {
				return;			// all tokens parsed
			}

			// skip /* */ comments
			if ( text[0] == '/' && text[1] =='*' ) {
				while ( *text && ( text[0] != '*' || text[1] != '/' ) ) {
					text++;
				}
				if ( !*text ) {
					return;		// all tokens parsed
				}
				text += 2;
			} else {
				break;			// we are ready to parse a token
			}
		}

		// handle quoted strings
    // NOTE TTimo this doesn't handle \" escaping
		if ( !ignoreQuotes && *text == '"' ) {
			cmd_argv[cmd_argc] = textOut;
			cmd_argc++;
			text++;
			while ( *text && *text != '"' ) {
				*textOut++ = *text++;
			}
			*textOut++ = 0;
			if ( !*text ) {
				return;		// all tokens parsed
			}
			text++;
			continue;
		}

		// regular token
		cmd_argv[cmd_argc] = textOut;
		cmd_argc++;

		// skip until whitespace, quote, or command
		while ( *text > ' ' ) {
			if ( !ignoreQuotes && text[0] == '"' ) {
				break;
			}

			if ( text[0] == '/' && text[1] == '/' ) {
				break;
			}

			// skip /* */ comments
			if ( text[0] == '/' && text[1] =='*' ) {
				break;
			}

			*textOut++ = *text++;
		}

		*textOut++ = 0;

		if ( !*text ) {
			return;		// all tokens parsed
		}
	}
	
}

/*
============
Cmd_TokenizeString
============
*/
void Cmd_TokenizeString( const char *text_in ) {
	Cmd_TokenizeString2( text_in, qfalse, qfalse );
}

/*
============
Cmd_TokenizeStringIgnoreQuotes
============
*/
void Cmd_TokenizeStringIgnoreQuotes( const char *text_in ) {
	Cmd_TokenizeString2( text_in, qtrue, qfalse );
}

/*
============
Cmd_FindCommand
============
*/
cmd_function_t *Cmd_FindCommand( const char *cmd_name )
{
	cmd_function_t *cmd;
	for( cmd = cmd_functions; cmd; cmd = cmd->next )
		if( !Q_stricmp( cmd_name, cmd->name ) )
			return cmd;
	return NULL;
}

/*
============
Cmd_AddCommand
============
*/
void	Cmd_AddCommand( const char *cmd_name, xcommand_t function ) {
	cmd_function_t	*cmd;
	
	// fail if the command already exists
	if( Cmd_FindCommand( cmd_name ) )
	{
		// allow completion-only commands to be silently doubled
		if( function != NULL )
			Com_Printf( "Cmd_AddCommand: %s already defined\n", cmd_name );
		return;
	}

	// use a small malloc to avoid zone fragmentation
	cmd = S_Malloc (sizeof(cmd_function_t));
	cmd->name = CopyString( cmd_name );
	cmd->function = function;
	cmd->complete = NULL;
	cmd->next = cmd_functions;
	cmd_functions = cmd;
}

/*
============
Cmd_SetCommandCompletionFunc
============
*/
void Cmd_SetCommandCompletionFunc( const char *command, completionFunc_t complete ) {
	cmd_function_t	*cmd;

	for( cmd = cmd_functions; cmd; cmd = cmd->next ) {
		if( !Q_stricmp( command, cmd->name ) ) {
			cmd->complete = complete;
		}
	}
}

/*
============
Cmd_RemoveCommand
============
*/
void	Cmd_RemoveCommand( const char *cmd_name ) {
	cmd_function_t	*cmd, **back;

	back = &cmd_functions;
	while( 1 ) {
		cmd = *back;
		if ( !cmd ) {
			// command wasn't active
			return;
		}
		if ( !strcmp( cmd_name, cmd->name ) ) {
			*back = cmd->next;
			if (cmd->name) {
				Z_Free(cmd->name);
			}
			Z_Free (cmd);
			return;
		}
		back = &cmd->next;
	}
}

/*
============
Cmd_RemoveCommandSafe

Only remove commands with no associated function
============
*/
void Cmd_RemoveCommandSafe( const char *cmd_name )
{
	cmd_function_t *cmd = Cmd_FindCommand( cmd_name );

	if( !cmd )
		return;
	if( cmd->function )
	{
		Com_Error( ERR_DROP, "Restricted source tried to remove "
			"system command \"%s\"", cmd_name );
		return;
	}

	Cmd_RemoveCommand( cmd_name );
}

/*
============
Cmd_CommandCompletion
============
*/
void	Cmd_CommandCompletion( void(*callback)(const char *s) ) {
	cmd_function_t	*cmd;
//alias
	cmdalias_t		*a;
//-alias
	
	for (cmd=cmd_functions ; cmd ; cmd=cmd->next) {
		callback( cmd->name );
	}

//alias
	for ( a = cmd_alias ; a ; a = a->next ) {
		callback( a->name );
	}
//-alias
}

/*
============
Cmd_CompleteArgument
============
*/
void Cmd_CompleteArgument( const char *command, char *args, int argNum ) {
	cmd_function_t	*cmd;

	for( cmd = cmd_functions; cmd; cmd = cmd->next ) {
		if( !Q_stricmp( command, cmd->name ) && cmd->complete ) {
			cmd->complete( args, argNum );
		}
	}
}


/*
============
Cmd_ExecuteString

A complete command line has been parsed, so try to execute it
============
*/
void	Cmd_ExecuteString( const char *text ) {	
	cmd_function_t	*cmd, **prev;
//alias
	cmdalias_t		*acd, **aprv;
//-alias

	// execute the command line
	Cmd_TokenizeString( text );		
	if ( !Cmd_Argc() ) {
		return;		// no tokens
	}

	// check registered command functions	
	for ( prev = &cmd_functions ; *prev ; prev = &cmd->next ) {
		cmd = *prev;
		if ( !Q_stricmp( cmd_argv[0],cmd->name ) ) {
			// rearrange the links so that the command will be
			// near the head of the list next time it is used
			*prev = cmd->next;
			cmd->next = cmd_functions;
			cmd_functions = cmd;

			// perform the action
			if ( !cmd->function ) {
				// let the cgame or game handle it
				break;
			} else {
				cmd->function ();
			}
			return;
		}
	}

//alias
	// check alias
	for ( aprv = &cmd_alias ; *aprv ; aprv = &acd->next ) {
		acd = *aprv;
		if ( !Q_stricmp ( cmd_argv[0], acd->name ) ) {
			// rearrange the links so that the command will be
			// near the head of the list next time it is used
			*aprv = acd->next;
			acd->next = cmd_alias;
			cmd_alias = acd;

			if ( ++alias_count == ALIAS_LOOP_COUNT ) {
				Com_Printf ("ALIAS_LOOP_COUNT\n");
				return;
			}
			Cbuf_InsertText ( acd->value );

			return;
		}
	}
//-alias
	
	// check cvars
	if ( Cvar_Command() ) {
		return;
	}

	// check client game commands
	if ( com_cl_running && com_cl_running->integer && CL_GameCommand() ) {
		return;
	}

	// check server game commands
	if ( com_sv_running && com_sv_running->integer && SV_GameCommand() ) {
		return;
	}

	// check ui commands
	if ( com_cl_running && com_cl_running->integer && UI_GameCommand() ) {
		return;
	}

	// send it as a server command if we are connected
	// this will usually result in a chat message
	CL_ForwardCommandToServer ( text );
}

/*
============
Cmd_List_f
============
*/
void Cmd_List_f (void)
{
	cmd_function_t	*cmd;
	int				i;
	char			*match;

	if ( Cmd_Argc() > 1 ) {
		match = Cmd_Argv( 1 );
	} else {
		match = NULL;
	}

	i = 0;
	Com_Printf( "\n" );
	for ( cmd = cmd_functions ; cmd ; cmd=cmd->next ) {
		if ( match && !Com_Filter(match, cmd->name, qfalse) ) continue;

		Com_Printf( "%s\n", cmd->name );
		i++;
	}
	Com_Printf( S_COLOR_GREY70 "\n--------------------------------\n" );
	Com_Printf ( S_COLOR_WHITE "%i " S_COLOR_GREY70 "total commands\n\n", i );
}

/*
============
Cmd_BloomPreset_f
============
*/
typedef struct presetBloom_s {
	char	*name;
	char	*brightThreshold;
	char	*saturation;
	char	*intensity;
	char	*sceneSaturation;
	char	*sceneIntensity;
} presetBloom_t;

presetBloom_t	bloom_presetlist[] = {
	//	NAME				BT		 S		  I		   SS		SI
	{ "Default",			"0.125", "0.800", "0.750", "1.000", "1.000" },
	{ "Subtle",				"0.125", "0.750", "1.250", "0.750", "1.000" },
	{ "Moderate",			"0.125", "0.750", "1.500", "0.750", "1.250" },
	{ "Excessive",			"0.125", "0.750", "1.750", "0.750", "1.500" },
	{ "Dream Sequence",		"0.100", "0.750", "1.000", "1.000", "0.900" },
	{ "Desaturated",		"0.150", "0.500", "1.000", "0.333", "1.000" },
	{ "Oversaturated",		"0.125", "1.250", "1.000", "1.250", "1.000" },
	{ "Selective Grey",		"0.250", "1.250", "1.000", "0.125", "1.250" },
	{ "Metallic",			"0.0625","0.000", "2.000", "0.250", "1.250" }
};
const int bloom_presetcount = ARRAY_LEN( bloom_presetlist );

static int bloom_preset = 0;
void Cmd_BloomPreset_f( void ) {
	presetBloom_t	*p;
	int				num = atoi(Cmd_Argv(1));

	if ( num > 0 ) {
		bloom_preset = num-1;
	} else if ( !strcmp(Cmd_Argv(1), "-") ) {
		bloom_preset--;
	} else if ( !strcmp(Cmd_Argv(1), "+") ) {
		bloom_preset++;
	}	// otherwise just reload current preset

	if ( bloom_preset > bloom_presetcount-1 ) bloom_preset = 0;
	else if ( bloom_preset < 0 ) bloom_preset = bloom_presetcount-1;

	p = &bloom_presetlist[bloom_preset];

	Cvar_Set( "r_BloomBrightThreshold", p->brightThreshold );
	Cvar_Set( "r_BloomSaturation", p->saturation );
	Cvar_Set( "r_BloomIntensity", p->intensity );
	Cvar_Set( "r_BloomSceneSaturation", p->sceneSaturation );
	Cvar_Set( "r_BloomSceneIntensity", p->sceneIntensity );

	Com_Printf( S_COLOR_GREY70 "Bloom Preset (%i/%i): " S_COLOR_DIRTYWH "%s" S_COLOR_WHITE "\n",
		bloom_preset+1, bloom_presetcount, p->name );
}

/*
==================
Cmd_CompleteCfgName
==================
*/
void Cmd_CompleteCfgName( char *args, int argNum ) {
	if( argNum == 2 ) {
		Field_CompleteFilename( "", "cfg", qfalse, qtrue );
	}
}

/*
============
Cmd_Init
============
*/
void Cmd_Init (void) {
	Cmd_AddCommand( "cmdList",Cmd_List_f );
	Cmd_AddCommand( "exec",Cmd_Exec_f );
	Cmd_AddCommand( "execq",Cmd_Exec_f );
	Cmd_SetCommandCompletionFunc( "exec", Cmd_CompleteCfgName );
	Cmd_SetCommandCompletionFunc( "execq", Cmd_CompleteCfgName );
	Cmd_AddCommand( "vstr",Cmd_Vstr_f );
	Cmd_SetCommandCompletionFunc( "vstr", Cvar_CompleteCvarName );
	Cmd_AddCommand( "echo",Cmd_Echo_f );
	Cmd_AddCommand( "wait", Cmd_Wait_f );
//alias
	Cmd_AddCommand( "alias", Cmd_Alias_f );
	Cmd_AddCommand( "unalias", Cmd_UnAlias_f );
	Cmd_AddCommand( "unaliasall", Cmd_UnAliasAll_f );
//-alias
//nql
	Cmd_AddCommand( "execc", Cmd_Exec_f );	// exec q3config.cfg if specified file doesn't exist
	Cmd_SetCommandCompletionFunc( "execc", Cmd_CompleteCfgName );
	
	Cmd_AddCommand( "bloom_preset", Cmd_BloomPreset_f );
//-nql
}
