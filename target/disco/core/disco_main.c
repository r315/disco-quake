#include "discovery.h"
#include "stm32f769i_discovery.h"
#include "stm32f769i_discovery_sdram.h"
#include "stm32f7xx_hal.h"
#include "disco_serial.h"
#include "quakedef.h"

typedef struct {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t psr;
}stackframe_t;

static void disco_ButtonInit(void);
static void disco_ClockConfig (void);
static void disco_init (void);
static void disco_MpuConfig (void);

int Sys_main(int argc, char **argv);

int main(void){
    const int argc = 3;

    disco_init();
    
    const char *argv[3] = {
        "quake",
        "-basedir",
        "quake"
    };

    Sys_main(argc, (char**)argv);
    
    return 0;
}

static void disco_init(void){
    SCB_EnableICache();
    SCB_EnableDCache();
    HAL_Init();
    disco_ClockConfig();
    BSP_SDRAM_Init();
    disco_MpuConfig();
    SERIAL_Init();
    disco_ButtonInit();
}

static void disco_ButtonInit(void)
{
    GPIO_InitTypeDef gpio_init_structure;
    
    gpio_init_structure.Pin = GPIO_PIN_0;
    gpio_init_structure.Pull = GPIO_NOPULL;
    gpio_init_structure.Speed = GPIO_SPEED_FAST;

    gpio_init_structure.Mode = GPIO_MODE_IT_RISING | GPIO_MODE_IT_FALLING;

    HAL_GPIO_Init(GPIOA, &gpio_init_structure);

    /* Enable and set Button EXTI Interrupt to the lowest priority */
    HAL_NVIC_SetPriority((IRQn_Type)(EXTI0_IRQn), 0x0F, 0x00);
    HAL_NVIC_EnableIRQ((IRQn_Type)(EXTI0_IRQn));
}

static void disco_ClockConfig(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
    RCC_OscInitTypeDef RCC_OscInitStruct;
    HAL_StatusTypeDef ret = HAL_OK;

    /* Enable HSE Oscillator and activate PLL with HSE as source */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 25;
    RCC_OscInitStruct.PLL.PLLN = 400;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 8;
    RCC_OscInitStruct.PLL.PLLR = 7;

    ret = HAL_RCC_OscConfig(&RCC_OscInitStruct);
    if(ret != HAL_OK)
    {
        while(1) { ; }
    }

    /* Activate the OverDrive to reach the 200 MHz Frequency */
    ret = HAL_PWREx_EnableOverDrive();
    if(ret != HAL_OK)
    {
        while(1) { ; }
    }

    /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 clocks dividers */
    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    ret = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_6);
    if(ret != HAL_OK)
    {
        while(1) { ; }
    }
}


static void disco_MpuConfig (void) 
{
  MPU_Region_InitTypeDef MPU_InitStruct;

  /* Disable the MPU */
  HAL_MPU_Disable();

  /* Configure the MPU attributes for SDRAM */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.BaseAddress = SDRAM_BASE_ADDR;
  MPU_InitStruct.Size = MPU_REGION_SIZE_16MB;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE; //MPU_ACCESS_NOT_BUFFERABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE; //MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE; //MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* Enable the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
  /* Enable div zero hard fault */
  SCB->CCR |= 0x10;
}

void HAL_MspInit(void)
{
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
}


void SysTick_Handler(void)
{
  HAL_IncTick();
}

void EXTI0_IRQHandler (void)
{
    if(__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_0) == SET)
    {
        if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET){
            Key_Event(0x1b, false);
        }else{
            Key_Event(0x1b, true);
        }
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_0);
    }
}

/*
void LTDC_IRQHandler(void)
{
	HAL_LTDC_IRQHandler(&hltdc_discovery);
}
*/ 
void MemManage_Handler(void){
    __asm volatile(
        "bkpt #01 \n"
        "b . \n"
    );   
}

void BusFault_Handler(void){
    __asm volatile(
        "bkpt #01 \n"
        "b . \n"
    );   
}

void UsageFault_Handler(void){
    __asm volatile(
        "bkpt #01 \n"
        "b . \n"
    );   
}

void Stack_Dump(stackframe_t *stack){
    GPIOJ->MODER = (1 << 26);
    HAL_GPIO_WritePin(GPIOJ, GPIO_PIN_13, GPIO_PIN_SET);

    __asm volatile(
        "bkpt #01 \n"
        "b . \n"
    );
}
