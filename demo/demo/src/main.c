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
#include "lpc17xx_dac.h"


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
static uint8_t barPos = 2;
uint32_t msTicks = 0;
Bool editing = FALSE;
Bool prevStateJoyClick = TRUE;
Bool prevStateJoyRight = TRUE;
Bool prevStateJoyLeft = TRUE;
Bool prevStateJoyUp = TRUE;
Bool prevStateJoyDown = TRUE;
uint32_t sample_index = 0;
uint32_t samples[NUM_SAMPLES];
Bool directionOfNextAlarm;
uint8_t eeprom_buffer[20];
extern const unsigned char sound_8k[];
extern int sound_sz;
Bool disableSound = FALSE;
uint32_t cnt = 0;
uint32_t sound_offset = 0;
//////////////////////////////////////////////
struct alarm_struct{
	Bool MODE; //Down->0, Up->1
	uint8_t HOUR;
	uint8_t MIN;
};




uint32_t len(uint32_t val){
	uint32_t i = 0;
	while(val > 0) {
		i++;
		val/=10;
	}
	return i;
}

char* uint32_t_to_str(uint32_t val, char* str){
	uint32_t val_len = len(val);
	for(int32_t i = val_len - 1; i >= 0; i--){
		str[i] = (char)(val%10 + '0');
		val/=10;
	}
	for(int32_t i = val_len; i<6; i++){
		str[i] = ' ';
	}
	str[5] = '\0';
}

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
// Note: CPU is running @100MHz
void PWM_vInit(void)
{
  //init PWM
  LPC_SC->PCLKSEL0 &=~(3<<12);      // reset
  LPC_SC->PCLKSEL0 |= (1<<12);      // set PCLK to full CPU speed (100MHz)
  LPC_SC->PCONP |= (1 << 6);        // PWM on
  LPC_PINCON->PINSEL4 &=~(15<<0);    // reset
  LPC_PINCON->PINSEL4 |= (1<<0);    // set PWM1.1 at P2.0
  LPC_PWM1->TCR = (1<<1);           // counter reset
  LPC_PWM1->PR  = (100000000UL-1)>>13;     // clock /100000000 / prescaler (= PR +1) = 1 s
  LPC_PWM1->MCR = (1<<1);           // reset on MR0
  LPC_PWM1->MR0 = 4;                // set PWM cycle 0,25Hz (according to manual)
  LPC_PWM1->MR1 = 2;                // set duty to 50%
  LPC_PWM1->LER = (1<<0);    // latch MR0 & MR1
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

int32_t get_sample_rate(void){
	/* ChunkID */
	if (sound_8k[cnt] != 'R' && sound_8k[cnt+1] != 'I' &&
		sound_8k[cnt+2] != 'F' && sound_8k[cnt+3] != 'F')
	{
		//Wrong format (RIFF)
		return -1;
	}
	cnt+=4;

	/* skip chunk size*/
	cnt += 4;

	/* Format */
	if (sound_8k[cnt] != 'W' && sound_8k[cnt+1] != 'A' &&
		sound_8k[cnt+2] != 'V' && sound_8k[cnt+3] != 'E')
	{
		//Wrong format (WAVE)
		return -2;
	}
	cnt+=4;

	/* SubChunk1ID */
	if (sound_8k[cnt] != 'f' && sound_8k[cnt+1] != 'm' &&
		sound_8k[cnt+2] != 't' && sound_8k[cnt+3] != ' ')
	{
		//Missing fmt
		return -3;
	}
	cnt+=4;

	/* skip chunk size, audio format, num channels */
	cnt+= 8;

	int32_t sampleRate = (sound_8k[cnt] | (sound_8k[cnt+1] << 8) |
			(sound_8k[cnt+2] << 16) | (sound_8k[cnt+3] << 24));

	if (sampleRate != 8000) {
		//Only 8kHz supported
		return -4;
	}

	cnt+=4;

	/* skip byte rate, align, bits per sample */
	cnt += 8;

	/* SubChunk2ID */
	if (sound_8k[cnt] != 'd' && sound_8k[cnt+1] != 'a' &&
		sound_8k[cnt+2] != 't' && sound_8k[cnt+3] != 'a')
	{
		//Missing data
		return -5;
	}
	cnt += 4;

	/* skip chunk size */
	cnt += 4;
	sound_offset = cnt;
	return sampleRate;
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
	temp_str[4] = ' ';
	temp_str[5] = 'C';
	temp_str[6] = '\0';
}



void SysTick_Handler(void){
	msTicks++;
}

uint32_t getMsTicks(void){
	return msTicks;
}

void showOurTemp(void){
    char naszString[7];
    write_temp_on_screen(&naszString);
    oled_putString(1, 0, &naszString, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
}

void showLuxometerReading(void){
	uint32_t light_val = light_read();
	char* xdd[6];
	uint32_t_to_str(light_val, xdd); //co
	oled_putString(43, 1, &xdd, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
}

void showEditmode(Bool editmode) {
	oled_line(86, 0, 91, 0, OLED_COLOR_WHITE);
	oled_line(85, 0, 85, 8, OLED_COLOR_WHITE);
	if (editmode) {
		oled_putChar(86, 1, 'E', OLED_COLOR_BLACK, OLED_COLOR_WHITE);
	} else {
		oled_putChar(86, 1, 'M', OLED_COLOR_BLACK, OLED_COLOR_WHITE);
	}
}

void showPresentTime(struct alarm_struct alarm[], int8_t y){
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
				if(hour < 0) {
					break;
				}
				hour = hour / 10;
		}


	time_str[2] = ':';

	uint8_t minute = LPC_RTC->MIN;

	for(uint8_t i = 4; i >= 3; i--) {
		time_str[i] = (char)(minute % 10 + '0');
				if(minute < 0) {
					break;
				}
				minute = minute / 10;
		}


	time_str[5] = ':';

	uint8_t sec = LPC_RTC->SEC;
	for(uint8_t i = 7; i >= 6; i--) {
		time_str[i] = (char)(sec % 10 + '0');
					if(sec < 0) {
						break;
					}
					sec = sec / 10;
			}
	char alarm_str[8];
	alarm_str[7] = '\0';
	uint8_t min;
	if(y == 3) {
		//alarm_str[0] = (char)(alarm[1].MODE + '0');
		alarm_str[0] = 'U';
		alarm_str[1] = ' ';
		hour = alarm[1].HOUR;
		min = alarm[1].MIN;
	}
	else {
		//alarm_str[0] = (char)(alarm[0].MODE + '0');
		alarm_str[0] = 'D';
		alarm_str[1] = ' ';
		hour = alarm[0].HOUR;
		min = alarm[0].MIN;
	}

	for(uint8_t i = 3; i >= 2; i--) {
		alarm_str[i] = (char)(hour % 10 + '0');
		if(hour < 0) {
			break;
		}
		hour = hour / 10;
	}
	alarm_str[4] = ':';

	for(uint8_t i = 6; i >= 5; i--) {
		alarm_str[i] = (char)(min % 10 + '0');
		if(min < 0) {
			break;
		}
		min = min / 10;
	}
	time_str[8] = '\0';
	oled_putString(1, 12, &date_str, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
	oled_putString(1, 24, &time_str, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
	oled_putString(37, 36, &alarm_str, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
}

struct pos {
	uint8_t x;
	uint8_t y;
	uint8_t length;
};

void valToString(uint32_t value, char* str, uint8_t len) {
	int i = len;
	uint16_t tmp = value;
	str[i] = '\0';
	for(int j= i - 1; j >= 0; j--) {
		str[j] = (char)(value % 10 + '0');
		value = value / 10;
	}
}

void chooseTime(struct pos map[4][3], int32_t LPC_values[], struct alarm_struct alarm[], int8_t x, int8_t y){
	char str[5];
	uint8_t len = map[y][x].length;
	if (x+y*3 < 6){
		valToString(LPC_values[x+y*3], str, len);
	} else {
		if(x==0) return;
		else if(x==1) valToString(alarm[y-2].HOUR, str, len);
		else if(x==2) valToString(alarm[y-2].MIN, str, len);
		setNextAlarm(alarm);
	}
	str[map[y][x].length] = '\0';
	oled_putString(map[y][x].x, map[y][x].y, str, OLED_COLOR_BLACK, OLED_COLOR_WHITE);
}

//void JoystickControls(char key){
//	uint32_t joyClick = ((GPIO_ReadValue(0) & (1<<17))>>17);
//	Bool joyClickDiff = joyClick - prevStateJoy;
//	if((joyClickDiff)&&(!editing)&&(!joyClick)){
//		editing = TRUE;
//		prevStateJoy = 0;
//		joyClickDiff = FALSE;
//	}
//	if((joyClickDiff)&&(editing)&&(!joyClick)){
//		editing = FALSE;
//		prevStateJoyClick = 0;
//		prevStateJoyClick = joyClick;
//	}
//}

Bool JoystickControls(char key, Bool output, Bool edit){
	uint8_t pin = 0;
	uint8_t portNum = 0;
	Bool prevStateJoy;
	if (key == 'u' || key == 'U'){
		portNum = 2;
		pin = 3;
		prevStateJoy = prevStateJoyUp;
	} else if (key == 'd' || key == 'D'){
		portNum = 0;
		pin = 15;
		prevStateJoy = prevStateJoyDown;
	} else if (key == 'r' || key == 'R'){
		portNum = 0;
		pin = 16;
		prevStateJoy = prevStateJoyRight;
	} else if (key == 'l' || key == 'L'){
		portNum = 2;
		pin = 4;
		prevStateJoy = prevStateJoyLeft;
	}
	uint32_t joyClick = ((GPIO_ReadValue(portNum) & (1<<pin))>>pin);
	Bool joyClickDiff = joyClick - prevStateJoy;
	if(edit){
		if((joyClickDiff)&&(editing)&&(!joyClick)){
			output = TRUE;
			prevStateJoy = 0;
			joyClickDiff = FALSE;
		}
		if((joyClickDiff)&&(!editing)&&(!joyClick)){
			output = FALSE;
			prevStateJoy = 0;
			prevStateJoy = joyClick;
		}
	} else {
		if((joyClickDiff)&&(!editing)&&(!joyClick)){
			output = TRUE;
			prevStateJoy = 0;
			joyClickDiff = FALSE;
		}
		if((joyClickDiff)&&(editing)&&(!joyClick)){
			output = FALSE;
			prevStateJoy = 0;
			prevStateJoy = joyClick;
		}
	}

	switch(key){
		case 'u':
		case 'U':
			prevStateJoyUp = joyClick;
			break;
		case 'd':
		case 'D':
			prevStateJoyDown = joyClick;
			break;
		case 'l':
		case 'L':
			prevStateJoyLeft = joyClick;
			break;
		case 'r':
		case 'R':
			prevStateJoyRight = joyClick;
			break;
	}
	return output;
}

void initTimer0(void){
	LPC_SC->PCONP |= (1<<0); //Wlaczenie zasilania

	//LPC_SC->PCLKSEL0 &= ~(3 << 2);

	LPC_SC->PCLKSEL0 |= (1 << 2);


	LPC_TIM0->PR = 0;
	LPC_TIM0->MR0 = 12345;
	LPC_TIM0->MCR |= (1<<0) | (1<<1);
	LPC_TIM0->TCR = 0x02; //reset
	LPC_TIM0->TCR = 0x01; //wlaczanie timera

	NVIC_EnableIRQ(TIMER0_IRQn);
}

void TIMER0_IRQHandler(void){
	if(LPC_TIM0->IR & (1<<0)) {
		LPC_TIM0->IR = (1<<0);
		if(!disableSound){
			if(cnt++ < sound_sz)
			{
				DAC_UpdateValue( LPC_DAC,(uint32_t)(sound_8k[cnt]));
			}
			else {
				LPC_TIM0->TCR = 0x02;
			}
		}
	}
}


void configPin(void) {
    //LPC_PINCON->PINSEL0 &= ~(3 << 8); // Clear bits for P0.04 (Pinsel 9:8 -> 11) -CAP2.0
    LPC_PINCON->PINSEL0 |= (3 << 8);  // Set bits for CAP2.0 function
}

void configTimer2(void) {
    LPC_SC->PCONP |= (1 << 22); // Power up Timer 2

    LPC_TIM2->CTCR = (1<<0); // Counter mode
    LPC_TIM2->TCR = 0x02; // Reset Timer
    LPC_TIM2->PR = 0; // No prescale
    //LPC_TIM2->CCR |= (1 << 0) | (1 << 2); // Capture on rising edge on CAP2.0 and interrupt on capture event
    LPC_TIM2->TCR = 0x01; // Start Timer
    //NVIC_EnableIRQ(TIMER2_IRQn); // Enable Timer 2 interrupt
}

void TIMER2_IRQHandler(void) {
    if (LPC_TIM2->IR & (1 << 4)) { // If interrupt generated by CAP2.0
        LPC_TIM2->IR = (1 << 4); // Clear interrupt flag
        // Handle the capture event
        uint32_t captureValue = LPC_TIM2->CR0; // Read capture value
        // Implement your logic to handle captureValue
    }
}


void generate_samples(void) {
	for (uint32_t i = 0; i < NUM_SAMPLES; i++){
		samples[i] = (sin(2*3.14159*i/ NUM_SAMPLES)+ 1) * 512;
	}
}
void changeValue(int16_t value, int32_t LPC_values[], struct alarm_struct alarm[2], uint8_t x, uint8_t y) {
	uint8_t pos_on_map = y*3+x;
	int32_t tmp;
	uint8_t i;
	if(pos_on_map < 6) {
		tmp = LPC_values[pos_on_map] + value;
	}
	else if(pos_on_map < 12){
		if(pos_on_map < 9) i = 0;
		else i = 1;
		if(pos_on_map == 6 ) tmp = alarm[0].MODE + value;
		if(pos_on_map == 7 ) tmp = alarm[0].HOUR + value;
		if(pos_on_map == 8 ) tmp = alarm[0].MIN + value;
		if(pos_on_map == 9) tmp = alarm[1].MODE + value;
		if(pos_on_map == 10) tmp = alarm[1].HOUR + value;
		if(pos_on_map == 11) tmp = alarm[1].MIN + value;
	}
	switch (pos_on_map){
	case 0:
		if(tmp > 2099) tmp = 2000;
		else if(tmp < 2000) tmp = 2099;
		LPC_RTC->YEAR = tmp;
		break;
	case 1:
		if(tmp > 12) tmp = 1;
		else if(tmp < 1) tmp = 12;
		LPC_RTC->MONTH = tmp;
		break;
	case 2:
		if(LPC_values[1] == 1 || LPC_values[1] == 3 || LPC_values[1] == 5 || LPC_values[1] == 7
				|| LPC_values[1] == 8 || LPC_values[1] == 10 || LPC_values[1] == 12){
			if(tmp > 31) tmp = 1;
			else if(tmp < 1) tmp = 31;
		}
		else if(LPC_values[1] == 4 || LPC_values[1] == 6 || LPC_values[1] == 9 || LPC_values[1] == 11){
			if(tmp > 30) tmp = 1;
			else if(tmp < 1) tmp = 30;
		}
		else if(LPC_values[1] == 2){
			if(tmp > 28) tmp = 1;
			else if(tmp < 1) tmp = 28;
		}
		LPC_RTC->DOM= tmp;
		break;
	case 3:
		if(tmp > 23) tmp = 0;
		else if(tmp < 0) tmp = 23;
		LPC_RTC->HOUR= tmp;
		break;
	case 4:
		if(tmp > 59) tmp = 0;
		else if(tmp < 0) tmp = 59;
		LPC_RTC->MIN= tmp;
		break;
	case 5:
		if(tmp > 59) tmp = 0;
		else if(tmp < 0) tmp = 59;
		LPC_RTC->SEC= tmp;
		break;
	case 6:
	case 9:
//		if(tmp > 1) tmp = 0;
//		else if(tmp < 0) tmp = 1;
//		alarm[i].MODE = tmp;
		break;
	case 7:
	case 10:
		if(tmp > 23) tmp = 0;
		else if(tmp < 0) tmp = 23;
		alarm[i].HOUR = tmp;
		break;
	case 8:
	case 11:
		if(tmp > 59) tmp = 0;
		else if(tmp < 0) tmp = 59;
		alarm[i].MIN = tmp;
		break;
	}
}

void correctDateValues(void){
	Bool leapYear = FALSE;
	if (LPC_RTC->YEAR % 400 == 0) leapYear = TRUE;
	else if (LPC_RTC->YEAR % 100 == 0) leapYear = FALSE;
	else if (LPC_RTC->YEAR % 4 == 0) leapYear = TRUE;
	else leapYear = FALSE;
	uint8_t lenghts[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	if (LPC_RTC->DOM > lenghts[LPC_RTC->MONTH]){
		if(leapYear && LPC_RTC->MONTH == 2){
			if (LPC_RTC->DOM > 29){
				LPC_RTC->DOM = 29;
			}
		}
		LPC_RTC->DOM = lenghts[LPC_RTC->MONTH];
	}
}

void setNextAlarm( struct alarm_struct alarm[]){
	int32_t hour0Diff = alarm[0].HOUR - LPC_RTC->HOUR + 24;
	hour0Diff = hour0Diff % 24;
	int32_t minute0Diff = alarm[0].MIN - LPC_RTC->MIN +60;
	minute0Diff = minute0Diff % 60;

	int32_t hour1Diff = alarm[1].HOUR - LPC_RTC->HOUR + 24;
	hour1Diff = hour1Diff % 24;
	int32_t minute1Diff = alarm[1].MIN - LPC_RTC->MIN +60;
	minute1Diff = minute1Diff % 60;
																		LPC_RTC->ALSEC = 0;
	if(hour0Diff<hour1Diff){
		LPC_RTC->ALHOUR = alarm[0].HOUR;
		LPC_RTC->ALMIN = alarm[0].MIN;
		directionOfNextAlarm = alarm[0].MODE;
	}else if(hour0Diff>hour1Diff){
		LPC_RTC->ALHOUR = alarm[1].HOUR;
		LPC_RTC->ALMIN = alarm[1].MIN;
		directionOfNextAlarm = alarm[1].MODE;
	}else if(minute0Diff<minute1Diff){
		LPC_RTC->ALHOUR = alarm[0].HOUR;
		LPC_RTC->ALMIN = alarm[0].MIN;
		directionOfNextAlarm = alarm[0].MODE;
	} else if(minute0Diff>minute1Diff){
	LPC_RTC->ALHOUR = alarm[1].HOUR;
	LPC_RTC->ALMIN = alarm[1].MIN;
	directionOfNextAlarm = alarm[1].MODE;
	}
}
int8_t read_time_from_eeprom(struct alarm_struct alarm[]){
	uint8_t temporary_buffer[4];
    int16_t eeprom_read_len = eeprom_read(temporary_buffer, EEPROM_OFFSET + 16, 4);
	if(eeprom_read_len != 4){
		return -5;
	}
    eeprom_read_len = eeprom_read(eeprom_buffer, EEPROM_OFFSET, 5);
    if(eeprom_read_len != 5){
    	return -5;
    }
    char header[] = "TIME";
    for (uint8_t i=0; i<4; i++){
    	if((char)eeprom_buffer[i] != header[i]){
    		return -i-1;
    	}
    	if((char)temporary_buffer[i] != header[i]){
			return -i-16;
		}
    }
    uint16_t len = eeprom_buffer[4];
    eeprom_read_len = eeprom_read(eeprom_buffer, EEPROM_OFFSET, len);
    if(eeprom_read_len != 20){
    	return -6;
    }
    LPC_RTC->YEAR = eeprom_buffer[5]*100 + eeprom_buffer[6];
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
	return 0;
}

int8_t write_time_to_eeprom(struct alarm_struct alarm[]){
	char header[] = "TIME";
	for(uint8_t i = 0; i < 4; i++){
		eeprom_buffer[i] = (uint8_t)(header[i]);
		eeprom_buffer[i+16] = (uint8_t)(header[i]);
	}
	eeprom_buffer[4] = 20;
	eeprom_buffer[5] = LPC_RTC->YEAR / 100;
	eeprom_buffer[6] = LPC_RTC->YEAR % 100;
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
	if (eeprom_write_len != 20){
		return -1;
	}
	return 0;
}
void RTC_IRQHandler(void){
	if (LPC_RTC->ILR&2){
		LPC_RTC->ILR = 2;
		cnt = sound_offset;
		initTimer0();
		//Tu nalezy wywolac interrupt do DAC
	}
}

int main (void) {
    uint8_t dir = 1;
    uint8_t wait = 0;

    uint8_t state                                                = 0;

    uint32_t trim = 0;

    uint8_t btn1 = 0;


    init_i2c();
    init_ssp();
    init_adc();
    eeprom_init();


    uint8_t positionInGUI = 0;
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

    generate_samples();

    if (SysTick_Config(SystemCoreClock / 1000)) {
		while(1); // error
	}

    temp_init(&getMsTicks);


    PWM_vInit();


    uint32_t cnt = 0;
	uint32_t off = 0;
	int32_t sampleRate = 0;
	uint32_t delay = 0;

	GPIO_SetDir(2, 1<<0, 1);
	GPIO_SetDir(2, 1<<1, 1);

	GPIO_SetDir(0, 1<<27, 1);
	GPIO_SetDir(0, 1<<28, 1);
	GPIO_SetDir(2, 1<<13, 1);
	GPIO_SetDir(0, 1<<26, 1);

	GPIO_ClearValue(0, 1<<27); //LM4811-clk
	GPIO_ClearValue(0, 1<<28); //LM4811-up/dn
	GPIO_ClearValue(2, 1<<13); //LM4811-shutdn


    light_enable();
    light_setMode(LIGHT_MODE_D1); //visible + infrared
    light_setRange(LIGHT_RANGE_64000);
    light_setWidth(LIGHT_WIDTH_16BITS);
    light_setIrqInCycles(LIGHT_CYCLE_1);


    //initTimer0();

    RTC_Init(LPC_RTC);
    LPC_RTC->YEAR = 2022;
    LPC_RTC->MONTH = 2;
    LPC_RTC->DOM = 2;
    LPC_RTC->HOUR = 2;
    LPC_RTC->MIN = 2;
    LPC_RTC->SEC = 2;
    LPC_RTC->CCR = 1;

    LPC_RTC->AMR &= ~((1<<2)|(1<<1)|(1<<0));
    LPC_RTC->ILR = 1;
    LPC_RTC->CIIR = 0;
    NVIC_EnableIRQ(RTC_IRQn);

    //Z przykladu ustawienie wyjcia z DAC

	PINSEL_CFG_Type PinCfg;

	PinCfg.Funcnum = 2;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Pinnum = 26;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);

    DAC_Init(LPC_DAC);


    int32_t LPC_values[] = {LPC_RTC->YEAR, LPC_RTC->MONTH, LPC_RTC->DOM, LPC_RTC->HOUR, LPC_RTC->MIN, LPC_RTC->SEC};

    PWM_ChangeDirection();
    GPIO_SetDir(0,1<<4,0);
    GPIO_SetDir(1,1<<31,0);
    oled_clearScreen(OLED_COLOR_BLACK);


    char naszString[4];
    write_temp_on_screen(&naszString);
    oled_putString(1, 0, &naszString, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
    showLuxometerReading();



	PWM_Stop_Mov();
	uint32_t ifCheckTheTemp;

	char xdx[] = "ALARM:\0";
	oled_putString(1, 36, xdx, OLED_COLOR_WHITE, OLED_COLOR_BLACK);


	sampleRate = get_sample_rate();
	if(sampleRate < 0){
		//err handle
		disableSound = TRUE;
	}

	Bool joystickOutput;
	Bool output = FALSE;
	uint8_t posX = 0;
	uint8_t posY = 0;

	struct alarm_struct alarm[2] = {{0, 2, 2},{1, 22, 22}};

//	map[0][0].x = 1;
//	map[0][0].y = 12;
//	map[0][0].length = 4;
    struct pos map[4][3] ={
    		{{1,12, 4}, {31,12, 2}, {49,12, 2}},
			{{1,24, 2}, {19,24, 2}, {37,24, 2}},
			{{37,36,1}, {49,36, 2}, {67,36, 2}},
			{{37,36,1}, {49,36, 2}, {67,36, 2}}};
    setNextAlarm(alarm);

    int8_t eeprom_read_ret_value = read_time_from_eeprom(alarm);
    if(eeprom_read_ret_value != 0){
    	//obsluga bledu
    }
    configPin();
    configTimer2();
    while (1) {
    	uint32_t joyClick = ((GPIO_ReadValue(0) & (1<<17))>>17);
    	int32_t LPC_values[] = {LPC_RTC->YEAR, LPC_RTC->MONTH, LPC_RTC->DOM, LPC_RTC->HOUR, LPC_RTC->MIN, LPC_RTC->SEC};
		Bool joyClickDiff = joyClick - prevStateJoyClick;

		if((joyClickDiff)&&(!editing)&&(!joyClick)){
			editing = TRUE;
			prevStateJoyClick = 0;
			joyClickDiff = FALSE;
		}

		if((joyClickDiff)&&(editing)&&(!joyClick)){
			editing = FALSE;
			prevStateJoyClick = 0;
		}
		prevStateJoyClick = joyClick;

        showEditmode(editing);


        if(!editing) {
			if (JoystickControls('u', output, FALSE)){
				posY += 3;
				posY = posY % 4;
			}
			output = FALSE;
			if (JoystickControls('d', output, FALSE)){
				posY += 5;
				posY = posY % 4;
			}
			output = FALSE;
			if (JoystickControls('l', output, FALSE)){
				posX += 2;
				posX = posX % 3;
			}
			output = FALSE;
			if (JoystickControls('r', output, FALSE)){
				posX += 4;
				posX = posX  % 3;
			}
			output = FALSE;
        } else {
			if (JoystickControls('u', output, TRUE)){
				changeValue(1, LPC_values, alarm, posX, posY);
			}
			output = FALSE;
			if (JoystickControls('d', output, TRUE)){
				changeValue(-1, LPC_values, alarm, posX, posY);
			}
			output = FALSE;
			if (JoystickControls('l', output, TRUE)){
				changeValue(-5, LPC_values, alarm, posX, posY);
			}
			output = FALSE;
			if (JoystickControls('r', output, TRUE)){
				changeValue(5, LPC_values, alarm, posX, posY);
			}
			output = FALSE;
        }



        chooseTime(map, LPC_values, alarm, posX, posY);
        correctDateValues();
    	showPresentTime(alarm, posY);



		ifCheckTheTemp++;
		if((ifCheckTheTemp%(1<<10))==0){
			int8_t eeprom_write_ret_value = write_time_to_eeprom(alarm);
			if(eeprom_write_ret_value != 0){
				//obsluga bledu
			}
		}
    	if((ifCheckTheTemp%(1<<8))==0){
        	showOurTemp();
			showLuxometerReading();
		}
    	if((ifCheckTheTemp%(1<<3))==0) {
    		//chooseTime(&line1, LPC_values);
    	}

    	uint32_t but1 = ((GPIO_ReadValue(0) >> 4) & 0x01);
    	uint32_t but2 = ((GPIO_ReadValue(1) >> 31) & 0x01);
    	//PWM_Stop_Mov();
    	if(but1==0){
    		PWM_Left();
    	}
    	else if(but2==0){
            PWM_Right();
    	}else
    	{
    		PWM_Stop_Mov();
    	}
    }
}




void check_failed(uint8_t *file, uint32_t line)
{
	/* User can add his own implementation to report the file name and line number,
	 ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

	/* Infinite loop */
	while(1);
}
