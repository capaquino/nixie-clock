/*
 * rtc.c
 *
 * Created: 1/4/2020 8:04:14 PM
 *  Author: runne
 */ 


#include "i2cmaster.h"
#include "rtc.h"
#include <stdbool.h>

uint8_t rtc_read(unsigned char reg)
{
	uint8_t data;
	
	i2c_start(DS3231_SLAVE_ADDRESS+I2C_WRITE);
	i2c_write(reg);
	i2c_stop();
		
	i2c_start(DS3231_SLAVE_ADDRESS+I2C_READ);
	data = i2c_readNak();
	i2c_stop();
	
	return data;
}

void rtc_write(unsigned char reg, unsigned char value)
{
	i2c_start(DS3231_SLAVE_ADDRESS+I2C_WRITE);
	i2c_write(reg);
	i2c_write(value);
	i2c_stop();
}

uint8_t toSeconds(uint8_t i2c_seconds_register_read_data)
{
	return (i2c_seconds_register_read_data&0x0F)			// ones, 0b0000 1111
		+ (((i2c_seconds_register_read_data&0x70)>>4)*10);  // tens, 0b0111 0000 >> 4 * 10
}

uint8_t toMinutes(uint8_t i2c_minutes_register_read_data)
{
	return (i2c_minutes_register_read_data&0x0F)			// ones, 0b0000 1111
		+ (((i2c_minutes_register_read_data&0x70)>>4)*10);	// tens, 0b0111 0000 >> 4 * 10
}

uint8_t toHours(uint8_t i2c_hours_register_read_data)
{
	return (i2c_hours_register_read_data&0x0F)				// ones, mask 0b0000 1111
		+ (((i2c_hours_register_read_data&0x10)>>4)*10);	// tens, 0b0001 0000 >> 4 * 10
}





// extras below


// problem with this is that there could be stuff IN FRONT OF the 10s sec/min/hrs place
// so still better to be exact like above in my opinion.

uint8_t toRegisterValue(uint8_t decimal)
{
	// getting the first digit in the tens position by dividing by 10 ( xxXx base 10 )
	// that bit pattern represents # 10s needed, so shift by 4 to 2nd nibble so it is right
	// section of the register to represent the #10s.
	// %10 or 0x0F to get 1s and leave them be cause they are already in right position.
	return ((decimal/10)<<4) + (decimal%10); // gets 10s place, shift, add remainder
}

uint8_t fromRegisterValue(uint8_t hex)
{
	// getting the first bit in 2nd nibble ( xxxX xxxx base 2) by dividing by 16
	// that bit pattern represents # 10s of sec/min/hr, so multiply by 10
	// %16 or &0x0F to get 1s of seconds represented by first nibble
	return ((hex>>4) * 10) + (hex % 16);
}

// these from exploreembedded library, give me some ideas.
uint8_t dec2bcd(char num)
{
	return ((num/10 * 16) + (num % 10));
}

uint8_t bcd2dec(char num)
{
	return ((num/16 * 10) + (num % 16)); // first 4 bits never represent more than 9. Also, same as & 0x0F
										 // modulus 16 basically kills off anything past first nibble.
}