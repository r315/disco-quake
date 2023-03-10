#include <errno.h>
#include "stm32f7xx_hal.h"
#include "quakedef.h"
#include "fatfs.h"

#define MAX_HANDLES             10
#define MIN_SYS_MEMORY          (8 * 1024 * 1024) // 8MB


/*
===============================================================================

Variables

===============================================================================
*/
static char	text_out[MAX_OSPATH];
static FILE *sys_handles[MAX_HANDLES];
static quakeparms_t parms;
qboolean isDedicated = false;


/*
===============================================================================

Functions prototypes

===============================================================================
*/

//int Q_strlen (char *str){ int len = 0; while(str[len])len++; return len;}
//void Host_Init (quakeparms_t *parms){}
//void Host_Shutdown(void){}
//void COM_InitArgv (int argc, char **argv){}
//void Host_Frame (float time){}
int syscalls_getsize(int fd);
/*
===============================================================================

Static functions

===============================================================================
*/
static int findhandle (void)
{
    int i;
    
    for (i=1 ; i<MAX_HANDLES ; i++)
        if (!sys_handles[i])
            return i;

    Sys_Error ("out of handles");
    
    return -1;
}

/*
===============================================================================

FILE IO

===============================================================================
*/

int Sys_FileOpenRead (char *path, int *hndl)
{
    int             i;
    FILE    		*fp;	
    
    i = findhandle ();

    fp = fopen(path, "rb");

    if (!fp){
        *hndl = -1;
        return -1;
    }
    
    sys_handles[i] = fp;
    *hndl = i;
    
    return syscalls_getsize(fp->_file);	
}

int Sys_FileOpenWrite (char *path)
{
    FILE    *f;
    int             i;
    
    i = findhandle ();

    f = fopen(path, "wb");
    
    if (!f){
        //Sys_Error ("Error opening %s: %s", path,strerror(errno));
        return -1;
    }
    
    sys_handles[i] = f;
    
    return i;
}

void Sys_FileClose (int handle)
{
    fclose (sys_handles[handle]);
    sys_handles[handle] = NULL;
}

void Sys_FileSeek (int handle, int position)
{
    fseek (sys_handles[handle], position, SEEK_SET);
}

int Sys_FileRead (int handle, void *dest, int count)
{
    return fread (dest, 1, count, sys_handles[handle]);
}

int Sys_FileWrite (int handle, void *data, int count)
{
    return fwrite (data, 1, count, sys_handles[handle]);
}

int Sys_FileTime (char *path)
{
    FILE    *f;
    
    f = fopen(path, "rb");
    if (f)
    {
        fclose(f);
        return 1;
    }
    
    return -1;
}

void Sys_mkdir (char *path)
{
}


/*
===============================================================================

SYSTEM IO

===============================================================================
*/

void Sys_Init( void )
{
    
}


void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length)
{
}


void Sys_Error (char *error, ...)
{
    va_list         argptr;
    
    va_start (argptr, error);
    vsprintf (text_out, error, argptr);
    va_end (argptr);

    printf("Sys Error: %s\n", text_out);
    
    Host_Shutdown();

    exit (1);
}

void Sys_Printf (char *fmt, ...)
{
    va_list		argptr;	
    va_start (argptr,fmt);
    vsprintf (text_out, fmt, argptr);
    va_end (argptr);
    printf(text_out);
}

void Sys_Quit (void)
{
    int i;
    Host_Shutdown();

    for( i = 0; i < MAX_HANDLES; i++ )
    {
        if( sys_handles[ i ] )
        {
            fclose( sys_handles[ i ] );
            sys_handles[ i ] = NULL;
        }
    }

    if( parms.membase )
    {
        free( parms.membase );
    }
    
    exit (0);
}

/**
 * @brief Returns float time in seconds
 * 
 * @return float 
 */
float Sys_FloatTime (void)
{
    static uint32_t start = 0;
    
    if(!start){
        start = HAL_GetTick();
    }

    return (HAL_GetTick() - start) / 1000.0;
}

char *Sys_ConsoleInput (void)
{
    return NULL;
}

void Sys_Sleep (void)
{
}

void Sys_SendKeyEvents (void)
{
    IN_SendKeyEvents();
}

void Sys_HighFPPrecision (void)
{
}

void Sys_LowFPPrecision (void)
{
}

//=============================================================================

int Sys_main( int argc, char **argv )
{
    char *pc_basedir_term;
    float oldtime, newtime, time;

    Sys_Init();
    
    parms.memsize = MIN_SYS_MEMORY;
    parms.membase = malloc (parms.memsize);
    parms.basedir = ".";
    parms.argc = com_argc = argc;
    parms.argv = com_argv = argv;

    if( !parms.membase )
    {
        Sys_Error( "could not alloc %.2f kb of memory\n", ( ( float )parms.memsize ) / 1024.0 );
    }

    pc_basedir_term = argv[ 0 ] + Q_strlen( argv[ 0 ] );

    while( pc_basedir_term >= argv[ 0 ] )
    {
        if( *pc_basedir_term == '/' || *pc_basedir_term == '\\' )
        {
            *pc_basedir_term = 0;
            break;
        }
        pc_basedir_term--;
    }

    parms.basedir = argv[ 0 ];

    Sys_Printf("membase: %p\n", parms.membase);
    
    memset(parms.membase, 0, parms.memsize);

    Sys_Printf("Starting Quake %s\n", VERSION_STRING);
    
    COM_InitArgv (com_argc, com_argv);

    Host_Init (&parms);

    Sys_Printf("Total system memory: %d bytes\n", sysmem_total());
    Sys_Printf("Used: %d bytes\n", sysmem_used());
    Sys_Printf("Free: %d bytes\n", sysmem_free());

    oldtime = Sys_FloatTime() - 0.1f;
    
    while (1)
    {
        newtime = Sys_FloatTime();
        time = newtime - oldtime;		
        
        Host_Frame ( time );

        oldtime += time;		
    }
    return 0;
}
