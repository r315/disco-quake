# Discovery Quake

Does it run Doom? sure!! and Quake? yep!!

![In game](/sd/quake/ingame.jpg)

Discovery Quake is a port of Quake game from id Software released in 1997 to ST stm32f769i-discovery development kit.
It is based on sdl Quake 1.09 an is a wip project and at time of writing this readme it is running with working touch controller and audio.
As expected it does not run smoothly, average 15FPS with dynamic lighting disabled by cvar v_dlights "0" on config.cfg. 

Window resize is limited from 320x110 to 320x120 pixel, smaller sizes don't make sense for the display. Status bar can be always visible
with cvar sb_alwayson "1" and background removed by sb_transparent "1".

## Build

### Requirements

- STM32CubeMX or STM32CubeIDE
- STM32Cube_FW_F7_V1.16.0 library
- arm-none-eabi-gcc 9.2.1
- openocd v0.10.0

Clone repository and open on terminal
```
$ cd target/disco
$ make
$ make program
```

A x86_x64 target can be compiled as well but may require some SDL library configuration.

## Running

To run the game, a copy of original game pack0.pak and pack1.pak or shareware pack0.pack is required.

Program stm32f769i-discovery devkit with above command and copy all files under folder sd to an SD Card formated to FAT32.
Place your *.pak files into id1 directory on the SD Card.
Insert SD Card on devkit and press reset, if a serial terminal is connected console output can be visible.

### Other

Yet to be implemented

- Function/num keys, bitmap is visible but touch is not yet implemented to read them.
- Maybe optimize rendering to improve speed.
- Implement audio tracks.


