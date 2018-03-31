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

int sw3 = 0;

int tempWarning = 0;
int offCourseWarning = 0;
int obstacleWarning = 0;

// values for acc
int8_t x = 0;
int8_t y = 0;
int8_t z = 0;

// offset values for acc
int32_t xoff = 0;
int32_t yoff = 0;

//string values for modes
char STRING_STATIONARY[] = "STATIONARY";
char STRING_LAUNCH[] = "LAUNCH";
char STRING_RETURN[] = "RETURN";

void SysTick_Handler(void) { msTicks++; }

// EINT3 Interrupt Handler
void EINT3_IRQHandler(void)
{
	// Determine whether GPIO Interrupt P2.10 has occurred
	if ((LPC_GPIOINT->IO2IntStatF>>10)& 0x1)
	{
        sw3 = 1;
        LPC_GPIOINT->IO2IntClr = 1<<10;
	}
}

uint32_t getTicks(void){ return msTicks; }

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
    NVIC_EnableIRQ(EINT3_IRQn);
}

static void toggleMode() {
	if (currentState == 0) {
		//begin countdown
	}
}

static void readTemp() {
	uint32_t temp_value = temp_read();
	printf("%2.2f degrees\n",temp_value/10.0);

	char temp_arr[10];
	sprintf(temp_arr, "%2.2f", temp_value/10.0);

	oled_putString(0, 10, temp_arr, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
}

int main (void) {

	setup();
    /*
     * Declaring variables to use
     */


    /*
     * Assume base board in zero-g position when reading first value.
     */
    acc_read(&x, &y, &z);
    xoff = 0-x;
    yoff = 0-y;

    oled_clearScreen(OLED_COLOR_BLACK);

    while (1)
    {
    	acc_read(&x, &y, &z);
        x = x+xoff;
        y = y+yoff;

        char Xvalue[5];
        char Yvalue[5];

        sprintf(Xvalue, "%d", x);
        sprintf(Yvalue, "%d", y);

        char acc_val[40];
        strcpy(acc_val, Xvalue);
        strcat(acc_val, " ");
        strcat(acc_val, Yvalue);
        strcat(acc_val, "\0");

        printf("%s\n", acc_val);

    	oled_putString(0, 0, STRING_STATIONARY, OLED_COLOR_WHITE, OLED_COLOR_BLACK);

    	readTemp();

    	if (currentState == 0) led7seg_setChar(0x71, 1);
    }


}

void check_failed(uint8_t *file, uint32_t line)
{
	/* User can add his own implementation to report the file name and line number,
	 ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

	/* Infinite loop */
	while(1);
}

