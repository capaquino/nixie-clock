/*
 * nixie-clock.c
 *
 * Created: 2/29/2020 2:14:40 PM
 * Author : Nathan
 */ 

#include <avr/io.h>
#include <stdlib.h>
#include <stdbool.h>

#define F_CPU 8000000UL
#include <util/delay.h>

//////////////////////////////////////////////////////////////////////////
/// Shift Register
//////////////////////////////////////////////////////////////////////////

#define HC595_PORT PORTD
#define HC595_DDR DDRD
#define HC595_DATA PORTD0
#define HC595_CLOCK PORTD1
#define HC595_LATCH PORTD2

void hc595_clock_pulse(void)
{
	HC595_PORT |= 1<<HC595_CLOCK;
	HC595_PORT &= ~(1<<HC595_CLOCK);
}

void hc595_latch_pulse(void)
{
	HC595_PORT |= 1<<HC595_LATCH;
	//_delay_us(1);
	HC595_PORT &= ~(1<<HC595_LATCH);
	//_delay_us(1);
}

void shift_bytes_msb(uint8_t bytes[], unsigned int numberOfBytes)
{
	uint8_t data = 0;
	
	for (unsigned int b = 0; b < numberOfBytes; b++)
	{
		data = bytes[b];
		for (uint8_t i = 0; i < 8; i++)
		{
			if (data & 0x80)
			{
				HC595_PORT |= 1<<HC595_DATA;
			}
			else
			{
				HC595_PORT &= ~(1<<HC595_DATA);
			}
			
			hc595_clock_pulse();
			
			data<<=1;
		}
	}
	
	hc595_latch_pulse();
}

void shift_byte_msb(uint8_t data)
{
	for (uint8_t i = 0; i < 8; i++)
	{
		if (data & 0x80)
		{
			HC595_PORT |= 1<<HC595_DATA;
		}
		else
		{
			HC595_PORT &= ~(1<<HC595_DATA);
		}
		
		hc595_clock_pulse();
		
		data<<=1;
	}
	
	hc595_latch_pulse();
}

void shift_byte_lsb(uint8_t data)
{
	for (uint8_t i = 0; i < 8; i++)
	{
		if (data & 0x01)
		{
			HC595_PORT |= 1<<HC595_DATA;
		}
		else
		{
			HC595_PORT &= ~(1<<HC595_DATA);
		}
		
		hc595_clock_pulse();
		
		data>>=1;
	}
	
	hc595_latch_pulse();
}

//////////////////////////////////////////////////////////////////////////
/// Nixie - requires Shift Register
//////////////////////////////////////////////////////////////////////////

#define NUMBER_OF_TUBES 2
#define OFF 0xF
#define ONES_TUBE 2
#define TENS_TUBE 1

void set_tube_digit(uint8_t bytes[], uint8_t digit, unsigned int tube)
{
	// no bounds check done
	bytes[tube-1] = digit;
}

void display(uint8_t bytes[], unsigned int numberOfBytes)
{
	unsigned int squishedBytesSize = 0;
	
	if (numberOfBytes % 2 == 0) //even
	{
		squishedBytesSize = numberOfBytes/2;
	}
	else // odd
	{
		squishedBytesSize = (numberOfBytes+1)/2;
	}
	
	// squish the array into half of its size since 1 74HC595 controls 2 K155ID1
	for (unsigned int i = 0; i < numberOfBytes; i++)
	{
		// no bounds checking on going over display bytes size, better hope its correct.
		// on odd elements, shift it left 4 and put it in the same byte as the previous element.
		// on even elements,
		if (i%2 == 0) // even
		{
			bytes[i/2] = bytes[i];
		}
		
		else // odd
		{
			bytes[(i-1)/2] |= bytes[i]<<4;
		}
	}
	
	shift_bytes_msb(bytes, squishedBytesSize);
}

void scroll(unsigned int numberOfTubes)
{
	uint8_t scrollBytes[numberOfTubes];
	
	for (uint8_t j = 0; j <= 9; j++) // scroll from 0 to 9 for each tube.
	{
		for (unsigned int k = 0; k < numberOfTubes; k++)
		{
			scrollBytes[k] = j;
		}
		
		display(scrollBytes, numberOfTubes);
		_delay_ms(20);
	}
}

//////////////////////////////////////////////////////////////////////////
/// I2C
//////////////////////////////////////////////////////////////////////////

#include "i2cmaster.h"
#include "rtc.h"

//////////////////////////////////////////////////////////////////////////
/// Interrupts
//////////////////////////////////////////////////////////////////////////

#include <avr/interrupt.h>

volatile bool nixieOutputOn = false; // volatile is NECESSARY

ISR(PCINT0_vect)
{
	if (PINB & 1<<PINB0) // rising edge, do nothing
	{

	}
	else // falling edge
	{
		if (nixieOutputOn == true)
		{
			nixieOutputOn = false;
		}
		else
		{
			nixieOutputOn = true;
		}
	}
	
	/*PCIFR = 0x01; Clear interrupt flag. Automatically done.*/
}

//////////////////////////////////////////////////////////////////////////
/// Main
//////////////////////////////////////////////////////////////////////////


int main(void)
{
	// Init Shift register
	HC595_DDR = 1<<HC595_DATA | 1<<HC595_CLOCK | 1<<HC595_LATCH;
	
	// Init I2C
	i2c_init();
	
	// Init DS3231
	rtc_write(DS3231_CONTROL_REG_OFFSET,0x00);
	rtc_write(DS3231_HOURS_REG_OFFSET,toRegisterValue(10)); // this may or may not be 12
	rtc_write(DS3231_MINUTES_REG_OFFSET,toRegisterValue(29));
	rtc_write(DS3231_SECONDS_REG_OFFSET,toRegisterValue(50));
	uint8_t rtc_data_sec = 0;
	uint8_t rtc_data_min = 0;
	uint8_t rtc_data_hour = 0;
	
	// Init nixie tube
	uint8_t nixie[NUMBER_OF_TUBES];
	set_tube_digit(nixie, OFF, 1);
	set_tube_digit(nixie, OFF, 2);
	display(nixie, NUMBER_OF_TUBES);
	
	// init interrupts
	DDRB &= ~(1<<PORTB0); // Set as input
	PORTB |= 1<<PORTB0; // Set internal pullup.
	PCICR |= 1<<PCIE0;// Enable interrupt 0 (interrupt for pins that have PCINT0-7)
	PCMSK0 |= 1<<PCINT0; // Set which pins from PCINT0-7 cause interrupt. In this case, set PB0.
	
	PCIFR = 0x01; // clear old/stray interrupts for PCINT0
	sei(); // enable interrupts
	
	while (1)
	{
		if (nixieOutputOn == true)
		{
			// Get clock data
			rtc_data_sec = rtc_read(DS3231_SECONDS_REG_OFFSET);
			rtc_data_min = rtc_read(DS3231_MINUTES_REG_OFFSET);
			rtc_data_hour = rtc_read(DS3231_HOURS_REG_OFFSET);
		
			// Organize into nixie tube data.
		
			// Hours
		
		
			// Minutes
		
		
			// Seconds
			set_tube_digit(nixie, toSeconds(rtc_data_sec)%10, ONES_TUBE);
			set_tube_digit(nixie, toSeconds(rtc_data_sec)/10, TENS_TUBE);
		
			// Display
			display(nixie, NUMBER_OF_TUBES);
		
			// scroll effect, delaying is very bad when interrutpts are enabled.
			//_delay_ms(800);
			//scroll(NUMBER_OF_TUBES);
		}
		else
		{
			set_tube_digit(nixie, OFF, ONES_TUBE);
			set_tube_digit(nixie, OFF, TENS_TUBE);
			display(nixie, NUMBER_OF_TUBES);
		}
	}
}

