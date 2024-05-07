//SNOOZELOCK
//Panel module
//Has a screen. tells the alarm to shut up if appropriate

#include "mbed.h"
#include "nRF24L01P.h"
#include "Adafruit_SSD1306.h"

#define ALARM_DURATION  600
#define NO_SPLASH_ADAFRUIT

#define TRANSFER_SIZE   4

#define NODE_ALARM      0
#define NODE_DOOR       1
#define NODE_PANEL      2

#define COM_OFF         0
#define COM_ON          1
#define COM_ALARM_T     2
#define COM_CUR_T       3
#define COM_ENABLED     4

#define MODE_ALARM_ON   0
#define MODE_ALARM_OFF  1
#define MODE_SET_AL_H   2
#define MODE_SET_AL_M   3
#define MODE_SET_CUR_H  4
#define MODE_SET_CUR_M  5
#define MODE_ENGAGED    99

I2C i2c(PB_7,PB_6);
Adafruit_SSD1306_I2c oled(i2c,PB_5);
nRF24L01P radio(PA_7, PA_6, PA_5, PB_0, PB_1, PA_10);    // mosi, miso, sck, csn, ce, irq

InterruptIn localBtn(PA_3);
InterruptIn upBtn(PA_2);
InterruptIn downBtn(PA_1);
InterruptIn selectBtn(PA_0);
DigitalOut led(PC_13);

//general purpose information
bool doorflag = false;
bool localflag = false;
bool syncflag = false;
bool alarm_changeflag = false;
bool cur_changeflag = false;
bool alarmflag = false;
bool time_unknown_flag = true;
char reported_cur_h = 0;
char reported_cur_m = 0;
char reported_alarm_h = 0;
char reported_alarm_m = 0;
char alarm_h = 0;
char alarm_m = 0;
char cur_h = 0;
char cur_m = 0;
int dispmode = MODE_ALARM_OFF;

//display functions
void alarmOnMode();
void alarmOffMode();
void setAlarmHourMode();
void setAlarmMinMode();
void setCurHourMode();
void setCurMinMode();
void alarmEngagedMode();

//buttons and switch interrupts
void isr_upDetect();
void isr_upConfirm();
void isr_downDetect();
void isr_downConfirm();
void isr_selectDetect();
void isr_selectConfirm();
void isr_localDetect();
void isr_localConfirm();
Timeout to_up;
Timeout to_down;
Timeout to_select;
Timeout to_local;
bool upBtnPressed = false;
bool downBtnPressed = false;
bool selectBtnPressed = false;

//sleep isrs and functions
void isr_sleeptrigger();
void set_sleepclk(int sec);
Timeout sleepclk;
bool sleepflag = true;

int main() {
    localBtn.fall(&isr_localDetect);
    upBtn.fall(&isr_upDetect);
    downBtn.fall(&isr_downDetect);
    selectBtn.fall(&isr_selectDetect);
    
    //self indulgent splash
    oled.begin();
    oled.display();
    oled.clearDisplay();
    wait(0.5);
    oled.setRotation(4); //2 is programmer header bottom, 4 is programmer top
    oled.setTextSize(1);
    oled.setTextCursor(0,0);
    oled.printf("Start");
    oled.display();
    wait(0.5);
    
    //buffer & setup variables
    char txData[TRANSFER_SIZE], rxData[TRANSFER_SIZE];
    int txDataCnt = 0;
    int rxDataCnt = 0;
    
    int doorcount = 0;

    //radio setup
    radio.powerUp();
    radio.setTransferSize( TRANSFER_SIZE );
    radio.setReceiveMode();
    radio.enable();
    
    //power-on wake period
    set_sleepclk(10);

    while (1) {
        led = !led;
        if (sleepflag) {
            led=1;
            wait_ms(100); //CRUCIAL. Prevents prior display call conflicts
            oled.command(0xAE);//displayoff
            radio.disable();
    
            wait_ms(100);
            sleep();
            
            radio.enable();
            radio.flushRx();
            radio.flushTx();
            time_unknown_flag = true;
            oled.command(0xAF);//displayon
        }
        
        //run display states
        switch(dispmode) {
            case MODE_ALARM_ON: 
                alarmOnMode();
                break;
            case MODE_ALARM_OFF: 
                alarmOffMode();
                break;
            case MODE_SET_AL_H: 
                setAlarmHourMode();
                break;
            case MODE_SET_AL_M: 
                setAlarmMinMode();
                break;
            case MODE_SET_CUR_H: 
                setCurHourMode();
                break;
            case MODE_SET_CUR_M: 
                setCurMinMode();
                break;
            case MODE_ENGAGED:
                alarmEngagedMode();
                break;
        }
        
        //read radio data
        if ( radio.readable() ) {
            rxDataCnt = radio.read( NRF24L01P_PIPE_P0, rxData, sizeof( rxData ) );
            if (rxData[0] == NODE_DOOR) {
                //Checks if it's open and updates door flag
                if (rxData[1] == COM_ON) {
                    doorflag = true;
                } else {
                    doorflag = false;
                }
            } else if (rxData[0] == NODE_ALARM) {
                //engage alarm or store variables
                if  (rxData[1] == COM_ON) {
                    dispmode = MODE_ENGAGED;
                    set_sleepclk(ALARM_DURATION);
                } else if (rxData[1] == COM_ALARM_T) {
                    reported_alarm_h = rxData[2];
                    reported_alarm_m = rxData[3];
                    //if we're synced up, turn off changeflag
                    if(reported_alarm_h==alarm_h && reported_alarm_m==alarm_m) alarm_changeflag=false;
                    time_unknown_flag = false;
                } else if  (rxData[1] == COM_CUR_T) {
                    reported_cur_h = rxData[2];
                    reported_cur_m = rxData[3];
                    //if we're synced up, turn off changeflag
                    if(reported_cur_h==cur_h && reported_cur_m==cur_m) cur_changeflag=false;
                    time_unknown_flag = false;
                }
            }
        }
        
        //evaluate alarm condition
        if (doorflag && localflag) {
        //if door is closed, local has been pressed at some point
            alarmflag = false;
        } else if (localflag) {
        //door is open, local has been pressed
            localflag = false;
        } else {
        //door is open, no activity
            alarmflag = true;
        }
        
        //send alarm data
        //alarm and current time data only sent if changeflags are set. 
        if(alarmflag && dispmode==MODE_ENGAGED) {
            txData[0] = NODE_PANEL;
            txData[1] = COM_ON;
            txData[2] = 0xFF;
            txData[3] = 0xFF;
            radio.write( NRF24L01P_PIPE_P0, txData, 4 );
        } else if (!alarmflag && dispmode==MODE_ENGAGED) {
            txData[0] = NODE_PANEL;
            txData[1] = COM_OFF;
            txData[2] = 0xFF;
            txData[3] = 0xFF;
            radio.write( NRF24L01P_PIPE_P0, txData, 4 );
        } else if ((dispmode == MODE_SET_AL_H || dispmode == MODE_SET_AL_M) && alarm_changeflag) {
            txData[0] = NODE_PANEL;
            txData[1] = COM_ALARM_T;
            txData[2] = alarm_h;
            txData[3] = alarm_m;
            syncflag = radio.write( NRF24L01P_PIPE_P0, txData, 4 );
        } else if ((dispmode == MODE_SET_CUR_H || dispmode == MODE_SET_CUR_M) && cur_changeflag) {
            txData[0] = NODE_PANEL;
            txData[1] = COM_CUR_T;
            txData[2] = cur_h;
            txData[3] = cur_m;
            syncflag = radio.write( NRF24L01P_PIPE_P0, txData, 4 );
        }
    }
}

//Display that the alarm is on
void alarmOnMode() {
    cur_h = reported_cur_h;
    cur_m = reported_cur_m;
    oled.clearDisplay();
    oled.setTextCursor(0,0);
    oled.setTextSize(1);
    oled.printf("Alarm On\n");
    if(time_unknown_flag) {
        oled.printf("No Time Reported\n");
    } else {
        oled.printf("Time: %d:%d\n",reported_cur_h,reported_cur_m);
        oled.printf("Alarm: %d:%d\n",reported_alarm_h,reported_alarm_m);
    }
    oled.printf("Door: %s", doorflag ? "true" : "false");
    oled.display(); 
}

//Display that the alarm is off
void alarmOffMode() {
    cur_h = reported_cur_h;
    cur_m = reported_cur_m;
    oled.clearDisplay();
    oled.setTextCursor(0,0);
    oled.setTextSize(1);
    oled.printf("Alarm Off\n");
    if(time_unknown_flag) {
        oled.printf("No Time Reported\n");
    } else {
        oled.printf("Time: %d:%d\n",reported_cur_h,reported_cur_m);
    }
    oled.printf("Door: %s", doorflag ? "true" : "false");
    oled.display();
}

void setAlarmHourMode() {
    if (upBtnPressed) {
        alarm_h++;
        if(alarm_h>23) alarm_h=0;
        upBtnPressed = false;
        alarm_changeflag = true;
    }
    if (downBtnPressed) {
        if(alarm_h==0) alarm_h=24;
        alarm_h--;
        downBtnPressed = false;
        alarm_changeflag = true;
    }
    oled.clearDisplay();
    oled.setTextCursor(0,0);
    oled.setTextSize(1);
    oled.printf("Set ALARM Hour:\n");
    oled.printf("%d:--\n",alarm_h);
    oled.printf("Reported: %d\n", reported_alarm_h);
    oled.printf("Synced: %s", (reported_alarm_h == alarm_h) ? "true" : "false");
    oled.display();
}

void setAlarmMinMode() {
    if (upBtnPressed) {
        alarm_m++;
        if(alarm_m>59) alarm_m=0;
        upBtnPressed = false;
        alarm_changeflag = true;
    }
    if (downBtnPressed) {
        if(alarm_m==0) alarm_m=60;
        alarm_m--;
        downBtnPressed = false;
        alarm_changeflag = true;
    }
    oled.clearDisplay();
    oled.setTextCursor(0,0);
    oled.setTextSize(1);
    oled.printf("Set ALARM Minute:\n");
    oled.printf("--:%d\n",alarm_m);
    oled.printf("Reported: %d\n", reported_alarm_m);
    oled.printf("Synced: %s", (reported_alarm_m == alarm_m) ? "true" : "false");
    oled.display();
}

void setCurHourMode() {
    if (upBtnPressed) {
        cur_h++;
        if(cur_h>23) cur_h=0;
        upBtnPressed = false;
        cur_changeflag = true;
    }
    if (downBtnPressed) {
        if(cur_h==0) cur_h=24;
        cur_h--;
        downBtnPressed = false;
        cur_changeflag = true;
    }
    oled.clearDisplay();
    oled.setTextCursor(0,0);
    oled.setTextSize(1);
    oled.printf("Set CURRENT Hour:\n");
    oled.printf("%d:--\n",cur_h);
    oled.printf("Reported: %d\n", reported_cur_h);
    oled.printf("Synced: %s", (reported_cur_h == cur_h) ? "true" : "false");
    oled.display();
}

void setCurMinMode() {
    if (upBtnPressed) {
        cur_m++;
        if(cur_m>59) cur_m=0;
        upBtnPressed = false;
        cur_changeflag = true;
    }
    if (downBtnPressed) {
        if(cur_m==0) cur_m=60;
        cur_m--;
        downBtnPressed = false;
        cur_changeflag = true;
    }
    oled.clearDisplay();
    oled.setTextCursor(0,0);
    oled.setTextSize(1);
    oled.printf("Set CURRENT Minute:\n");
    oled.printf("--:%d\n",cur_m);
    oled.printf("Reported: %d\n", reported_cur_m);
    oled.printf("Synced: %s", (reported_cur_m == cur_m) ? "true" : "false");
    oled.display();
}

void alarmEngagedMode() {
    oled.clearDisplay();
    oled.setTextCursor(0,0);
    oled.setTextSize(1);
    oled.printf("ALARM ENGAGED\n");
    oled.printf("Door: %s\n", doorflag ? "shut" : "open");
    oled.printf("Alarm is: %s\n", alarmflag ? "on" : "off");
    oled.printf("Current time: %d:%d\n",reported_cur_h,reported_cur_m);
    oled.display();
}

//buttons and switch interrupts
void isr_upDetect() {
    to_up.attach(&isr_upConfirm,0.02);
}

void isr_upConfirm() {
    if(!upBtn && dispmode!=MODE_ENGAGED) {
        set_sleepclk(20);
        upBtnPressed = true;
    }
}

void isr_downDetect() {
    to_down.attach(&isr_downConfirm,0.02);
}

void isr_downConfirm(){
    if(!downBtn && dispmode!=MODE_ENGAGED) {
        set_sleepclk(20);
        downBtnPressed = true;
    }
}

void isr_selectDetect() {
    to_select.attach(&isr_selectConfirm,0.02);
}

void isr_selectConfirm() {
    if(!selectBtn && dispmode!=MODE_ENGAGED) {
        set_sleepclk(20);
        dispmode++;
        if(dispmode>5) dispmode=0;
    }
}

void isr_localDetect() {
    to_local.attach(&isr_localConfirm,0.02);
}

void isr_localConfirm() {
    if(!localBtn) {
        if(dispmode!=MODE_ENGAGED) set_sleepclk(5);
        localflag = true;
    }
}

void isr_sleeptrigger() {
    sleepflag = true;
    alarmflag = false;
    doorflag = false;
    dispmode = MODE_ALARM_ON;
}

void set_sleepclk(int sec) { //... this isn't re-entrant and gets called all the time
    sleepflag = false;
    sleepclk.attach(&isr_sleeptrigger,sec);
}