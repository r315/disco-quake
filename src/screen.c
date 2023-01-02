/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// screen.c -- master for refresh, status bar, console, chat, notify, etc

#include "quakedef.h"
#include "r_local.h"

typedef struct pcx_s
{
    char	manufacturer;
    char	version;
    char	encoding;
    char	bits_per_pixel;
    unsigned short	xmin,ymin,xmax,ymax;
    unsigned short	hres,vres;
    unsigned char	palette[48];
    char	reserved;
    char	color_planes;
    unsigned short	bytes_per_line;
    unsigned short	palette_type;
    char	filler[58];
    unsigned char	data;			// unbounded
} pcx_t;

typedef enum { CPY_ALL, CPY_TOP, CPY_SB } scrcopy_t;

// only the refresh window will be updated unless these variables are flagged 
static scrcopy_t	scr_copy;

static int			scr_con_current;	// console current scan lines
static int			scr_conlines;		// lines of console to display
static qboolean		scr_initialized;	// ready to draw

cvar_t				scr_viewsize	= {"scr_viewsize", "100", true};
cvar_t				scr_fov			= {"scr_fov", "90"};	// 10 - 170
static cvar_t		scr_centertime	= {"scr_centertime","2"};
static cvar_t		scr_conspeed	= {"scr_conspeed","300"};
static cvar_t		scr_showram		= {"scr_showram","1"};
static cvar_t		scr_showturtle	= {"scr_showturtle","0"};
static cvar_t		scr_showpause	= {"scr_showpause","1"};
static cvar_t		scr_printspeed	= {"scr_printspeed","8"};
static cvar_t		scr_showfps 	= {"scr_showfps","1"};

static qpic_t		*scr_ram;
static qpic_t		*scr_net;
static qpic_t		*scr_turtle;

static int			scr_clearnotify;	// set to 0 whenever notify text is drawn
static int			scr_clearconsole;

static qboolean		scr_enable;
static qboolean		scr_drawloading;
static float		scr_disabled_time;

vrect_t				scr_vrect;

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

float			scr_centertime_off;
static char		scr_centerstring[1024];
static float	scr_centertime_start;	// for slow victory printing
static int		scr_center_lines;
static int		scr_erase_lines;
static int		scr_erase_center;

static char		*scr_notifystring;
static qboolean	scr_drawdialog;

void SCR_ScreenShot_f (void);

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void SCR_CenterPrint (char *str)
{
	strncpy (scr_centerstring, str, sizeof(scr_centerstring)-1);
	scr_centertime_off = scr_centertime.value;
	scr_centertime_start = cl.time;

// count the number of lines for centering
	scr_center_lines = 1;
	while (*str)
	{
		if (*str == '\n')
			scr_center_lines++;
		str++;
	}
}

void SCR_EraseCenterString (void)
{
	int		y;

	if (scr_erase_center++ > vid.numpages)
	{
		scr_erase_lines = 0;
		return;
	}

	if (scr_center_lines <= 4)
		y = vid.height*0.35;
	else
		y = 48;

	Draw_TileClear (0, y,vid.width, 8*scr_erase_lines);
}

void SCR_DrawCenterString (void)
{
	char	*start;
	int		l;
	int		j;
	int		x, y;
	int		remaining;

// the finale prints the characters one at a time
	if (cl.intermission)
		remaining = scr_printspeed.value * (cl.time - scr_centertime_start);
	else
		remaining = 9999;

	scr_erase_center = 0;
	start = scr_centerstring;

	if (scr_center_lines <= 4)
		y = vid.height*0.35;
	else
		y = 48;

	do	
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.width - l*8)/2;
		for (j=0 ; j<l ; j++, x+=8)
		{
			Draw_Character (x, y, start[j]);	
			if (!remaining--)
				return;
		}
			
		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;		// skip the \n
	} while (1);
}

void SCR_CheckDrawCenterString (void)
{
	if (scr_center_lines > scr_erase_lines)
		scr_erase_lines = scr_center_lines;

	scr_centertime_off -= host_frametime;
	
	if (scr_centertime_off <= 0 && !cl.intermission)
		return;
	if (key_dest != key_game)
		return;

	SCR_DrawCenterString ();
}

//=============================================================================

/*
====================
CalcFov
====================
*/
float CalcFov (float fov_x, float width, float height)
{
        float   a;
        float   x;

        if (fov_x < 1 || fov_x > 179)
                Sys_Error ("Bad fov: %f", fov_x);

        x = width/tan(fov_x/360*M_PI);

        a = atan (height/x);

        a = a*360/M_PI;

        return a;
}

/*
=================
SCR_CalcRefdef

Must be called whenever vid changes
Internal use only
=================
*/
static void SCR_CalcRefdef (void)
{
	vrect_t		vrect;
	float		size;

	vid.recalc_refdef = false;

//========================================
	
// bound viewsize
	if (scr_viewsize.value < SCR_MIN_VIEWSIZE)
		Cvar_SetValue ("scr_viewsize", SCR_MIN_VIEWSIZE);
	if (scr_viewsize.value > SCR_MAX_VIEWSIZE)
		Cvar_SetValue ("scr_viewsize", SCR_MAX_VIEWSIZE);

// bound field of view
	if (scr_fov.value < SCR_MIN_FOV)
		Cvar_SetValue ("scr_fov", SCR_MIN_FOV);
	if (scr_fov.value > SCR_MAX_FOV)
		Cvar_SetValue ("scr_fov", SCR_MAX_FOV);

	r_refdef.fov_x = scr_fov.value;
	r_refdef.fov_y = CalcFov (r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height);

// intermission is always full screen	
	if (cl.intermission)
		size = SCR_MAX_VIEWSIZE;
	else
		size = scr_viewsize.value;

	if (size >= SCR_MAX_VIEWSIZE)
		sb_lines = 0;				// no status bar at all
	else if (size >= SCR_MAX_VIEWSIZE - 10)
		sb_lines = SBAR_HEIGHT;		// no inventory
	else
		sb_lines = SBAR_HEIGHT + 16 + 8;

// these calculations mirror those in R_Init() for r_refdef, but take no
// account of water warping
	vrect.x = 0;
	vrect.y = 0;
	vrect.width = vid.width;
	vrect.height = vid.height;

	R_SetVrect (&vrect, &scr_vrect, sb_lines);

// guard against going from one mode to another that's less than half the
// vertical resolution
	if (scr_con_current > vid.height)
		scr_con_current = vid.height;

// notify the refresh of the change
	R_ViewChanged (&vrect, sb_lines, vid.aspect);
}


void SCR_SetClearNotify (void)
{
	scr_clearnotify = 0;
}

/*
=================
SCR_Changed 

=================
*/
void SCR_Changed (void)
{
	vid.recalc_refdef = true;
}

/*
=================
SCR_ChangeViewSize

Used by menu to increase/decrease screen view size
=================
*/
void SCR_ChangeViewSize (int dir)
{
	Cvar_SetValue ("scr_viewsize", scr_viewsize.value + (dir * 10));
	vid.recalc_refdef = true;
}

/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
static void SCR_SizeUp_f (void)
{
	SCR_ChangeViewSize(1);
}


/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
static void SCR_SizeDown_f (void)
{
	SCR_ChangeViewSize(-1);
}

/*
==================
SCR_ViewSize_f

cvar bind command
==================
*/
static void SCR_ViewSize_f (void)
{
	Cvar_SetFromCommand("scr_viewsize");
	vid.recalc_refdef = true;
}

/*
==================
SCR_Fov_f

cvar bind command
==================
*/
static void SCR_Fov_f (void)
{
	Cvar_SetFromCommand("scr_fov");
	vid.recalc_refdef = true;
}

/*
=================
SCR_GetSize

Used by menu to get screen view size ratio
returns screen size between 0 and 1
=================
*/
float SCR_GetSize(void)
{
	return (scr_viewsize.value - SCR_MIN_VIEWSIZE) / (SCR_MAX_VIEWSIZE - SCR_MIN_VIEWSIZE);
}

//============================================================================

/*
==================
SCR_Init
==================
*/
void SCR_Init (void)
{	
	Cvar_RegisterVariable (&scr_fov);
	Cvar_RegisterVariable (&scr_viewsize);
	Cvar_RegisterVariable (&scr_conspeed);
	Cvar_RegisterVariable (&scr_showram);
	Cvar_RegisterVariable (&scr_showturtle);
	Cvar_RegisterVariable (&scr_showpause);
	Cvar_RegisterVariable (&scr_centertime);
	Cvar_RegisterVariable (&scr_printspeed);
	Cvar_RegisterVariable (&scr_showfps);

//
// register our commands
//
	Cmd_AddCommand ("screenshot",SCR_ScreenShot_f);
	Cmd_AddCommand ("sizeup",SCR_SizeUp_f);
	Cmd_AddCommand ("sizedown",SCR_SizeDown_f);
	Cmd_AddCommand ("viewsize", SCR_ViewSize_f);
	Cmd_AddCommand ("fov", SCR_Fov_f);

	scr_ram = Draw_PicFromWad ("ram");
	scr_net = Draw_PicFromWad ("net");
	scr_turtle = Draw_PicFromWad ("turtle");

	scr_initialized = true;
}



/*
==============
SCR_DrawRam
==============
*/
void SCR_DrawRam (void)
{
	if (!scr_showram.value)
		return;

	if (!r_cache_thrash)
		return;

	Draw_Pic (scr_vrect.x+32, scr_vrect.y, scr_ram);
}

/*
==============
SCR_DrawTurtle
==============
*/
void SCR_DrawTurtle (void)
{
	static int	count;
	
	if (!scr_showturtle.value)
		return;

	if (host_frametime < 0.1)
	{
		count = 0;
		return;
	}

	count++;
	if (count < 3)
		return;

	Draw_Pic (scr_vrect.x, scr_vrect.y, scr_turtle);
}

/*
==============
SCR_DrawNet
==============
*/
void SCR_DrawNet (void)
{
	if (realtime - cl.last_received_message < 0.3)
		return;
	if (cls.demoplayback)
		return;

	Draw_Pic (scr_vrect.x+64, scr_vrect.y, scr_net);
}

/*
==============
DrawPause
==============
*/
void SCR_DrawPause (void)
{
	qpic_t	*pic;

	if (!scr_showpause.value)		// turn off for screenshots
		return;

	if (!cl.paused)
		return;

	pic = Draw_CachePic ("gfx/pause.lmp");
	Draw_Pic ( (vid.width - pic->width)/2, 
		(vid.height - 48 - pic->height)/2, pic);
}



/*
==============
SCR_DrawLoading
==============
*/
void SCR_DrawLoading (void)
{
	qpic_t	*pic;

	if (!scr_drawloading)
		return;
		
	pic = Draw_CachePic ("gfx/loading.lmp");
	Draw_Pic ( (vid.width - pic->width)/2, 
		(vid.height - 48 - pic->height)/2, pic);
}



//=============================================================================


/*
==================
SCR_SetUpToDrawConsole
==================
*/
void SCR_SetUpToDrawConsole (void)
{
	Con_CheckResize ();
	
	if (scr_drawloading)
		return;		// never a console with loading plaque
		
// decide on the height of the console
	con_forcedup = !cl.worldmodel || cls.signon != SIGNONS;

	if (con_forcedup)
	{
		scr_conlines = vid.height;		// full screen
		scr_con_current = scr_conlines;
	}
	else if (key_dest == key_console)
		scr_conlines = vid.height/2;	// half screen
	else
		scr_conlines = 0;				// none visible
	
	if (scr_conlines < scr_con_current)
	{
		scr_con_current -= scr_conspeed.value*host_frametime;
		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;

	}
	else if (scr_conlines > scr_con_current)
	{
		scr_con_current += scr_conspeed.value*host_frametime;
		if (scr_conlines < scr_con_current)
			scr_con_current = scr_conlines;
	}

	if (scr_clearconsole++ < vid.numpages)
	{
		Draw_TileClear (0,(int)scr_con_current,vid.width, vid.height - (int)scr_con_current);
		Sbar_Changed ();
	}
	else if (scr_clearnotify++ < vid.numpages)
	{
		Draw_TileClear (0,0,vid.width, con_notifylines);
	}
	else
		con_notifylines = 0;
}
	
/*
==================
SCR_DrawConsole
==================
*/
void SCR_DrawConsole (void)
{
	if (scr_con_current)
	{	
		Con_DrawConsole (scr_con_current, true);
		scr_clearconsole = 0;
	}
	else
	{
		if (key_dest == key_game || key_dest == key_message)
			Con_DrawNotify ();	// only draw notify in game
	}
}


/* 
============================================================================== 
 
						SCREEN SHOTS 
 
============================================================================== 
*/ 
 


/* 
============== 
WritePCXfile 
============== 
*/ 
static void WritePCXfile (char *filename, byte *data, int width, int height,
	int rowbytes, byte *palette) 
{
	int		i, j, length;
	pcx_t	*pcx;
	byte		*pack;
	  
	pcx = Hunk_TempAlloc (width*height*2+1000);
	if (pcx == NULL)
	{
		Con_Printf("SCR_ScreenShot_f: not enough memory\n");
		return;
	} 
 
	pcx->manufacturer = 0x0a;	// PCX id
	pcx->version = 5;			// 256 color
 	pcx->encoding = 1;		// uncompressed
	pcx->bits_per_pixel = 8;		// 256 color
	pcx->xmin = 0;
	pcx->ymin = 0;
	pcx->xmax = LittleShort((short)(width-1));
	pcx->ymax = LittleShort((short)(height-1));
	pcx->hres = LittleShort((short)width);
	pcx->vres = LittleShort((short)height);
	Q_memset (pcx->palette,0,sizeof(pcx->palette));
	pcx->color_planes = 1;		// chunky image
	pcx->bytes_per_line = LittleShort((short)width);
	pcx->palette_type = LittleShort(2);		// not a grey scale
	Q_memset (pcx->filler,0,sizeof(pcx->filler));

// pack the image
	pack = &pcx->data;
	
	for (i=0 ; i<height ; i++)
	{
		for (j=0 ; j<width ; j++)
		{
			if ( (*data & 0xc0) != 0xc0)
				*pack++ = *data++;
			else
			{
				*pack++ = 0xc1;
				*pack++ = *data++;
			}
		}

		data += rowbytes - width;
	}
			
// write the palette
	*pack++ = 0x0c;	// palette ID byte
	for (i=0 ; i<768 ; i++)
		*pack++ = *palette++;
		
// write output file 
	length = pack - (byte *)pcx;
	COM_WriteFile (filename, pcx, length);
} 
 


/* 
================== 
SCR_ScreenShot_f
================== 
*/  
void SCR_ScreenShot_f (void) 
{ 
	int     i; 
	char		pcxname[80]; 
	char		checkname[MAX_OSPATH];

// 
// find a file name to save it to 
// 
	strcpy(pcxname,"quake00.pcx");
		
	for (i=0 ; i<=99 ; i++) 
	{ 
		pcxname[5] = i/10 + '0'; 
		pcxname[6] = i%10 + '0'; 
		sprintf (checkname, "%s/%s", com_gamedir, pcxname);
		if (Sys_FileTime(checkname) == -1)
			break;	// file doesn't exist
	} 
	if (i==100) 
	{
		Con_Printf ("SCR_ScreenShot_f: Couldn't create a PCX file\n"); 
		return;
 	}

// 
// save the pcx file 
// 


	WritePCXfile (pcxname, vid.buffer, vid.width, vid.height, vid.rowbytes,
				  host_basepal);


	Con_Printf ("Wrote %s\n", pcxname);
} 


//=============================================================================


/*
===============
SCR_BeginLoadingPlaque

================
*/
void SCR_BeginLoadingPlaque (void)
{
	S_StopAllSounds (true);

	if (cls.state != ca_connected)
		return;
	if (cls.signon != SIGNONS)
		return;
	
// redraw with no console and the loading plaque
	Con_ClearNotify ();
	scr_centertime_off = 0;
	scr_con_current = 0;

	scr_drawloading = true;
	Sbar_Changed ();
	SCR_UpdateScreen ();
	scr_drawloading = false;

	scr_enable = false;
	scr_disabled_time = realtime;
}

/*
===============
SCR_EndLoadingPlaque

================
*/
void SCR_EndLoadingPlaque (void)
{
	scr_enable = true;
	Con_ClearNotify ();
}

//=============================================================================

void SCR_DrawNotifyString (void)
{
	char	*start;
	int		l;
	int		j;
	int		x, y;

	start = scr_notifystring;

	y = vid.height*0.35;

	do	
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.width - l*8)/2;
		for (j=0 ; j<l ; j++, x+=8)
			Draw_Character (x, y, start[j]);	
			
		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;		// skip the \n
	} while (1);
}

/*
==================
SCR_ModalMessage

Displays a text string in the center of the screen and waits for a Y or N
keypress.  
==================
*/
int SCR_ModalMessage (char *text)
{
	if (cls.state == ca_dedicated)
		return true;

	scr_notifystring = text;
 
// draw a fresh screen
	scr_drawdialog = true;
	SCR_UpdateScreen ();
	scr_drawdialog = false;
	
	S_ClearBuffer ();		// so dma doesn't loop current sound

	do
	{
		key_count = -1;		// wait for a key down and up
		Sys_SendKeyEvents ();
	} while (key_lastpress != 'y' && key_lastpress != 'n' && key_lastpress != K_ESCAPE);

	SCR_UpdateScreen ();

	return key_lastpress == 'y';
}


//=============================================================================

/*
===============
SCR_BringDownConsole

Brings the console down and fades the palettes back to normal
================
*/
void SCR_BringDownConsole (void)
{
	int		i;
	
	scr_centertime_off = 0;
	
	for (i=0 ; i<20 && scr_conlines != scr_con_current ; i++)
		SCR_UpdateScreen ();

	cl.cshifts[0].percent = 0;		// no area contents palette on next frame
	VID_SetPalette (host_basepal);
}

void SCR_DrawFrameCount(void) {
	int cx;
	int cy = 10;
	char data[20];

	int len = sprintf(data, "%d", host_framecount);

	cx = vid.width - (len * 8);

	for (int i = 0; i < len; i++) {
		Draw_Character(cx, cy, data[i]);
		cx += 8;
	}
}


void SCR_DrawFps(void) {
	static float last_time = 0;
	static int fps = 0, fps_count = 0;

	// update every second
	if ((Sys_FloatTime() - last_time) > 1.0f) {
		last_time = Sys_FloatTime();
		fps = fps_count;
		fps_count = 0;
	}
	else {
		fps_count++;
	}

	if(scr_showfps.value){	
		int cx;
		int cy = 0;
		char data[20];	
		int len = sprintf(data, "%d", fps);
		
		cx = vid.width - (len * 8);

		for (int i = 0; i < len; i++) {
			Draw_Character(cx, cy, data[i]);
			cx += 8;
		}
	}
}
/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.

WARNING: be very careful calling this from elsewhere, because the refresh
needs almost the entire 256k of stack space!
==================
*/
void SCR_UpdateScreen (void)
{
	vrect_t		vrect;

	if (!scr_enable)
	{
		if (realtime - scr_disabled_time > 60)
		{
			scr_enable = true;
			Con_Printf ("load failed.\n");
		}
		else
			return;
	}

	//
	// Update only if requirements are met
	//

	if (cls.state == ca_dedicated || !scr_initialized || !con_initialized)
		return;

	//
	// Check if screen size has changed
	//

	if (vid.recalc_refdef)
	{
		// something changed, so reorder the screen
		SCR_CalcRefdef ();

		// force the status bar to redraw
		Sbar_Changed ();
		scr_copy = CPY_ALL;
	}

	//
	// If something requested full update, then fill in status bar backgound
	//

	if (scr_copy == CPY_ALL) {
		Draw_TileClear(0, vid.height - sb_lines, vid.width, sb_lines);
	}

	//
	// do 3D refresh drawing, and then update the screen
	//

	SCR_SetUpToDrawConsole ();
	SCR_EraseCenterString ();

	V_RenderView ();

	//
	// Full copy is something has changed on status bar
	//

	if (Sbar_HasChanged()) {
		scr_copy = CPY_ALL;
	}

	// 
	// Handle edge cases and draw screen accordingly
	//

	if (scr_drawdialog)
	{
	// Fade screen when asked something to player like starting new game when already in game
		Sbar_Draw ();
		Draw_FadeScreen ();
		SCR_DrawNotifyString ();
	}
	else if (scr_drawloading)
	{
		SCR_DrawLoading ();
		Sbar_Draw ();
	}
	else if (cl.intermission == 1 && key_dest == key_game)
	{
		Sbar_IntermissionOverlay ();
	}
	else if (cl.intermission == 2 && key_dest == key_game)
	{
		Sbar_FinaleOverlay ();
		SCR_CheckDrawCenterString ();
	}
	else if (cl.intermission == 3 && key_dest == key_game)
	{
		SCR_CheckDrawCenterString ();
	}
	else
	{
		SCR_DrawRam ();
		SCR_DrawNet ();
		SCR_DrawTurtle ();
		SCR_DrawPause ();
		SCR_CheckDrawCenterString ();
		Sbar_Draw ();
		SCR_DrawConsole ();
		M_Draw ();
	}
	
	SCR_DrawFps();	

	V_UpdatePalette ();

//
// update one of three areas
//

	if (scr_copy == CPY_ALL)
	{
		vrect.x = 0;
		vrect.y = 0;
		vrect.width = vid.width;
		vrect.height = vid.height;
		scr_copy = CPY_TOP;
	}
	else if (scr_copy == CPY_TOP)
	{
		vrect.x = 0;
		vrect.y = 0;
		vrect.width = vid.width;
		vrect.height = vid.height - sb_lines;
	}	
	else
	{
		vrect.x = scr_vrect.x;
		vrect.y = scr_vrect.y;
		vrect.width = scr_vrect.width;
		vrect.height = scr_vrect.height;	
	}

	vrect.pnext = NULL;
	VID_Update (&vrect);
}


/*
==================
SCR_SetFullUpdate
==================
*/
void SCR_SetFullUpdate (void)
{
	scr_copy = CPY_ALL;
	Sbar_Changed();
}

int SCR_GetConsoleSize (void)
{
	return scr_con_current;
}

void SCR_SetEnable (qboolean en)
{
	scr_enable = en;
}

qboolean SCR_GetEnable (void)
{
	return scr_enable;
}