ROOT_DIR =../..
TARGET_DIR =target/disco
BUILD_DIR =$(ROOT_DIR)/build/$(TARGET_DIR)
EXEC =$(BUILD_DIR)/discoquake.elf

REPO_F769 =.

BSP_DIR = \
$(ROOT_DIR)/$(TARGET_DIR)/core \
$(REPO_F769)/Drivers/STM32F7xx_HAL_Driver/Src \
$(REPO_F769)/Drivers/BSP/STM32F769I-Discovery \
$(REPO_F769)/Drivers/BSP/Components/otm8009a \
$(REPO_F769)/Drivers/BSP/Components/ft6x06 \
$(REPO_F769)/Drivers/BSP/Components/wm8994 \
$(REPO_F769)/Middlewares/Third_Party/FatFs/src \

BSP_INCLUDE = \
$(ROOT_DIR)/$(TARGET_DIR)/core \
$(REPO_F769)/Drivers/STM32F7xx_HAL_Driver/Inc \
$(REPO_F769)/Drivers/BSP/STM32F769I-Discovery \
$(REPO_F769)/Drivers/BSP/Components/Common \
$(REPO_F769)/Drivers/BSP/Components/otm8009a \
$(REPO_F769)/Drivers/BSP/Components/wm8994 \
$(REPO_F769)/Drivers/CMSIS/Device/ST/STM32F7xx/Include \
$(REPO_F769)/Drivers/CMSIS/Include \
$(REPO_F769)/Middlewares/Third_Party/FatFs/src \

QUAKE_SOURCES = \
chase.c \
cd_null.c \
cl_demo.c \
cl_input.c \
cl_main.c \
cl_parse.c \
cl_tent.c \
cmd.c \
common.c \
console.c \
crc.c \
cvar.c \
d_edge.c \
d_fill.c \
d_init.c \
d_modech.c \
d_part.c \
d_polyse.c \
d_scan.c \
d_sky.c \
d_sprite.c \
d_surf.c \
d_vars.c \
d_zpoint.c \
draw.c \
host.c \
host_cmd.c \
keys.c \
mathlib.c \
menu.c \
model.c \
net_loop.c \
net_main.c \
net_vcr.c \
net_none.c \
pr_cmds.c \
pr_edict.c \
pr_exec.c \
r_aclip.c \
r_alias.c \
r_bsp.c \
r_draw.c \
r_edge.c \
r_efrag.c \
r_light.c \
r_main.c \
r_misc.c \
r_part.c \
r_sky.c \
r_sprite.c \
r_surf.c \
r_vars.c \
sbar.c \
screen.c \
snd_mix.c \
sv_main.c \
sv_move.c \
sv_phys.c \
sv_user.c \
view.c \
wad.c \
world.c \
zone.c \
vid_discovery.c \
snd_discovery.c \
in_discovery.c \

BSP_SOURCES = \
sys_discovery.c \
sd_diskio.c \
fatfs.c \
sysmem.c \
syscalls.c \
fifo.c \
disco_main.c \
disco_serial.c \
disco_audio.c \
system_stm32f7xx.c \
stm32f7xx_hal.c \
stm32f7xx_hal_cortex.c \
stm32f7xx_hal_gpio.c \
stm32f7xx_hal_ltdc.c \
stm32f7xx_hal_ltdc_ex.c \
stm32f7xx_hal_dsi.c \
stm32f7xx_hal_rcc.c \
stm32f7xx_hal_rcc_ex.c \
stm32f7xx_hal_dma.c \
stm32f7xx_hal_dma2d.c \
stm32f7xx_hal_pwr_ex.c \
stm32f7xx_hal_sdram.c \
stm32f7xx_hal_sd.c \
stm32f7xx_hal_i2c.c \
stm32f7xx_hal_uart.c \
stm32f7xx_hal_sai.c \
stm32f7xx_ll_fmc.c \
stm32f7xx_ll_sdmmc.c \
stm32f769i_discovery.c \
stm32f769i_discovery_lcd.c \
stm32f769i_discovery_ts.c \
stm32f769i_discovery_sdram.c \
stm32f769i_discovery_sd.c \
stm32f769i_discovery_audio.c \
diskio.c \
ff.c \
ff_gen_drv.c \
otm8009a.c \
ft6x06.c \
wm8994.c \

ASOURCES =startup_stm32f769nihx.s

SOURCES =$(BSP_SOURCES) $(QUAKE_SOURCES)

INCLUDE =$(BSP_INCLUDE) $(ROOT_DIR)/inc

OBJS = $(addprefix $(BUILD_DIR)/, $(SOURCES:.c=.o)) $(addprefix $(BUILD_DIR)/, $(ASOURCES:.s=.o)) 

SYMBOLS =-DUSE_HAL_DRIVER -DSTM32F769xx -DDISCOVERY -DDATA_IN_ExtSDRAM #-DUI_LAYER_ACTIVE=0 -DUI_LAYER=0

CPU =-mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard 
GCCFLAGS =$(CPU) -Og -g -gdwarf-2 -Wall -std=c11 -fdata-sections -ffunction-sections -fstack-usage
GCCFLAGS +=$(SYMBOLS)
GCCFLAGS +=$(addprefix -I, $(INCLUDE))

# -lm -lnosys  --specs=nano.specs -u _printf_float  #-lgcc
LIBS =
LDFLAGS =$(CPU) -Wl,--gc-sections -Wl,-Map="$(EXEC:.elf=.map)"
LDFLAGS +=-T"$(ROOT_DIR)/$(TARGET_DIR)/core/STM32F769NIHX_FLASH.ld"
LDFLAGS += $(LIBS)

PREFIX =arm-none-eabi-
GCC = $(PREFIX)gcc
AS = $(PREFIX)as
LD = $(PREFIX)ld
OBJCOPY = $(PREFIX)objcopy
SIZE = $(PREFIX)size
NM = $(PREFIX)nm

VPATH +=$(ROOT_DIR)/src $(BSP_DIR)

VERBOSE ?=@

export GCC_COLORS='error=01;31:warning=01;35:note=01;36:caret=01;32:locus=01:quote=01'

all: $(BUILD_DIR) $(EXEC) size

program: all
	openocd -f ./core/stm32f769-disco.cfg -c "program $(EXEC) verify reset exit"

bin: $(EXEC)
	$(OBJCOPY) -O binary -j .startup -j .text -j .data -j .ram_code -j .rodata $< $(EXEC:.elf=.bin)

$(BUILD_DIR)/%.o: %.c #Makefile | $(BUILD_DIR)
	@echo Compile $<
	$(VERBOSE)$(GCC) -c $(GCCFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s #Makefile | $(BUILD_DIR)
	@echo Compile $<
	$(VERBOSE)$(GCC) -c $(GCCFLAGS) $< -o $@
	
$(EXEC): $(OBJS) #Makefile
	@echo "Linking"
	$(VERBOSE)$(GCC) $(OBJS) $(LDFLAGS) -o $@

size: $(EXEC)
	$(VERBOSE)$(SIZE) -B -x $<

bss-usage: $(EXEC)
	$(VERBOSE)$(NM) $< > $(BUILD_DIR)/bss.bu -S --size-sort

stack-usage: $(EXEC)
	@echo "" > $(BUILD_DIR)/stack.su
	@for file in $(BUILD_DIR)/*.su ; do cat $${file} >> $(BUILD_DIR)/stack.su.tmp; done
	@sort $(BUILD_DIR)/stack.su.tmp -n -k2 | column -t > $(BUILD_DIR)/stack.su
	@rm -f $(BUILD_DIR)/stack.su.tmp
#@sort $(BUILD_DIR)/stack.su.tmp -n -k2 > $(BUILD_DIR)/stack.su
#@column -t $(BUILD_DIR)/stack.su.tmp >  $(BUILD_DIR)/stack.su

clean:
#rm -f *.o *.elf
#rm -f $(SOURCE_DIR)/$(TARGET)
	$(VERBOSE)rm -r $(BUILD_DIR)

$(BUILD_DIR):
	$(VERBOSE)mkdir -p $@	