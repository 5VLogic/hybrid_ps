#define F_CPU 8000000

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <util/twi.h>
#include <avr/eeprom.h>
//#include "./lib/usart.h"// Only for debugging
#include "./lib/i2c_atmega88pb.h"

#define PIN_UI PB0
#define PIN_DISP_LATCH PB6
#define PIN_DISP_OE PB7
#define PIN_DISP_D1 PE0
#define PIN_DISP_D2 PE1
#define PIN_DISP_D3 PE2
#define PIN_MOSI PB3
#define PIN_MISO PB4
#define PIN_SCK PB5
#define PIN_SS PB2

#define UI_PIN_STATE_U 1
#define UI_PIN_STATE_I 0
#define UI_PIN_TIMEOUT 300
#define UI_PIN_COMMAND_NR 2

#define I2C_SLA 0x28// = (0) 0101 000
#define I2C_COMMAND 0x00// = 0000 00 x0
#define REG_OFFSET 30// difference between sw.reg. and lin.reg. out in [100mV]

#define INITIAL_AVG_CONST_VOLTAGE 200
#define INITIAL_AVG_CONST_CURRENT 300

#define PARAM_VOLTAGE 1
#define PARAM_CURRENT 0

#define MODE_NORMAL 0
#define MODE_VOLTAGE_CALIBRATION 1
#define MODE_CURRENT_CALIBRATION 2

uint8_t const array[10] = {0x03, 0x9F, 0x25, 0x0D, 0x99, 0x49, 0x41, 0x1F, 0x01, 0x09};
uint8_t const calibrSegmentArray[6] = {0b10000011, 0b10011111, 0b00000011, 0b11110011, 0b10011110, 0b00000011};

uint8_t digitValue, digitIndex = 0;
uint8_t valL, valH;
uint8_t oldPinState, uiPin, measParam = PARAM_VOLTAGE;
uint8_t potValue;
uint8_t mode = MODE_NORMAL;
uint8_t calibrVoltageFlag = 0, calibrCurrentFlag = 0;

uint16_t avgVoltage = 0, avgCurrent = 0, counter = 0, cycles = 0, uiPinCounter = 0;
uint16_t constAvgVoltage = INITIAL_AVG_CONST_VOLTAGE;
uint16_t constAvgCurrent = INITIAL_AVG_CONST_CURRENT;

uint32_t sumVoltage, sumCurrent, instVoltage, instCurrent, instMeasure;
uint32_t msCounter = 0;

void setPot();
void spiSetup();
void adcSetup();
void uiPinHandler();
void startupSequence();
void spiSendDigit(uint8_t character, uint8_t dp, uint8_t digitIndexLocal);
void switchMeasParam();
void getCalibrationsFromEEPROM();
void measAverageAndCalibrate();

void main(void)
{
	// Read bytes 1 to 3 of EEPROM to retreive last calibration settings saved.
	getCalibrationsFromEEPROM();

	// Setup SPI to master transmitter for 7 segmente display.
	spiSetup();

	adcSetup();

	// Display startup sequence with single segments going in circles.
	startupSequence();

	//usart_setup_tx();// Only in case of need to debug.

	// Setup I2C for 400kHz operation.
	i2c_Setup(400);

	// Force the uiPinHandler() to execute on startup.
	oldPinState = !(PINB & (1 << PIN_UI));

	while(1)
	{
		// Check if voltage/current switch was operated and act accordingly.
		uiPinHandler();

		// Read from last ADC conversion.
		valL = ADCL;
		valH = ADCH;
		instMeasure = (uint32_t)(valL | (valH << 8));

		// Add last measure to correct variable.
		// Divide by constant to calculate average and correct cycle.
		// Perform calibration if requested, and save new const in EEPROM.
		measAverageAndCalibrate();

		// Send correct display segment values for showing the measurement.
		if(mode == MODE_NORMAL)
		{
			switch(digitIndex){
				case 0:
					digitValue = (uiPin == UI_PIN_STATE_U ? avgVoltage : avgCurrent) / 100;
					break;
				case 1:
					digitValue = ( (uiPin == UI_PIN_STATE_U ? avgVoltage : avgCurrent) / 10) % 10;
					break;
				case 2:
					digitValue = (uiPin == UI_PIN_STATE_U ? avgVoltage : avgCurrent) % 10;
			}
			spiSendDigit(\
				( (digitValue == 0) && (digitIndex == 0) && (uiPin == UI_PIN_STATE_U) ) ? 0xFF : array[digitValue],\
				(digitIndex == uiPin),\
				digitIndex);
		}
		// Show calibration display ("U10" for voltage, "I1.0" for current).
		else{
			digitValue = calibrSegmentArray[3*mode - 3 + digitIndex];
			spiSendDigit(digitValue, 0, digitIndex);
		}

		// Update the digit variable for display multiplexing.
		digitIndex = (digitIndex + 1) % 3;

		// Switch between current and voltage measurements.
		switchMeasParam();

		// Update the value of the digital potentiometer for buck converter.
		setPot();

		// Start ADC conversion.
		ADCSRA |= (1 << ADSC);

		// Handle the millisecond counter and the mode for the double-toggle command.
		msCounter++;
		if(msCounter >= UI_PIN_TIMEOUT){
			if(mode != MODE_NORMAL && uiPinCounter == 1)
				mode = (mode + 1) % 3;
			uiPinCounter = 0;
			msCounter = 0;
		}

		_delay_ms(1);
	}
}

void measAverageAndCalibrate()
{
	// Add last measurement to correct variable for further averaging
	switch(measParam)
	{
		case PARAM_VOLTAGE:
			instVoltage = instMeasure;
			sumVoltage += instVoltage;
			break;
		case PARAM_CURRENT:
			instCurrent = instMeasure;
			sumCurrent += instCurrent;
	}

	// When cycle of measurements is done perform division for averaging.
	// If a calibration has been requested do it.
	if(cycles >= 333)
	{
		if(calibrVoltageFlag){
			constAvgVoltage = sumVoltage / 100;// Perform calibration
			eeprom_write_byte((uint8_t*)1, (uint8_t)(constAvgVoltage >> 8));// Store in EEPROM
			eeprom_write_byte((uint8_t*)2, (uint8_t)(constAvgVoltage & 0x00ff));
			calibrVoltageFlag = 0;
		}
		else if(calibrCurrentFlag){
			constAvgCurrent = sumCurrent / 100;
			eeprom_write_byte((uint8_t*)3, (uint8_t)(constAvgCurrent >> 8));
			eeprom_write_byte((uint8_t*)4, (uint8_t)(constAvgCurrent & 0x00ff));
			calibrCurrentFlag = 0;
		}
		// Calculate average voltage and current over samles collected.
		avgVoltage = (uint16_t)( sumVoltage / constAvgVoltage );
		avgCurrent = (uint16_t)( sumCurrent / constAvgCurrent );
		sumVoltage = 0;
		sumCurrent = 0;
		cycles = 0;
	}
	cycles++;
}

// Read the calibration values stored in EEPROM to avoid needing to recalibrate.
void getCalibrationsFromEEPROM()
{
	constAvgVoltage = ( (uint16_t)eeprom_read_byte((const uint8_t*)1) << 8 );
	constAvgVoltage += (uint16_t)eeprom_read_byte((const uint8_t*)2);

	constAvgCurrent = ( (uint16_t)eeprom_read_byte((const uint8_t*)3) << 8 );
	constAvgCurrent += (uint16_t)eeprom_read_byte((const uint8_t*)4);
}

// Set the value of the potentiometer for buck converter voltage output.
void setPot()
{
	//if(uiPin == UI_PIN_STATE_U)
	{
		if(instVoltage < 10) potValue = 255;
		else {
			uint16_t temp = 18600 / (instVoltage - 10 + REG_OFFSET);
			if(temp > 255) potValue = 255;
			else potValue = (uint8_t)temp;
		}

		i2c_Master_Start(TW_START);
		i2c_Master_SLA_Ack(I2C_SLA, TW_WRITE, 0xfffe);
		i2c_Master_Data_Ack(&(uint8_t){I2C_COMMAND}, TW_WRITE);
		i2c_Master_Data_Ack(&potValue, TW_WRITE);
		i2c_Master_Stop();
	}
}

// Send a single digit to the 7-segment display and handle multiplexing.
void spiSendDigit(uint8_t character, uint8_t dp, uint8_t digitIndexLocal)
{
	PORTE = 0x07;// pfet OFF
	PORTB &= ~(1 << PIN_DISP_LATCH);// latch low
	SPDR = character & (dp ? 0xFE : 0xFF);// data send
	while( (SPSR & (1 << SPIF)) == 0);
	PORTB |= (1 << PIN_DISP_LATCH);// latch high
	PORTE = ~(1 << digitIndexLocal);// pfet ON
}

void startupSequence()
{
	// Loop through each of the three digits.
	for(uint8_t digitStartup = 0; digitStartup < 3; digitStartup++)
	{
		// Loop through each of the 7 segments.
		for(uint8_t segment = 0; segment < 6; segment++)
		{
			spiSendDigit(0xFF & ~(0x80 >> segment), 0, digitStartup);
			_delay_ms(80);
		}
	}
}

void adcSetup()
{
	PRR &= ~(1 << PRADC);// should already be 0 by default

	// default reference is external AREF

	ADCSRA |= (1 << ADEN);// enable the ADC

	ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);// Set prascaler

	ADCSRB |= (1 << ACME);// Enable the analog Mux
}

void uiPinHandler()
{
	// Don't do anything if the switch wasn't operated.
	if(oldPinState == (PINB & (1 << PIN_UI))) return;

	// Change the uiPin variable depending on the switch state.
	if(PINB & (1 << PIN_UI)){
		uiPin = UI_PIN_STATE_I;
	}
	else{
		uiPin = UI_PIN_STATE_U;
	}
	oldPinState = (PINB & (1 << PIN_UI));

	// Handle the double-toggle command for calibration.
	uiPinCounter++;
	if(uiPinCounter == 2){
		if(mode == MODE_VOLTAGE_CALIBRATION)
			calibrVoltageFlag = 1;
		else if(mode == MODE_CURRENT_CALIBRATION)
			calibrCurrentFlag = 1;
		mode = (mode + 1) % 3;
	}
	msCounter = 0;
}

void spiSetup()
{
	// Enable SPI
	PRR &= ~(1 < PRSPI);

	// Set Pins MOSI, SCK as outputs and also SS because of HW reasons
	DDRB |= (1 << PIN_MOSI) | (1 << PIN_SCK) | (1 << PIN_SS) | (1 << PIN_DISP_OE) | (1 << PIN_DISP_LATCH);

	// Setup SPI: enable, set master, clk = fclk/4, enable interrupt
	SPCR = (1 << SPE) | (1 << MSTR) | (1 << DORD) | (1 << SPIE);

	// Set up phase and polarity
	SPCR &= ~(1 << CPHA) & ~(1 << CPOL);

	PORTB &= ~(1 << PIN_DISP_OE);

	DDRE |= (1 << PIN_DISP_D1) | (1 << PIN_DISP_D2) | (1 << PIN_DISP_D3);
}

void switchMeasParam()
{
	if(measParam == PARAM_VOLTAGE){
		ADMUX |= (1 << MUX0);// Select A1 input to measure votage divider
		measParam = PARAM_CURRENT;
	}
	else{
		ADMUX &= ~(1 << MUX0);// Select A0 input to measure current amp output
		measParam = PARAM_VOLTAGE;
	}
}



