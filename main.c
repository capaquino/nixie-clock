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
	rtc_write(DS3231_HOURS_REG_OFFSET,toRegisterValue(10)); // this may or may not be 12
	rtc_write(DS3231_MINUTES_REG_OFFSET,toRegisterValue(59));
	rtc_write(DS3231_SECONDS_REG_OFFSET,toRegisterValue(45));
	uint8_t rtc_data_sec = 0;
	uint8_t rtc_data_min = 0;
	uint8_t rtc_data_hour = 0;
	
	// Init nixie tube
	uint8_t nixie[NUMBER_OF_TUBES];
	clear_tubes(nixie, NUMBER_OF_TUBES);
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
			set_tube_digit(nixie, toMinutes(rtc_data_min)%10, HUNDREDS_TUBE);
			set_tube_digit(nixie, toMinutes(rtc_data_min)/10, THOUSANDS_TUBE);
		
			// Seconds
			set_tube_digit(nixie, toSeconds(rtc_data_sec)%10, ONES_TUBE);
			set_tube_digit(nixie, toSeconds(rtc_data_sec)/10, TENS_TUBE);
		
			// Display
			display(nixie, NUMBER_OF_TUBES);
		
			// scroll effect, delaying is very bad when interrutpts are enabled.
			//_delay_ms(800);
			//scroll(NUMBER_OF_TUBES);
			
			// International Women's Day Post
			// Symbol:		W 0  M/m A/F n/pi_upper
			// Tube Type:	B 12 A   B   A
			// Tube Digit:	1 2  3   4   5
			
			//set_tube_digit(nixie, IN15B_W, 1);
			//set_tube_digit(nixie, 0, 2);
			//set_tube_digit(nixie, IN15A_m, 3);
			//set_tube_digit(nixie, IN15B_A, 4);
			//set_tube_digit(nixie, IN15A_pi_upper, 5);
			//display(nixie, NUMBER_OF_TUBES);
			
			// things learned:
			// 1: _delay requires a compile time constant... hence the massive text below
			// 2: _delay affects messy nixie tube circuits.
			
			
			// ...
			// to blink tubes uncomment the following
			//_delay_ms(2000);
			//turn_off_display(NUMBER_OF_TUBES);
			//_delay_ms(2000);
			//
			//
			//// down
			//display(nixie, NUMBER_OF_TUBES);
			//_delay_ms(1600);
			//turn_off_display(NUMBER_OF_TUBES);
			//_delay_ms(1600);
					//display(nixie, NUMBER_OF_TUBES);
					//_delay_ms(1200);
					//turn_off_display(NUMBER_OF_TUBES);
					//_delay_ms(1200);
							//display(nixie, NUMBER_OF_TUBES);
							//_delay_ms(800);
							//turn_off_display(NUMBER_OF_TUBES);
							//_delay_ms(800);
									//display(nixie, NUMBER_OF_TUBES);
									//_delay_ms(600);
									//turn_off_display(NUMBER_OF_TUBES);
									//_delay_ms(600);
											//display(nixie, NUMBER_OF_TUBES);
											//_delay_ms(400);
											//turn_off_display(NUMBER_OF_TUBES);
											//_delay_ms(400);
													//display(nixie, NUMBER_OF_TUBES);
													//_delay_ms(300);
													//turn_off_display(NUMBER_OF_TUBES);
													//_delay_ms(300);
															//display(nixie, NUMBER_OF_TUBES);
															//_delay_ms(200);
															//turn_off_display(NUMBER_OF_TUBES);
															//_delay_ms(200);
																	//display(nixie, NUMBER_OF_TUBES);
																	//_delay_ms(100);
																	//turn_off_display(NUMBER_OF_TUBES);
																	//_delay_ms(100);
			////up
			//display(nixie, NUMBER_OF_TUBES);
			//_delay_ms(100);
			//turn_off_display(NUMBER_OF_TUBES);
			//_delay_ms(100);
					//display(nixie, NUMBER_OF_TUBES);
					//_delay_ms(200);
					//turn_off_display(NUMBER_OF_TUBES);
					//_delay_ms(200);
							//display(nixie, NUMBER_OF_TUBES);
							//_delay_ms(300);
							//turn_off_display( NUMBER_OF_TUBES);
							//_delay_ms(300);
									//display(nixie, NUMBER_OF_TUBES);
									//_delay_ms(400);
									//turn_off_display(NUMBER_OF_TUBES);
									//_delay_ms(400);
											//display(nixie, NUMBER_OF_TUBES);
											//_delay_ms(600);
											//turn_off_display(NUMBER_OF_TUBES);
											//_delay_ms(600);
													//display(nixie, NUMBER_OF_TUBES);
													//_delay_ms(800);
													//turn_off_display(NUMBER_OF_TUBES);
													//_delay_ms(800);
															//display(nixie, NUMBER_OF_TUBES);
															//_delay_ms(1200);
															//turn_off_display(NUMBER_OF_TUBES);
															//_delay_ms(1200);
																	//display(nixie, NUMBER_OF_TUBES);
																	//_delay_ms(1600);
																	//turn_off_display(NUMBER_OF_TUBES);
																	//_delay_ms(1600);
			
		}
		else
		{
			turn_off_display(NUMBER_OF_TUBES);
		}
	}
}

			//for (int tube = 0; tube<NUMBER_OF_TUBES;tube++)
			//{
			//switch (tube)
			//{
			//case 0:	set_tube_digit(nixie, IN15B_W,			1); break;
			//case 1:	set_tube_digit(nixie, 0,				2); break;
			//case 2:	set_tube_digit(nixie, IN15A_m,			3); break;
			//case 3:	set_tube_digit(nixie, IN15B_A,			4); break;
			//case 4:	set_tube_digit(nixie, IN15A_pi_upper,	5); break;
			//}
			//display(nixie, NUMBER_OF_TUBES);
			//