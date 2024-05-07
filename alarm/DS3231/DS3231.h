#include "mbed.h"

//#define DS3231_ADDRESS  0x68
#define DS3231_ADDRESS  0xd0
#define DS3231_CONTROL  0x0E
#define DS3231_STATUSREG 0x0F
#define DS3231_MINUTES	0x01
#define DS3231_HOURS	0x02


class RTC_DS3231 {
public:
	RTC_DS3231(PinName sda, PinName scl);
    bool begin(void);
    //static void adjust(const DateTime& dt);
    bool lostPower(void);
    void get_time(char * data);
    
    uint8_t read_i2c_register(uint8_t addr, uint8_t reg);
	bool write_i2c_register(uint8_t addr, uint8_t reg, uint8_t val);
	void set_hourmin(int h, int m);
    //static Ds3231SqwPinMode readSqwPinMode();
    //static void writeSqwPinMode(Ds3231SqwPinMode mode);
private:
	I2C i2c;
	bool read_i2c_bytes(uint8_t addr, char reg, char *buffer, uint8_t len);
	uint8_t bcd_2_bin (uint8_t val);
	uint8_t bin_2_bcd (uint8_t val);
};
