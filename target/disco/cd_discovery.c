
#include "quakedef.h"

cvar_t snd_bgmvolume 	= {"bgmvolume", "0", true};

int CDAudio_Init(void)
{
    Cvar_RegisterVariable(&snd_bgmvolume);
    return 0;
}

void CDAudio_Play(byte track, qboolean looping)
{
}

void CDAudio_Stop(void)
{
}

void CDAudio_Pause(void)
{
}

void CDAudio_Resume(void)
{
}

void CDAudio_Shutdown(void)
{
}

void CDAudio_Update(void)
{
}



