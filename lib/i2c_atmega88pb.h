
#ifndef CUSTOM_TWI
#define CUSTOM_TWI

#include <avr/io.h>
#include <util/twi.h>

// To send a START or RE_START condition
uint8_t i2c_Master_Start(uint8_t start_type)
{
	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);

	while ( !(TWCR & (1 << TWINT))) ;

	if ( (TWSR & 0xF8) != start_type )
		return 1;
	return 0;
}

// To send a STOP condition
void i2c_Master_Stop()
{
	TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

// Call the Slave device and expect an ACK in return
// rw is either TW_READ (1) or TW_WRITE (0)
// address is the absolute 7bit value, R/W is "appended" after.
uint8_t i2c_Master_SLA_Ack(uint8_t address, uint8_t rw, uint16_t timeout)
{
	TWDR = ( rw ) ? ( (address << 1) | 0x01 ) : ( (address << 1) & 0xFE );
	TWCR = (1 << TWINT) | (1 << TWEN);

	uint16_t counter = 0;
	while ( !(TWCR & (1 << TWINT))) {
		if (counter >= timeout) return 2;
		counter++;
	}

	if ( (TWSR & 0xF8) != ( rw ? TW_MR_SLA_ACK : TW_MT_SLA_ACK ) )
		return 1;
	return 0;
}

// Send or receive a byte of data as Master and send/expect ACK
uint8_t i2c_Master_Data_Ack(uint8_t* data, uint8_t rw)
{
	if ( rw ) { // TW_READ
		TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA);
	}
	else { // TW_WRITE
		TWDR = *data;
		TWCR = (1 << TWINT) | (1 << TWEN);
	}
	while ( !(TWCR & (1 << TWINT))) ;

	if ( (TWSR & 0xF8) != ( rw ? TW_MR_DATA_ACK : TW_MT_DATA_ACK ) )
		return 1;
	*data = TWDR;
	return 0;
}

// Send or receive a byte of data as Master and send/expect NACK
uint8_t i2c_Master_Data_Nack(uint8_t* data, uint8_t rw)
{
	if ( rw ) { // TW_READ
		TWCR = (1 << TWINT) | (1 << TWEN);
	}
	else { // TW_WRITE
		TWDR = *data;
		TWCR = (1 << TWINT) | (1 << TWEN);
	}
	while ( !(TWCR & (1 << TWINT))) ;

	if ( (TWSR & 0xF8) != ( rw ? TW_MR_DATA_NACK : TW_MT_DATA_NACK ) )
		return 1;
	*data = TWDR;
	return 0;
}

// Set Slave addr and wait indefinetly for a Master to call
uint8_t i2c_Slave_Listen(uint8_t address)
{
	TWAR = address;
	TWCR = (1 << TWEA) | (1 << TWEN) | (1 << TWINT);
	while ( !(TWCR & (1 << TWINT)) );
	if ( (TWSR & 0xF8) == TW_ST_SLA_ACK )
		return 0;
	return 1;
}

// Send a byte of data as Slave
uint8_t i2c_Slave_Transmit(uint8_t data)
{
	TWDR = data;
	TWCR = (1 << TWEA) | (1 << TWEN) | (1 << TWINT);
	while ( !(TWCR & (1 << TWINT)) );
	if ( (TWSR & 0xF8) == TW_ST_DATA_ACK )
		return 0;
	TWCR = (1 << TWINT);
	return 1;
}

// Setup the TWI peripheral
// Arg: frequency in kHz
void i2c_Setup(uint16_t frequency)
{
	// Set baud rate reg.
	TWBR = (uint32_t)F_CPU / ( (uint32_t)frequency * (uint32_t)1000 );
	PRR &= ~(1 << PRTWI);// Disable power reduction to TWI
}

#endif
