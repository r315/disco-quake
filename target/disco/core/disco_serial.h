#ifndef _disco_serial_h_
#define _disco_serial_h_

#include <stdint.h>

uint8_t SERIAL_GetChar(uint8_t *c);
void SERIAL_Init(void);
void SERIAL_DeInit(void);
uint32_t SERIAL_Write(uint8_t *data, uint32_t len);
#endif