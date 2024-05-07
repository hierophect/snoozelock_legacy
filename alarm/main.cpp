//SNOOZELOCK
//Alarm module
//An alternative version that only activates once per day 


#include "mbed.h"
#include "nRF24L01P.h"
#include "DS3231.h"

#define START_DURATION  10 
#define MANUAL_DURATION 60
#define ALARM_DURATION  600

#define TRANSFER_SIZE   4

#define NODE_ALARM      0
#define NODE_DOOR       1
#define NODE_PANEL      2

#define COM_OFF         0
#define COM_ON          1
#define COM_ALARM_T     2
#define COM_CUR_T       3
#define COM_ENABLED     4

nRF24L01P radio(PA_7, PA_6, PA_5, PB_0, PB_1, PA_10);    // mosi, miso, sck, csn, ce, irq
RTC_DS3231 rtc(PB_7,PB_6); //sda, scl
InterruptIn btn(PA_4);
InterruptIn rtcIn(PB_14);

DigitalOut spkgnd(PB_12); 
DigitalOut spkdrive(PB_13);
DigitalOut led(PC_13); //blinks when RTC interrupt detected. 

bool alarmflag = false;
bool alarmActive = false;
bool sleepflag = true;
bool rtcIRflag = false;
bool btnIRflag = false;
int alarm_h = 0;
int alarm_m = 0;

void isr_btnDetect();
void isr_btnConfirm();
void isr_rtcDetect();
void isr_sleeptrigger();
void isr_beepOn();
void isr_beepOff();

void rtc_alarm_reset();
void set_sleepclk(int sec);
Timeout to_btn;
Timeout to_rtc;
Timeout sleepclk; 
Timeout spktime; 

int main() {   
    printf("SETUP:\n");
    led = 0;
    
    //wakeup chirp
    spkgnd = 0;
    spkdrive = 1;
    wait(0.5);
    spkdrive = 0;
    printf("chirp chirp\n");
    
    rtc.begin();
    //set alarm interrupt registers
    uint8_t controlReg;
    controlReg = rtc.read_i2c_register(DS3231_ADDRESS, 0x0E);
    controlReg &= ~0x01; //set Alarm1 Interrupt Enable
    controlReg &= ~0x04; //set Interrupt Control Enable
    rtc.write_i2c_register(DS3231_ADDRESS, 0x0E, controlReg);
    //
    rtc_alarm_reset();
    //bool RTC_DS3231::write_i2c_register(uint8_t addr, uint8_t reg, uint8_t val)
    //set alarm register (7) to match 5  seconds in BCD with alarm bit set
    rtc.write_i2c_register(DS3231_ADDRESS, 0x07, (0x00 & 0x7f)); //seconds
    rtc.write_i2c_register(DS3231_ADDRESS, 0x08, (0x00 & 0x7f)); //minutes
    rtc.write_i2c_register(DS3231_ADDRESS, 0x09, (0x00 & 0x7f)); //hours
    rtc.write_i2c_register(DS3231_ADDRESS, 0x0a, (0x00 | 0x80)); //days
    
    //set alarm interrupt registers
    controlReg = rtc.read_i2c_register(DS3231_ADDRESS, 0x0E);
    controlReg |= 0x01; //set Alarm1 Interrupt Enable
    controlReg |= 0x04; //set Interrupt Control Enable
    rtc.write_i2c_register(DS3231_ADDRESS, 0x0E, controlReg);
    
    //create RTC return data array
    char rtcData[7];

    //print rtc registers for debug
    rtc.get_time(rtcData);
    printf("RTC Time: h:%d,m:%d",rtcData[2],rtcData[1]);
    printf("RTC Control R:%hhx\n",rtc.read_i2c_register(DS3231_ADDRESS, 0x0E));
    printf("RTC Status R:%hhx\n",rtc.read_i2c_register(DS3231_ADDRESS, 0x0F));

    //NRF24l01 setup
    char txData[TRANSFER_SIZE], rxData[TRANSFER_SIZE];
    int txDataCnt = 0;
    int rxDataCnt = 0;
    radio.powerUp();
    radio.setTransferSize( TRANSFER_SIZE );
    radio.setReceiveMode();
    radio.enable();
    
    //power-on wake period
    set_sleepclk(START_DURATION);

    //wait until RTC setup is wrapped up before enabling the ISRs
    wait_ms(100);
    btn.rise(&isr_btnDetect);
    rtcIn.fall(&isr_rtcDetect);

    printf("Setup complete\n");
    printf("\nLOOP:\n------------------\n");

    while (1) {
        //debugging flags
        if (rtcIRflag==true) {
            rtcIRflag = false;
            printf("RTC Interrupt recieved. Alarm:%s\n", alarmflag ? "true" : "false");
        }
        if (btnIRflag==true) {
            btnIRflag=false;
            printf("BTN Interrupt recieved\n");
        }

        //waking will only go through while loop once unless alarm triggered
        if (sleepflag) {
            printf("Sleeping...\n");
            led = 1;
            wait_ms(100);
            radio.disable();
            wait_ms(100);

            sleep();
            
            radio.enable();
            radio.flushRx();
            radio.flushTx();

            rtc_alarm_reset();
            printf("Woken\n");
        }
        
        //turn on the beeper loop
        if (alarmflag && !alarmActive) {
            isr_beepOn();
            alarmActive = true;
        } else {
            //halt immediately and disable timeout
            spkdrive = 0;
            alarmActive = false;
            spktime.detach();
        }
                
        //read incoming data
        if ( radio.readable() ) {
            rxDataCnt = radio.read( NRF24L01P_PIPE_P0, rxData, sizeof( rxData ) ); 
            
            if (rxData[0] == NODE_PANEL) {
                printf("RR: ");
                if (rxData[1]==COM_OFF) {
                    alarmflag = false;
                    printf("Alarm disengaged by panel\n");
                } else if (rxData[1]==COM_ON) {
                    alarmflag = true;
                    printf("Alarm re-engaged by panel\n");
                } else if (rxData[1]==COM_ALARM_T) {
                    alarm_h = rxData[2];//convert to bcd
                    alarm_m = rxData[3];
                    int alarm_h_bcd = alarm_h + 6 * (alarm_h / 10);
                    int alarm_m_bcd = alarm_m + 6 * (alarm_m / 10);
                    rtc.write_i2c_register(DS3231_ADDRESS, 0x08, (alarm_m_bcd & 0x7f)); //minutes
                    rtc.write_i2c_register(DS3231_ADDRESS, 0x09, (alarm_h_bcd & 0x7f)); //hours
                    printf("Alarm time reset by panel: %d:%d\n",alarm_h,alarm_m);
                } else if (rxData[1]==COM_CUR_T) {
                    rtc.set_hourmin(rxData[2],rxData[3]);
                    printf("General time reset by panel: %d:%d\n",rxData[2],rxData[3]);
                }
            }
        }
        
        //send data to panel
        rtc.get_time(rtcData);

        if(alarmflag) {
            txData[0] = NODE_ALARM;
            txData[1] = COM_ON;
            txData[2] = 0xFF;
            txData[3] = 0xFF;
            radio.write( NRF24L01P_PIPE_P0, txData, 4 );
            wait_ms(200);
        } else {
            txData[0] = NODE_ALARM;
            txData[1] = COM_CUR_T;
            txData[2] = rtcData[2]; //hours
            txData[3] = rtcData[1]; //minutes
            radio.write( NRF24L01P_PIPE_P0, txData, 4 );
            wait_ms(100);
            
            txData[0] = NODE_ALARM;
            txData[1] = COM_ALARM_T;
            txData[2] = alarm_h; //hours
            txData[3] = alarm_m; //minutes
            radio.write( NRF24L01P_PIPE_P0, txData, 4 );
            wait_ms(100);
        }
        //toggle LED
        led = !led;
    }
}

void rtc_alarm_reset() {
    //reset alarm flag
    uint8_t statusReg;
    statusReg = rtc.read_i2c_register(DS3231_ADDRESS, 0x0F);
    if (statusReg & 0x01) //alarm 1 flag = 0x01
    {
        statusReg &= 0xFE;
        rtc.write_i2c_register(DS3231_ADDRESS, 0x0F, statusReg);
    }
}

void isr_btnDetect() {
    to_btn.attach(&isr_btnConfirm,0.02);
}

void isr_btnConfirm() {
    if(btn) {
        //this should only turn off the alarm! 
        alarmflag = false;
        btnIRflag = true;
        //maybe this will make it less twitchy
        set_sleepclk(MANUAL_DURATION); //you have 1m to set the time. 
    }
}

void isr_rtcDetect() {
    set_sleepclk(ALARM_DURATION);
    alarmflag = true;
    rtcIRflag = true;
}

void isr_sleeptrigger() {
    sleepflag = true;   
}
void set_sleepclk(int sec) {
    sleepflag = false;
    sleepclk.attach(&isr_sleeptrigger,sec);
}

void isr_beepOn() {
    spkdrive = 1;
    spktime.attach(&isr_beepOff,0.5);
}

void isr_beepOff() {
    spkdrive = 0;
    spktime.attach(&isr_beepOn,0.5);
}

