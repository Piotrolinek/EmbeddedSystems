/*****************************************************************************
 *   A demo example using several of the peripherals on the base board
 *
 *   Copyright(C) 2010, Embedded Artists AB
 *   All rights reserved.
 *
 ******************************************************************************/


#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_i2c.h"
#include "lpc17xx_timer.h"
#include "lpc17xx_systick.h"
#include "temp.h"
#include "lpc17xx_dac.h"
#include "lpc17xx_ssp.h"


#include "joystick.h"
#include "pca9532.h"
#include "acc.h"
#include "rotary.h"
#include "led7seg.h"
#include "oled.h"
#include "rgb.h"
#include "light.h"
#include "eeprom.h"

#define NUM_SAMPLES 1000
#define EEPROM_OFFSET 256

//////////////////////////////////////////////
//Global vars
static Bool leapYear = FALSE;
static uint32_t msTicks = 0;
static Bool editing = FALSE;
static Bool directionOfNextAlarm;  //True - up, False - down
static uint8_t eeprom_buffer[20];
extern int sound_sz_up;
extern int sound_sz_down;
extern unsigned char sound_up[21396];
extern unsigned char sound_down[16006];
static Bool disableSound = FALSE;
static uint32_t cnt = 0;
static uint32_t sound_offset = 0;
static uint8_t activationMode = 3;
static uint32_t lumenActivation = 500;
static int32_t prevCount = -1;
static Bool sound_to_play; //True - up, False - down
static int8_t roleteState = 0; //Zmienna odpowiedzialn za stan rolety -1 - dol, 0 - nieokreślony, 1 - gora
//////////////////////////////////////////////
struct alarm_struct {
    Bool MODE; //Down->0, Up->1
    uint8_t HOUR;
    uint8_t MIN;
};
struct pos {
    uint8_t x;
    uint8_t y;
    uint8_t length;
};

//////////////////////////////////////////////
//HEADER SECTION


uint32_t len(uint32_t val);

void uint32_t_to_str(uint32_t val,unsigned char *str);

static void isLeap(void);

static void init_ssp(void);

static void init_i2c(void);

static void PWM_vInit(void);

static Bool checkDifference(void);

static void PWM_Right(void);

static void PWM_Left(void);

static void PWM_Stop_Mov(void);

static int32_t get_sample_rate(Bool to_validate);

static void write_temp_on_screen(unsigned char *temp_str);

void SysTick_Handler(void);

static uint32_t getMsTicks(void);

static void showOurTemp(void);

static void showLuxometerReading(void);

static void showEditmode(Bool editmode);

static void showPresentTime(struct alarm_struct alarm[], int8_t y);

static void valToString(uint32_t value,unsigned char *str, uint8_t len);

static void chooseTime(struct pos map[4][3], int32_t LPC_values[], struct alarm_struct alarm[], int8_t x, int8_t y);

static Bool JoystickControls(char key, Bool edit,Bool *prevStateJoyRight,Bool *prevStateJoyLeft,Bool *prevStateJoyUp,Bool *prevStateJoyDown);

static void initTimer0(void);

void TIMER0_IRQHandler(void);

static void configTimer2(void);

void TIMER2_IRQHandler(void);

static void changeValue(int16_t value, int32_t LPC_values[], struct alarm_struct alarm[2], uint8_t x, uint8_t y);

static void correctDateValues(void);

static void setNextAlarm(struct alarm_struct alarm[]);

static int8_t read_time_from_eeprom(struct alarm_struct alarm[]);

static int8_t write_time_to_eeprom(struct alarm_struct alarm[]);

void RTC_IRQHandler(void);

static void activateMotor(void);

///////////////////////////////////////////////////////
///LIB FUNCTION HEADERS TO SATISFY MISRA
uint32_t GPIO_ReadValue(uint8_t portNum);

uint32_t SysTick_Config(uint32_t ticks);

void RTC_Init (LPC_RTC_TypeDef *RTCx);
///////////////////////////////////////////////////////

/*!
 *  @brief    		Returns length of uint32_t number.
 *  @param val  	uint32_t,
 *             		value to check the length of.
 *  @returns  		Lenght of param 'val'.
 *  @side effects:  None.
 */
uint32_t len(uint32_t val) {
    uint32_t i = 0;
    uint32_t param_val = val;
    while (param_val > 0U) {
        i++;
        param_val /= 10;
    }
    return i;
}

/*!
 *  @brief    		Converts uint32_t number into char array. Adds null character at the end.
 *  @param val  	uint32_t,
 *             		value to convert to char array.
 *  @param str 		char*,
 *             		char array to write to, of length value + 1 (for null char at the end).
 *  @returns  		
 *  @side effects:	For this function to be safe, char* str parameter has to be carefully initialised with proper size.
 */
void uint32_t_to_str(uint32_t val,unsigned char *str) {
    uint32_t val_len = len(val);
    uint32_t param_val = val;
    for (int32_t i = (int32_t)val_len - 1; i >= 0; i--) {
        str[i] = (char)((param_val % 10U) + '0');
        param_val /= 10;
    }
    for (int32_t i = val_len; i < 6; i++) {
        str[i] = ' ';
    }
    str[5] = '\0';
}

/*!
 *  @brief    		Returns bool value whether or not Real Time Clock: Year is a leap year or not,
					Example: 2000 is leap, 2001 is not leap, 1900 is not leap, 1600 is leap.
 *  @returns  		TRUE - if is leap, FALSE - otherwise.
 *  @side effects:	LPC_RTC has to be initialised prior to running this function.
 */
void isLeap(void) {
    Bool a = FALSE;
    if ((LPC_RTC->YEAR % 400) == 0) { a = TRUE; }
    else if ((LPC_RTC->YEAR % 100) == 0) { a = FALSE; }
    else if ((LPC_RTC->YEAR % 4) == 0) { a = TRUE; }
    else { a = FALSE; }
    leapYear = a;
    return;
}

/*!
 *  @brief    Initializes a SSP
 *  @returns
 *  @side effects:
 *            No error detection
 */
static void init_ssp(void) {
    SSP_CFG_Type SSP_ConfigStruct;
    PINSEL_CFG_Type PinCfg;

    /*
     * Initialize SPI pin connect
     * P0.7 - SCK;
     * P0.8 - MISO
     * P0.9 - MOSI
     * P2.2 - SSEL - used as GPIO
     */
    PinCfg.Funcnum = 2;
    PinCfg.OpenDrain = 0;
    PinCfg.Pinmode = 0;
    PinCfg.Portnum = 0;
    PinCfg.Pinnum = 7;
    PINSEL_ConfigPin(&PinCfg);
    PinCfg.Pinnum = 8;
    PINSEL_ConfigPin(&PinCfg);
    PinCfg.Pinnum = 9;
    PINSEL_ConfigPin(&PinCfg);

    SSP_ConfigStructInit(&SSP_ConfigStruct);

    // Initialize SSP peripheral with parameter given in structure above
    SSP_Init(LPC_SSP1, &SSP_ConfigStruct);

    // Enable SSP peripheral
    SSP_Cmd(LPC_SSP1, ENABLE);

}

/*!
 *  @brief    		Initializes I2C interface.
 *  @returns
 *  @side effects:	None.
 */
static void init_i2c(void) {
    PINSEL_CFG_Type PinCfg;

    /* Initialize I2C2 pin connect */
    PinCfg.Funcnum = 2;
    PinCfg.Pinnum = 10;
    PinCfg.Portnum = 0;
    PINSEL_ConfigPin(&PinCfg);
    PinCfg.Pinnum = 11;
    PINSEL_ConfigPin(&PinCfg);

    // Initialize I2C peripheral
    I2C_Init(LPC_I2C2, 100000);

    /* Enable I2C1 operation */
    I2C_Cmd(LPC_I2C2, ENABLE);
}

/*!
 *  @brief    		Initializes PWM. TODO
 *  @returns
 *  @side effects:	None.
 *  @Note: 			CPU is running @100MHz
 */
void PWM_vInit(void) {
    LPC_SC->PCLKSEL0 &= ~((uint32_t)3U << 12U);     // Ustawienie 00 na bitach 13 i 12
    LPC_SC->PCLKSEL0 |= ((uint32_t)1U << 12U);      // Ustawienie zegara PWM na pełną prędkość procesora
    LPC_SC->PCONP |= (1U << 6U);                    // Włączenie zasilania dla modułu PWM

    LPC_PINCON->PINSEL4 |= (1U << 0U);              // set PWM1.1 at P2.0
    LPC_PWM1->TCR = (1U << 1U);                     // counter reset
    LPC_PWM1->PR = (100000000UL - 1U) >> 10U;    	// Prescaler ma wartość około 100 000 co daje nam zmiane TC na poziomie 1000Hz
    LPC_PWM1->MCR = (1U << 1U);           			// reset on MR0
    LPC_PWM1->MR0 = 4U;                				// gdy TC bedzie tyle rowne to resetujemy
    LPC_PWM1->MR1 = 2U;                				// wypelnienie 50%
    LPC_PWM1->LER = (3U << 0U);    					// latch MR0 & MR1
    LPC_PWM1->PCR |= ((uint32_t)1U << 9U);          // PWM1 output enable
    LPC_PWM1->TCR = (1U << 0U) | (1U << 3U);        // counter enable, PWM enable //sprawdzic co to w ogole robi

    GPIO_SetDir(2, ((uint32_t)1U << 10U), 1);       // spin left
    GPIO_SetDir(2, ((uint32_t)1U << 11U), 1);       // spin right
}




/*!
 *  @brief    		Uses TIMER2 to determine whether or not the motor is currently running, by comparing previous TC value, to current TC value
 *  @returns  		TRUE - if is different (impiles that the motor is running), FALSE - otherwise
 *  @side effects:	LPC_TIM2 has to be initialised prior to running this function.
 */
Bool checkDifference(void) {
    Bool output = FALSE;
    if (prevCount != LPC_TIM2->TC) {
        prevCount = LPC_TIM2->TC;
        output = TRUE;
    }
    return output;
}


/*!
 *  @brief			Changes motor spin direction to right, by modifying proper GPIO pins
 *  @returns
 *  @side effects:  None.
 */
void PWM_Right(void) {
    if (!(GPIO_ReadValue(2) & ((uint32_t)1U << 11U))) {
        GPIO_ClearValue(2, ((uint32_t)1U << 10U));
        GPIO_SetValue(2, ((uint32_t)1U << 11U));
    }
}

/*!
 *  @brief			Changes motor spin direction to left, by modifying proper GPIO pins
 *  @returns
 *  @side effects:  None.
 */
void PWM_Left(void) {
    if (!(GPIO_ReadValue(2) & ((uint32_t)1U << 10U))) {
        GPIO_ClearValue(2, ((uint32_t)1U << 11U));
        GPIO_SetValue(2, ((uint32_t)1U << 10U));
    }
}

/*!
 *  @brief			Stops motor rotation, by clearing proper GPIO pins
 *  @returns
 *  @side effects:  None.
 */
void PWM_Stop_Mov(void) {
    GPIO_ClearValue(2, ((uint32_t)3U << 10U));
}

/*!
 *  @brief    		Validates external sound files (const unsigned char[]), specifically
					([a - b] where a - 1  is starting index in const unsigned char[], b - 1 is ending index of specified section):
					[1 - 4] - “RIFF” Marks the file as a riff file. Characters are each 1 byte long.
					[5 - 8] - File size (integer) Size of the overall file - 8 bytes, in bytes (32-bit integer). Typically, you’d fill this in after creation.
					[9 -12] - “WAVE” File Type Header. For our purposes, it always equals “WAVE”.
					[13-16] - “fmt " Format chunk marker. Includes trailing null
					[17-20] - 16 Length of format data as listed above
					[21-22] - 1 Type of format (1 is PCM) - 2 byte integer
					[23-24] - 2 Number of Channels - 2 byte integer
					[25-28] - 44100 Sample Rate - 32 byte integer. Common values are 44100 (CD), 48000 (DAT). Sample Rate = Number of Samples per second, or Hertz.
					[29-32] - 176400 (Sample Rate * BitsPerSample * Channels) / 8.
					[33-34] - 4 (BitsPerSample * Channels) / 8.1 - 8 bit mono2 - 8 bit stereo/16 bit mono4 - 16 bit stereo
					[35-36] - 16 Bits per sample
					[37-40] - “data” “data” chunk header. Marks the beginning of the data section.
					[41-44] - File size (data) Size of the data section.
 *  @param to_validate Bool,
 *            		Selector which sound to validate, TRUE - validate extern const unsinged char sound_up[], FALSE - validate extern const unsigned char sound_down[]
 *  @returns  		int32_t sampleRate equal to sample rate from wav file IF validation passes,
 * 					IF validation fails, returns error code:
 * 					-1 - missing or wrong format (RIFF expected)
 * 					-2 - missing or wrong format (WAVE expected)
 * 					-3 - missing "fmt"
 * 					-4 - missing or unsupported sample rate (8000 expected)
 * 					-5 - missing "data"
 *  @side effects:  If validation fails, sounds are disabled globally
 */
int32_t get_sample_rate(Bool to_validate) {
    cnt = 0;
    int32_t error = 0;
    unsigned char *sound_8k;
    if (to_validate) {
        sound_8k = sound_up;
    } else {
        sound_8k = sound_down;
    }
    /* ChunkID */
    if ((sound_8k[cnt] != (unsigned char)'R')
    &&(sound_8k[cnt + 1U] != (unsigned char)'I')
    &&(sound_8k[cnt + 2U] != (unsigned char)'F')
    &&(sound_8k[cnt + 3U] != (unsigned char)'F')) {
        //Wrong format (RIFF)
        error = -1;
    }
    cnt += 4U;

    /* skip chunk size*/
    cnt += 4U;

    /* Format */
    if ((sound_8k[cnt] != (unsigned char)'W')
    && (sound_8k[cnt + 1U] != (unsigned char)'A')
    && (sound_8k[cnt + 2U] != (unsigned char)'V')
    && (sound_8k[cnt + 3U] != (unsigned char)'E')) {
        //Wrong format (WAVE)
        error = -2;
    }
    cnt += 4U;

    /* SubChunk1ID */
    if ((sound_8k[cnt] != (unsigned char)'f')
    && (sound_8k[cnt + 1U] != (unsigned char)'m')
    && (sound_8k[cnt + 2U] != (unsigned char)'t')
    && (sound_8k[cnt + 3U] != (unsigned char)' ')) {
        //Missing fmt
        error = -3;
    }
    cnt += 4U;

    /* skip chunk size, audio format, num channels */
    cnt += 8U;

    int32_t sampleRate = (sound_8k[cnt] | (sound_8k[cnt + 1U] << 8) |
                          (sound_8k[cnt + 2U] << 16) | (sound_8k[cnt + 3U] << 24));

    if (sampleRate != 8000) {
        //Only 8kHz supported
        error = -4;
    }

    cnt += 4U;

    /* skip byte rate, align, bits per sample */
    cnt += 8U;

    /* SubChunk2ID */
    if ((sound_8k[cnt] != (unsigned char)'d')
    && (sound_8k[cnt + 1U] != (unsigned char)'a')
    && (sound_8k[cnt + 2U] != (unsigned char)'t')
    && (sound_8k[cnt + 3U] != (unsigned char)'a')) {
        //Missing data
        error = -5;
    }
    cnt += 4U;

    /* skip chunk size */
    cnt += 4U;
    sound_offset = cnt;
    if(error != 0) {
        sampleRate = error;
    }
    return sampleRate;
}

/*!
 *  @brief    		Reads temperature value and prepares a char array to be displayed on OLED screen.
 *  @param temp_str	char*,
 *             		char array to write to, of length value + 4 ("xx.x C", where xxx is a value returned by temp_read()).
 *  @returns
 *  @side effects:	For this function to be safe, char* temp_str parameter has to be carefully initialized with proper size, reaching negative value causes loop to break preemptively.
 */
void write_temp_on_screen(unsigned char *temp_str) {
    int32_t temp = temp_read();
    for (int32_t i = 3; i >= 0; i--) {
        temp_str[i] = (char) ((temp % 10) + '0');
        if (temp <= 0) {
            break;
        }
        if (i == 2) {
            temp_str[i] = '.';
        } else {
            temp = temp / 10;
        }
    }
    temp_str[4] = ' ';
    temp_str[5] = 'C';
    temp_str[6] = '\0';
}

/*!
 *  @brief    Function that increment amount of msTicks
 *  @returns
 *  @side effects:
 *            None.
 */
void SysTick_Handler(void) {
    msTicks++;
}

/*!
 *  @brief    Get msTicks
 *  @returns  msTicks amount (uint32_t)
 *  @side effects:
 *            None.
 */
uint32_t getMsTicks(void) {
    return msTicks;
}

/*!
 *  @brief    Shows temp on screen
 *  @returns
 *  @side effects:
 *            Can be used without further OLED initialization
 *            Program gets slowly when funtion is procceding
 */
void showOurTemp(void) {
    unsigned char naszString[7];
    write_temp_on_screen(naszString);
    oled_putString(1, 0, naszString, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
}

/*!
 *  @brief    Shows a value of light in lumens
 *  @returns
 *  @side effects:
 *            Can be used without further Luxometer and I2C initialization
 */
void showLuxometerReading(void) {
    uint32_t light_val = light_read();
    unsigned char xdd[6];
    uint32_t_to_str(light_val, xdd);
    oled_putString(43, 1, xdd, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
}

/*!
 *  @brief    Show on screen which mode is actual
 *  @param Bool editmode
 *             True - editing mode is on, moving mode is off
 *             False - moving mode is on, editing mode is off
 *  @returns
 *  @side effects:
 *            None.
 */
void showEditmode(Bool editmode) {
    oled_line(86, 0, 91, 0, OLED_COLOR_WHITE);
    oled_line(85, 0, 85, 8, OLED_COLOR_WHITE);
    if (editmode) {
        oled_putChar(86, 1, 'E', OLED_COLOR_BLACK, OLED_COLOR_WHITE);
    } else {
        oled_putChar(86, 1, 'M', OLED_COLOR_BLACK, OLED_COLOR_WHITE);
    }
}

/*!
 *  @brief    Function shows present time and alarms on screen
 *  @param struct alarm_struct  alarm[]
 *             struct table which has alarms data
 *  @param int8_t y
 *             y parameter of OLED
 *  @returns
 *  @side effects:
 *            Not handling OLED errors
 */
void showPresentTime(struct alarm_struct alarm[], int8_t y) {
    unsigned char date_str[10];
    uint16_t year = LPC_RTC->YEAR;

    for (int8_t i = 3; i >= 0; i--) {
        date_str[i] = (unsigned char)((uint8_t)(year % 10U) + '0');
        if (year == 0U) {
            break;
        }
        year = year / 10U;
    }
    date_str[4] = '-';

    uint8_t month = LPC_RTC->MONTH;
    for (uint8_t i = 6U; i >= 5U; i--) {
        date_str[i] = (unsigned char)((uint8_t)(month % 10U) + '0');
        if (month == 0U) {
            break;
        }
        month = month / 10U;
    }

    date_str[7] = '-';

    uint8_t day = LPC_RTC->DOM;
    for (uint8_t i = 9U; i >= 8U; i--) {
        date_str[i] = (unsigned char)((uint8_t)(day % 10U) + '0');
        if (day == 0U) {
            break;
        }
        day = day / 10U;
    }

    unsigned char time_str[9];

    uint8_t hour = LPC_RTC->HOUR;

    for (int8_t i = 1; i >= 0; i--) {
        time_str[i] = (unsigned char)((uint8_t)(hour % 10U) + '0');
        hour = hour / 10U;
    }


    time_str[2] = ':';

    uint8_t minute = LPC_RTC->MIN;

    for (uint8_t i = 4U; i >= 3U; i--) {
        time_str[i] = (unsigned char)((uint8_t)(minute % 10U) + '0');
        minute = minute / 10U;
    }


    time_str[5] = ':';

    uint8_t sec = LPC_RTC->SEC;
    for (uint8_t i = 7U; i >= 6U; i--) {
        time_str[i] = (unsigned char)((uint8_t)(sec % 10U) + '0');
        sec = sec / 10U;
    }
    unsigned char alarm_str[8];
    alarm_str[7] = '\0';
    uint8_t min;
    if (y == 3) {
        alarm_str[0] = 'U';
        alarm_str[1] = ' ';
        hour = alarm[1].HOUR;
        min = alarm[1].MIN;
    } else {
        alarm_str[0] = 'D';
        alarm_str[1] = ' ';
        hour = alarm[0].HOUR;
        min = alarm[0].MIN;
    }

    for (uint8_t i = 3U; i >= 2U; i--) {
        alarm_str[i] = (unsigned char)((uint8_t)(hour % 10U) + '0');


        hour = hour / 10U;
    }
    alarm_str[4] = ':';

    for (uint8_t i = 6U; i >= 5U; i--) {
        alarm_str[i] = (unsigned char)((uint8_t)(min % 10U) + '0');


        min = min / 10U;
    }
    time_str[8] = '\0';
    unsigned char activation[8];
    if (activationMode == 0U) {
        activation[0] = 'N';
    } else if (activationMode == 1U) {
        activation[0] = 'A';
    } else if (activationMode == 2U) {
        activation[0] = 'L';
    } else {
        activation[0] = 'B';
    }
    activation[1] = ' ';
    uint32_t lumens_to_display = lumenActivation;
    for (uint8_t i = 6U; i >= 2U; i--) {
        activation[i] = (unsigned char)((uint8_t)(lumens_to_display % 10U) + '0');

        if ((lumens_to_display == 0U) && (i != 6U)) {
            activation[i] = ' ';
        }
        lumens_to_display = lumens_to_display / 10U;
    }

    activation[7] = '\0';
    oled_putString(1, 12, date_str, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
    oled_putString(1, 24, time_str, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
    oled_putString(37, 36, alarm_str, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
    oled_putString(31, 48, activation, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
}

/*!
 *  @brief    Converts uint32_t value to an char arrow
 *  @param uint32_t value
 *            A value to convert
 *  @param char* str
 *            A char arrow catch a converted value
 *  @param uint8_t len
 *            length of arrow
 *  @returns
 *  @side effects:
 *            None.
 */
void valToString(uint32_t value,unsigned char *str, uint8_t len) {
    int i = len;
    uint32_t param_val = value;
    str[i] = '\0';
    for (int j = i - 1; j >= 0; j--) {
        str[j] = (char)((uint8_t)(param_val % 10U) + '0');
        param_val = param_val / 10U;
    }
}

/*!
 *  @brief    Function that allows us to change these fields:
 *            Actual date and time,
 *            Time of up and down alarms,
 *            Sunshide manipulating mode,
 *            Lumen activation level
 *  @param struct pos map[4][3]
 *            A map of positions
 *  @param int32_t LPC_values[]
 *            An arrow of LPC_RTC values
 *  @param struct alarm_struct alarm[]
 *            An arrow of alarm values
 *  @param int8_t x
 *            x OLED position
 *  @param int8_t y
 *            y OLED position
 *  @returns
 *  @side effects:
 *            None.
 */
void chooseTime(struct pos map[4][3], int32_t LPC_values[], struct alarm_struct alarm[], int8_t x, int8_t y) {
    unsigned char str[5];
    uint8_t leng = map[y][x].length;
    uint8_t toAdd = 0;
    if ((x + (y * 3)) < 6) {
        valToString(LPC_values[x + (y * 3)], str, leng);
    } else if ((x + (y * 3)) < 12) {
        if (x == 1) { valToString(alarm[y - 2].HOUR, str, leng); }
        else if (x == 2) { valToString(alarm[y - 2].MIN, str, leng); }
        else {}
        if (x != 0) { setNextAlarm(alarm); }
        else {}
    } else {
        if (x == 0) {
            if (activationMode == 0U) {
                str[0] = 'N';
            } else if (activationMode == 1U) {
                str[0] = 'A';
            } else if (activationMode == 2U) {
                str[0] = 'L';
            } else {
                str[0] = 'B';
            }
        } else if (x == 1) {
            if (lumenActivation >= 10000U) {
                toAdd = 0U;
            } else if (lumenActivation >= 1000U) {
                toAdd = 6U;
            } else if (lumenActivation >= 100U) {
                toAdd = 12U;
            } else if (lumenActivation >= 10U) {
                toAdd = 18U;
            }
            else {}
            valToString(lumenActivation, str, len(lumenActivation));
        }
        else {}

    }
    if(((x != 0) && (x + (y * 3)) < 12) || ((x + (y * 3)) < 6) || ((x + (y * 3)) >= 12)) {
        str[map[y][x].length] = '\0';
        oled_putString(map[y][x].x + toAdd, map[y][x].y, str, OLED_COLOR_BLACK, OLED_COLOR_WHITE);
    }
}

/*!
 *  @brief    Controls moving of the joystick
 *  @param char key
 *            acronim of the move
 *  @param Bool edit
 *            True - editing mode
 *            False - moving mode
 *  @param Bool *prevStateJoyRight
 *            State of the tilt to the right
 *  @param Bool *prevStateJoyLeft
 *            State of the tilt to the left
 *  @param Bool *prevStateJoyUp
 *            State of the tilt to the up
 *  @param Bool *prevStateJoyDown
 *            State of the tilt to the down
 *  @returns  TRUE when output is true, FALSE otherwise
 *  @side effects:
 *            None
 */
Bool JoystickControls(char key, Bool edit,Bool *prevStateJoyRight,Bool *prevStateJoyLeft,Bool *prevStateJoyUp,Bool *prevStateJoyDown) {

    Bool output = FALSE;
    uint8_t pin = 0;
    uint8_t portNum = 0;
    Bool prevStateJoy;
    if ((key == 'u') || (key == 'U')) {
        portNum = 2;
        pin = 3;
        prevStateJoy = *prevStateJoyUp;
    } else if ((key == 'd') || (key == 'D')) {
        portNum = 0;
        pin = 15;
        prevStateJoy = *prevStateJoyDown;
    } else if ((key == 'r') || (key == 'R')) {
        portNum = 0;
        pin = 16;
        prevStateJoy = *prevStateJoyRight;
    } else if ((key == 'l') || (key == 'L')) {
        portNum = 2;
        pin = 4;
        prevStateJoy = *prevStateJoyLeft;
    } else {
        prevStateJoy = TRUE;
    }
    uint32_t joyClick = ((GPIO_ReadValue(portNum) & ((uint32_t)1U << pin)) >> pin);
    Bool joyClickDiff = joyClick - prevStateJoy;
    if (edit) {
        if ((joyClickDiff) && (editing) && (!joyClick)) {
            output = TRUE;
            prevStateJoy = 0;
            joyClickDiff = FALSE;
        }
        if ((joyClickDiff) && (!editing) && (!joyClick)) {
            output = FALSE;
            prevStateJoy = 0;
            prevStateJoy = joyClick;
        }
    } else {
        if ((joyClickDiff) && (!editing) && (!joyClick)) {
            output = TRUE;
            prevStateJoy = 0;
            joyClickDiff = FALSE;
        }
        if ((joyClickDiff) && (editing) && (!joyClick)) {
            output = FALSE;
            prevStateJoy = 0;
            prevStateJoy = joyClick;
        }
    }

    switch (key) {
        case 'u':
        case 'U':
            *prevStateJoyUp = joyClick;
            break;
        case 'd':
        case 'D':
            *prevStateJoyDown = joyClick;
            break;
        case 'l':
        case 'L':
            *prevStateJoyLeft = joyClick;
            break;
        case 'r':
        case 'R':
            *prevStateJoyRight = joyClick;
            break;
        default:
            break;
    }
    return output;
}

/*!
 *  @brief    Initializes Timer0
 *  @returns  
 *  @side effects:
 *            efekty uboczne
 */
void initTimer0(void) {
    LPC_SC->PCONP |= (1U << 0U); //Power on
    LPC_SC->PCLKSEL0 |= (1U << 2U);
    LPC_TIM0->PR = 0;
    LPC_TIM0->MR0 = 12500;
    LPC_TIM0->MCR |= (1U << 0U) | (1U << 1U);
    LPC_TIM0->TCR = 0x02; 
    LPC_TIM0->TCR = 0x01;

    NVIC_EnableIRQ(TIMER0_IRQn);
}

/*!
 *  @brief    Executing when an Timer0 interrupt is called
 *  @returns  
 *  @side effects:
 *            Execute even if the sound is not correctly loaded
 */
void TIMER0_IRQHandler(void) {
    if (LPC_TIM0->IR & (1U << 0U)) {
        LPC_TIM0->IR = (1U << 0U);
        if (!disableSound) {
            if (sound_to_play) {
                if (cnt++ < sound_sz_up) { DAC_UpdateValue(LPC_DAC, (uint32_t)(sound_up[cnt])); }
            } else {
                if (cnt++ < sound_sz_down) { DAC_UpdateValue(LPC_DAC, (uint32_t)(sound_down[cnt])); }
            }
        }
    }
}

/*!
 *  @brief    Configures Timer2
 *  @returns  
 *  @side effects:
 *            None
 */
void configTimer2(void) {
    LPC_SC->PCONP |= ((uint32_t)1U << 22U);         // Power up Timer 2
    LPC_SC->PCLKSEL1 |= ((uint32_t)1U << 12U);
    LPC_PINCON->PINSEL0 |= ((uint32_t)3U << 10U);
    LPC_TIM2->CTCR |= (1U << 0U) | (1U << 2U);      // Counter mode
    LPC_TIM2->PR = 0U;                              // No prescale
    LPC_TIM2->CCR = 0U;                             // Capture on rising edge on CAP2.0
    LPC_TIM2->TCR = 2U;                             //Reset
    LPC_TIM2->TCR = 1U;                             // Start Timer
}

/*!
 *  @brief    Changes a value x,y in pos
 *  @param int16_t value
 *            Value to add
 *  @param int32_t LPC_values[]
 *            Arrow of LPC Values
 *  @param struct alarm_struct alarm[2]
 *            Arrow of alarm values
 *  @param uint8_t x
 *            x logic position
 *  @param uint8_t y
 *            y logic position
 *  @returns  
 *  @side effects:
 *            None
 */void changeValue(int16_t value, int32_t LPC_values[], struct alarm_struct alarm[2], uint8_t x, uint8_t y) {
    uint8_t pos_on_map = (y * 3U) + x;
    int32_t tmp;
    uint8_t i = 0U;
    if (pos_on_map < 6U) {
        tmp = LPC_values[pos_on_map] + value;
    } else if (pos_on_map < 12U) {
        if (pos_on_map < 9U) { i = 0U; }
        else { i = 1U; }
        if (pos_on_map == 6U) { tmp = alarm[0].MODE + value; }
        if (pos_on_map == 7U) { tmp = (int32_t) alarm[0].HOUR + (int32_t) value; }
        if (pos_on_map == 8U) { tmp = (int32_t) alarm[0].MIN + (int32_t) value; }
        if (pos_on_map == 9U) { tmp = (int32_t) alarm[1].MODE + (int32_t) value; }
        if (pos_on_map == 10U) { tmp = (int32_t) alarm[1].HOUR + (int32_t) value; }
        if (pos_on_map == 11U) { tmp = (int32_t) alarm[1].MIN + (int32_t) value; }
    } else if (pos_on_map < 15U) {
        if (pos_on_map == 12U) { tmp = (int32_t) activationMode + (int32_t) value; }
        else {}
        if (pos_on_map == 13U) { tmp = (int32_t) lumenActivation + (int32_t) value * 50; }
        else {}
    }
    else {}
    switch (pos_on_map) {
        case 0:
            if (tmp > 2099) { tmp = 2000; }
            else if (tmp < 2000) { tmp = 2099; }
            else {}
            LPC_RTC->YEAR = tmp;
            break;
        case 1:
            if (tmp > 12) { tmp = 1; }
            else if (tmp < 1) { tmp = 12; }
            else {}
            LPC_RTC->MONTH = tmp;
            break;
        case 2:
            if ((LPC_values[1] == 1)
            || (LPC_values[1] == 3)
            || (LPC_values[1] == 5)
            || (LPC_values[1] == 7)
            || (LPC_values[1] == 8 )
            || (LPC_values[1] == 10)
            || (LPC_values[1] == 12)) {
                if (tmp > 31) { tmp = 1; }
                else if (tmp < 1) { tmp = 31; }
                else {}
            } else if ((LPC_values[1] == 4 )
            || (LPC_values[1] == 6)
            || (LPC_values[1] == 9)
            || (LPC_values[1] == 11)) {
                if (tmp > 30) { tmp = 1; }
                else if (tmp < 1) { tmp = 30; }
                else {}
            } else if (LPC_values[1] == 2) {
                isLeap();
                if (leapYear) {
                    if (tmp > 29) { tmp = 1; }
                    else if (tmp < 1) { tmp = 29; }
                    else {}
                } else {
                    if (tmp > 28) { tmp = 1; }
                    else if (tmp < 1) { tmp = 28; }
                    else {}
                }
            }
            else {}
            LPC_RTC->DOM = tmp;
            break;
        case 3:
            if (tmp > 23) { tmp = 0; }
            else if (tmp < 0) { tmp = 23; }
            else {}
            LPC_RTC->HOUR = tmp;
            break;
        case 4:
            if (tmp > 59) { tmp = 0; }
            else if (tmp < 0) { tmp = 59; }
            else {}
            LPC_RTC->MIN = tmp;
            break;
        case 5:
            if (tmp > 59) { tmp = 0; }
            else if (tmp < 0) { tmp = 59; }
            else {}
            LPC_RTC->SEC = tmp;
            break;
        case 6:
        case 9:
            break;
        case 7:
        case 10:
            if (tmp > 23) { tmp = 0; }
            else if (tmp < 0) { tmp = 23; }
            else {}
            alarm[i].HOUR = tmp;
            break;
        case 8:
        case 11:
            if (tmp > 59) { tmp = 0; }
            else if (tmp < 0) { tmp = 59; }
            else {}
            alarm[i].MIN = tmp;
            break;
        case 12:
            if (tmp > 3) { tmp = 0; }
            else if (tmp < 0) { tmp = 3; }
            else {}
            activationMode = (uint8_t)tmp;
            if ((activationMode == 1U) || (activationMode == 3U)) {
                LPC_RTC->AMR &= ~((1U << 2) | (1U << 1) | (1U << 0));
            } else {
                LPC_RTC->AMR |= ((1U << 2) | (1U << 1) | (1U << 0));
            }
            break;
        case 13:
            if (tmp < 0) { tmp = 64000; }
            else if (tmp > 64000) { tmp = 0; }
            else {}
            lumenActivation = tmp;
            break;
        default:
            break;
    }
}

/*!
 *  @brief    Correct days of month when it's incorrect
 *  @returns  
 *  @side effects:
 *            None
 */
void correctDateValues(void) {
    isLeap();
    const uint8_t lenghts[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (LPC_RTC->DOM > lenghts[LPC_RTC->MONTH - 1]) {
        if (leapYear && (LPC_RTC->MONTH == 2)) {
            if (LPC_RTC->DOM > 29) {
                LPC_RTC->DOM = 29;
            }
        } else {
            LPC_RTC->DOM = lenghts[LPC_RTC->MONTH - 1];
        }
    }
}

/*!
 *  @brief    Sets nearest alarm in the future
 *  @param struct alarm_struct alarm[]
 *            Arrow of alarms
 *  @returns  
 *  @side effects:
 *            When the alarm was in the same minute, and next alarm will be in the next minute, programm sets again an alarm that was a moment ago
 */
void setNextAlarm(struct alarm_struct alarm[]) {
    int32_t hour0Diff = alarm[0].HOUR - LPC_RTC->HOUR + 24;
    hour0Diff = hour0Diff % 24;
    int32_t minute0Diff = alarm[0].MIN - LPC_RTC->MIN + 60;
    minute0Diff = minute0Diff % 60;
    int32_t second0Diff = 60 - LPC_RTC->SEC;
    second0Diff = second0Diff % 60;

    int32_t hour1Diff = alarm[1].HOUR - LPC_RTC->HOUR + 24;
    hour1Diff = hour1Diff % 24;
    int32_t minute1Diff = alarm[1].MIN - LPC_RTC->MIN + 60;
    minute1Diff = minute1Diff % 60;
    int32_t second1Diff = 60 - LPC_RTC->SEC;
    second1Diff = second1Diff % 60;

    LPC_RTC->ALSEC = 0;

    if (hour0Diff < hour1Diff) {
        LPC_RTC->ALHOUR = alarm[0].HOUR;
        LPC_RTC->ALMIN = alarm[0].MIN;
        directionOfNextAlarm = alarm[0].MODE;
    } else if (hour0Diff > hour1Diff) {
        LPC_RTC->ALHOUR = alarm[1].HOUR;
        LPC_RTC->ALMIN = alarm[1].MIN;
        directionOfNextAlarm = alarm[1].MODE;
    } else if (minute0Diff < minute1Diff) {
        LPC_RTC->ALHOUR = alarm[0].HOUR;
        LPC_RTC->ALMIN = alarm[0].MIN;
        directionOfNextAlarm = alarm[0].MODE;
    } else if (minute0Diff > minute1Diff) {
        LPC_RTC->ALHOUR = alarm[1].HOUR;
        LPC_RTC->ALMIN = alarm[1].MIN;
        directionOfNextAlarm = alarm[1].MODE;
    } else if (second0Diff < second1Diff) {
        LPC_RTC->ALHOUR = alarm[0].HOUR;
        LPC_RTC->ALMIN = alarm[0].MIN;
        directionOfNextAlarm = alarm[0].MODE;
    } else {
        LPC_RTC->ALHOUR = alarm[1].HOUR;
        LPC_RTC->ALMIN = alarm[1].MIN;
        directionOfNextAlarm = alarm[1].MODE;
    }
}

/*!
 *  @brief    Reads data from EEPROM
 *  @param struct alarm_struct alarm[]
 *            An alarm arrow to insert alarm data from EEPROM 
 *  @returns  error code:
 *            0 - funtion ended correct
 *            -5, -6 - length error
 *            -1..-5, -16..-20 -tag error
 *  @side effects:
 *            Reading wrong data
 */
int8_t read_time_from_eeprom(struct alarm_struct alarm[]) {
    uint8_t temporary_buffer[4];
    int16_t eeprom_read_len = eeprom_read(temporary_buffer, EEPROM_OFFSET + 16, 4);
    int8_t errorCode = 0;
    if (eeprom_read_len != 4) {
        errorCode = -5;
    }
    eeprom_read_len = eeprom_read(eeprom_buffer, EEPROM_OFFSET, 5);
    if (eeprom_read_len != 5) {
        errorCode = -5;
    }
    const char header[] = "TIME";
    for (uint8_t i = 0U; i < 4U; i++) {
        if ((char)eeprom_buffer[i] != (char)header[i]) {
            errorCode = -i - 1;
        }
        if ((char)temporary_buffer[i] != (char)header[i]) {
            errorCode = -i - 16;
        }
    }
    uint16_t lenFromEeprom = eeprom_buffer[4];
    eeprom_read_len = eeprom_read(eeprom_buffer, EEPROM_OFFSET, lenFromEeprom);
    if (eeprom_read_len != 20) {
        errorCode = -6;
    }
    if(errorCode == 0) {
        LPC_RTC->YEAR = (uint16_t)((eeprom_buffer[5] * 100U) + eeprom_buffer[6]);
        LPC_RTC->MONTH = eeprom_buffer[7];
        LPC_RTC->DOM = eeprom_buffer[8];
        LPC_RTC->HOUR = eeprom_buffer[9];
        LPC_RTC->MIN = eeprom_buffer[10];
        LPC_RTC->SEC = eeprom_buffer[11];
        alarm[0].HOUR = eeprom_buffer[12];
        alarm[0].MIN = eeprom_buffer[13];
        alarm[1].HOUR = eeprom_buffer[14];
        alarm[1].MIN = eeprom_buffer[15];
        setNextAlarm(alarm);
    }
    return errorCode;
}

/*!
 *  @brief    Writes data to the EEPROM
 *  @param struct alarm_struct alarm[]
 *            An alarm arrow with data to write to the EEPROM
 *  @returns  error code:
 *            0 - funtion ended correct
 *            -1 - length error
 *  @side effects:
 *            Try to write even when I2C is not working
 */
int8_t write_time_to_eeprom(struct alarm_struct alarm[]) {
    const char header[] = "TIME";
    int8_t errorCode = 0;
    for (uint8_t i = 0U; i < 4U; i++) {
        eeprom_buffer[i] = (uint8_t)(header[i]);
        eeprom_buffer[i + 16U] = (uint8_t)(header[i]);
    }
    eeprom_buffer[4] = 20U;
    eeprom_buffer[5] = (uint8_t)(LPC_RTC->YEAR / 100U);
    eeprom_buffer[6] = (uint8_t)(LPC_RTC->YEAR % 100U);
    eeprom_buffer[7] = LPC_RTC->MONTH;
    eeprom_buffer[8] = LPC_RTC->DOM;
    eeprom_buffer[9] = LPC_RTC->HOUR;
    eeprom_buffer[10] = LPC_RTC->MIN;
    eeprom_buffer[11] = LPC_RTC->SEC;
    eeprom_buffer[12] = alarm[0].HOUR;
    eeprom_buffer[13] = alarm[0].MIN;
    eeprom_buffer[14] = alarm[1].HOUR;
    eeprom_buffer[15] = alarm[1].MIN;
    int16_t eeprom_write_len = eeprom_write(eeprom_buffer, EEPROM_OFFSET, 20);
    if (eeprom_write_len != 20) {
        errorCode = -1;
    }
    return errorCode;
}

/*!
 *  @brief    RTC Interrupts Handler
 *  @returns  
 *  @side effects:
 *            None
 */
void RTC_IRQHandler(void) {
    if (LPC_RTC->ILR & 2) {
        LPC_RTC->ILR = 2;
        cnt = sound_offset;
        sound_to_play = directionOfNextAlarm;
        initTimer0();
        prevCount = -1;
        if (directionOfNextAlarm) {
            PWM_Right();
        } else {
            PWM_Left();
        }
    }
}

/*!
 *  @brief    Activates motor, when condition is met
 *  @returns  
 *  @side effects:
 *            Possible initializing a move of non existing motor
 */
void activateMotor(void) {
    uint32_t but1 = ((GPIO_ReadValue(0) >> 4U) & (uint32_t)0x01);
    uint32_t but2 = ((GPIO_ReadValue(1) >> 31U) & (uint32_t)0x01);
    Bool moveUp = lumenActivation < light_read();
    Bool moveDown = lumenActivation > light_read();

    if ((activationMode == 2U) || (activationMode == 3U)) { //LUXOMETER ONLY
        if (moveUp && (roleteState != 1)) {
            PWM_Left();
        } else if (moveDown && (roleteState != -1)) {
            PWM_Right();
        } else {}
    }
    else {}
    moveUp = but1 == 0U;
    moveDown = but2 == 0U;
    if (moveUp && (roleteState != 1)) {
        PWM_Left();
    } else if (moveDown && (roleteState != -1)) {
        PWM_Right();
    } else {}
}

int main(void) {
    init_i2c();
    init_ssp();
    eeprom_init();
    joystick_init();
    oled_init();

    Bool prevStateJoyRight = TRUE;
    Bool prevStateJoyLeft = TRUE;
    Bool prevStateJoyUp = TRUE;
    Bool prevStateJoyDown = TRUE;

    if ((Bool)SysTick_Config(SystemCoreClock / 1000)) {
        while(1){}; // error
    }

    temp_init(&getMsTicks);

    PWM_vInit();
    Bool prevStateJoyClick = TRUE;

    int32_t sampleRate = 0;

    GPIO_SetDir(0, ((uint32_t)1U << 27U), 1);
    GPIO_SetDir(0, ((uint32_t)1U << 28U), 1);
    GPIO_SetDir(2, ((uint32_t)1U << 13U), 1);
    GPIO_SetDir(0, ((uint32_t)1U << 26U), 1);

    GPIO_ClearValue(0, ((uint32_t)1U << 27U)); //LM4811-clk
    GPIO_ClearValue(0, ((uint32_t)1U << 28U)); //LM4811-up/dn
    GPIO_ClearValue(2, ((uint32_t)1U << 13U)); //LM4811-shutdn

    light_enable();
    light_setMode(LIGHT_MODE_D1); //visible + infrared
    light_setRange(LIGHT_RANGE_64000);
    light_setWidth(LIGHT_WIDTH_16BITS);
    light_setIrqInCycles(LIGHT_CYCLE_1);

    RTC_Init(LPC_RTC);
    LPC_RTC->YEAR = 2022;
    LPC_RTC->MONTH = 2;
    LPC_RTC->DOM = 2;
    LPC_RTC->HOUR = 2;
    LPC_RTC->MIN = 2;
    LPC_RTC->SEC = 2;
    LPC_RTC->CCR = 1;

    LPC_RTC->AMR &= ~((1U << 2U) | (1U << 1U) | (1U << 0U));
    LPC_RTC->ILR = 1;
    LPC_RTC->CIIR = 0;
    NVIC_EnableIRQ(RTC_IRQn);

    PINSEL_CFG_Type PinCfg;

    PinCfg.Funcnum = 2;
    PinCfg.OpenDrain = 0;
    PinCfg.Pinmode = 0;
    PinCfg.Pinnum = 26;
    PinCfg.Portnum = 0;
    PINSEL_ConfigPin(&PinCfg);

    DAC_Init(LPC_DAC);

    GPIO_SetDir(0, (1U << 4U), 0);
    GPIO_SetDir(1, ((uint32_t)1U << 31U), 0);
    oled_clearScreen(OLED_COLOR_BLACK);

    unsigned char naszString[4];
    write_temp_on_screen(naszString);
    oled_putString(1, 0, naszString, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
    showLuxometerReading();

    PWM_Stop_Mov();
    uint32_t ifCheckTheTemp = 0;

    //const unsigned char xdx[] = "ALARM:\0";
    oled_putString(1, 36,(uint8_t*) "ALARM:\0", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
    //const unsigned char dxd[] = "MODE:\0";
    oled_putString(1, 48,(uint8_t*) "MODE:\0", OLED_COLOR_WHITE, OLED_COLOR_BLACK);

    sampleRate = get_sample_rate(1);
    if (sampleRate < 0) {
        //err handle
        disableSound = TRUE;
    }
    sampleRate = get_sample_rate(0);
    if (sampleRate < 0) {
        //err handle
        disableSound = TRUE;
    }

    uint8_t posX = 0;
    uint8_t posY = 0;

    struct alarm_struct alarm[2] = {{0, 2,  2},
                                    {1, 22, 22}};
    struct pos map[5][3] = {
            {{1,  12, 4}, {31, 12, 2}, {49, 12, 2}},
            {{1,  24, 2}, {19, 24, 2}, {37, 24, 2}},
            {{37, 36, 1}, {49, 36, 2}, {67, 36, 2}},
            {{37, 36, 1}, {49, 36, 2}, {67, 36, 2}},
            {{31, 48, 1}, {43, 48, 5}, {73, 48, 0}}};
    setNextAlarm(alarm);

    int8_t eeprom_read_ret_value = read_time_from_eeprom(alarm);
    if (eeprom_read_ret_value != 0) {
        //err handle
    }
    configTimer2();
    while (1) {

        uint32_t joyClick = ((GPIO_ReadValue(0) & ((uint32_t)1U << 17U)) >> 17U);
        int32_t LPC_values[] = {LPC_RTC->YEAR, LPC_RTC->MONTH, LPC_RTC->DOM, LPC_RTC->HOUR, LPC_RTC->MIN, LPC_RTC->SEC};
        Bool joyClickDiff = joyClick - prevStateJoyClick;

        if ((joyClickDiff) && (!editing) && (!joyClick)) {
            editing = TRUE;
            prevStateJoyClick = 0;
            joyClickDiff = FALSE;
        }

        if ((joyClickDiff) && (editing) && (!joyClick)) {
            editing = FALSE;
            prevStateJoyClick = 0;
        }
        prevStateJoyClick = joyClick;

        showEditmode(editing);

        if (!editing) {
            if (JoystickControls('u', FALSE,&prevStateJoyRight,&prevStateJoyLeft,&prevStateJoyUp,&prevStateJoyDown)) {
                posY += 4U;
                posY = posY % 5U;
            }
            if (JoystickControls('d', FALSE,&prevStateJoyRight,&prevStateJoyLeft,&prevStateJoyUp,&prevStateJoyDown)) {
                posY += 6U;
                posY = posY % 5U;
            }
            if (JoystickControls('l', FALSE,&prevStateJoyRight,&prevStateJoyLeft,&prevStateJoyUp,&prevStateJoyDown)) {
                posX += 2U;
                posX = posX % 3U;
            }
            if (JoystickControls('r', FALSE,&prevStateJoyRight,&prevStateJoyLeft,&prevStateJoyUp,&prevStateJoyDown)) {
                posX += 4U;
                posX = posX % 3U;
            }
        } else {
            if (JoystickControls('u', TRUE,&prevStateJoyRight,&prevStateJoyLeft,&prevStateJoyUp,&prevStateJoyDown)) {
                changeValue(1, LPC_values, alarm, posX, posY);
            }
            if (JoystickControls('d', TRUE,&prevStateJoyRight,&prevStateJoyLeft,&prevStateJoyUp,&prevStateJoyDown)) {
                changeValue(-1, LPC_values, alarm, posX, posY);
            }
            if (JoystickControls('l', TRUE,&prevStateJoyRight,&prevStateJoyLeft,&prevStateJoyUp,&prevStateJoyDown)) {
                changeValue(-5, LPC_values, alarm, posX, posY);
            }
            if (JoystickControls('r', TRUE,&prevStateJoyRight,&prevStateJoyLeft,&prevStateJoyUp,&prevStateJoyDown)) {
                changeValue(5, LPC_values, alarm, posX, posY);
            }
        }

        chooseTime(map, LPC_values, alarm, posX, posY);
        correctDateValues();
        showPresentTime(alarm, posY);

        ifCheckTheTemp++;
        if ((ifCheckTheTemp % ((uint32_t)1U << 10U)) == 0U) {
            int8_t eeprom_write_ret_value = write_time_to_eeprom(alarm);
            if (eeprom_write_ret_value != 0) {
                //err handle
            }
        }
        showLuxometerReading();
        if ((ifCheckTheTemp % ((uint32_t)1U << 8U)) == 0U) {
            showOurTemp();
        }

        uint32_t but1 = ((GPIO_ReadValue(0) >> 4U) & (uint32_t)0x01);
        uint32_t but2 = ((GPIO_ReadValue(1) >> 31U) & (uint32_t)0x01);
        if ((but1 == 0U) || (but2 == 0U)) {
            prevCount = -1;
        }

        int32_t jeden = ((GPIO_ReadValue(2) & (uint32_t)((uint32_t)1U << 10U)) >> 10U);
        int32_t dwa = ((GPIO_ReadValue(2) & (uint32_t)((uint32_t)1U << 11U)) >> 11U);

        if ((!checkDifference()) && ((jeden == 1) || (dwa == 1))) {
            if (((GPIO_ReadValue(2) & ((uint32_t)1U << 10U)) >> 10U) == 1U) {
                roleteState = 1;
            } else {
                roleteState = -1;
            }
            PWM_Stop_Mov();
        }
        activateMotor();
    }
}
