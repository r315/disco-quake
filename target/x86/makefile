ROOT_DIR =../..
TARGET_DIR =target/x86
BUILD_DIR =$(ROOT_DIR)/build/$(TARGET_DIR)
SRC_DIR =$(ROOT_DIR)/src
EXEC =$(BUILD_DIR)/sdlquake

VERBOSE :=

SDL_PATH :=$(HOME)/Documents/SDL-1.2.15

Q_OBJS =\
$(BUILD_DIR)/chase.o \
$(BUILD_DIR)/cl_demo.o \
$(BUILD_DIR)/cl_input.o \
$(BUILD_DIR)/cl_main.o \
$(BUILD_DIR)/cl_parse.o \
$(BUILD_DIR)/cl_tent.o \
$(BUILD_DIR)/cmd.o \
$(BUILD_DIR)/common.o \
$(BUILD_DIR)/console.o \
$(BUILD_DIR)/crc.o \
$(BUILD_DIR)/cvar.o \
$(BUILD_DIR)/d_edge.o \
$(BUILD_DIR)/d_fill.o \
$(BUILD_DIR)/d_init.o \
$(BUILD_DIR)/d_modech.o \
$(BUILD_DIR)/d_part.o \
$(BUILD_DIR)/d_polyse.o \
$(BUILD_DIR)/d_scan.o \
$(BUILD_DIR)/d_sky.o \
$(BUILD_DIR)/d_sprite.o \
$(BUILD_DIR)/d_surf.o \
$(BUILD_DIR)/d_vars.o \
$(BUILD_DIR)/d_zpoint.o \
$(BUILD_DIR)/host.o \
$(BUILD_DIR)/host_cmd.o \
$(BUILD_DIR)/keys.o \
$(BUILD_DIR)/mathlib.o \
$(BUILD_DIR)/menu.o \
$(BUILD_DIR)/net_bsd.o \
$(BUILD_DIR)/net_dgrm.o \
$(BUILD_DIR)/net_loop.o \
$(BUILD_DIR)/net_main.o \
$(BUILD_DIR)/net_udp.o \
$(BUILD_DIR)/net_vcr.o \
$(BUILD_DIR)/net_wso.o \
$(BUILD_DIR)/nonintel.o \
$(BUILD_DIR)/pr_cmds.o \
$(BUILD_DIR)/pr_edict.o \
$(BUILD_DIR)/pr_exec.o \
$(BUILD_DIR)/r_aclip.o \
$(BUILD_DIR)/r_alias.o \
$(BUILD_DIR)/r_bsp.o \
$(BUILD_DIR)/r_draw.o \
$(BUILD_DIR)/r_edge.o \
$(BUILD_DIR)/r_efrag.o \
$(BUILD_DIR)/r_main.o \
$(BUILD_DIR)/r_part.o \
$(BUILD_DIR)/r_sky.o \
$(BUILD_DIR)/r_sprite.o \
$(BUILD_DIR)/r_surf.o \
$(BUILD_DIR)/r_vars.o \
$(BUILD_DIR)/sbar.o \
$(BUILD_DIR)/sv_main.o \
$(BUILD_DIR)/sv_move.o \
$(BUILD_DIR)/sv_phys.o \
$(BUILD_DIR)/sv_user.o \
$(BUILD_DIR)/view.o \
$(BUILD_DIR)/wad.o \
$(BUILD_DIR)/world.o \
$(BUILD_DIR)/zone.o \


RENDER_OBJS =\
$(BUILD_DIR)/draw.o \
$(BUILD_DIR)/model.o \
$(BUILD_DIR)/r_light.o \
$(BUILD_DIR)/r_misc.o \
$(BUILD_DIR)/screen.o \

SND_NULL =\
$(BUILD_DIR)/snd_null.o \

SND_SDL =\
$(BUILD_DIR)/snd_sdl.o \

SND_LINUX =\
$(BUILD_DIR)/snd_linux.o \

SDL_OBJS =\
$(BUILD_DIR)/sys_sdl.o \
$(BUILD_DIR)/vid_sdl.o \
$(BUILD_DIR)/cd_null.o \
$(BUILD_DIR)/snd_dma.o \
$(BUILD_DIR)/snd_mem.o \
$(BUILD_DIR)/snd_mix.o \
$(SND_LINUX)

NULL_OBJS = \
$(BUILD_DIR)/cd_null.o \
$(BUILD_DIR)/in_null.o \
$(BUILD_DIR)/snd_null.o \
$(BUILD_DIR)/vid_null.o \
$(BUILD_DIR)/net_none.o \
$(BUILD_DIR)/sys_null.o \

INCLUDE =-I$(ROOT_DIR)/inc -I$(SDL_PATH)/include

OBJECTS=$(Q_OBJS) $(RENDER_OBJS) $(SDL_OBJS)

CFLAGS =-g -Og -D_GNU_SOURCE=1 -D_REENTRANT -DSDL -DELF -m32 -Wno-unused-result -std=c11
#CFLAGS =-g -Og

LIBS =-lm -lSDL -ldl -lpthread #-lSDLmain
LDFLAGS =-Og -g -m32 -L$(SDL_PATH)/lib/x86

ifeq ($(VERBOSE), )
Q =@
else
Q =
endif

#TOOL-PREFIX =arm-none-eabi-
CC =$(TOOL-PREFIX)gcc
COMPILE = $(CC) $(INCLUDE) $(CPPFLAGS) $(CFLAGS) 

VPATH +=$(SRC_DIR)

all: $(BUILD_DIR) $(EXEC)

$(EXEC): $(OBJECTS)
	@echo Linking
	@rm -f $@
	$(Q)$(CC) $(LDFLAGS) $^ -o $@ $(LIBS)

clean: 
	rm -f $(BUILD_DIR)/*.o $(EXEC)
	
$(BUILD_DIR):
	$(Q)mkdir -p $@

$(BUILD_DIR)/%.o: %.c #$(SRC_DIR)/$(notdir %.c)
	@echo compile: $<
	$(Q)$(COMPILE) -c $< -o $@

#.c.o:
#	$(COMPILE) -c $<

.w.o:
	$(Q)$(COMPILE) -c $<

