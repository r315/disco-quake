#include "quakedef.h"
#include "disco_serial.h"
#include "discovery.h"
#include "stm32f769i_discovery_ts.h"

#define DPAD_DEBUG      0

#define DPAD_POSX       40
#define DPAD_POSY       (LCD_LANDSCAPE_HEIGHT/2 - 64)
#define DPAD_WIDTH      128
#define DPAD_HIGHT      128
#define DPAD_RADIUS     100

#define STICK_POSX      (LCD_LANDSCAPE_WIDTH - 128 - 40)
#define STICK_POSY      (LCD_LANDSCAPE_HEIGHT/2 - 32)
#define STICK_WIDTH     128
#define STICK_HIGHT     128
#define STICK_RADIUS    100

#define A_BTN_POSX      (800 - 256)
#define A_BTN_POSY      (480 - 55)
#define B_BTN_POSX      (800 - 150)
#define B_BTN_POSY      (480 - 80)

enum key_states_e{
    KEY_UP  = 0,
    KEY_DOWN,
    KEY_PRESSED,
    KEY_RELEASED,
    KEY_HOLD
};

// Bit order is important
enum dpad_e{
    DPAD_NONE = 0,
    DPAD_UP = 1,
    DPAD_DOWN = 2,
    DPAD_LEFT = 4,
    DPAD_RIGHT = 8
};

typedef struct dpad_s {
    float x;            // Last X touch
    float y;            // Last Y touch
    float mag;          // magnitude
    float angle;        // angle
    uint16_t px;        // X center position
    uint16_t py;        // Y Center position
    uint16_t w;         // window Width
    uint16_t h;         // window Hight
    uint8_t dir;        // current direction
    uint8_t last_dir;   // last direction
    uint8_t radius;     // Active radius
    lcdarea_t *fg;      // Foreground image
    lcdarea_t *bg;      // Backgound image
}dpad_t;

const uint8_t dpad_dir[] = {
    DPAD_RIGHT,             // 8    E
    DPAD_RIGHT | DPAD_UP,   // 9    NE
    DPAD_UP,                // 1    N
    DPAD_UP | DPAD_LEFT,    // 5    NO
    DPAD_LEFT,              // 4    O
    DPAD_LEFT | DPAD_DOWN,  // 6    SO
    DPAD_DOWN,              // 2    S
    DPAD_DOWN | DPAD_RIGHT  // 10   SE
};

static uint8_t pressed_keys[256];
static TS_StateTypeDef ts_state;
static dpad_t l_dpad, r_dpad, a_button, b_button;
static int dpad_keys[4];

/**
 * @brief Draw dpad bitmap
 * 
 * @param dpad directional pad
 * @param x    x offset from dpad center x position
 * @param y    y offset from dpad center y position
 */
static void dpad_draw(dpad_t *dpad, float x, float y){
    x = (int)(x * 25);
    y = (int)(y * 25);
    x = (dpad->fg->w / 2) - (((dpad->bg->w) / 2) + x);
    y = (dpad->fg->h / 2) - (((dpad->bg->h) / 2) - y);

    LCD_BlendWindow(dpad->fg, x + (y * dpad->fg->w), dpad->bg, 0, dpad->px - (dpad->w/2), dpad->py - (dpad->h/2), dpad->w, dpad->h);        
}

static uint8_t dpad_vector(dpad_t *dpad, uint16_t ts_x, uint16_t ts_y){
    // Translate to origin
    dpad->x = ts_x - dpad->px;
    dpad->y = -(ts_y - dpad->py);

    // Get magnitude and check if within dpad radius
    dpad->mag = sqrtf(dpad->x * dpad->x + dpad->y * dpad->y);    

    // Normalize vector
    dpad->x /= dpad->mag;
    dpad->y /= dpad->mag;

    // Get angle
    dpad->angle = atan2f(dpad->y, dpad->x);
    if(dpad->angle < 0){
        dpad->angle = 2*M_PI + dpad->angle;
    }

    return dpad->mag < (float)dpad->radius;
}

static void dpad_event(dpad_t *dpad, uint16_t ts_x, uint16_t ts_y){    
    if(dpad_vector(dpad, ts_x, ts_y) == 0){
        return;
    }

    // Adjust DPAD rotation relative to coordinate system
    dpad->angle += M_PI/8;

    // map direction
    dpad->dir = dpad_dir[(uint8_t)(dpad->angle/(M_PI/4)) & 7];

#if DPAD_DEBUG
    const char *dir[] = {"", "N", "S", "", "O", "NO", "SO", "", "E", "NE", "SE"};
    Sys_Printf("DPAD: %s\n", dir[dpad->dir]);
#endif
}

static void stick_event(dpad_t *dpad, uint16_t ts_x, uint16_t ts_y){
    if(dpad_vector(dpad, ts_x, ts_y) == 0){
        return;
    }

#if DPAD_DEBUG
    Sys_Printf("r_dpad angle: %f\n", dpad->angle);
#endif

    dpad_draw(dpad, dpad->x, dpad->y);

    // dummy direction sync thumb stick state machine
    dpad->dir = DPAD_DOWN;
}

static void button_event(dpad_t *dpad, uint16_t ts_x, uint16_t ts_y){
    if(dpad_vector(dpad, ts_x, ts_y) == 0){
        return;
    }
    dpad->dir = DPAD_DOWN;
}

static void touch_event(void){
    r_dpad.dir = DPAD_NONE;
    l_dpad.dir = DPAD_NONE;
    a_button.dir  = DPAD_NONE;
    b_button.dir  = DPAD_NONE;

    for(uint8_t index = 0; index < ts_state.touchDetected; index++){
        dpad_event(&l_dpad, ts_state.touchX[index], ts_state.touchY[index]);
        stick_event(&r_dpad, ts_state.touchX[index], ts_state.touchY[index]);
        button_event(&a_button, ts_state.touchX[index], ts_state.touchY[index]);
        button_event(&b_button, ts_state.touchX[index], ts_state.touchY[index]);
    }

    if(r_dpad.dir == DPAD_NONE && r_dpad.last_dir != DPAD_NONE){
        dpad_draw(&r_dpad, 0, 0);
    }
 
    r_dpad.last_dir = r_dpad.dir;
    l_dpad.last_dir = l_dpad.dir;
}

static void button_state(dpad_t *btn, int key)
{
 if(btn->dir != DPAD_NONE){
        if(btn->last_dir == DPAD_NONE){
            Key_Event (key, true);
#if DPAD_DEBUG
    Sys_Printf("button pressed '%c'\n", (char)key);
#endif
        }
    }else if(btn->last_dir != DPAD_NONE){
        Key_Event (key, false);
#if DPAD_DEBUG
    Sys_Printf("button released '%c'\n", (char)key);
#endif
    }

    btn->last_dir = btn->dir;
}
/**
 * @brief Process mouse buttons
 * 
 */
void IN_Commands (void)
{
   button_state(&a_button, K_MOUSE1);   
   button_state(&b_button, (key_dest == key_menu)? K_ENTER : K_MOUSE2);
}

/**
 * @brief 
 * 
 */
void IN_Init (void){
    if(BSP_TS_Init(LCD_LANDSCAPE_WIDTH, LCD_LANDSCAPE_HEIGHT) != TS_OK){
        Sys_Error("Touch not present");
    }else{
        l_dpad.x = 0;
        l_dpad.y = 0;
        l_dpad.px = DPAD_POSX + (DPAD_WIDTH / 2);
        l_dpad.py = DPAD_POSY + (DPAD_HIGHT / 2);
        l_dpad.w = DPAD_WIDTH;
        l_dpad.h = DPAD_HIGHT;
        l_dpad.dir = DPAD_NONE;
        l_dpad.radius = DPAD_RADIUS;

        r_dpad.x = 0;
        r_dpad.y = 0;
        r_dpad.px = STICK_POSX + (STICK_WIDTH / 2);
        r_dpad.py = STICK_POSY + (DPAD_HIGHT / 2);
        r_dpad.w = STICK_WIDTH;
        r_dpad.h = STICK_HIGHT;
        r_dpad.dir = DPAD_NONE;
        r_dpad.radius = STICK_RADIUS;

        a_button.x = 0;
        a_button.y = 0;
        a_button.px = A_BTN_POSX;
        a_button.py = A_BTN_POSY;
        a_button.w = 80;
        a_button.h = 80;
        a_button.dir = DPAD_NONE;
        a_button.radius = 40;

        b_button.x = 0;
        b_button.y = 0;
        b_button.px = B_BTN_POSX;
        b_button.py = B_BTN_POSY;
        b_button.w = 80;
        b_button.h = 80;
        b_button.dir = DPAD_NONE;
        b_button.radius = 40;

        a_button.fg = LCD_LoadBmp("a_btn_bg.bmp", 0);
        a_button.bg = NULL;        
        if(a_button.fg){
            dpad_draw(&a_button, 0, 0);
            free(a_button.fg->data);
        }

        b_button.fg = LCD_LoadBmp("b_btn_bg.bmp", 0);
        b_button.bg = NULL;
        if(b_button.fg){
            dpad_draw(&b_button, 0, 0);
            free(b_button.fg->data);
        }

        l_dpad.fg = LCD_LoadBmp("dpad_bg.bmp", 0);
        l_dpad.bg = NULL;
        if(a_button.fg){
            dpad_draw(&l_dpad, 0, 0);
            free(l_dpad.fg->data);
        }

        r_dpad.bg = LCD_LoadBmp("stick_bg.bmp", 0);
        r_dpad.fg = LCD_LoadBmp("stick_fg.bmp", 1);       
        
        if(r_dpad.fg == NULL || r_dpad.bg == NULL){
            Sys_Error("Unable to load dpad bitmaps\n");
        }
        
        dpad_draw(&r_dpad, 0, 0);               

        dpad_keys[0] = K_UPARROW;
        dpad_keys[1] = K_DOWNARROW;
        dpad_keys[2] = 'a';
        dpad_keys[3] = 'd';
    }
    memset(pressed_keys, KEY_UP, sizeof(pressed_keys));
}

/**
 * @brief Process mouse move
 * 
 * @param cmd 
 */
void IN_Move (usercmd_t *cmd){
    
    if(r_dpad.dir == DPAD_NONE){
        return;
    }

    r_dpad.x *= sensitivity.value * 80;
    r_dpad.y *= -sensitivity.value * 80;

    if ( (in_strafe.state & 1) || (lookstrafe.value && (in_mlook.state & 1) ))
        cmd->sidemove += m_side.value * r_dpad.x;
    else
        cl.viewangles[YAW] -= m_yaw.value * r_dpad.x;

    if (in_mlook.state & 1)
        V_StopPitchDrift ();
   
    if ( (in_mlook.state & 1) && !(in_strafe.state & 1)) {
        cl.viewangles[PITCH] += m_pitch.value * r_dpad.y;
        if (cl.viewangles[PITCH] > 80)
            cl.viewangles[PITCH] = 80;
        if (cl.viewangles[PITCH] < -70)
            cl.viewangles[PITCH] = -70;
    } else {
        if ((in_strafe.state & 1) && noclip_anglehack)
            cmd->upmove -= m_forward.value * r_dpad.y;
        else
            cmd->forwardmove -= m_forward.value * r_dpad.y;
    }

    r_dpad.x = r_dpad.y = 0;
}

void IN_SendKeyEvents(void){
    uint8_t sym, esc_seq;

    esc_seq = 0;

    BSP_TS_GetState(&ts_state);

    while(SERIAL_GetChar(&sym)){
        if(esc_seq == 2){
            switch(sym){
                case 0x41: sym = K_UPARROW; break;
                case 0x42: sym = K_DOWNARROW; break;
                case 0x43: sym = K_RIGHTARROW; break;
                case 0x44: sym = K_LEFTARROW; break;
            }
            esc_seq = 0;
        }else{
            switch(sym){
                case 0x1b: esc_seq = 1; break;
                case '\n': sym = '\r'; break;
                case '[':
                    if(esc_seq == 1)
                        esc_seq = 2;                   
                    break;

                case '\b':
                    sym = K_BACKSPACE;
                    break;

                default:
                    if(esc_seq == 1)
                        esc_seq = 0;

                break;
            }
        }        

        if(esc_seq == 0){
            pressed_keys[sym] = (pressed_keys[sym] == KEY_UP)? KEY_PRESSED : KEY_HOLD;
        }
    }

    if(esc_seq == 1){
        // escape key pressed
        pressed_keys[sym] = KEY_PRESSED;
    }

    touch_event();

    uint8_t dp = l_dpad.dir;
    
    for(uint8_t i = 0; i < 4; i++, dp >>= 1){    
        if(dp & 1){
            sym = dpad_keys[i];
            pressed_keys[sym] = (pressed_keys[sym] == KEY_UP) ? KEY_PRESSED : KEY_HOLD;
        }
    }

    sym = 255;
    do{
        switch(pressed_keys[sym]){
            case KEY_UP: break;

            case KEY_PRESSED:
                pressed_keys[sym] = KEY_DOWN;
                Key_Event(sym, KEY_DOWN);
                break;

            case KEY_HOLD:
                pressed_keys[sym] = KEY_DOWN;
                break;

            default:
                pressed_keys[sym] = KEY_UP;
                Key_Event(sym, KEY_UP);
                break;
        }
    }while(sym--);    
}

void IN_Shutdown (void)
{
    if(l_dpad.fg){
        if(l_dpad.fg->data) free(l_dpad.fg->data);
        free(l_dpad.fg);
    }

    if(r_dpad.bg){
        if(r_dpad.bg->data) free(r_dpad.bg->data);
        free(r_dpad.bg);
    }

    if(r_dpad.fg){
        if(r_dpad.fg->data) free(r_dpad.fg->data);
        free(r_dpad.fg);
    }

    if(a_button.fg){
        free(a_button.fg);
    }

    if(b_button.fg){
        free(b_button.fg);
    }
}