#include "DS3231.h"

RTC_DS3231::RTC_DS3231(PinName sda, PinName scl): i2c(sda,scl) {
}

uint8_t RTC_DS3231::read_i2c_register(uint8_t addr, uint8_t reg) {
	char data;
	if(i2c.write(addr, (char *)&reg, 1, true))
	    return false;
	if(i2c.read(addr, &data, 1))
	    return false;
	return (uint8_t)data;
}

bool RTC_DS3231::write_i2c_register(uint8_t addr, uint8_t reg, uint8_t val) {
    char buff[2] = {reg, val};
    if (i2c.write(addr, buff, 2, false) !=0) {
      return false;
    } else {
      return true;
    }
}

bool RTC_DS3231::read_i2c_bytes(uint8_t addr, char reg, char *buffer, uint8_t len) {
    i2c.write(addr, &reg, 1, true);
    if (i2c.read(addr, buffer, len, false) !=0) {
      return (false);
    } else {
      return true;
    }
}

uint8_t RTC_DS3231::bcd_2_bin (uint8_t val) { return val - 6 * (val >> 4); }
uint8_t RTC_DS3231::bin_2_bcd (uint8_t val) { return val + 6 * (val / 10); }

bool RTC_DS3231::begin(void) {
	i2c.frequency(100000);
 	return read_i2c_register(DS3231_ADDRESS,DS3231_STATUSREG);
}

bool RTC_DS3231::lostPower(void) {
 	return (read_i2c_register(DS3231_ADDRESS, DS3231_STATUSREG) >> 7);
}

void RTC_DS3231::set_hourmin(int h, int m) {
	write_i2c_register(DS3231_ADDRESS, DS3231_MINUTES, bin_2_bcd(m));
	write_i2c_register(DS3231_ADDRESS, DS3231_HOURS, bin_2_bcd(h));
}
// void RTC_DS3231::adjust(const DateTime& dt) {
//   Wire.beginTransmission(DS3231_ADDRESS);
//   Wire._I2C_WRITE((byte)0); // start at location 0
//   Wire._I2C_WRITE(bin2bcd(dt.second()));
//   Wire._I2C_WRITE(bin2bcd(dt.minute()));
//   Wire._I2C_WRITE(bin2bcd(dt.hour()));
//   Wire._I2C_WRITE(bin2bcd(0));
//   Wire._I2C_WRITE(bin2bcd(dt.day()));
//   Wire._I2C_WRITE(bin2bcd(dt.month()));
//   Wire._I2C_WRITE(bin2bcd(dt.year() - 2000));
//   Wire.endTransmission();

//   uint8_t statreg = read_i2c_register(DS3231_ADDRESS, DS3231_STATUSREG);
//   statreg &= ~0x80; // flip OSF bit
//   write_i2c_register(DS3231_ADDRESS, DS3231_STATUSREG, statreg);
// }

void RTC_DS3231::get_time(char * data) {

	read_i2c_bytes(DS3231_ADDRESS,0,data,7);
	data[0] = bcd_2_bin(data[0] & 0x7F); //seconds treatment
	data[1] = bcd_2_bin(data[1]);
	data[2] = bcd_2_bin(data[2]);
	data[3] = bcd_2_bin(data[3]); //ignore this
	data[4] = bcd_2_bin(data[4]);
	data[5] = bcd_2_bin(data[5]);
	data[6] = bcd_2_bin(data[6]);
}

// Ds3231SqwPinMode RTC_DS3231::readSqwPinMode() {
//   int mode;

//   Wire.beginTransmission(DS3231_ADDRESS);
//   Wire._I2C_WRITE(DS3231_CONTROL);
//   Wire.endTransmission();
  
//   Wire.requestFrom((uint8_t)DS3231_ADDRESS, (uint8_t)1);
//   mode = Wire._I2C_READ();

//   mode &= 0x93;
//   return static_cast<Ds3231SqwPinMode>(mode);
// }

// void RTC_DS3231::writeSqwPinMode(Ds3231SqwPinMode mode) {
//   uint8_t ctrl;
//   ctrl = read_i2c_register(DS3231_ADDRESS, DS3231_CONTROL);

//   ctrl &= ~0x04; // turn off INTCON
//   ctrl &= ~0x18; // set freq bits to 0

//   if (mode == DS3231_OFF) {
//     ctrl |= 0x04; // turn on INTCN
//   } else {
//     ctrl |= mode;
//   } 
//   write_i2c_register(DS3231_ADDRESS, DS3231_CONTROL, ctrl);

//   //Serial.println( read_i2c_register(DS3231_ADDRESS, DS3231_CONTROL), HEX);
// }
