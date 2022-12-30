#include "quakedef.h"
#include "discovery.h"

#define	MAX_SFX		256

/* Global variables */
cvar_t snd_loadas8bit  		= {"loadas8bit", "0"};
static cvar_t snd_volume 	= {"snd_volume", "0.7", true};
static cvar_t precache 		= {"precache", "1"};
static cvar_t ambient_lvl  	= {"ambient_level", "0.3"};
static cvar_t ambient_fade 	= {"ambient_fade", "100"};

channel_t           snd_channels[MAX_CHANNELS];
int                 snd_total_channels;
int                 snd_paintedtime;
dma_t      			*snd_shm = NULL;

/* Local variables */
static qboolean 	snd_initialized = false;
static dma_t        this_shm;
static qboolean 	sound_started;
static uint8_t 		num_sfx;
static sfx_t		*known_sfx;
static sfx_t		*ambient_sfx[NUM_AMBIENTS];
static vec3_t		v_listener_origin;
static vec3_t		v_listener_forward;
static vec3_t		v_listener_right;
static vec3_t		v_listener_up;
static vec_t        sound_nominal_clip_dist;


// =======================================================================
// Static functions
// =======================================================================

static void SND_SoundList_f(void)
{
    int		i;
    sfx_t	*sfx;
    sfxcache_t	*sc;
    int		size, total;

    total = 0;
    for (sfx=known_sfx, i=0 ; i<num_sfx ; i++, sfx++)
    {
        sc = Cache_Check (&sfx->cache);
        if (!sc)
            continue;
        size = sc->length*sc->width*(sc->stereo+1);
        total += size;
        if (sc->loopstart >= 0)
            Con_Printf ("L");
        else
            Con_Printf (" ");
        Con_Printf("(%2db) %6i : %s\n",sc->width*8,  size, sfx->name);
    }
    Con_Printf ("Total resident: %i\n", total);
}

static void SND_PlayVol_f(void)
{
    static int hash=543;
    int i;
    float vol;
    char name[256];
    sfx_t	*sfx;
    
    i = 1;
    while (i<Cmd_Argc())
    {
        if (!Q_strrchr(Cmd_Argv(i), '.'))
        {
            Q_strcpy(name, Cmd_Argv(i));
            Q_strcat(name, ".wav");
        }
        else
            Q_strcpy(name, Cmd_Argv(i));
        sfx = S_PrecacheSound(name);
        vol = Q_atof(Cmd_Argv(i+1));
        S_StartSound(hash++, 0, sfx, v_listener_origin, vol, 1.0);
        i+=2;
    }
}

static void SND_Play_f(void)
{
    static int hash=345;
    int 	i;
    char name[256];
    sfx_t	*sfx;
    
    i = 1;
    while (i<Cmd_Argc())
    {
        if (!Q_strrchr(Cmd_Argv(i), '.'))
        {
            Q_strcpy(name, Cmd_Argv(i));
            Q_strcat(name, ".wav");
        }
        else
            Q_strcpy(name, Cmd_Argv(i));
        sfx = S_PrecacheSound(name);
        S_StartSound(hash++, 0, sfx, v_listener_origin, 1.0, 1.0);
        i++;
    }
}

static void SND_SoundInfo_f(void)
{
	if (!sound_started || !snd_shm)
	{
		Con_Printf ("SND: sound system not started\n");
		return;
	}
	
	Con_Printf("\n-----------------------------\n");
    Con_Printf("channels %d\n", snd_shm->channels - 1);
    Con_Printf("buffer size %d\n", snd_shm->samples);
    Con_Printf("samplepos %d\n", snd_shm->samplepos);
    Con_Printf("samplebits %d\n", snd_shm->samplebits);
    Con_Printf("submission_chunk %d\n", snd_shm->submission_chunk);
    Con_Printf("sample rate %d\n", snd_shm->speed);
    Con_Printf("dma buffer 0x%x\n", snd_shm->buffer);
	Con_Printf("total_channels %d\n", snd_total_channels);
	Con_Printf("-----------------------------\n\n");
}

static sfx_t *SND_FindName (char *name)
{
	int		i;
	sfx_t	*sfx;

	if (!name)
		Sys_Error ("S_FindName: NULL\n");

	if (Q_strlen(name) >= MAX_QPATH)
		Sys_Error ("Sound name too long: %s", name);

    // see if already loaded
	for (i=0 ; i < num_sfx ; i++)
		if (!Q_strcmp(known_sfx[i].name, name))
		{
			return &known_sfx[i];
		}

	if (num_sfx == MAX_SFX)
		Sys_Error ("S_FindName: out of sfx_t");
	
	sfx = &known_sfx[i];
	strcpy (sfx->name, name);

	num_sfx++;
	
	return sfx;
}

static void SND_UpdateAmbientSounds (void)
{
	mleaf_t		*l;
	float		vol;
	int			ambient_channel;
	channel_t	*chan;

    // calc ambient sound levels
	if (!cl.worldmodel)
		return;

	l = Mod_PointInLeaf (v_listener_origin, cl.worldmodel);
	if (!l || !ambient_lvl.value)
	{
		for (ambient_channel = 0 ; ambient_channel< NUM_AMBIENTS ; ambient_channel++)
			snd_channels[ambient_channel].sfx = NULL;
		return;
	}

	for (ambient_channel = 0 ; ambient_channel< NUM_AMBIENTS ; ambient_channel++)
	{
		chan = &snd_channels[ambient_channel];	
		chan->sfx = ambient_sfx[ambient_channel];
	
		vol = ambient_lvl.value * l->ambient_sound_level[ambient_channel];
		if (vol < 8)
			vol = 0;

	    // don't adjust volume too fast
		if (chan->master_vol < vol)
		{
			chan->master_vol += host_frametime * ambient_fade.value;
			if (chan->master_vol > vol)
				chan->master_vol = vol;
		}
		else if (chan->master_vol > vol)
		{
			chan->master_vol -= host_frametime * ambient_fade.value;
			if (chan->master_vol < vol)
				chan->master_vol = vol;
		}
		
		chan->leftvol = chan->rightvol = chan->master_vol;
	}
}

// ========================================================
// Called from Audio driver
// ========================================================
static void SND_DMA_cb (void *stream, uint32_t len){
	if(snd_shm){
    	snd_shm->buffer = stream;
    	snd_shm->samplepos += len / (snd_shm->samplebits / 8) / snd_shm->channels;
	}
}

static void SND_Spatialize(channel_t *ch)
{
    vec_t   dot, dist;
    vec_t   lscale, rscale, scale;
    vec3_t  source_vec;

    // anything coming from the view entity will allways be full volume
	if (ch->entnum == cl.viewentity)
	{
		ch->leftvol = ch->master_vol;
		ch->rightvol = ch->master_vol;
		return;
	}

    // calculate stereo seperation and distance attenuation
	VectorSubtract(ch->origin, v_listener_origin, source_vec);
	
	dist = VectorNormalize(source_vec) * ch->dist_mult;
	
	dot = DotProduct(v_listener_right, source_vec);

	if (snd_shm->channels == 1)
	{
		rscale = 1.0;
		lscale = 1.0;
	}
	else
	{
		rscale = 1.0 + dot;
		lscale = 1.0 - dot;
	}

    // add in distance effect
	scale = (1.0 - dist) * rscale;
	ch->rightvol = (int) (ch->master_vol * scale);
	if (ch->rightvol < 0)
		ch->rightvol = 0;

	scale = (1.0 - dist) * lscale;
	ch->leftvol = (int) (ch->master_vol * scale);
	if (ch->leftvol < 0)
		ch->leftvol = 0;
}
/**
 * picks a channel based on priorities, empty slots, number of channels
 * */
static channel_t *SND_PickChannel(int entnum, int entchannel)
{
    int ch_idx;
    int first_to_die;
    int life_left;

    // Check for replacement sound, or find the best one to replace
    first_to_die = -1;
    life_left = 0x7fffffff;
    for (ch_idx=NUM_AMBIENTS ; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS ; ch_idx++)
    {
		if (entchannel != 0		// channel 0 never overrides
		&& snd_channels[ch_idx].entnum == entnum
		&& (snd_channels[ch_idx].entchannel == entchannel || entchannel == -1) )
		{	// allways override sound from same entity
			first_to_die = ch_idx;
			break;
		}

		// don't let monster sounds override player sounds
		if (snd_channels[ch_idx].entnum == cl.viewentity && entnum != cl.viewentity && snd_channels[ch_idx].sfx)
			continue;

		if (snd_channels[ch_idx].end - snd_paintedtime < life_left)
		{
			life_left = snd_channels[ch_idx].end - snd_paintedtime;
			first_to_die = ch_idx;
		}
   }

	if (first_to_die == -1)
		return NULL;

	if (snd_channels[first_to_die].sfx)
		snd_channels[first_to_die].sfx = NULL;

    return &snd_channels[first_to_die];    
}       

/**
 * @brief copied from snd_mem.c
 * 
 * @param sfx 
 * @param inrate 
 * @param inwidth 
 * @param data 
 */
static void SND_ResampleSfx (sfx_t *sfx, int inrate, int inwidth, byte *data)
{
	int		outcount;
	int		srcsample;
	float	stepscale;
	int		i;
	int		sample, samplefrac, fracstep;
	sfxcache_t	*sc;
	
	sc = Cache_Check (&sfx->cache);
	if (!sc)
		return;

	stepscale = (float)inrate / snd_shm->speed;	// this is usually 0.5, 1, or 2

	outcount = sc->length / stepscale;
	sc->length = outcount;
	if (sc->loopstart != -1)
		sc->loopstart = sc->loopstart / stepscale;

	sc->speed = snd_shm->speed;
	if (snd_loadas8bit.value)
		sc->width = 1;
	else
		sc->width = inwidth;
	sc->stereo = 0;

    // resample / decimate to the current source rate

	if (stepscale == 1 && inwidth == 1 && sc->width == 1)
	{
        // fast special case
		for (i=0 ; i<outcount ; i++)
			((signed char *)sc->data)[i]
			= (int)( (unsigned char)(data[i]) - 128);
	}
	else
	{
        // general case
		samplefrac = 0;
		fracstep = stepscale * 256;
		for (i=0 ; i<outcount ; i++)
		{
			srcsample = samplefrac >> 8;
			samplefrac += fracstep;
			if (inwidth == 2)
				sample = LittleShort ( ((short *)data)[srcsample] );
			else
				sample = (int)( (unsigned char)(data[srcsample]) - 128) << 8;
			if (sc->width == 2)
				((short *)sc->data)[i] = sample;
			else
				((signed char *)sc->data)[i] = sample >> 8;
		}
	}
}

/**
 * @brief copied from snd_mem.c
 * 
 * @param ptr 
 * @param val 
 * @return byte* 
 */
static byte *SND_GetLittleShort(byte *ptr, short *val)
{
	*val = (ptr[1]<<8) | ptr[0];
	return ptr + 2;
}

/**
 * @brief copied from snd_mem.c
 * 
 * @param ptr 
 * @param val 
 * @return byte* 
 */
static byte *SND_GetLittleLong(byte *ptr, int *val)
{
	*val = (ptr[3]<<24) | (ptr[2]<<16) | (ptr[1]<<8) | ptr[0];
	return ptr + 4;
}

/**
 * @brief copied from snd_mem.c
 * 
 * @param name 
 * @param start 
 * @param end 
 * @return byte* 
 */
static byte *SND_FindChunk(char *name, byte *start, byte *end)
{
	byte *ptr;
	int chunk_size;

	while (1)
	{
		ptr = start;

		if (ptr >= end){
			// didn't find the chunk
			return NULL;
		}
		
		ptr += 4;
		ptr = SND_GetLittleLong(ptr, &chunk_size);

		if (chunk_size < 0) {
            // invalid size
			return NULL;
		}

		ptr -= 8;

		if (!Q_strncmp((char*)ptr, name, 4))
			return ptr;

		start = ptr + 8 + ( (chunk_size + 1) & ~1 );
	}
}

/**
 * @brief copied from snd_mem.c
 * 
 * @param name 
 * @param wavdata 
 * @param wavlength 
 * @return wavinfo_t 
 */
wavinfo_t SND_GetWavinfo (char *name, byte *wavdata, int wavlength)
{
    wavinfo_t	info;
	int     i;
	short   format;
	int		samples;
	byte	*data_p, *iff_data, *iff_end;

	memset (&info, 0, sizeof(info));

	if (!wavdata)
		return info;

    iff_data = wavdata;
	iff_end = wavdata + wavlength;

	// find "RIFF" chunk
	data_p = SND_FindChunk("RIFF", iff_data, iff_end);
	if (!(data_p && !Q_strncmp((char*)(data_p + 8), "WAVE", 4)))
	{
		Con_Printf("Missing RIFF/WAVE chunks\n");
		return info;
	}

	iff_data = data_p + 12;

	data_p = SND_FindChunk("fmt ", iff_data, iff_end);
	if (!data_p)
	{
		Con_Printf("Missing fmt chunk\n");
		return info;
	}

	data_p += 8;
	data_p = SND_GetLittleShort(data_p, &format);

	if (format != 1)
	{
		Con_Printf("Microsoft PCM format only\n");
		return info;
	}

	data_p = SND_GetLittleShort(data_p, (short*)&info.channels);
	data_p = SND_GetLittleLong(data_p, &info.rate);
	data_p += 4 + 2;
	data_p = SND_GetLittleShort(data_p, (short*)&info.width);
	info.width /= 8;

    // get cue chunk
	data_p = SND_FindChunk("cue ", iff_data, iff_end);
	if (data_p)
	{
		data_p += 32;
		data_p = SND_GetLittleLong(data_p, &info.loopstart);
        //	Con_Printf("loopstart=%d\n", sfx->loopstart);

	    // if the next chunk is a LIST chunk, look for a cue length marker
		data_p = SND_FindChunk ("LIST", data_p, iff_end);
		if (data_p)
		{
			if (!Q_strncmp ((char*)(data_p + 28), "mark", 4))
			{	// this is not a proper parse, but it works with cooledit...
				data_p += 24;
				data_p = SND_GetLittleLong (data_p, &i);	// samples in loop
				info.samples = info.loopstart + i;
                //	on_Printf("looped length: %i\n", i);
			}
		}
	}
	else
		info.loopstart = -1;

    // find data chunk
	data_p = SND_FindChunk("data", iff_data, iff_end);
	if (!data_p)
	{
		Con_Printf("Missing data chunk\n");
		return info;
	}

	data_p += 4;
	data_p = SND_GetLittleLong (data_p, &samples);
	samples /= info.width;

	if (info.samples)
	{
		if (samples < info.samples)
			Sys_Error ("Sound %s has a bad loop length", name);
	}
	else
		info.samples = samples;

	info.dataofs = data_p - wavdata;
	
	return info;	
}

static sfxcache_t *SND_LoadSound (sfx_t *s)
{
    char	    namebuffer[256];
	byte	    *data;
	wavinfo_t   info;
	int		    len;
	float	    stepscale;
	sfxcache_t  *sc;
	byte        stackbuf[1*1024];		// avoid dirtying the cache heap

    // see if still in memory
	sc = Cache_Check (&s->cache);
	if (sc)
		return sc;

    //Con_Printf ("SND_LoadSound: %x\n", (int)stackbuf);
    // load it in
    Q_strcpy(namebuffer, "sound/");
    Q_strcat(namebuffer, s->name);

    //	Con_Printf ("loading %s\n",namebuffer);
    data = COM_LoadStackFile(namebuffer, stackbuf, sizeof(stackbuf));

	if (!data) {
		Con_Printf ("Couldn't load %s\n", namebuffer);
		return NULL;
	}

	info = SND_GetWavinfo (s->name, data, com_filesize);
	if (info.channels != 1)
	{
		Con_Printf ("%s is a stereo sample\n",s->name);
		return NULL;
	}

	stepscale = (float)info.rate / snd_shm->speed;	
	len = info.samples / stepscale;

	len = len * info.width * info.channels;

	sc = Cache_Alloc ( &s->cache, len + sizeof(sfxcache_t), s->name);
	if (!sc)
		return NULL;
	
	sc->length = info.samples;
	sc->loopstart = info.loopstart;
	sc->speed = info.rate;
	sc->width = info.width;
	sc->stereo = info.channels;

	SND_ResampleSfx (s, sc->speed, sc->width, data + info.dataofs);

	return sc;
}

static void SND_StopAllSounds_f(void)
{
	S_StopAllSounds(true);
}

static void SND_Update(void)
{
	#if 0
	/* TOOD: this should keep audio when frames take tool long to render */
	int		fullsamples;
	int 	samplepos;
	int		soundtime;
	int		samps;
	unsigned int endtime;
	static	int		buffers;
	static	int		oldsamplepos;

	// Updates DMA time
	fullsamples = snd_shm->samples / snd_shm->channels;
	samplepos = SNDDMA_GetDMAPos();

	if (samplepos < oldsamplepos)
	{
		buffers++;					// buffer wrapped
		
		if (snd_paintedtime > 0x40000000)
		{	// time to chop things off to avoid 32 bit limits
			buffers = 0;
			snd_paintedtime = fullsamples;
			S_StopAllSounds (true);
		}
	}
	oldsamplepos = samplepos;

	soundtime = buffers * fullsamples + samplepos / snd_shm->channels;

	// check to make sure that we haven't overshot
	if (snd_paintedtime < soundtime)
	{
		//Con_Printf ("S_Update_ : overflow\n");
		snd_paintedtime = soundtime;
	}

	endtime = soundtime + (0.1 * snd_shm->speed); // + _snd_mixahead.value * shm->speed;
	samps = snd_shm->samples >> (snd_shm->channels-1);
	if (endtime - soundtime > samps)
		endtime = soundtime + samps;
	
	S_MIX_PaintChannels(endtime);
	#else
	S_MIX_PaintChannels(snd_shm->samplepos);
	#endif
}

static void SND_Volume_f (void)
{
    Cvar_SetFromCommand("snd_volume");
}

/*
==================
Public functions
==================
*/
void S_BeginPrecaching (void) { /* not used */ }

void S_ChangeVolume(int dir)
{
	snd_volume.value += dir * 0.1;
	if (snd_volume.value < 0)
		snd_volume.value = 0;
	if (snd_volume.value > 1)
		snd_volume.value = 1;
	Cvar_SetValue ("snd_volume", snd_volume.value);
}

void S_ClearBuffer (void)
{
    int		clear;

    if (!sound_started || !snd_shm || !snd_shm->buffer){
        return;
    }

	clear = (snd_shm->samplebits == 8) ? 0x80 : 0;

    Q_memset(snd_shm->buffer, clear, snd_shm->samples * snd_shm->samplebits/8);
}

void S_ClearPrecache (void) { /* not used */ }

void S_EndPrecaching (void){ /* not used */ }

void S_ExtraUpdate (void)
{ 
	//if (snd_noextraupdate.value)
	//	return;		// don't pollute timings
	SND_Update();
 }

float S_GetVolume (void){
	return snd_volume.value;
}

void S_Init (void)
{
    discoaudio_t specs;

	if (COM_CheckParm("-nosound"))
		return;

	Con_Printf("\nSound Initialising...\n");

	Cmd_AddCommand("play", SND_Play_f);
	Cmd_AddCommand("playvol", SND_PlayVol_f);
	Cmd_AddCommand("stopsound", SND_StopAllSounds_f);
	Cmd_AddCommand("soundlist", SND_SoundList_f);
	Cmd_AddCommand("soundinfo", SND_SoundInfo_f);
	Cmd_AddCommand("volume", SND_Volume_f);

	Cvar_RegisterVariable(&snd_volume);
	Cvar_RegisterVariable(&precache);
    Cvar_RegisterVariable(&ambient_lvl);
	Cvar_RegisterVariable(&ambient_fade);
    Cvar_RegisterVariable(&snd_loadas8bit);

    if (host_parms.memsize < 0x800000)
	{
		Cvar_Set ("loadas8bit", "1");
		Con_Printf ("loading all sounds as 8bit\n");
	}

	snd_initialized = true;
	sound_started = true;
    sound_nominal_clip_dist = 1000.0f;

    specs.freq = 11025;
	specs.channels = 2;
    specs.format = 16;
    specs.callback = SND_DMA_cb;

	DISCO_Audio_Init(&specs);

    snd_shm = &this_shm;
	snd_shm->splitbuffer = 0;
	snd_shm->samplebits = 16;
	snd_shm->speed = specs.freq;
	snd_shm->channels = specs.channels;
	snd_shm->samples = specs.size * specs.channels;
	snd_shm->samplepos = 0;
	snd_shm->submission_chunk = 1;
	snd_shm->buffer = NULL;

	S_MIX_InitScaletable();
	num_sfx = 0;
	known_sfx = Hunk_AllocName (MAX_SFX * sizeof(sfx_t), "sfx_t");	
	ambient_sfx[AMBIENT_WATER] = S_PrecacheSound ("ambience/water1.wav");
	ambient_sfx[AMBIENT_SKY] = S_PrecacheSound ("ambience/wind2.wav");

	S_StopAllSounds (true);
}

/**
 * @brief copied from snd_mem.c
 * 
 * @param s 
 * @return sfxcache_t* 
 */
sfxcache_t *S_LoadSound (sfx_t *s)
{
    char	    namebuffer[256];
	byte	    *data;
	wavinfo_t	info;
	int		    len;
	float	    stepscale;
	sfxcache_t  *sc;
	byte	    stackbuf[1 * 1024];		// avoid dirtying the cache heap

    // see if still in memory
	sc = Cache_Check (&s->cache);
	if (sc)
		return sc;

    //Con_Printf ("S_LoadSound: %x\n", (int)stackbuf);
    // load it in
    Q_strcpy(namebuffer, "sound/");
    Q_strcat(namebuffer, s->name);

    //	Con_Printf ("loading %s\n",namebuffer);

	data = COM_LoadStackFile(namebuffer, stackbuf, sizeof(stackbuf));

	if (!data){
		Con_Printf ("Couldn't load %s\n", namebuffer);
		return NULL;
	}

	info = SND_GetWavinfo (s->name, data, com_filesize);
	if (info.channels != 1)	{
		Con_Printf ("%s is a stereo sample\n",s->name);
		return NULL;
	}

	stepscale = (float)info.rate / snd_shm->speed;	
	len = info.samples / stepscale;

	len = len * info.width * info.channels;

	sc = Cache_Alloc ( &s->cache, len + sizeof(sfxcache_t), s->name);
	if (!sc)
		return NULL;
	
	sc->length = info.samples;
	sc->loopstart = info.loopstart;
	sc->speed = info.rate;
	sc->width = info.width;
	sc->stereo = info.channels;

	SND_ResampleSfx (s, sc->speed, sc->width, data + info.dataofs);

	return sc;
}

void S_LocalSound (char *sound)
{
    sfx_t	*sfx;

	if (!sound_started)// || nosound.value)
		return;
		
	sfx = S_PrecacheSound (sound);

	if (!sfx) {
		Con_Printf ("S_LocalSound: can't cache %s\n", sound);
		return;
	}

	S_StartSound (cl.viewentity, -1, sfx, vec3_origin, 1, 1);
}

sfx_t *S_PrecacheSound (char *name)
{
	sfx_t	*sfx;

	if (!sound_started) // || nosound.value)
		return NULL;

	sfx = SND_FindName (name);
	
	// cache it in
	if (precache.value)
	    SND_LoadSound (sfx);
	
	return sfx;
}

void S_Shutdown (void)
{
    if (!sound_started)
		return;

	if (snd_shm)
		snd_shm->gamealive = 0;

	snd_shm = NULL;
	sound_started = 0;

	SNDDMA_Shutdown ();
}

void S_StartSound (int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol,  float attenuation)
{
    channel_t *target_chan, *check;
	sfxcache_t	*sc;
	int		vol;
	int		ch_idx;
	int		skip;

	if (!sound_started || !sfx) // || nosound.value)
		return;

	vol = fvol * 255;

    // pick a channel to play on
	target_chan = SND_PickChannel(entnum, entchannel);
	if (!target_chan)
		return;
		
    // spatialize
	memset (target_chan, 0, sizeof(*target_chan));
	VectorCopy(origin, target_chan->origin);
	target_chan->dist_mult = attenuation / sound_nominal_clip_dist;
	target_chan->master_vol = vol;
	target_chan->entnum = entnum;
	target_chan->entchannel = entchannel;
	SND_Spatialize(target_chan);

	if (!target_chan->leftvol && !target_chan->rightvol)
		return;		// not audible at all

    // new channel
	sc = SND_LoadSound (sfx);
	if (!sc)
	{
		target_chan->sfx = NULL;
		return;		// couldn't load the sound's data
	}

	target_chan->sfx = sfx;
	target_chan->pos = 0.0;
    target_chan->end = snd_paintedtime + sc->length;	

    // if an identical sound has also been started this frame, offset the pos
    // a bit to keep it from just making the first one louder
	check = &snd_channels[NUM_AMBIENTS];
    for (ch_idx=NUM_AMBIENTS ; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS ; ch_idx++, check++)
    {
		if (check == target_chan)
			continue;
		if (check->sfx == sfx && !check->pos)
		{
			skip = rand () % (int)(0.1*snd_shm->speed);
			if (skip >= target_chan->end)
				skip = target_chan->end - 1;
			target_chan->pos += skip;
			target_chan->end -= skip;
			break;
		}
		
	}
}

void S_StaticSound (sfx_t *sfx, vec3_t origin, float vol, float attenuation)
{
    channel_t   *ss;
	sfxcache_t  *sc;

	if (!sfx)
		return;

	if (snd_total_channels == MAX_CHANNELS)	{
		Con_Printf ("No more audio channels available\n");
		return;
	}

	ss = &snd_channels[snd_total_channels];
	snd_total_channels++;

	sc = SND_LoadSound (sfx);
	if (!sc)
		return;

	if (sc->loopstart == -1) {
		Con_Printf ("Sound %s not looped\n", sfx->name);
		return;
	}
	
	ss->sfx = sfx;
	VectorCopy (origin, ss->origin);
	ss->master_vol = vol;
	ss->dist_mult = (attenuation/64) / sound_nominal_clip_dist;
    ss->end = snd_paintedtime + sc->length;	
	
	SND_Spatialize (ss);
}

void S_StopAllSounds (qboolean clear)
{
    int		i;

	if (!sound_started)
		return;

	snd_total_channels = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;	// no statics

	for (i=0 ; i<MAX_CHANNELS ; i++)
		if (snd_channels[i].sfx)
			snd_channels[i].sfx = NULL;

	Q_memset(snd_channels, 0, MAX_CHANNELS * sizeof(channel_t));

	if (clear)
		S_ClearBuffer ();
}

void S_StopSound (int entnum, int entchannel)
{
	for (uint8_t i=0 ; i < MAX_DYNAMIC_CHANNELS; i++) {
		if (snd_channels[i].entnum == entnum && snd_channels[i].entchannel == entchannel){
			snd_channels[i].end = 0;
			snd_channels[i].sfx = NULL;
			return;
		}
	}
}

void S_TouchSound (char *name)
{
	sfx_t	*sfx;
	
	if (!sound_started)
		return;

	sfx = SND_FindName (name);
	Cache_Check (&sfx->cache);
}

void S_Update (vec3_t v_origin, vec3_t v_forward, vec3_t v_right, vec3_t v_up)
{
    int			i, j;
	channel_t	*ch;
	channel_t	*combine;

	if (!sound_started)
		return;

	VectorCopy(v_origin, v_listener_origin);
	VectorCopy(v_forward, v_listener_forward);
	VectorCopy(v_right, v_listener_right);
	VectorCopy(v_up, v_listener_up);
	
    // update general area ambient sound sources
	SND_UpdateAmbientSounds ();

	combine = NULL;

    // update spatialization for static and dynamic sounds	
	ch = snd_channels + NUM_AMBIENTS;
	for (i = NUM_AMBIENTS; i < snd_total_channels; i++, ch++)
	{
		if (!ch->sfx)
			continue;

		SND_Spatialize(ch);         // respatialize channel
		
        if (!ch->leftvol && !ch->rightvol)
			continue;

	    // try to combine static sounds with a previous channel of the same
	    // sound effect so we don't mix five torches every frame
		if (i >= MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS)
		{
		    // see if it can just use the last one
			if (combine && combine->sfx == ch->sfx)
			{
				combine->leftvol += ch->leftvol;
				combine->rightvol += ch->rightvol;
				ch->leftvol = ch->rightvol = 0;
				continue;
			}
		    // search for one
			combine = snd_channels + MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;
			for (j=MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS ; j<i; j++, combine++)
				if (combine->sfx == ch->sfx)
					break;
					
			if (j == snd_total_channels)
			{
				combine = NULL;
			}
			else
			{
				if (combine != ch)
				{
					combine->leftvol += ch->leftvol;
					combine->rightvol += ch->rightvol;
					ch->leftvol = ch->rightvol = 0;
				}
				continue;
			}
		}	
	}

	// mix sound
	SND_Update();
}
