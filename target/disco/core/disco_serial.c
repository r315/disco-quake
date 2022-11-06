#include <stdlib.h>
#include "stm32f7xx_hal.h"
#include "fifo.h"

#ifdef __GNUC__
/* With GCC, small printf (option LD Linker->Libraries->Small printf
   set to 'Yes') calls __io_putchar() */
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif /* __GNUC__ */

#define UART_FIFO_SIZE      256

static UART_HandleTypeDef huart;
static fifo_t txfifo;
static fifo_t rxfifo;
//const uint8_t hex_tbl[] = "0123456789ABCDEF";

void SERIAL_Init(void){
 GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    /* USER CODE BEGIN USART1_MspInit 0 */

    /* USER CODE END USART1_MspInit 0 */
    /* Peripheral clock enable */
    __HAL_RCC_USART1_CLK_ENABLE();
  
    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART1 GPIO Configuration    
        PA10    ------> USART1_RX
        PA9     ------> USART1_TX 
    */

    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    huart.Instance = USART1;
    huart.Init.BaudRate = 115200;
    huart.Init.WordLength = UART_WORDLENGTH_8B;
    huart.Init.StopBits = UART_STOPBITS_1;
    huart.Init.Parity = UART_PARITY_NONE;
    huart.Init.Mode = UART_MODE_TX_RX;
    huart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart.Init.OverSampling = UART_OVERSAMPLING_16;
    huart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart) != HAL_OK)
    {
        while(1){
        /* */
        }
    }

    rxfifo.buf = (uint8_t*)malloc(UART_FIFO_SIZE);
    txfifo.buf = (uint8_t*)malloc(UART_FIFO_SIZE);

    if((rxfifo.buf == NULL) || (txfifo.buf == NULL)){
        while(1){
        /* */
        }
    }

    rxfifo.size = UART_FIFO_SIZE;
	txfifo.size = UART_FIFO_SIZE;

	fifo_init(&txfifo);
	fifo_init(&rxfifo);
	fifo_flush(&txfifo);
	fifo_flush(&rxfifo);

	HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(USART1_IRQn);

	SET_BIT(huart.Instance->CR1, USART_CR1_RXNEIE);
}

void SERIAL_DeInit(void){
    free(rxfifo.buf);
    free(txfifo.buf);
}

uint8_t SERIAL_GetChar(uint8_t *c){
    return fifo_get(&rxfifo, c);
}

uint32_t SERIAL_Write(uint8_t *data, uint32_t len) {
	uint32_t sent = 0;
	while(len--){
		if(fifo_free(&txfifo) == 0){
			SET_BIT(huart.Instance->CR1, USART_CR1_TXEIE);
			while(fifo_free(&txfifo) == 0);
		}
		fifo_put(&txfifo, *data++);
		sent++;
	}

	SET_BIT(huart.Instance->CR1, USART_CR1_TXEIE);

	return sent;
}

PUTCHAR_PROTOTYPE {
    SERIAL_Write((uint8_t *)&ch, 1);
    return ch;
}

/**
 * @brief USART 1 Interupt handler
 * 
 */
void USART1_IRQHandler(void){
    uint32_t isrflags = huart.Instance->ISR;
	uint32_t cr1its = huart.Instance->CR1;
    uint8_t sym;

	/* If no error occurs */
	uint32_t errorflags = isrflags & (uint32_t)(
			USART_ISR_PE | USART_ISR_FE | USART_ISR_NE | USART_ISR_ORE | USART_ISR_RTOF);

	if (errorflags == 0U) {
		if (((isrflags & USART_ISR_RXNE) != 0U)
				&& ((cr1its & USART_CR1_RXNEIE) != 0U)) {
                    sym = READ_REG(huart.Instance->RDR);
			fifo_put(&rxfifo, (uint8_t) sym);

            //huart.Instance->TDR = hex_tbl[sym >> 4];
            //while(!( huart.Instance->ISR & USART_ISR_TXE));
            //huart.Instance->TDR = hex_tbl[sym & 15];
            //huart.Instance->TDR = sym;
            //while(!( huart.Instance->ISR & USART_ISR_TXE));
		}

		if (((isrflags & USART_ISR_TXE) != 0U)
				&& ((cr1its & USART_CR1_TXEIE) != 0U)) {
			if (fifo_get(&txfifo, (uint8_t*) &huart.Instance->TDR) == 0U) {
				/* No data transmitted, disable TXE interrupt */
				CLEAR_BIT(huart.Instance->CR1, USART_CR1_TXEIE);
			}
		}
	} else {
		fifo_flush(&rxfifo);
        fifo_flush(&txfifo);
        huart.Instance->ICR = errorflags;
	}
}