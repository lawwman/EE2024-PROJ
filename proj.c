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
#include "lpc17xx_uart.h"

#include "temp.h"
#include "oled.h"
#include "acc.h"
#include "led7seg.h"
#include "rgb.h"
#include "light.h"
#include "pca9532.h"


#define RGB_RED   0x01
#define RGB_BLUE  0x02

#define OBSTACLE_NEAR_THRESHOLD 3000
#define TEMP_THRESHOLD 360
/*
 * GLOBAL VARIABLES
 */
volatile uint32_t msTicks;

static uint8_t uart_stationary[] = "Entering STATIONARY Mode \r\n";
static uint8_t uart_launch[] = "Entering LAUNCH Mode \r\n";

//for timer 1 interrupt
#define SBIT_TIMER1  2
#define SBIT_MR0I    0
#define SBIT_MR0R    1
#define SBIT_CNTEN   0

/*
 * 0 = stationary
 * 1 = launch
 * 2 = return
 */
int currentState = 0;
static uint32_t lastPressedTime = 0; //record last instance of sw3 being pressed (Launch mode)

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

/////////////////////FOR UART///////////////////////
uint8_t rev_buf[4];    // Reception buffer
uint32_t rev_cnt = 0;  // Reception counter
uint8_t teraterm[4];   // To check against intMsg
uint8_t intMsg[4] = {'R', 'P', 'T', '\0'};

uint32_t isReceived = 0;  // Init to be not received


/////////////////////FOR TEMP READ///////////////////////
int tempFlag = 0; //whether there is enough readings to print on OLED

static uint32_t t1 = 0;
static uint32_t t2 = 0;
static uint8_t state = 0;
static uint32_t count = 0; //count number of readings taken
static uint32_t period = 0;
static int32_t tempReading = 0;

/////////////////////RGB///////////////////////
int toggleRGB = 0;

/////////////////////ACC///////////////////////
int32_t xoff = 0;
int32_t yoff = 0;
int32_t zoff = 0;
int8_t x = 0;
int8_t y = 0;
int8_t z = 0;

/////////////////////LIGHT SENSOR///////////////////////
uint32_t light_value = 0;
int clearLightWarningFlag = 0;

//string values for modes
char STRING_STATIONARY[] = "STATIONARY";
char STRING_LAUNCH[] = "LAUNCH";
char STRING_RETURN[] = "RETURN";
char tempWarningMsg[] = "Temp. Too high";
char accWarningMsg[] = "Veer off course";
char lightWarningMsg[] = "Obstacle near";

void SysTick_Handler(void) { msTicks++; }

uint32_t getTicks(void){ return msTicks; }

// EINT3 Interrupt Handler
void EINT3_IRQHandler(void)
{
	// Determine whether GPIO Interrupt P2.10 has occurred
	// SW3 interrupt
	if ((LPC_GPIOINT->IO2IntStatF>>10)& 0x1)
	{
        sw3 = 1;
        LPC_GPIOINT->IO2IntClr = 1<<10;
	}

	// Determine whether GPIO Interrupt P0.2 has occurred
	// Temp sensor interrupt
	if ((LPC_GPIOINT->IO0IntStatF>>2)& 0x1)
	{
		myReadTemp();
        LPC_GPIOINT->IO0IntClr = 1<<2;
	}

	// Determine whether GPIO Interrupt P2.5 has occurred
	// Light sensor interrupt
	if ((LPC_GPIOINT->IO2IntStatF>>5) & 0x1)
	{
		obstacleWarning = 1;
        light_clearIrqStatus();
        LPC_GPIOINT ->IO2IntClr = 1<<5;
	}

}

void UART3_IRQHandler(void)
{
	UART3_StdIntHandler();
}

//TIMER 1 Interrupt Handler
void TIMER1_IRQHandler(void)
{
    unsigned int isrMask;

    isrMask = LPC_TIM1->IR;
    LPC_TIM1->IR = isrMask;         /* Clear the Interrupt Bit */
    toggleRGB = !toggleRGB;
}

void myReadTemp(void) {
	if (currentState != 2) {
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
}

static void distanceLED(void){

	int readings = light_value;
	if(readings>= 0 && readings<250){
		pca9532_setLeds(0x1,0xFFFF);
	}
	else if(readings>= 250 && readings<500){
		pca9532_setLeds(0x3,0xFFFF);
	}
	else if(readings>= 500 && readings<750){
		pca9532_setLeds(0x7,0xFFFF);
	}
	else if(readings>= 750 && readings<1000){
		pca9532_setLeds(0xF,0xFFFF);
	}
	else if(readings>= 1000 && readings<1250){
		pca9532_setLeds(0x1F,0xFFFF);
	}
	else if(readings>= 1250 && readings<1500){
		pca9532_setLeds(0x3F,0xFFFF);
	}
	else if(readings>= 1500 && readings<1750){
		pca9532_setLeds(0x7F,0xFFFF);
	}
	else if(readings>= 1750 && readings<2000){
		pca9532_setLeds(0xFFF,0xFFFF);
	}
	else if(readings>= 2000 && readings<2250){
		pca9532_setLeds(0x1FF,0xFFFF);
	}
	else if(readings>= 2250 && readings<2500){
		pca9532_setLeds(0x3FF,0xFFFF);
	}
	else if(readings>= 2500 && readings<2750){
		pca9532_setLeds(0x7FF,0xFFFF);
	}
	else if(readings>= 2750 && readings<3000){
		pca9532_setLeds(0xFFF,0xFFFF);
	}
	else if(readings>= 3000 && readings<3250){
		pca9532_setLeds(0x1FFF,0xFFFF);
	}
	else if(readings>= 3250 && readings<3500){
		pca9532_setLeds(0x3FFF,0xFFFF);
	}
	else if(readings>= 3500 && readings<3750){
		pca9532_setLeds(0x7FFF,0xFFFF);
	}
	else if(readings>= 3750 && readings<4000){
		pca9532_setLeds(0xFFFF,0xFFFF);
	}
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

void my_read_acc(void ) {
	acc_read(&x, &y, &z);
    x = x+xoff;
    y = y+yoff;

    char Xvalue[5];
    char Yvalue[5];

    sprintf(Xvalue, "%.2f", x/9.8);
    sprintf(Yvalue, "%.2f", y/9.8);

    if (x/1.0 > 3.92 || y/1.0 > 3.92) {
    	offCourseWarning = 1;
    }

    char acc_valX[40];
    char acc_valY[40];

    strcpy(acc_valX, "X:");
    strcat(acc_valX, Xvalue);
    strcat(acc_valX, " ");
    strcat(acc_valX, "  ");
    strcpy(acc_valY, "Y:");
    strcat(acc_valY, Yvalue);
    strcat(acc_valY, " ");
    strcat(acc_valY, "  ");

	oled_putString(0, 20, acc_valX, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
	oled_putString(0, 30, acc_valY, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
}

void UART_IntReceive(void)
{
    /* Read the received data */
    if(UART_Receive(LPC_UART3, &rev_buf[rev_cnt], 1, NONE_BLOCKING) == 1) {
        if(rev_buf[rev_cnt] == '\r'){
        	rev_buf[rev_cnt] = 'X';
        	rev_buf[3] = '\0';
            isReceived = 1;
            strcpy(teraterm, rev_buf);
        }
        if (rev_buf[rev_cnt] == '\n') {
        	rev_cnt = -1;
        }
        rev_cnt++;
        if(rev_cnt == 4) rev_cnt = 0;
    }
}

void my_rgb_setLeds (uint8_t ledMask)
{
    if ((ledMask & RGB_RED) != 0) {
        GPIO_SetValue( 2, 1);
    } else {
        GPIO_ClearValue( 2, 1 );
    }

    if ((ledMask & RGB_BLUE) != 0) {
        GPIO_SetValue( 0, (1<<26) );
    } else {
        GPIO_ClearValue( 0, (1<<26) );
    }
}

unsigned int getPrescalarForUs(uint8_t timerPclkBit)
{
    unsigned int pclk,prescalarForUs;
    pclk = (LPC_SC->PCLKSEL0 >> timerPclkBit) & 0x03;  /* get the pclk info for required timer */

    switch ( pclk )                                    /* Decode the bits to determine the pclk*/
    {
    case 0x00:
        pclk = SystemCoreClock/4;
        break;

    case 0x01:
        pclk = SystemCoreClock;
        break;

    case 0x02:
        pclk = SystemCoreClock/2;
        break;

    case 0x03:
        pclk = SystemCoreClock/8;
        break;

    default:
        pclk = SystemCoreClock/4;
        break;
    }

    prescalarForUs =pclk/1000000 - 1;                    /* Prescalar for 1us (1000000Counts/sec) */

    return prescalarForUs;
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


static void init_GPIO(void)
{
	PINSEL_CFG_Type PinCfg;
	PinCfg.Funcnum = 0;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Portnum = 1;
	PinCfg.Pinnum = 31;
	PINSEL_ConfigPin(&PinCfg); //for SW4
    GPIO_SetDir(1, 1<<31, 0);  //for SW4 to set as input

	PinCfg.Portnum = 2;
	PinCfg.Pinnum = 5;
	PINSEL_ConfigPin(&PinCfg); //for P2.5
	GPIO_SetDir(2, 1<<5, 0);   //for P2.5 to set as interrupt input

}

void pinsel_uart3(void){
    PINSEL_CFG_Type PinCfg;
    PinCfg.Funcnum = 2;
    PinCfg.Pinnum = 0;
    PinCfg.Portnum = 0;
    PINSEL_ConfigPin(&PinCfg);
    PinCfg.Pinnum = 1;
    PINSEL_ConfigPin(&PinCfg);
}


void init_uart(void){
    UART_CFG_Type uartCfg;
    uartCfg.Baud_rate = 115200;
    uartCfg.Databits = UART_DATABIT_8;
    uartCfg.Parity = UART_PARITY_NONE;
    uartCfg.Stopbits = UART_STOPBIT_1;
    //pin select for uart3;
    pinsel_uart3();
    //supply power & setup working parameters for uart3
    UART_Init(LPC_UART3, &uartCfg);
    //enable transmit for uart3
    UART_TxCmd(LPC_UART3, ENABLE);
}

void setUartInt(void) {
    // UART FIFO config
    UART_FIFO_CFG_Type UARTFIFOConfigStruct;

    UART_FIFOConfigStructInit(&UARTFIFOConfigStruct);
    // Init FIFO for UART3
    UART_FIFOConfig(LPC_UART3, &UARTFIFOConfigStruct);
    //------------------------------------------------
    UART_SetupCbs(LPC_UART3, 0, (void *)UART_IntReceive);
    /* Enable UART Rx interrupt */
    UART_IntConfig(LPC_UART3, UART_INTCFG_RBR, ENABLE);

    NVIC_ClearPendingIRQ(UART3_IRQn);
    /* Enable Interrupt for UART3 */
    NVIC_EnableIRQ(UART3_IRQn);
}

static void setup(void) {
	init_ssp();
	init_i2c();
	init_GPIO();

	SysTick_Config(SystemCoreClock/1000);

	light_init();
	light_enable();
	pca9532_init();
    oled_init();
    acc_init();
    rgb_init();
    led7seg_init();
    temp_init(getTicks);

    SystemInit();

    LPC_SC->PCONP |= (1<<SBIT_TIMER1);

    LPC_TIM1->MCR  = (1<<SBIT_MR0I) | (1<<SBIT_MR0R);/* Clear TC on MR0 match and Generate Interrupt*/
    LPC_TIM1->PR   = getPrescalarForUs(4);;          /* Prescalar for 1ms */
    LPC_TIM1->MR0  = 333 * 1000;                     /* Load timer value to generate 100ms delay*/
    LPC_TIM1->TCR  = (1 <<SBIT_CNTEN);               /* Start timer by setting the Counter Enable*/
    NVIC_EnableIRQ(TIMER1_IRQn);

    init_uart();
    setUartInt();

    LPC_GPIOINT->IO2IntEnF |= 1<<10; //Enable GPIO Interrupt P2.10 - sw3 int
    LPC_GPIOINT->IO0IntEnF |= 1<<2;  //Enable GPIO Interrupt P0.2 - temp sensor int
	LPC_GPIOINT->IO2IntEnF |= 1<<5;  //Enable GPIO Interrupt P2.5 - Light sensor int
	light_setRange(LIGHT_RANGE_4000);
	light_setHiThreshold(3000);
	light_clearIrqStatus();
    NVIC_ClearPendingIRQ(EINT3_IRQn);
    NVIC_EnableIRQ(EINT3_IRQn);

}

void stationaryMode(void) {
	oled_putString(0, 0, STRING_STATIONARY, OLED_COLOR_WHITE, OLED_COLOR_BLACK);

	//if countdownFlag raised (by sw3 int), begin countdown
	if (countdownFlag == 1) {
		uint32_t currentTime = getTicks();
		//1 second has passed
		if ((currentTime - countdownTimer) > 1000) {
			countdownTimer = getTicks();
			countdownCounter -= 1;
			led7seg_setChar(get7segChar(countdownCounter), 1);
			//countdownCounter successfully reach 0
			if (countdownCounter == 0) {
				countdownFlag = 0;
				currentState = 1;  //toggle to launch mode
				UART_Send(LPC_UART3, (uint8_t *)uart_launch , strlen(uart_launch), BLOCKING);
				oled_clearScreen(OLED_COLOR_BLACK);
			}
		}
	} else 	led7seg_setChar(get7segChar(countdownCounter), 1); //if no countdown, display F in 7seg

}

void launchMode(void) {
	oled_putString(0, 0, STRING_LAUNCH, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
	led7seg_setChar(get7segChar(0), 1);
	my_read_acc();
}

void returnMode(void) {
	oled_putString(0, 0, STRING_RETURN, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
	led7seg_setChar(get7segChar(0), 1);
	light_value = light_read();
	distanceLED();
}

static void toggleMode(void) {
	if (currentState==0) {
		if (sw3 == 1 && tempWarning == 0) {
			countdownFlag = 1;
			 //begin the countdownTimer. Only called once, sw3 is set to zero in next line.
			countdownTimer = getTicks();
			sw3 = 0; //reset the flag
		}
		uint32_t test = getTicks();
		if (test > 500 && test < 510) {
		    UART_Send(LPC_UART3, (uint8_t *)uart_stationary , strlen(uart_stationary), BLOCKING);
		}
		stationaryMode();
	}
	if (currentState == 1) {
		launchMode();
		if (sw3 == 1) {
			uint32_t currentTime = getTicks();
			if (currentTime - lastPressedTime < 1000) {
				currentState = 2;
				oled_putString(0, 10, "        ", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
				oled_putString(0, 20, "        ", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
				oled_putString(0, 30, "        ", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
			}
			sw3 = 0;
			lastPressedTime = getTicks();
		}
	}
	if (currentState == 2) {
		returnMode();
		if (sw3 == 1) {
			currentState = 0;
			pca9532_setLeds(0x0,0xFFFF);
			oled_putString(0, 20, "             ", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
		}
	}
}

void checkWarnings(void) {
	//checking for warnings in STATIONARY or RETURN mode
	if (currentState == 0 || currentState == 1) {
		char temp_char[40];
		//show tempReading on led if tempFlag is 1
		if (tempFlag == 1) {
			tempFlag = 0;
			sprintf(temp_char, "%.2f", tempReading/10.0);
			oled_putString(0, 10, temp_char, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
			if (tempReading >= TEMP_THRESHOLD) {
				countdownFlag = 0; //abort countdownFlag
				countdownCounter = 15; //reset counter to display 'F' on 7 seg
				oled_putString(0, 40, tempWarningMsg, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
				tempWarning = 1;
			}
		}
		if (offCourseWarning == 1) {
			oled_putString(0, 50, accWarningMsg, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
		}
		if (tempWarning == 1 && offCourseWarning == 0) {
			if (toggleRGB == 0) {
				my_rgb_setLeds(0x00);
			}
			if (toggleRGB == 1) {
				my_rgb_setLeds(RGB_RED);
			}
		} else if (tempWarning == 1 && offCourseWarning == 1) {
			if (toggleRGB == 0) {
				my_rgb_setLeds(RGB_BLUE);
			}
			if (toggleRGB == 1) {
				my_rgb_setLeds(RGB_RED);
			}
		} else if (tempWarning == 0 && offCourseWarning == 1) {
			if (toggleRGB == 0) {
				my_rgb_setLeds(RGB_BLUE);
			}
			if (toggleRGB == 1) {
				my_rgb_setLeds(0);
			}
		}
	}
	//only in return mode
	if (currentState == 2) {
		char light_reading[40];
		sprintf(light_reading,"%d",light_value);
		strcat(light_reading, "  ");
		if (obstacleWarning == 1) {
			oled_putString(0, 30, lightWarningMsg, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
			obstacleWarning = 0;
			clearLightWarningFlag = 1;
		} else {
			if (clearLightWarningFlag == 1) {
				oled_putString(0, 30, "              ", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
				clearLightWarningFlag = 0;
			}
			oled_putString(0, 20, light_reading, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
		}
	}
}

//SW4 pressed once (for now in any mode) will clear the oled screen.
void clearWarnings(void) {
	if (((GPIO_ReadValue(1) >> 31) & 0x01) == 0) {
		oled_clearScreen(OLED_COLOR_BLACK);
		tempWarning = 0;
		offCourseWarning = 0;
		my_rgb_setLeds(0x00);
	}
}

int main (void) {

	setup();

    oled_clearScreen(OLED_COLOR_BLACK);
    my_rgb_setLeds(0x00);
    /*
     * Assume base board in zero-g position when reading first value.
     */
	acc_read(&x, &y, &z);
	xoff = 0-x;
	yoff = 0-y;
	zoff = 0-z;

    while(1) {
    	toggleMode();
    	checkWarnings();
    	clearWarnings();
    	if (isReceived == 1) {
    		isReceived = 0;
    		printf("%s\n", teraterm);
    		if(strcmp(teraterm, intMsg) == 0){
    			printf("set flag\n");
    		}
    		printf("%d\n", rev_cnt);
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

