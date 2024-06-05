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
#include "lpc17xx_ssp.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_timer.h"
#include "lpc17xx_systick.h"
#include "temp.h"

#include "joystick.h"
#include "pca9532.h"
#include "acc.h"
#include "rotary.h"
#include "led7seg.h"
#include "oled.h"
#include "rgb.h"

//////////////////////////////////////////////
//Global vars
static uint8_t barPos = 2;
uint32_t msTicks = 0;
struct pos {
	uint8_t x;
	uint8_t y;
	uint16_t value;
};
struct pos map[2][3] ={{{1,10, LPC_RTC->YEAR}, {37,10, LPC_RTC->MONTH}, {55,10, LPC_RTC->DOM}},{{1,20, LPC_RTC->HOUR}, {25,20, LPC_RTC->MIN}, {43,20, LPC_RTC->SEC}}};

//////////////////////////////////////////////

//char* valToString(uint16_t value, char* str) {
//	int i = 0;
//	uint16_t tmp = value;
//	while(tmp > 0){
//		tmp = tmp / 10;
//		i++;
//	}
//	str = malloc( sizeof(char) * ( i + 1 ) );
//	for(int j= i - 1; j >= 0; j--) {
//		str[i] = (char)(value % 10 + '0');
//		value = value / 10;
//	}
//	return str;
//}
//
//void chooseAnHour(void){
//	char* str;
//	str = valToString(map[0][0].value, str);
//	oled_putString(map[0][0].x, map[0][0].y, str, OLED_COLOR_BLACK, OLED_COLOR_WHITE);
//	free(str);
//}


static void moveBar(uint8_t steps, uint8_t dir)
{
    uint16_t ledOn = 0;

    if (barPos == 0)
        ledOn = (1 << 0) | (3 << 14);
    else if (barPos == 1)
        ledOn = (3 << 0) | (1 << 15);
    else
        ledOn = 0x07 << (barPos-2);

    barPos += (dir*steps);
    barPos = (barPos % 16);

    pca9532_setLeds(ledOn, 0xffff);
}

static uint8_t ch7seg = '0';
static void change7Seg(uint8_t rotaryDir)
{

    if (rotaryDir != ROTARY_WAIT) {

        if (rotaryDir == ROTARY_RIGHT) {
            ch7seg++;
        }
        else {
            ch7seg--;
        }

        if (ch7seg > '9')
            ch7seg = '0';
        else if (ch7seg < '0')
            ch7seg = '9';

        led7seg_setChar(ch7seg, FALSE);

    }
}

static void drawOled(uint8_t joyState)
{
    static int wait = 0;
    static uint8_t currX = 48;
    static uint8_t currY = 32;
    static uint8_t lastX = 0;
    static uint8_t lastY = 0;

    if ((joyState & JOYSTICK_CENTER) != 0) {
        oled_clearScreen(OLED_COLOR_BLACK);
        return;
    }

    if (wait++ < 3)
        return;

    wait = 0;

    if ((joyState & JOYSTICK_UP) != 0 && currY > 0) {
        currY--;
    }

    if ((joyState & JOYSTICK_DOWN) != 0 && currY < OLED_DISPLAY_HEIGHT-1) {
        currY++;
    }

    if ((joyState & JOYSTICK_RIGHT) != 0 && currX < OLED_DISPLAY_WIDTH-1) {
        currX++;
    }

    if ((joyState & JOYSTICK_LEFT) != 0 && currX > 0) {
        currX--;
    }

    if (lastX != currX || lastY != currY) {
        oled_putPixel(currX, currY, OLED_COLOR_WHITE);
        lastX = currX;
        lastY = currY;
    }
}

static void changeRgbLeds(uint32_t value)
{
    uint8_t leds = 0;

    leds = value / 128;

    rgb_setLeds(leds);
}

#define NOTE_PIN_HIGH() GPIO_SetValue(0, 1<<26);
#define NOTE_PIN_LOW()  GPIO_ClearValue(0, 1<<26);




static uint32_t notes[] = {
        2272, // A - 440 Hz
        2024, // B - 494 Hz
        3816, // C - 262 Hz
        3401, // D - 294 Hz
        3030, // E - 330 Hz
        2865, // F - 349 Hz
        2551, // G - 392 Hz
        1136, // a - 880 Hz
        1012, // b - 988 Hz
        1912, // c - 523 Hz
        1703, // d - 587 Hz
        1517, // e - 659 Hz
        1432, // f - 698 Hz
        1275, // g - 784 Hz
};

static void playNote(uint32_t note, uint32_t durationMs) {

    uint32_t t = 0;

    if (note > 0) {

        while (t < (durationMs*1000)) {
            NOTE_PIN_HIGH();
            Timer0_us_Wait(note / 2);
            //delay32Us(0, note / 2);

            NOTE_PIN_LOW();
            Timer0_us_Wait(note / 2);
            //delay32Us(0, note / 2);

            t += note;
        }

    }
    else {
    	Timer0_Wait(durationMs);
        //delay32Ms(0, durationMs);
    }
}

static uint32_t getNote(uint8_t ch)
{
    if (ch >= 'A' && ch <= 'G')
        return notes[ch - 'A'];

    if (ch >= 'a' && ch <= 'g')
        return notes[ch - 'a' + 7];

    return 0;
}

static uint32_t getDuration(uint8_t ch)
{
    if (ch < '0' || ch > '9')
        return 400;

    /* number of ms */

    return (ch - '0') * 200;
}

static uint32_t getPause(uint8_t ch)
{
    switch (ch) {
    case '+':
        return 0;
    case ',':
        return 5;
    case '.':
        return 20;
    case '_':
        return 30;
    default:
        return 5;
    }
}

static void playSong(uint8_t *song) {
    uint32_t note = 0;
    uint32_t dur  = 0;
    uint32_t pause = 0;

    /*
     * A song is a collection of tones where each tone is
     * a note, duration and pause, e.g.
     *
     * "E2,F4,"
     */

    while(*song != '\0') {
        note = getNote(*song++);
        if (*song == '\0')
            break;
        dur  = getDuration(*song++);
        if (*song == '\0')
            break;
        pause = getPause(*song++);

        playNote(note, dur);
        //delay32Ms(0, pause);
        Timer0_Wait(pause);

    }
}

static uint8_t * song = (uint8_t*)"C2.C2,D4,C4,F4,E8,";
        //(uint8_t*)"C2.C2,D4,C4,F4,E8,C2.C2,D4,C4,G4,F8,C2.C2,c4,A4,F4,E4,D4,A2.A2,H4,F4,G4,F8,";
        //"D4,B4,B4,A4,A4,G4,E4,D4.D2,E4,E4,A4,F4,D8.D4,d4,d4,c4,c4,B4,G4,E4.E2,F4,F4,A4,A4,G8,";



static void init_ssp(void)
{
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
	PinCfg.Funcnum = 0;
	PinCfg.Portnum = 2;
	PinCfg.Pinnum = 2;
	PINSEL_ConfigPin(&PinCfg);

	SSP_ConfigStructInit(&SSP_ConfigStruct);

	// Initialize SSP peripheral with parameter given in structure above
	SSP_Init(LPC_SSP1, &SSP_ConfigStruct);

	// Enable SSP peripheral
	SSP_Cmd(LPC_SSP1, ENABLE);

}

static void init_i2c(void)
{
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

static void init_adc(void)
{
	PINSEL_CFG_Type PinCfg;

	/*
	 * Init ADC pin connect
	 * AD0.0 on P0.23
	 */
	PinCfg.Funcnum = 1;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Pinnum = 23;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);

	/* Configuration for ADC :
	 * 	Frequency at 0.2Mhz
	 *  ADC channel 0, no Interrupt
	 */
	ADC_Init(LPC_ADC, 200000);
	ADC_IntConfig(LPC_ADC,ADC_CHANNEL_0,DISABLE);
	ADC_ChannelCmd(LPC_ADC,ADC_CHANNEL_0,ENABLE);

}
// Note: CPU is running @96MHz
void PWM_vInit(void)
{
  //init PWM
  LPC_SC->PCLKSEL0 &=~(3<<12);      // reset
  LPC_SC->PCLKSEL0 |= (1<<12);      // set PCLK to full CPU speed (96MHz)
  LPC_SC->PCONP |= (1 << 6);        // PWM on
  LPC_PINCON->PINSEL4 &=~(15<<0);    // reset
  LPC_PINCON->PINSEL4 |= (1<<0);    // set PWM1.1 at P2.0
  LPC_PWM1->TCR = (1<<1);           // counter reset
  LPC_PWM1->PR  = (96000000UL-1)>>9;     // clock /96000000 / prescaler (= PR +1) = 1 s
  LPC_PWM1->MCR = (1<<1);           // reset on MR0
  LPC_PWM1->MR0 = 4;                // set PWM cycle 0,25Hz (according to manual)
  LPC_PWM1->MR1 = 2;                // set duty to 50%
  LPC_PWM1->MR2 = 2;                // set duty to 50%
  LPC_PWM1->LER = (1<<0)|(1<<1);    // latch MR0 & MR1
  LPC_PWM1->PCR |= (3<<9);           // PWM1 output enable
  LPC_PWM1->TCR = (1<<1)|(1<<0)|(1<<3);// counter enable, PWM enable
  LPC_PWM1->TCR = (1<<0)|(1<<3);    // counter enable, PWM enable


  GPIO_SetDir(2, 1<<10, 1); // To jest kontrolka do lewo
  GPIO_SetDir(2, 1<<11, 1); // To jest kontrolka do prawo
}

void PWM_ChangeDirection(void)
{
	uint32_t a = GPIO_ReadValue(2);
	uint32_t b =  1<<2;
	uint32_t c = a&b;
	if(GPIO_ReadValue(2) & 1<<2)
	{
		GPIO_ClearValue(2,1<<2);
		GPIO_SetValue(2,1<<1);

	}else
	{
		GPIO_ClearValue(2,1<<1);
		GPIO_SetValue(2,1<<2);
	}

//	if(LPC_PINCON->PINSEL4 & (1<<0)){
//		  LPC_PINCON->PINSEL4 &=~(15<<0);    // reset
//		  LPC_PINCON->PINSEL4 |= (1<<2);    // set PWM1.2 at P2.1
//	}
//	else{
//		  LPC_PINCON->PINSEL4 &=~(15<<0);    // reset
//		  LPC_PINCON->PINSEL4 |= (1<<0);    // set PWM1.1 at P2.0
//	}

}

void PWM_Right(){
	if(!(GPIO_ReadValue(2) & 1<<11))
	{
		GPIO_ClearValue(2,1<<10);
		GPIO_SetValue(2,1<<11);
	}
}

void PWM_Left(){
	if(!(GPIO_ReadValue(2) & 1<<10))
	{
		GPIO_ClearValue(2,1<<11);
		GPIO_SetValue(2,1<<10);
	}
}

void PWM_Stop_Mov(){
	GPIO_ClearValue(2, 3<<10);
}

void write_temp_on_screen(char *temp_str){
	int32_t temp = temp_read();
	for(int32_t i = 3; i >= 0; i--) {
		temp_str[i] = (char)(temp % 10 + '0');
		if(temp <= 0) {
			break;
		}
		if(i == 2) {
			temp_str[i] = '.';
		}
		else {
			temp = temp / 10;
		}
	}
}



void SysTick_Handler(void){
	msTicks++;
}

uint32_t getMsTicks(void){
	return msTicks;
}

void showOurTemp(void){
    char naszString[4];
    write_temp_on_screen(&naszString);
    oled_putString(1, 1, &naszString, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
}

void showPresentTime(void){
    char date_str[10];
    uint16_t year = LPC_RTC->YEAR;

	for(int8_t i = 3; i >= 0; i--) {
			date_str[i] = (char)(year % 10 + '0');
			if(year <= 0) {
				break;
			}
			year = year / 10;
	}
	date_str[4] = '-';

	uint8_t month = LPC_RTC->MONTH;
	for(uint8_t i = 6; i >= 5; i--) {
			date_str[i] = (char)(month % 10 + '0');
			if(month <= 0) {
				break;
			}
			month = month / 10;
	}

	date_str[7] = '-';

	uint8_t day = LPC_RTC->DOM;
	for(uint8_t i = 9; i >= 8; i--) {
			date_str[i] = (char)(day % 10 + '0');
			if(day <= 0) {
				break;
			}
			day = day / 10;
	}

	char time_str[9];

	uint8_t hour = LPC_RTC->HOUR;

	for(int8_t i = 1; i >= 0; i--) {
		time_str[i] = (char)(hour % 10 + '0');
				if(hour <= 0) {
					break;
				}
				hour = hour / 10;
		}


	time_str[2] = ':';

	uint8_t minute = LPC_RTC->MIN;

	for(uint8_t i = 4; i >= 3; i--) {
		time_str[i] = (char)(minute % 10 + '0');
				if(minute <= 0) {
					break;
				}
				minute = minute / 10;
		}


	time_str[5] = ':';

	uint8_t sec = LPC_RTC->SEC;
	for(uint8_t i = 7; i >= 6; i--) {
		time_str[i] = (char)(sec % 10 + '0');
					if(sec <= 0) {
						break;
					}
					sec = sec / 10;
			}
	time_str[8] = '\0';
	oled_putString(1, 10, &date_str, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
	oled_putString(1, 20, &time_str, OLED_COLOR_WHITE, OLED_COLOR_BLACK);

}

int main (void) {


    int32_t xoff = 0;
    int32_t yoff = 0;
    int32_t zoff = 0;

    int8_t x = 0;

    int8_t y = 0;
    int8_t z = 0;
    uint8_t dir = 1;
    uint8_t wait = 0;

    uint8_t state    = 0;

    uint32_t trim = 0;

    uint8_t btn1 = 0;


    init_i2c();
    init_ssp();
    init_adc();

//    rotary_init();
//    led7seg_init();
//
//    pca9532_init();
    joystick_init();
//    acc_init();
    oled_init();
//    rgb_init();
    SYSTICK_InternalInit(1);
    SYSTICK_Cmd(ENABLE);
    //SYSTICK_IntCmd(ENABLE);



    if (SysTick_Config(SystemCoreClock / 1000)) {
            while(1); // error
        }

    temp_init(&getMsTicks);


    PWM_vInit();

    /*
     * Assume base board in zero-g position when reading first value.
     */
//    acc_read(&x, &y, &z);
//    xoff = 0-x;
//    yoff = 0-y;
//    zoff = 64-z;

    /* ---- Speaker ------> */

//    GPIO_SetDir(2, 1<<0, 1);
//    GPIO_SetDir(2, 1<<1, 1);
//
//    GPIO_SetDir(0, 1<<27, 1);
//    GPIO_SetDir(0, 1<<28, 1);
//    GPIO_SetDir(2, 1<<13, 1);
//    GPIO_SetDir(0, 1<<26, 1);
//
//    GPIO_ClearValue(0, 1<<27); //LM4811-clk
//    GPIO_ClearValue(0, 1<<28); //LM4811-up/dn
//    GPIO_ClearValue(2, 1<<13); //LM4811-shutdn
//
//    btn1 = ((GPIO_ReadValue(0) >> 4) & 0x01);

    /* <---- Speaker ------ */

    //Timer0_Wait(10009);

    LPC_RTC->YEAR = 2024;
    LPC_RTC->MONTH = 6;
    LPC_RTC->DOM = 06;

    //LPC_RTC->HOUR = 5;
    //LPC_RTC->MIN = 5;
    //LPC_RTC->SEC = 5;

    PWM_ChangeDirection();
    GPIO_SetDir(0,1<<4,0);
    GPIO_SetDir(1,1<<31,0);
    oled_clearScreen(OLED_COLOR_BLACK);


    char naszString[4];
    write_temp_on_screen(&naszString);
    oled_putString(1, 1, &naszString, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
    //chooseAnHour();

    moveBar(1, dir);
	PWM_Stop_Mov();
	uint32_t ifCheckTheTemp;
    while (1) {

    	showPresentTime();
    	ifCheckTheTemp++;
    	if((ifCheckTheTemp%(1<<20))==0){
        	showOurTemp();
    	}

    	uint32_t but1 = ((GPIO_ReadValue(0) >> 4) & 0x01);
    	uint32_t but2 = ((GPIO_ReadValue(1) >> 31) & 0x01);
    	if(but1==0){
    		PWM_Left();
    	}
    	else if(but2==0){
            PWM_Right();
    	}else
    	{
    		PWM_Stop_Mov();
    	}

//    	Timer0_Wait(1600);
//        PWM_Stop_Mov();
//    	//PWM_ChangeDirection();
//        Timer0_Wait(1600);
//        PWM_Right();
//        Timer0_Wait(1600);
//        PWM_Left();

    }


}

void check_failed(uint8_t *file, uint32_t line)
{
	/* User can add his own implementation to report the file name and line number,
	 ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

	/* Infinite loop */
	while(1);
}