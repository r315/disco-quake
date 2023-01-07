/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#include "quakedef.h"
#include "d_local.h"

#include "stm32f769i_discovery_lcd.h"
#include "discovery.h"

#define	CANVAS_WIDTH	320 //480
#define	CANVAS_HEIGHT	200 //320

#define VIDEO_LAYER_WINDOW	0
#define VID_DRAW_PALETTE	1

#define VIDEO_LAYER			DMA2D_FOREGROUND_LAYER
#define VIDEO_LAYER_BASE    LCD_FG_BASE_ADDR

#if VIDEO_LAYER_WINDOW
#define VIDEO_WINDOW		VIDEO_LAYER_BASE
#else
#define VIDEO_WINDOW		(VIDEO_LAYER_BASE + ( (((800 - CANVAS_WIDTH)/2) + ((480 - CANVAS_HEIGHT) * 400)) * 4))
#endif

#define DMA2D_CR_M2M (0 << 16)
#define DMA2D_CR_M2M_PFC (1 << 16)
#define DMA2D_CR_M2M_BLEND (2 << 16)
#define DMA2D_CR_R2M (3 << 16)
#define DMA2D_FGPFCCR_SET_ALPHA(a) ((a << 24) | (1 << 16))
#define DMA2D_FGPFCCR_SET_CS(cs) ((cs) << 8)	// CLUT size
#define DMA2D_FGPFCCR_SET_CM(cm) ((cm) << 0)  // Input Color mode
#define DMA2D_OPFCCR_SET_CM(cm) ((cm) << 0)	// Output Color mode
#define DMA2D_NLR_PLNL(pl, nl) (((pl) << 16) | nl)

/**
 * Global variables
 */
viddef_t		vid;				// global video state
unsigned short	d_8to16table[256];

/**
 * Local variables
 */
static byte	*frame_buffer = NULL;

static void LCD_InitClock(void){
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

static void LCD_ConfigDma(uint32_t src, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    DMA2D->CR = DMA2D_CR_M2M_PFC;
    DMA2D->FGMAR = src;
    DMA2D->FGOR = 0;
    DMA2D->FGPFCCR = DMA2D_FGPFCCR_SET_ALPHA(0xFF) |        // Replace alpha 
                     DMA2D_FGPFCCR_SET_CS(256 - 1) |        // CLUT Size
                     DMA2D_FGPFCCR_SET_CM(DMA2D_INPUT_L8) | // Input color format
                     DMA2D_FGPFCCR_CCM;                     // RGB CLUT Mode
    DMA2D->OPFCCR = DMA2D_OPFCCR_SET_CM(DMA2D_OUTPUT_ARGB8888) |
                    //DMA2D_OPFCCR_RBS |                    // Swap Red Blue
                    0;
    uint32_t omar = x + (y * CANVAS_WIDTH);
    DMA2D->OMAR = (uint32_t)(VIDEO_WINDOW + (omar * 4));
    DMA2D->OOR = BSP_LCD_GetXSize() - w;
    DMA2D->NLR = DMA2D_NLR_PLNL(w, h);
}

/**
 * @brief Draw full or partial bitmap on display for a given window.
 * bitmap can be RGB888 or ARGB8888 format, output format is ARGB8888
 * 
 * @param fg        foreground bitmap area
 * @param fg_offset offset from start in pixels
 * @param bg        background bitmap area, NULL if not used
 * @param bg_offset offset from start in pixels
 * @param x         window x position
 * @param y         window y position
 * @param w         window width
 * @param h         window hight
 */
void LCD_BlendWindow(lcdarea_t *fg, uint32_t fg_offset, lcdarea_t *bg, uint32_t bg_offset, uint16_t x, uint16_t y, uint16_t w, uint16_t h){
    
    DMA2D->FGMAR = (uint32_t)fg->data + (fg_offset * (fg->bpp/8));
    DMA2D->FGOR = fg->w - w;
    DMA2D->FGCOLR = 0;    
    DMA2D->FGPFCCR = (fg->bpp == 32)? 0 : DMA2D_FGPFCCR_SET_ALPHA(0xFF) |           // Replace alpha 
                                          DMA2D_FGPFCCR_SET_CM(DMA2D_INPUT_RGB888);

    if(bg != NULL){
        DMA2D->BGMAR = (uint32_t)bg->data + (bg_offset * (bg->bpp/8));
        DMA2D->BGOR = bg->w - w;
        DMA2D->BGCOLR = 0;
        DMA2D->BGPFCCR = (bg->bpp == 32)? 0 : DMA2D_FGPFCCR_SET_ALPHA(0xFF) |
                                              DMA2D_FGPFCCR_SET_CM(DMA2D_INPUT_RGB888);
    }

    DMA2D->OPFCCR = 0;
    DMA2D->OCOLR = 0;
    DMA2D->OMAR = LCD_FG_BASE_ADDR + ((x + (y * BSP_LCD_GetXSize())) * 4);
    DMA2D->OOR = BSP_LCD_GetXSize() - w;
    DMA2D->NLR = DMA2D_NLR_PLNL(w, h);

    DMA2D->CR = (bg != NULL)? DMA2D_CR_M2M_BLEND | DMA2D_CR_START : DMA2D_CR_M2M_PFC | DMA2D_CR_START;   

    do{

    }while(DMA2D->CR & DMA2D_CR_START);

    // Restore dma settings to game canvas
    LCD_ConfigDma ((uint32_t)frame_buffer, 0, 0, CANVAS_WIDTH, CANVAS_HEIGHT);
}

/**
 * @brief Draws a bitmap from memory into lcd using BSP library
 * 
 * @param x  : x position
 * @param y  : y position
 * @param pbmp  : Bitmap header and data
 */
void LCD_DrawBitmap (int x, int y, uint8_t *pbmp)
{
    BSP_LCD_DrawBitmap(x, y, pbmp);
    LCD_ConfigDma ((uint32_t)frame_buffer, 0, 0, CANVAS_WIDTH, CANVAS_HEIGHT);
}

/**
 * @brief Converts bitmap data on memory to more useful lcdarea structure
 * 
 * @param pbmp : raw bitmap data
 * @param argb : 1 argb format output, 0 defined by bitmap bpp
 * @return lcdarea_t* 
 */
lcdarea_t *LCD_GetBmpData(uint8_t *pbmp, uint8_t argb){
    uint32_t index = 0, width = 0, height = 0, bit_pixel = 0;    

    /* Get bitmap data address offset */
    index = pbmp[10] + (pbmp[11] << 8) + (pbmp[12] << 16)  + (pbmp[13] << 24);

    /* Read bitmap width */
    width = pbmp[18] + (pbmp[19] << 8) + (pbmp[20] << 16)  + (pbmp[21] << 24);

    /* Read bitmap height */
    height = pbmp[22] + (pbmp[23] << 8) + (pbmp[24] << 16)  + (pbmp[25] << 24);

    /* Read bit/pixel */
    bit_pixel = pbmp[28] + (pbmp[29] << 8);

    lcdarea_t *texture = (lcdarea_t*)malloc(sizeof(lcdarea_t));
    
    if(texture == NULL){
        return NULL;
    }

    if(argb){
        texture->data = malloc(width * height * 4);
        texture->bpp = 32;
    }else{
        texture->data = malloc(width * height * (bit_pixel/8));
        texture->bpp = bit_pixel;
    }
    
    
    if(texture->data == NULL){
        free(texture);
        return NULL;
    }

    texture->w = width;
    texture->h = height;

    /* Bypass the bitmap header */
    pbmp += (index + (width * (height - 1) * (bit_pixel/8)));
    uint32_t line_len = width * (bit_pixel/8);
    uint8_t *pdst = texture->data;

    // Currently RGB888 only and no padding
    for(index=0; index < height; index++){
        if(argb){
            uint8_t b = 0;
            uint32_t color = 0;
            for(int i = 0; i < line_len; i++){
                color <<= 8;
                color |= pbmp[i];                
                if(++b == 3){
                    if(color != 0x00FF00FF){ // Transparent                        
                        color = 0xFF000000 | (color >> 16) | (color << 16); // Add alpha, swap RB
                    }
                    *((uint32_t*)pdst) = color;
                    pdst += 4;
                    b = 0;
                    color = 0;
                }
            }
        }else{
            memcpy(pdst, pbmp, line_len);
            pdst += line_len;
        }
        pbmp -= line_len;
    }
    
    return texture;
}

void LCD_InitLL(void){	
    
    LCD_InitClock ();

    memset((uint8_t*)LCD_FB_BASE_ADDR, 0, LCD_FB_SIZE);
    
    BSP_LCD_Init ();
    
    BSP_LCD_LayerDefaultInit(VIDEO_LAYER, VIDEO_LAYER_BASE);
    BSP_LCD_SetColorKeying(VIDEO_LAYER, LCD_COLOR_MAGENTA);

#if VIDEO_LAYER_WINDOW
    BSP_LCD_Clear(LCD_COLOR_MAGENTA);
    BSP_LCD_SetLayerWindow(VIDEO_LAYER, (BSP_LCD_GetXSize() - CANVAS_WIDTH)/2, (BSP_LCD_GetYSize() - CANVAS_HEIGHT)/2, CANVAS_WIDTH, CANVAS_HEIGHT);
#endif
    
    BSP_LCD_SelectLayer(VIDEO_LAYER);
    LCD_ConfigDma ((uint32_t)frame_buffer, 0, 0, CANVAS_WIDTH, CANVAS_HEIGHT);
}

/**
 * @brief Loads a bitmap file from file system
 * 
 * @param filename  path to file
 * @param argb      Convert image to ARGB8888 format
 * @return lcdarea_t* 
 */
lcdarea_t *LCD_LoadBmp(const char *filename, uint8_t argb)
{
    uint32_t index = 0;
    uint32_t width = 0, height = 0;
    uint16_t bit_pixel = 0;
    FILE *fp;

    fp = fopen(filename, "rb");
    
    if(fp == NULL){
        return NULL;
    }

    // check bmp header
    fread(&index, 1, 2, fp);
    if(index != 0x4D42){
        return NULL;    // BM not found on header
    }    

    /* Get bitmap data address offset */
    fseek(fp, 10, SEEK_SET);
    fread(&index, 1, 4, fp);

    /* Read bitmap width */
    fseek(fp, 18, SEEK_SET);
    fread(&width, 1, 4, fp);

    /* Read bitmap height */
    fseek(fp, 22, SEEK_SET);
    fread(&height, 1, 4, fp);

    /* Read bit/pixel */
    fseek(fp, 28, SEEK_SET);
    fread(&bit_pixel, 1, 2, fp);

    lcdarea_t *texture = (lcdarea_t*)malloc(sizeof(lcdarea_t));

    if(texture == NULL){
        return NULL;
    }

    if(argb){
        texture->data = calloc(width * height, 4);
        texture->bpp = 32;
    }else{
        texture->data = calloc(width * height, (bit_pixel/8));
        texture->bpp = bit_pixel;
    }    
    
    if(texture->data == NULL){
        fclose(fp);
        free(texture);
        return NULL;
    }

    texture->w = width;
    texture->h = height;

    /* Skip bitmap header */
    fseek(fp, index, SEEK_SET);

    uint32_t row_size = (width * texture->bpp) / 8;
    uint8_t *pdst = texture->data + (row_size * (height - 1));

    // Currently RGB888 and no padding
    for(index=0; index < height; index++){
        if(argb){
            for(int i = 0; i < width; i++){
                uint32_t color = LCD_COLOR_TRANSPARENT;
                fread(&color, 1, (bit_pixel/8), fp); // Read one RGB pixel                
                ((uint32_t*)pdst)[i] = (color == LCD_COLOR_MAGENTA)? 0 : color;                
            }
        }else{        
            fread(pdst, 1, row_size, fp);
        }
        pdst -= row_size;
    }    

    fclose(fp);
    
    return texture;
}

void VID_Init (unsigned char *palette)
{
    byte *cache;
    int chunk, cachesize;	
        
    frame_buffer = (byte*)malloc(CANVAS_WIDTH * CANVAS_HEIGHT);

    if(!frame_buffer){
        Sys_Error("VID_Init: Fail to allocate memory\n");
    }

    LCD_InitLL();
    
    vid.width = CANVAS_WIDTH;
    vid.height = CANVAS_HEIGHT;

    vid.maxwarpwidth = WARP_WIDTH;
    vid.maxwarpheight = WARP_HEIGHT;
    vid.aspect = 1.0;
    vid.numpages = 1;
    vid.direct = 0;

    vid.conwidth = vid.width;
    vid.conheight = vid.height;
    
    vid.colormap = host_colormap;
    //vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));
    vid.buffer = vid.conbuffer = frame_buffer;
    vid.rowbytes = vid.conrowbytes = CANVAS_WIDTH;
    
    VID_SetPalette(palette);

    // allocate z buffer and surface cache
    
    //d_pzbuffer = zbuffer;
    //D_InitCaches (surfcache, sizeof(surfcache));
    
    chunk = vid.width * vid.height * sizeof (*d_pzbuffer);
    cachesize = D_SurfaceCacheForRes (vid.width, vid.height);
    chunk += cachesize;
    d_pzbuffer = Hunk_HighAllocName(chunk, "video");
    if (d_pzbuffer == NULL){
        Sys_Error ("Not enough memory for video mode\n");
    }

    // initialize the cache memory 
    cache = (byte *) d_pzbuffer + vid.width * vid.height * sizeof (*d_pzbuffer);
    D_InitCaches (cache, cachesize);
}


/**
 * Video API
 * */
void VID_SetPalette (unsigned char *palette)
{
    if(!palette){
        return;
    }

#if 1
    // Write palette directly to DMA2D foreground CLUT
    for (uint32_t i = 0; i < 256; ++i){
        uint8_t r = *palette++;
        uint8_t g = *palette++;
        uint8_t b = *palette++;
        ((uint32_t*)DMA2D->FGCLUT)[i] = (r << 16) | (g << 8) | (b << 0);		
    }
#else
    // Auto CLUT load is 32bit aligned and ARGB fixed structure
    // RGB palette cannot be directly used
    DMA2D->FGCMAR = (uint32_t)palette;
    DMA2D->FGPFCCR |= DMA2D_FGPFCCR_START;
    do{
    }while(DMA2D->FGPFCCR & DMA2D_FGPFCCR_START);
#endif

#ifdef VID_DRAW_PALETTE
    // Draw 3x3 px squares on bottom of display
    for(int l = 0; l < 3; l++){
        uint32_t *pdst = (uint32_t*)(VIDEO_LAYER_BASE + ((479 - l) * 800 * 4));
        for(int i = 0; i < 256 * 3; i++){
            pdst[i] = ((uint32_t*)DMA2D->FGCLUT)[i/3] | 0xFF000000;
        }
    }
#endif
}

void VID_ShiftPalette (unsigned char *palette)
{
    VID_SetPalette(palette);
}

void VID_Shutdown (void)
{
    free(frame_buffer);
}

void VID_Update (vrect_t *rects)
{
    //while(!(LTDC->CDSR & LTDC_CDSR_VSYNCS));  // Sync with VSYNC
    //uint32_t offset = rects->x + (rects->y * CANVAS_WIDTH);
    //DMA2D->OMAR = (uint32_t)(VIDEO_WINDOW + (offset * 4));
    //DMA2D->NLR = DMA2D_NLR_PLNL(rects->width, rects->height);

    DMA2D->CR |= DMA2D_CR_START;
    do{

    }while(DMA2D->CR & DMA2D_CR_START);
}

/*
================
D_BeginDirectRect
================
*/
void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
    if(frame_buffer == NULL){
        return;
    }

    LCD_ConfigDma ((uint32_t)pbitmap, x, y, width, height);
    
    DMA2D->CR |= DMA2D_CR_START;

    do{

    }while(DMA2D->CR & DMA2D_CR_START);

    LCD_ConfigDma ((uint32_t)frame_buffer, 0, 0, CANVAS_WIDTH, CANVAS_HEIGHT);
}


/*
================
D_EndDirectRect
================
*/
void D_EndDirectRect (int x, int y, int width, int height)
{
}