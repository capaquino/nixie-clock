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

#pragma region 74HC595N Shift Register

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

#pragma endregion 74HC595N Shift Register

#pragma region Nixie Functions

//////////////////////////////////////////////////////////////////////////
/// Nixie - requires Shift Register
//////////////////////////////////////////////////////////////////////////

#define NUMBER_OF_TUBES 4
#define OFF 0xF

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

// turns off the display without modifiying nixie tube data in array
void turn_off_display(unsigned int numberOfTubes)
{
	uint8_t clearBytes[NUMBER_OF_TUBES];
	
	for (uint8_t i=0; i<numberOfTubes; i++)
	{
		clearBytes[i] = OFF;
	}
	
	display(clearBytes, numberOfTubes);
}

// overwrites the actual tube data in array with with OFF
void clear_tubes(uint8_t bytes[], unsigned int numberOfTubes)
{
	for (uint8_t i=0; i<numberOfTubes; i++)
	{
		bytes[i] = OFF;
	}
	
	display(bytes, numberOfTubes);
}

#pragma endregion Nixie Functions

#pragma region I2C

//////////////////////////////////////////////////////////////////////////
/// I2C
//////////////////////////////////////////////////////////////////////////

#include "i2cmaster.h"
#include "rtc.h"

#pragma endregion I2C

#pragma region Interrupts

//////////////////////////////////////////////////////////////////////////
/// Interrupts
//////////////////////////////////////////////////////////////////////////

#include <avr/interrupt.h>

/* Globals accessed during interrupts. volatile is necessary for any variables accessed in ISRs */

volatile bool nixieOutputOn = false;


/* Routines */

// In main: initialize with for interrupts on PC0/1/2.
//DDRC &= ~(1<<PORTC0 | 1<<PORTC1 | 1<<PORTC2);	// set pins to be used as interrupts as inputs
//PORTC |= 1<<PORTC0 | 1<<PORTC1 | 1<<PORTC2;	// enable internal pullups
//PCICR |= 1<<PCIE1; // Enable interrupt 1 (interrupt for pins that have PCINT8-14 aka PORTC
//PCMSK1 |= 1<<PCINT8 | 1<<PCINT9 | 1<<PCINT10; // Set which pins from PCINT8-14 cause interrupt. In this case, set PC0 PC1 PC2.
//PCIFR |= 0x02;

ISR(PCINT1_vect)
{
	if (   (PINC & 1<<PINC0)   // Use && and not || because when no buttons are pressed
		&& (PINC & 1<<PINC1)   // all pins read 1 because of pullups. If they are not all
		&& (PINC & 1<<PINC2) ) // 1 (meaning unpressed) then that means atleast 1 IS pressed.
							   // Can get rid of this if/else if you have no rising edge functionality.
	{
		// rising edge do nothing	
	}
	else // falling edge, figure out which triggered
	{
		//	conditional statements equivalent to:
		//	uint8_t mask	= 1<<PINC0 | 1<<PINC1 | 1<<PINC2; == 0x07	
		//  uint8_t filter  = PINC & mask;
		//			filter  = ~filter;
		// 	if (filter & 1<<PINCX)
			
		if (~(PINC & 0x07) & 1<<PINC0) // PC0 triggered
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
		else if (~(PINC & 0x07) & 1<<PINC1) // PC1 triggered
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
		
		else if (~(PINC & 0x07) & 1<<PINC2) // PC2 triggered
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
	}
	
	
	/*PCIFR = 0x01; Clear interrupt flag. Automatically done.*/
}

// In main: initialize with
//
// PCICR |= 1<<PCIE0;// Enable interrupt 0 (interrupt for pins that have PCINT0-7 aka PORTB
// PCMSK0 |= 1<<PCINT0; // Set which pins from PCINT0-7 cause interrupt. In this case, set PB0.
//
// PCIFR |= 0x01; // clear old/stray interrupts for PCINT0
// sei(); // enable interrupts
//

// display on/off pushbutton
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

#pragma endregion Interrupts

#pragma region IN15A/B Symbol Map

//////////////////////////////////////////////////////////////////////////
/// IN15A Symbol Map
//////////////////////////////////////////////////////////////////////////

#define IN15A_n			1
#define IN15A_percent	2
#define IN15A_pi_upper	3
#define IN15A_k			4
#define IN15A_M			5
#define IN15A_m			6
#define IN15A_plus		7
#define IN15A_minus		8
#define IN15A_P			9
#define IN15A_u			0

//////////////////////////////////////////////////////////////////////////
/// IN15B Symbol Map
//////////////////////////////////////////////////////////////////////////

#define IN15B_A			1
#define IN15B_ohm		2
#define IN15B_S			4
#define IN15B_V			5
#define IN15B_H			6
#define IN15B_hz		7
#define IN15B_F			9
#define IN15B_W			0

#pragma endregion IN15A/B Symbol Map

#pragma region Main

//////////////////////////////////////////////////////////////////////////
/// Main
//////////////////////////////////////////////////////////////////////////

// This needs to change depending on the number of tubes

#define ONES_TUBE		4		// 2 with two tubes	// 4 with four tubes // 6 with 6 tubes
#define TENS_TUBE		3		// 1 with two tubes // 3 with four tubes // 5 with 6 tubes
#define HUNDREDS_TUBE	2							// 2 with four tubes // 4 with 6 tubes
#define THOUSANDS_TUBE	1							// 1 with four tubes // 3 with 6 tubes



int main(void)
{
	// Init Shift register
	HC595_DDR = 1<<HC595_DATA | 1<<HC595_CLOCK | 1<<HC595_LATCH;
	
	// Init I2C
	i2c_init();
	
	// Init DS3231
	rtc_write(DS3231_CONTROL_REG_OFFSET,0x00);
	rtc_write(DS3231_HOURS_REG_OFFSET,toRegisterValue(10));
	rtc_write(DS3231_MINUTES_REG_OFFSET,toRegisterValue(59));
	rtc_write(DS3231_SECONDS_REG_OFFSET,toRegisterValue(45));
	uint8_t rtc_data_sec = 0;
	uint8_t rtc_data_min = 0;
	uint8_t rtc_data_hour = 0;
	
	// Init nixie tube
	uint8_t nixie[NUMBER_OF_TUBES];
	clear_tubes(nixie, NUMBER_OF_TUBES);
	display(nixie, NUMBER_OF_TUBES);
	
	/* init interrupts */
	
	// PORTB interrupts (Display on/off)
	DDRB &= ~(1<<PORTB0); // Set as input
	PORTB |= 1<<PORTB0; // Set internal pullup.
	PCICR |= 1<<PCIE0;// Enable interrupt 0 (interrupt for pins that have PCINT0-7)
	PCMSK0 |= 1<<PCINT0; // Set which pins from PCINT0-7 cause interrupt. In this case, set PB0.
	PCIFR |= 0x01; // clear old/stray interrupts for PCINT0
	
	// PORTC interrupts (time programming interrupts)
	DDRC &= ~(1<<PORTC0 | 1<<PORTC1 | 1<<PORTC2); // set as inputs
	PORTC |= 1<<PORTC0 | 1<<PORTC1 | 1<<PORTC2; // set internal pullups
	PCICR |= 1<<PCIE1; // Enable interrupt 1 (interrupt for pins that have PCINT8-14 aka PORTC
	PCMSK1 |= 1<<PCINT8 | 1<<PCINT9 | 1<<PCINT10; // Set which pins from PCINT8-14 cause interrupt. In this case, set PC0 PC1 PC2.
	PCIFR |= 0x02;

	sei(); // enable interrupts
	
	while (1)
	{
		if (nixieOutputOn == true)
		{
			// Get clock data
			rtc_data_sec = rtc_read(DS3231_SECONDS_REG_OFFSET);
			rtc_data_min = rtc_read(DS3231_MINUTES_REG_OFFSET);
			//rtc_data_hour = rtc_read(DS3231_HOURS_REG_OFFSET);
		
			// Organize into nixie tube data.
		
			// Hours
		
		
			// Minutes
			set_tube_digit(nixie, toMinutes(rtc_data_min)%10, HUNDREDS_TUBE);
			set_tube_digit(nixie, toMinutes(rtc_data_min)/10, THOUSANDS_TUBE);
		
			// Seconds
			set_tube_digit(nixie, toSeconds(rtc_data_sec)%10, ONES_TUBE);
			set_tube_digit(nixie, toSeconds(rtc_data_sec)/10, TENS_TUBE);
		
			// Display
			display(nixie, NUMBER_OF_TUBES);			
		}
		else
		{
			turn_off_display(NUMBER_OF_TUBES);
		}
	}
}

#pragma endregion Main
