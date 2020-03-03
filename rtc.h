/*
 * rtc.h
 *
 * Created: 1/4/2020 8:30:52 PM
 *  Author: runne
 */ 


#ifndef RTC_H_
#define RTC_H_

#define DS3231_SLAVE_ADDRESS 0xD0 // (0x68<<1) see datasheet 0x68 but 0xD0 for an "8bit" i2c lib which Fleury's lib is.
// means his functions assume you are giving them a fully constructed byte. 0x68 is given as 7 bit w/o r/w byte. Adding
// a r/w bit at the end I2C_WRITE/READ essentially shifts the value left one, doubling it.
#define DS3231_SECONDS_REG_OFFSET 0x00
#define DS3231_MINUTES_REG_OFFSET 0x01
#define DS3231_HOURS_REG_OFFSET 0x02
#define DS3231_CONTROL_REG_OFFSET 0x0E

extern uint8_t rtc_read(unsigned char reg);
extern void rtc_write(unsigned char reg, unsigned char value);
extern uint8_t toSeconds(uint8_t i2c_seconds_register_read_data);
extern uint8_t toMinutes(uint8_t i2c_minutes_register_read_data);
extern uint8_t toHours(uint8_t i2c_hours_register_read_data);


extern uint8_t fromRegisterValue(uint8_t hex);
extern uint8_t toRegisterValue(uint8_t decimal);
extern uint8_t dec2bcd(char num);
extern uint8_t bcd2dec(char num);

#endif /* RTC_H_ */