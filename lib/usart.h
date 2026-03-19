#include <avr/io.h>
#include <util/delay.h>

//#define BAUD 115200
//#define BAUD 19200
#define BAUD 38400


// Setup USART0 of atmega328p for transmission
void usart_setup_tx(void)
{
	// the hard coded 8 should only be used when the internal RC 8MHz CLK
	// is used, because the formula rounds down to 7.
	// See table on datasheet page 164
#if F_CPU == 8000000
	UBRR0 = 8;
#else
	UBRR0 = (F_CPU / 8 / BAUD) - 1;
#endif
	// for 8MHz clock, U2X = 1, 38400 baud
	UBRR0 = 25;

	UCSR0B = (1 << TXEN0);// | (1 << RXEN0);
	/* 8-bit character */
	UCSR0C = (3 << 1);

	UCSR0A = (1 << U2X0);
	/* Set TX pin as output */
	DDRD = (1 << PD1);
}


// sends string, maximum 256 characters.
// if more than that it returns 1.
uint8_t send(char* string)
{
	uint8_t character = 0;
	uint8_t i;

	while(string[character] != 0){

		// If USART isn't ready, wait for some time
		if(!(UCSR0A & (1 << UDRE0))) _delay_us(70000000/BAUD);
		// If USART still not ready, might be a problem
		if(!(UCSR0A & (1 << UDRE0))) return 1;

		// Send the character
		UDR0 = string[character];
		character++;

		// If string longer than 254 characters, abort, counter will overflow
		if(character == 255) return 2;
	}
	return 0;
}

char send_byte_in_hex(uint8_t byte)
{
	uint8_t nibble_high = byte >> 4;
	uint8_t nibble_low = byte & 0x0f;
	char string[3];
	string[0] = nibble_high + (nibble_high < 10 ? 48 : 55);
	string[1] = nibble_low + (nibble_low < 10 ? 48 : 55);
	string[2] = 0;
	send(string);
}


void str_print(char* string, uint16_t value)
{
	for(uint8_t i = 0; i < 16; i++)
		string[i] = (value & (32768 >> i)) ? 49 : 48;
	string[16] = '\n';
	string[17] = 0;
}




