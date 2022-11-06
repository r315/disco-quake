
#ifndef _fifo_h_
#define _fifo_h_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct {
	uint32_t head;
	uint32_t tail;
	uint32_t size;
	uint8_t	*buf;
} fifo_t;

void fifo_init(fifo_t *fifo);

/**
 * Discard all data on fifo
 *
 * @param fifo     fifo to be discarted
 *
 * */
void fifo_flush(fifo_t *fifo);

/**
 * Adds a character to the fifo
 *
 * @param fifo:    destination fifo
 * @param c:       character to put
 *
 * returns         1 success, 0 fifo full
 * */
uint32_t fifo_put(fifo_t *fifo, uint8_t c);

/**
 *  retrives one character from the given fifo if fifo
 *  is not empty
 *
 * @param fifo:		pointer to target fifo
 * @param pc:		placeholder for retrieved char
 * returns			1 if one char was placed on placeholder, 0 no data on fifo
 * */
uint32_t fifo_get(fifo_t *fifo, uint8_t *pc);

/**
 * get the number of bytes on fifo
 * 
 * @param fifo:		pointer to target fifo
 * returns			number of bytes on fifo, 0 if empty
 * */
uint32_t fifo_avail(fifo_t *fifo);

/**
 * get the number of available bytes on fifo
 * 
 * @param fifo:		pointer to target fifo
 * returns			number of bytes available on fifo, 0 if full
 * */
uint32_t fifo_free(fifo_t *fifo);

/**
 * Observe data on fifo head
 * 
 * @param fifo:		pointer to target fifo
 * return : 		value pointed by fifo head
 * */
uint32_t fifo_peek(fifo_t *fifo);

#ifdef __cplusplus
}
#endif

#endif
