/**
 ******************************************************************************
 * @file      sysmem.c
 * @author    Auto-generated by STM32CubeIDE
 * @brief     STM32CubeIDE Minimal System Memory calls file
 *
 *            For more information about which c-functions
 *            need which of these lowlevel functions
 *            please consult the Newlib libc-manual
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */

/* Includes */
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include "discovery.h"

#define MAX_HEAP_SIZE    (SDRAM_SIZE - LCD_FB_SIZE)    /* 16 - 3 MBytes */

/* Variables */
extern int errno;
static uint8_t *heap_end = NULL;

/* Functions */
int sysmem_free(void){
	return (heap_end == NULL)? 0 : (int)((SDRAM_BASE_ADDR + MAX_HEAP_SIZE) - (uint32_t)heap_end);
}

int sysmem_used(void){
	return (heap_end == NULL)? 0 : (int)(heap_end - SDRAM_BASE_ADDR);
}

int sysmem_total(void){
	return MAX_HEAP_SIZE;
}
/**
 * Heap grows up
 * 
 * +------------------+
 * |      LCD FB      |
 * +------------------+  SDRAM BASE + HEAP_SIZE (0xC0D12000)
 * |                  |
 * |                  | <- heap_end 
 * |                  |
 * +------------------+  SDRAM BASE
 */
caddr_t _sbrk(int incr)
{
	uint8_t *prev_heap_end;	

	// Init heap to base memory
	if (heap_end == NULL){
		heap_end = (uint8_t*)SDRAM_BASE_ADDR;
	}

	if(incr == 0){
		return (caddr_t)heap_end;
	}

	prev_heap_end = heap_end;

	if ((uint32_t)(heap_end + incr) >= (SDRAM_BASE_ADDR + MAX_HEAP_SIZE)){
		errno = ENOMEM;
		return (caddr_t)-1;
	}

	heap_end += incr;

	return (caddr_t)prev_heap_end;
}

