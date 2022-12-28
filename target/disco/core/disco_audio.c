#include "discovery.h"
#include "stm32f769i_discovery.h"
#include "stm32f769i_discovery_audio.h"
#include "wm8994.h"

#include "sys.h"

#define DISCO_AUDIO_USE_BSP		        0
#define DISCO_AUDIO_TEST_MODE           0
#define DISCO_AUDIO_DYNAMIC_BUFFER      0

#define DISCO_AUDIO_CHANNELS            2
#define DISCO_AUDIO_SAMPLES             512
#define DISCO_AUDIO_BUFFER_SAMPLES      (DISCO_AUDIO_SAMPLES * DISCO_AUDIO_CHANNELS * 2) // Double buffer
#define DISCO_AUDIO_BUFFER_SIZE         (DISCO_AUDIO_BUFFER_SAMPLES * sizeof(uint16_t)) // Byte size 
#define DISCO_AUDIO_HALF_BUFFER_SIZE    (DISCO_AUDIO_BUFFER_SIZE / 2)
#define DISCO_AUDIO_DEFAULT_VOLUME      5

#if !DISCO_AUDIO_USE_BSP
/* SAI peripheral configuration defines */
#define AUDIO_SAIx                           SAI1_Block_A

#define AUDIO_SAIx_CLK_ENABLE()              __HAL_RCC_SAI1_CLK_ENABLE()

#define AUDIO_SAIx_FS_GPIO_PORT              GPIOE
#define AUDIO_SAIx_FS_AF                     GPIO_AF6_SAI1
#define AUDIO_SAIx_FS_PIN                    GPIO_PIN_4

#define AUDIO_SAIx_SCK_GPIO_PORT             GPIOE
#define AUDIO_SAIx_SCK_AF                    GPIO_AF6_SAI1
#define AUDIO_SAIx_SCK_PIN                   GPIO_PIN_5

#define AUDIO_SAIx_SD_GPIO_PORT              GPIOE
#define AUDIO_SAIx_SD_AF                     GPIO_AF6_SAI1
#define AUDIO_SAIx_SD_PIN                    GPIO_PIN_6

#define AUDIO_SAIx_MCLK_GPIO_PORT            GPIOG
#define AUDIO_SAIx_MCLK_AF                   GPIO_AF6_SAI1
#define AUDIO_SAIx_MCLK_PIN                  GPIO_PIN_7

static DMA_HandleTypeDef hSaiDma;
static SAI_HandleTypeDef hSai;
static AUDIO_DrvTypeDef *audio_codec;
#endif


#if DISCO_AUDIO_DYNAMIC_BUFFER
static uint16_t     *audio_buffer;
#else
static uint8_t     audio_buffer[DISCO_AUDIO_BUFFER_SIZE];
#endif
static void (*audio_callback)(void *stream, uint32_t len);

#if DISCO_AUDIO_TEST_MODE
static uint16_t     *audio_ptr;
static uint16_t     *audio_ptr_end;

static void single_buffer_callback(void *stream, uint32_t len){
    int remaning_samples = (int)(audio_ptr_end - audio_ptr);
    uint16_t *dst = stream;
    uint16_t *src = audio_ptr;

    if(remaning_samples <= 0){        
        for(int i = 0; i < len; i++){
            dst[i] = 0;
        }
        audio_ptr = audio_ptr_end;
        return;
    }

    if(remaning_samples > len){
        remaning_samples = len;
    }    

    for(int i = 0; i < remaning_samples; i++){
        *dst++ = *src++;
    }

    while(remaning_samples < len){
        *dst++ = 0;
        remaning_samples++;
    }

    audio_ptr = src;
}

void DISCO_Audio_Test(void){
    discoaudio_t specs;   
  
    specs.freq = 32000;
	specs.channels = 2;
    specs.format = 16;
    specs.callback = single_buffer_callback;
    specs.volume = 5; // not same as 'volume' variable

    DISCO_Audio_Init(&specs);

#if DISCO_AUDIO_USE_BSP
	BSP_AUDIO_OUT_Play((uint16_t *)audio_buffer, DISCO_AUDIO_BUFFER_SAMPLES);
#else
	audio_ptr = (uint16_t*)audio_buffer;
    audio_ptr_end = (uint16_t*)(audio_buffer + 1024 * 2);
	#if 0
	while(audio_ptr < audio_ptr_end){
		if((SAI1_Block_A->SR >> 16) < 5){			
			SAI1_Block_A->DR = *(uint16_t*)audio_ptr++;
		}
	}
	#endif
#endif
  // TODO: FIX for byte size
    for(int i = 0; i < 1024; i++){
        audio_buffer[i] = i * 64;
    }

    for(int i = 1024; i > 0; i--){
        audio_buffer[2048 - i] = i * 64;
    }
    while(1){}
}
#endif

#if !DISCO_AUDIO_USE_BSP
static void DISCO_Audio_Init_LL(discoaudio_t *spec, SAI_HandleTypeDef *handle, DMA_HandleTypeDef *hdma)
{
    /* Configure I2S PLL */
    RCC_PeriphCLKInitTypeDef RCC_PeriphCLKInitStruct = {0};

	RCC_PeriphCLKInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SAI1;
	RCC_PeriphCLKInitStruct.Sai1ClockSelection   = RCC_SAI1CLKSOURCE_PLLI2S;

    if ((spec->freq == AUDIO_FREQUENCY_11K) || (spec->freq == AUDIO_FREQUENCY_22K) || (spec->freq == AUDIO_FREQUENCY_44K)){
        /* PLLI2S_VCO: VCO_429M 
        SAI_CLK(first level) = PLLSAI_VCO/PLLSAIQ = 429/2 = 214.5 Mhz
        SAI_CLK_x = SAI_CLK(first level)/PLLSAIDIVQ = 214.5/19 = 11.289 Mhz */
        RCC_PeriphCLKInitStruct.PLLI2S.PLLI2SN = 429;
        RCC_PeriphCLKInitStruct.PLLI2S.PLLI2SQ = 2;
        RCC_PeriphCLKInitStruct.PLLI2SDivQ = 19;
    } else { /* AUDIO_FREQUENCY_8K, AUDIO_FREQUENCY_16K, AUDIO_FREQUENCY_48K, AUDIO_FREQUENCY_96K */
        /* PLLSAI_VCO: VCO_344M 
        SAI_CLK(first level) = PLLSAI_VCO/PLLSAIQ = 344/7 = 49.142 Mhz 
        SAI_CLK_x = SAI_CLK(first level)/PLLSAIDIVQ = 49.142/1 = 49.142 Mhz */
        RCC_PeriphCLKInitStruct.PLLI2S.PLLI2SN = 344;
        RCC_PeriphCLKInitStruct.PLLI2S.PLLI2SQ = 7;
        RCC_PeriphCLKInitStruct.PLLI2SDivQ = 1;
    }

    if (HAL_RCCEx_PeriphCLKConfig(&RCC_PeriphCLKInitStruct) != HAL_OK){
        Sys_Error("AUDIO: Error setting SAI clock\n");
    }

    /* Initialize SAI for I2S mode */
    __HAL_SAI_RESET_HANDLE_STATE(handle);

    handle->Instance = AUDIO_SAIx;

    __HAL_SAI_DISABLE(handle);

    handle->Init.AudioFrequency = spec->freq;
    handle->Init.AudioMode      = SAI_MODEMASTER_TX;
    handle->Init.Synchro        = SAI_ASYNCHRONOUS;
    handle->Init.OutputDrive    = SAI_OUTPUTDRIVE_ENABLE;
    handle->Init.NoDivider      = SAI_MASTERDIVIDER_ENABLE;
    handle->Init.FIFOThreshold  = SAI_FIFOTHRESHOLD_1QF;
    handle->Init.Protocol       = SAI_FREE_PROTOCOL;
    handle->Init.DataSize       = SAI_DATASIZE_16;             // only 16 bits inside slot are data
    handle->Init.FirstBit       = SAI_FIRSTBIT_MSB;
    handle->Init.ClockStrobing  = SAI_CLOCKSTROBING_FALLINGEDGE;

    handle->FrameInit.FrameLength       = 32;                 // Frame size = 32 BCLK
    handle->FrameInit.ActiveFrameLength = 16;                 // FS is active half time
    handle->FrameInit.FSDefinition      = SAI_FS_CHANNEL_IDENTIFICATION;
    handle->FrameInit.FSPolarity        = SAI_FS_ACTIVE_LOW;
    handle->FrameInit.FSOffset          = SAI_FS_BEFOREFIRSTBIT;

    handle->SlotInit.FirstBitOffset = 0;                      // First bit on second rising edge BLCK following FS transition
    handle->SlotInit.SlotSize       = SAI_SLOTSIZE_DATASIZE;  // Size of each slot
    handle->SlotInit.SlotNumber     = spec->channels;         // One slot per channel
    handle->SlotInit.SlotActive     = (spec->channels == 2) ? (SAI_SLOTACTIVE_0 | SAI_SLOTACTIVE_1) : SAI_SLOTACTIVE_0;

    if (HAL_OK != HAL_SAI_Init(handle)) {
        Sys_Error("AUDIO: Error initialising SAI\n");
    }

	/* Enable SAI to generate clock used by audio driver */
   	__HAL_SAI_ENABLE(handle);

	/* Initialize audio codec */
	audio_codec = AUDIO_Get_Driver();

    if (WM8994_ID != audio_codec->ReadID(AUDIO_I2C_ADDRESS)) {
        Sys_Error("SND: Error communicating with codec\n");
    }

    audio_codec->Reset(AUDIO_I2C_ADDRESS);

    if (audio_codec->Init(AUDIO_I2C_ADDRESS, OUTPUT_DEVICE_AUTO, spec->volume, spec->freq) != 0) {
        Sys_Error("SND: initialising codec\n");
    }

    /* Start the playback */
    if (audio_codec->Play(AUDIO_I2C_ADDRESS, NULL, 0) != 0) {
        Sys_Error("SND: Error starting codec playback\n");
    }

    spec->buf = audio_buffer;
    spec->size = DISCO_AUDIO_SAMPLES;
    audio_callback = spec->callback;

    HAL_SAI_Transmit_DMA(&hSai, (uint8_t *)spec->buf, DISCO_AUDIO_BUFFER_SAMPLES);
}
#endif


void DISCO_Audio_Init(discoaudio_t *spec)
{
#if DISCO_AUDIO_DYNAMIC_BUFFER
    audio_buffer = (uint8_t*)malloc(DISCO_AUDIO_BUFFER_SIZE);
    if(!audio_buffer){
        return;
    }
#endif
    
    if(spec->callback == NULL)
        return;

    spec->volume = DISCO_AUDIO_DEFAULT_VOLUME;

#if DISCO_AUDIO_USE_BSP
	BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_AUTO, spec->volume, spec->freq);
    BSP_AUDIO_OUT_SetAudioFrameSlot(SAI_SLOTACTIVE_0 | SAI_SLOTACTIVE_2);
#else
    DISCO_Audio_Init_LL(spec, &hSai, &hSaiDma);

    GPIOC->MODER = (5 << 12);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);
#endif
}


#if !DISCO_AUDIO_USE_BSP
/**
  * @brief  SAI MSP Init.
  * @param  hsai : pointer to a SAI_HandleTypeDef structure that contains
  *                the configuration information for SAI module.
  * @retval None
  */
void HAL_SAI_MspInit(SAI_HandleTypeDef *hsai)
{
    GPIO_InitTypeDef GPIO_Init;

    /* Enable SAI1 clock */
    __HAL_RCC_SAI1_CLK_ENABLE();

    /* Configure GPIOs
	 * PE3    SAI1_B SD		DI
	 * PE4    SAI1_A FS		LRCLK
	 * PE5	  DAI1_A SCK	BCLK
	 * PE6	  SAI1_A SD		DO
	 * PG7	  SAI1_A MCLK	MCLK
	 * PJ12	  Codec INT
	 * */
   	__HAL_RCC_GPIOG_CLK_ENABLE();
	__HAL_RCC_GPIOE_CLK_ENABLE();

    GPIO_Init.Mode = GPIO_MODE_AF_PP;
    GPIO_Init.Pull = GPIO_NOPULL;
    GPIO_Init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

    GPIO_Init.Alternate = AUDIO_SAIx_FS_AF;
    GPIO_Init.Pin = AUDIO_SAIx_FS_PIN;
    HAL_GPIO_Init(AUDIO_SAIx_FS_GPIO_PORT, &GPIO_Init);
    GPIO_Init.Alternate = AUDIO_SAIx_SCK_AF;
    GPIO_Init.Pin = AUDIO_SAIx_SCK_PIN;
    HAL_GPIO_Init(AUDIO_SAIx_SCK_GPIO_PORT, &GPIO_Init);
    GPIO_Init.Alternate = AUDIO_SAIx_SD_AF;
    GPIO_Init.Pin = AUDIO_SAIx_SD_PIN;
    HAL_GPIO_Init(AUDIO_SAIx_SD_GPIO_PORT, &GPIO_Init);
    GPIO_Init.Alternate = AUDIO_SAIx_MCLK_AF;
    GPIO_Init.Pin = AUDIO_SAIx_MCLK_PIN;
    HAL_GPIO_Init(AUDIO_SAIx_MCLK_GPIO_PORT, &GPIO_Init);

	if (hsai->Instance == AUDIO_SAIx)
    {
        hSaiDma.Init.Channel = DMA_CHANNEL_10;
        hSaiDma.Init.Direction = DMA_MEMORY_TO_PERIPH;
        hSaiDma.Init.PeriphInc = DMA_PINC_DISABLE;
        hSaiDma.Init.MemInc = DMA_MINC_ENABLE;
        hSaiDma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
        hSaiDma.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
        hSaiDma.Init.Mode = DMA_CIRCULAR;
        hSaiDma.Init.Priority = DMA_PRIORITY_HIGH;
        hSaiDma.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
        hSaiDma.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
        hSaiDma.Init.MemBurst = DMA_MBURST_SINGLE;
        hSaiDma.Init.PeriphBurst = DMA_PBURST_SINGLE;

        /* Select the DMA instance to be used for the transfer : DMA2_Stream6 */
        hSaiDma.Instance = DMA2_Stream6;

        /* Associate the DMA handle */
        __HAL_LINKDMA(hsai, hdmatx, hSaiDma);

        /* Deinitialize the Stream for new transfer */
        HAL_DMA_DeInit(&hSaiDma);

        /* Configure the DMA Stream */
        if (HAL_OK != HAL_DMA_Init(&hSaiDma)) {
            Sys_Error("SND: Fail to initialise SAI DMA\n");
        }
    	
		HAL_NVIC_SetPriority(DMA2_Stream6_IRQn, 0x01, 0);
    	HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);
    }

}
#endif

#if DISCO_AUDIO_USE_BSP
void BSP_AUDIO_OUT_TransferComplete_CallBack(void)
{
   
}

void BSP_AUDIO_OUT_HalfTransfer_CallBack(void)
{

}

void DMA2_Stream1_IRQHandler(void)
{
    extern SAI_HandleTypeDef haudio_out_sai;
    HAL_DMA_IRQHandler(haudio_out_sai.hdmatx);
}
#else
void DMA2_Stream6_IRQHandler(void)
{
    DMA_HandleTypeDef *hdma = &hSaiDma;
    uint32_t tmpisr = DMA2->HISR;

    if (tmpisr & ((DMA_FLAG_TEIF0_4 | DMA_FLAG_FEIF0_4 | DMA_FLAG_DMEIF0_4) << hdma->StreamIndex)){
        //regs->LIFCR = (tmpisr & (DMA_FLAG_TEIF0_4 | DMA_FLAG_FEIF0_4 | DMA_FLAG_DMEIF0_4)) << hdma->StreamIndex;
    }

    if (tmpisr & (DMA_FLAG_HTIF0_4 << hdma->StreamIndex)){
        DMA2->HIFCR = DMA_FLAG_HTIF0_4 << hdma->StreamIndex;
        audio_callback(audio_buffer, DISCO_AUDIO_HALF_BUFFER_SIZE);
        #if DISCO_AUDIO_TEST_MODE
		HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_7);
        #endif
    }

    if (tmpisr & (DMA_FLAG_TCIF0_4 << hdma->StreamIndex)){
        DMA2->HIFCR = DMA_FLAG_TCIF0_4 << hdma->StreamIndex;
        audio_callback(audio_buffer + DISCO_AUDIO_HALF_BUFFER_SIZE, DISCO_AUDIO_HALF_BUFFER_SIZE);
        #if DISCO_AUDIO_TEST_MODE
		HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_6);
        #endif
    }
}
#endif

void SNDDMA_Submit(void)
{
}

void SNDDMA_Shutdown(void)
{
}

int SNDDMA_GetDMAPos(void)
{
    DMA_HandleTypeDef *hdma = &hSaiDma;
    return DISCO_AUDIO_BUFFER_SAMPLES - hdma->Instance->NDTR;
}