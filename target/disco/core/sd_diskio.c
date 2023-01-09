/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    sd_diskio.c (based on sd_diskio_dma_rtos_template.c v2.1.0 as FreeRTOS is enabled)
  * @brief   SD Disk I/O driver
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* USER CODE BEGIN firstSection */
/* can be used to modify / undefine following code or add new definitions */
/* USER CODE END firstSection*/

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include <stdio.h>

#include "ff_gen_drv.h"
#include "sd_diskio.h"
#include "stm32f7xx_hal.h"
//#include "debug.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

/*
 * the following Timeout is useful to give the control back to the applications
 * in case of errors in either BSP_SD_ReadCpltCallback() or BSP_SD_WriteCpltCallback()
 * the value by default is as defined in the BSP platform driver otherwise 30 secs
 */
#define SD_TIMEOUT (5 * 1000)
#define SD_DEFAULT_BLOCK_SIZE 512
#define ENABLE_SD_DMA_CACHE_MAINTENANCE 1   // Required for DMA

#define DISCO_SD_READ_USE_DMA     1
#define DISCO_SD_WRITE_USE_DMA    1

/* Private variables ---------------------------------------------------------*/
static uint32_t scratch[BLOCKSIZE / 4];

/* Disk status */
static volatile DSTATUS Stat = STA_NOINIT;
static volatile uint32_t WriteStatus = 0, ReadStatus = 0, TransferError = 0;
/* Private function prototypes -----------------------------------------------*/
static DSTATUS SD_CheckStatus(BYTE lun);
DSTATUS SD_initialize(BYTE);
DSTATUS SD_status(BYTE);
DRESULT SD_read(BYTE, BYTE *, DWORD, UINT);
DRESULT SD_write(BYTE, const BYTE *, DWORD, UINT);
DRESULT SD_ioctl(BYTE, BYTE, void *);
/* Public variables -----------------------------------------------*/
const Diskio_drvTypeDef SD_Driver = {
        SD_initialize,
        SD_status,
        SD_read,
        SD_write,
        SD_ioctl,
};

/* External variables-----------------------------------------------*/
extern SD_HandleTypeDef uSdHandle;

/* USER CODE BEGIN beforeFunctionSection */
/* can be used to modify / undefine following code or add new code */
/* USER CODE END beforeFunctionSection */

/* Private functions ---------------------------------------------------------*/

static const char *sd_errors[] = {
    "SD_ERROR_CMD_CRC_FAIL", 
    "SD_ERROR_DATA_CRC_FAIL", 
    "SD_ERROR_CMD_RSP_TIMEOUT", 
    "SD_ERROR_DATA_TIMEOUT", 
    "SD_ERROR_TX_UNDERRUN", 
    "SD_ERROR_RX_OVERRUN", 
    "SD_ERROR_ADDR_MISALIGNED", 
    "SD_ERROR_BLOCK_LEN_ERR", 
    "SD_ERROR_ERASE_SEQ_ERR", 
    "SD_ERROR_BAD_ERASE_PARAM", 
    "SD_ERROR_WRITE_PROT_VIOLATION", 
    "SD_ERROR_LOCK_UNLOCK_FAILED",
    "SD_ERROR_COM_CRC_FAILED", 
    "SD_ERROR_ILLEGAL_CMD", 
    "SD_ERROR_CARD_ECC_FAILED", 
    "SD_ERROR_CC_ER", 
    "SD_ERROR_GENERAL_UKNOWN_ERR", 
    "SD_ERROR_STREAM_READ_UNDERRUN",
    "SD_ERROR_STREAM_WRITE_OVERRUN", 
    "SD_ERROR_CID_CSD_OVERWRITE", 
    "SD_ERROR_WP_ERASE_SKIP", 
    "SD_ERROR_CARD_ECC_DISABLED",
    "SD_ERROR_ERASE_RESET", 
    "SD_ERROR_AKE_SEQ_ERR", 
    "SD_ERROR_INVALID_VOLTRANGE", 
    "SD_ERROR_REQUEST_NOT_APPLICABLE", 
    "SD_ERROR_PARAM", 
    "SD_ERROR_UNSUPPORTED_FEATURE",
    "SD_ERROR_BUSY", 
    "SD_ERROR_DMA", 
    "SD_ERROR_TIMEOUT"
};

static DRESULT sd_error(void){   
    for(uint8_t bit = 0; bit < 32; bit++){
        if(uSdHandle.ErrorCode & (1<<bit))
            printf("sd_diskio: %s\n", sd_errors[bit]);
    }

    return uSdHandle.ErrorCode;
}

/**
  * @brief  Initializes a Drive
  * @param  timeout : timeout value in ms
  * @retval int: Operation status
  */
static int SD_CheckStatusWithTimeout(uint32_t timeout)
{
    uint32_t timer = HAL_GetTick();
    /* block until SDIO IP is ready again or a timeout occur */
    while (HAL_GetTick() - timer < timeout)
    {
        if (BSP_SD_GetCardState() == SD_TRANSFER_OK)
        {
            return 0;
        }
    }

    return -1;
}

/**
  * @brief  Initializes a Drive
  * @param  lun : not used
  * @retval DSTATUS: Operation status
  */
static DSTATUS SD_CheckStatus(BYTE lun)
{
    Stat = STA_NOINIT;

    if (BSP_SD_GetCardState() == MSD_OK)
    {
        Stat &= ~STA_NOINIT;
    }

    return Stat;
}
/* Public functions ------------------------------------------------*/
/**
  * @brief  Initializes a Drive
  * @param  lun : not used
  * @retval DSTATUS: Operation status
  */
DSTATUS SD_initialize(BYTE lun)
{
#if !defined(DISABLE_SD_INIT)
    HAL_SD_CardInfoTypeDef ci;
    
    uint8_t res = BSP_SD_Init();

    if(res == MSD_OK){
        printf("\nSd card initialized\n");
        BSP_SD_GetCardInfo(&ci);
        printf("\tType: %x\n", (int)ci.CardType);
        printf("\tVersion: %x\n", (int)ci.CardVersion);
        printf("\tClass: %x\n", (int)ci.Class);
        printf("\tRelative address: %x\n", (int)ci.RelCardAdd);
        printf("\tNumber of blocks: %x, (%d)\n", (int)ci.BlockNbr, (int)ci.BlockNbr);
        printf("\tBlock Size: %d\n", (int)ci.BlockSize);
        printf("\tLogical Number of blocks: %x, (%d)\n", (int)ci.LogBlockNbr, (int)ci.LogBlockNbr);
        printf("\tLogical Block Size: %d\n\n", (int)ci.LogBlockSize);

        Stat = SD_CheckStatus(lun);       
    
    }else if(res == MSD_ERROR_SD_NOT_PRESENT){
        printf("SD card not present\n");
    }else{
        printf("SD card error\n");
    }
#else
    Stat = SD_CheckStatus(lun);
#endif
    return Stat;
}

/**
  * @brief  Gets Disk Status
  * @param  lun : not used
  * @retval DSTATUS: Operation status
  */
DSTATUS SD_status(BYTE lun)
{
    return SD_CheckStatus(lun);
}

/**
 * @brief 
 * 
 * @param data 
 * @param sector 
 * @param count 
 * @return DRESULT 
 */
static DRESULT read_sector(uint32_t *data, uint32_t sector, uint32_t count){
    uint32_t timeout;

#if DISCO_SD_READ_USE_DMA
    ReadStatus = 0;
    
    if(BSP_SD_ReadBlocks_DMA(data, sector, count) == MSD_OK){
        /* Wait that read process is completed or a timeout occurs */
        timeout = HAL_GetTick();

        while(ReadStatus == 0){
            if ((HAL_GetTick() - timeout) >= SD_TIMEOUT) {
                /* Timeout occurred */
                goto sd_err;
            }        
        }

        /* End of transfer interrupt occurred, check card status */
        timeout = HAL_GetTick();
        do {
            if (BSP_SD_GetCardState() == SD_TRANSFER_OK) {
                /* All good */
                return RES_OK;                
            }
        }while ((HAL_GetTick() - timeout) < SD_TIMEOUT);
    }
#else
    /* Blocking  */
    if(BSP_SD_ReadBlocks(data, sector, count, SD_TIMEOUT) == MSD_OK){
        /* Wait that writing process is completed or a timeout occurs */
        timeout = HAL_GetTick();
        do {
            if (BSP_SD_GetCardState() == SD_TRANSFER_OK) {
                /* All good */
                return RES_OK;                
            }
        }while ((HAL_GetTick() - timeout) < SD_TIMEOUT);
    }
#endif
sd_err:    
    return sd_error();
}

/**
  * @brief  Reads Sector(s)
  * @param  lun : not used
  * @param  *buff: Data buffer to store read data
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to read (1..128)
  * @retval DRESULT: Operation result
  */
DRESULT SD_read(BYTE lun, BYTE *dest, DWORD sector, UINT count)
{
#if DISCO_SD_READ_USE_DMA
    if (SD_CheckStatusWithTimeout(SD_TIMEOUT) < 0) {
        return RES_ERROR;
    }
#endif
    /* Check if destination is 32 bit aligned */
    if (!((uint32_t)dest & 0x3)) {
        return read_sector((uint32_t*)dest, sector, count);
    }else{
        while(count--){
            if(read_sector((uint32_t*)scratch, sector++, 1) != RES_OK){
                return RES_ERROR;
            }
#if (ENABLE_SD_DMA_CACHE_MAINTENANCE == 1) && (DISCO_SD_READ_USE_DMA == 1)
            /* Invalidate the scratch buffer before the next
                read to get the actual data instead of the cached one */
            SCB_InvalidateDCache_by_Addr((uint32_t*)scratch, BLOCKSIZE);
#endif
            /* Copy sector data to buffer */
            for(uint32_t i = 0; i < BLOCKSIZE; i++, dest++){
                *dest = ((BYTE*)scratch)[i];
            }
        }
    }
    return RES_OK;
}

/**
 * @brief Reads a sector using DMA. On SD max speed CPU cannot fill 
 * SDMMC controller fifo fast enough
 * 
 * @param data      : Data to be written, must be 32bit aligned 
 * @param sector    : sector number
 * @param count     : number of sectors to be written
 * 
 * @return DRESULT 
 */
static DRESULT write_sector(uint32_t *data, uint32_t sector, uint32_t count){    
    uint32_t timeout;
#if DISCO_SD_WRITE_USE_DMA
    WriteStatus = 0;
    
    if(BSP_SD_WriteBlocks_DMA(data, sector, count) == MSD_OK){
        /* Wait that writing process is completed or a timeout occurs */
        timeout = HAL_GetTick();

        while (WriteStatus == 0) {
            if((HAL_GetTick() - timeout) >= SD_TIMEOUT){
                /* Timeout occurred */
                goto sd_err;
            }
        }

        /* End of transfer interrupt occurred, check card status */
        timeout = HAL_GetTick();
        do{
            if (BSP_SD_GetCardState() == SD_TRANSFER_OK) {
                /* All good */
                return RES_OK;                
            }
        }while ((HAL_GetTick() - timeout) < SD_TIMEOUT);
    }
#else /* DISCO_SD_USE_DMA */
    int retry = 3;
    /* Blocking  */    
    do{
        res = BSP_SD_WriteBlocks((uint32_t *)src, (uint32_t)sector, count, SD_TIMEOUT);
        if (res != MSD_OK){
            sd_error();
        }
        while (BSP_SD_GetCardState() != MSD_OK);
    }while(res != MSD_OK && --retry); // On error, try again
#endif    
sd_err:
    return sd_error();
}

/* USER CODE BEGIN beforeWriteSection */
/* can be used to modify previous code / undefine following code / add new code */
/* USER CODE END beforeWriteSection */
/**
  * @brief  Writes Sector(s)
  * 
  * @param  lun : not used
  * @param  buff: Data to be written
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to write (1..128)
  * @retval DRESULT: Operation result
  */
DRESULT SD_write(BYTE lun, const BYTE *src, DWORD sector, UINT count)
{
#if DISCO_SD_WRITE_USE_DMA
    if (SD_CheckStatusWithTimeout(SD_TIMEOUT) < 0) {
        return RES_ERROR;
    }
#endif    
    /* Check if source is 32 bit aligned */
    if (!((uint32_t)src & 0x3)) {
#if (ENABLE_SD_DMA_CACHE_MAINTENANCE == 1) && (DISCO_SD_WRITE_USE_DMA == 1)
        /* the SCB_CleanDCache_by_Addr() requires a 32-Byte aligned address
        adjust the address and the D-Cache size to clean accordingly. */
        uint32_t alignedAddr;
        alignedAddr = (uint32_t)src & ~0x1F;
        SCB_CleanDCache_by_Addr((uint32_t *)alignedAddr, count * BLOCKSIZE + ((uint32_t)src - alignedAddr));
#endif /* ENABLE_SD_DMA_CACHE_MAINTENANCE */
        return write_sector((uint32_t*)src, sector, count);
    }else{
        /* Slow path, fetch each sector a part and memcpy to destination buffer */
#if (ENABLE_SD_DMA_CACHE_MAINTENANCE == 1) && (DISCO_SD_WRITE_USE_DMA == 1)
        /* invalidate the scratch buffer before the next write to get the 
           actual data instead of the cached one */
         SCB_InvalidateDCache_by_Addr((uint32_t *)scratch, BLOCKSIZE);
#endif /* ENABLE_SD_DMA_CACHE_MAINTENANCE */
        while(count--) {
             /* Copy sector data from buffer */
            for(uint32_t i = 0; i < BLOCKSIZE; i++, src++){
                ((BYTE*)scratch)[i] = *src;
            }
            if (write_sector((uint32_t *)scratch, (uint32_t)sector++, 1) != RES_OK) {
                return RES_ERROR;
            }
        }               
    }

    return RES_OK;
}

/* USER CODE BEGIN beforeIoctlSection */
/* can be used to modify previous code / undefine following code / add new code */
/* USER CODE END beforeIoctlSection */
/**
  * @brief  I/O control operation
  * @param  lun : not used
  * @param  cmd: Control code
  * @param  *buff: Buffer to send/receive control data
  * @retval DRESULT: Operation result
  */
DRESULT SD_ioctl(BYTE lun, BYTE cmd, void *buff)
{
    DRESULT res = RES_ERROR;
    BSP_SD_CardInfo CardInfo;

    if (Stat & STA_NOINIT)
        return RES_NOTRDY;

    switch (cmd)
    {
    /* Make sure that no pending write process */
    case CTRL_SYNC:
        res = RES_OK;
        break;

    /* Get number of sectors on the disk (DWORD) */
    case GET_SECTOR_COUNT:
        BSP_SD_GetCardInfo(&CardInfo);
        *(DWORD *)buff = CardInfo.LogBlockNbr;
        res = RES_OK;
        break;

    /* Get R/W sector size (WORD) */
    case GET_SECTOR_SIZE:
        BSP_SD_GetCardInfo(&CardInfo);
        *(WORD *)buff = CardInfo.LogBlockSize;
        res = RES_OK;
        break;

    /* Get erase block size in unit of sector (DWORD) */
    case GET_BLOCK_SIZE:
        BSP_SD_GetCardInfo(&CardInfo);
        *(DWORD *)buff = CardInfo.LogBlockSize / SD_DEFAULT_BLOCK_SIZE;
        res = RES_OK;
        break;

    default:
        res = RES_PARERR;
    }

    return res;
}

#if DISCO_SD_READ_USE_DMA
void DMA2_Stream0_IRQHandler(void){
    HAL_DMA_IRQHandler(uSdHandle.hdmarx);
}
#endif

#if DISCO_SD_WRITE_USE_DMA
void DMA2_Stream5_IRQHandler(void){
    HAL_DMA_IRQHandler(uSdHandle.hdmatx);
}

void SDMMC2_IRQHandler(){
    HAL_SD_IRQHandler(&uSdHandle);
    WriteStatus = 1;
    ReadStatus = 1;
}
#endif
