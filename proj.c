/*****************************************************************************
 *   A demo example using several of the peripherals on the base board
 *
 *   Copyright(C) 2011, EE2024
 *   All rights reserved.
 *
 ******************************************************************************/

#include <string.h>
#include <stdio.h>
#include "lpc17xx_pinsel.h"
#include "lpc17xx_ssp.h"
#include "lpc17xx_i2c.h"
#include "lpc17xx_timer.h"

#include "temp.h"
#include "oled.h"
#include "acc.h"
#include "led7seg.h"

#define TEMP_THRESHOLD 27.60
/*
 * GLOBAL VARIABLES
 */
volatile uint32_t msTicks;

/*
 * 0 = stationary
 * 1 = launch
 * 2 = return
 */
int currentState = 0;

/////////////////////FLAGS//////////////////////////////
int sw3 = 0;
int countdownFlag = 0;
int tempWarning = 0;
int offCourseWarning = 0;
int obstacleWarning = 0;

int clearWarningFlag = 0;
/////////////////////FOR COUNTDOWN///////////////////////
uint32_t countdownTimer = 0;
int countdownCounter = 15;

/////////////////////FOR TEMP READ///////////////////////
int tempFlag = 0; //whether there is enough readings to print on OLED

static uint32_t t1 = 0;
static uint32_t t2 = 0;
static uint8_t state = 0;
static uint32_t count = 0;
static uint32_t period = 0;

static int32_t tempReading = 0;

//string values for modes
char STRING_STATIONARY[] = "STATIONARY";
char STRING_LAUNCH[] = "LAUNCH";
char STRING_RETURN[] = "RETURN";
char tempWarningMsg[] = "TEMP WARNING";

void SysTick_Handler(void) { msTicks++; }

uint32_t getTicks(void){ return msTicks; }

// EINT3 Interrupt Handler
void EINT3_IRQHandler(void)
{
	// Determine whether GPIO Interrupt P2.10 has occurred
	if ((LPC_GPIOINT->IO2IntStatF>>10)& 0x1)
	{
        sw3 = 1;
        LPC_GPIOINT->IO2IntClr = 1<<10;
	}

	// Determine whether GPIO Interrupt P0.2 has occurred
	if ((LPC_GPIOINT->IO0IntStatF>>2)& 0x1)
	{
		myReadTemp();
        LPC_GPIOINT->IO0IntClr = 1<<2;
	}
}

void myReadTemp(void) {
    if (state == 0) {
    	t1 = getTicks();
    } else {
    	t2 = getTicks();
    }
    state = !state;

    if (t2 > t1) {
        period += t2-t1;
    }
    else {
    	period += t1-t2;
    }
    count++;
    if (count == 340) {
        count = 0;
        tempFlag = 1;
        tempReading = ( (1000 * period)/340 - 2731 );
    	period = 0;
    }
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

/*
 * Initializes interrupts
 * Enable Interrupts
 */
static void setup(void) {
	init_ssp();
	init_i2c();

	SysTick_Config(SystemCoreClock/1000);

    oled_init();
    acc_init();
    led7seg_init();
    temp_init(getTicks);

    NVIC_ClearPendingIRQ(EINT3_IRQn);
    LPC_GPIOINT->IO2IntEnF |= 1<<10; // Enable GPIO Interrupt P2.10
    LPC_GPIOINT->IO0IntEnF |= 1<<2; // Enable GPIO Interrupt P0.2
    NVIC_EnableIRQ(EINT3_IRQn);

}

uint32_t get7segChar(int number) {
	uint32_t toReturn = 0;
	switch(number) {
	case 15:
		toReturn = 0x71;
		break;
	case 14:
		toReturn = 0x70;
		break;
	case 13:
		toReturn = 0xA8;
		break;
	case 12:
		toReturn = 0x74;
		break;
	case 11:
		toReturn = 0x38;
		break;
	case 10:
		toReturn = 0x21;
		break;
	case 9:
		toReturn = 0x22;
		break;
	case 8:
		toReturn = 0x20;
		break;
	case 7:
		toReturn = 0xA7;
		break;
	case 6:
		toReturn = 0x30;
		break;
	case 5:
		toReturn = 0x32;
		break;
	case 4:
		toReturn = 0x2B;
		break;
	case 3:
		toReturn = 0xA2;
		break;
	case 2:
		toReturn = 0xE0;
		break;
	case 1:
		toReturn = 0xAF;
		break;
	case 0:
		toReturn = 0x24;
		break;
	}
	return toReturn;
}

void stationaryMode(void) {
	oled_putString(0, 0, STRING_STATIONARY, OLED_COLOR_WHITE, OLED_COLOR_BLACK);

	//if sw3 has been pressed, begin countdown
	if (countdownFlag == 1) {
		uint32_t currentTime = getTicks();
		//1 second has passed
		if ((currentTime - countdownTimer) > 1000) {
			countdownTimer = getTicks();
			countdownCounter -= 1;
			led7seg_setChar(get7segChar(countdownCounter), 1);
			//countdown successfully reach 0
			if (countdownCounter == 0) {
				countdownFlag = 0;
				currentState = 1;  //toggle to launch mode
				oled_clearScreen(OLED_COLOR_BLACK);
			}
		}
	} else 	led7seg_setChar(get7segChar(countdownCounter), 1);

}

void launchMode(void) {
	oled_putString(0, 0, STRING_LAUNCH, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
	led7seg_setChar(get7segChar(countdownCounter), 1);
}

static void toggleMode(void) {
	if (currentState==0) {
		if (sw3 == 1) {
			countdownFlag = 1;
			countdownTimer = getTicks(); //begin the countdown timer
			sw3 = 0; //reset the flag
		}
		stationaryMode();
	}
	if (currentState == 1) {
		launchMode();
	}
}

void checkWarnings() {
	char temp_char[40];

	if (tempFlag == 1) {
		tempFlag = 0;
    	sprintf(temp_char, "%.2f", tempReading/10.0);
    	oled_putString(0, 10, temp_char, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
    	if (tempReading/10.0 >= TEMP_THRESHOLD) {
    		oled_putString(0, 20, tempWarningMsg, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
    	}
	}
}

int main (void) {

	setup();

    oled_clearScreen(OLED_COLOR_BLACK);
    while(1) {
    	toggleMode();
    	checkWarnings();
    }
}

void check_failed(uint8_t *file, uint32_t line)
{
	/* User can add his own implementation to report the file name and line number,
	 ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
	/* Infinite loop */
	while(1);
}

