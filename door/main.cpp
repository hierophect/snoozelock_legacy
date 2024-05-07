//SNOOZELOCK
//Door module
#include "mbed.h"
#include "nRF24L01P.h"

#define TRANSFER_SIZE   4
#define DOOR_OPEN 1
#define DOOR_CLOSED 0
#define WAKE_DURATION  60

#define NODE_ALARM      0
#define NODE_DOOR       1
#define NODE_PANEL      2

#define COM_OFF         0
#define COM_ON          1
#define COM_ALARM_T     2
#define COM_CUR_T       3
#define COM_ENABLED     4

nRF24L01P radio(PA_7, PA_6, PA_5, PB_0, PB_1, PA_10);    // mosi, miso, sck, csn, ce, irq
InterruptIn doorSw(PA_3);
bool closedflag = false;

DigitalOut led1(PA_1); //tracks the door
DigitalOut led2(PA_0); //blinks when not sleeping

//interrupts and debouncing timers
void isr_openDetect();
void isr_openConfirm();
void isr_closeDetect();
void isr_closeConfirm();
void isr_sleeptrigger();
void set_sleepclk(int sec);
Timeout to_open;
Timeout to_close;

Timeout sleepclk;
bool sleepflag = true;

int main() {
    //set switch interrupts
    doorSw.rise(&isr_openDetect);
    doorSw.fall(&isr_closeDetect);
    
    led1 = 0;
    led2 = 0;
    
    //radio setup
    char txData[TRANSFER_SIZE];
    radio.powerUp();
    radio.setTransferSize( TRANSFER_SIZE );
    radio.setReceiveMode();
    radio.enable();
    
    //power-on wake period
    set_sleepclk(10);

    while (1) {
        //go to sleep
        if (sleepflag) {
            wait_ms(100);
            led1 = 0;
            led2 = 0;
            radio.disable();
            wait_ms(100);
            sleep();
            radio.enable();
        }
        //otherwise, send latest switch state to panel node
        if (!closedflag) {
            txData[0] = NODE_DOOR;
            txData[1] = COM_OFF;
            txData[2] = 0xFF;
            txData[3] = 0xFF;
            radio.write( NRF24L01P_PIPE_P0, txData, 4 );
        } else {
            txData[0] = NODE_DOOR;
            txData[1] = COM_ON;
            txData[2] = 0xFF;
            txData[3] = 0xFF;
            radio.write( NRF24L01P_PIPE_P0, txData, 4 );
        }
        led2 = !led2;
        wait_ms(200);
    }
}

void set_sleepclk(int sec) {
    sleepflag = false;
    sleepclk.attach(&isr_sleeptrigger,sec);
}

void isr_openDetect() {
    to_open.attach(&isr_openConfirm,0.02);
}

void isr_openConfirm() {
    if(doorSw == DOOR_OPEN) {
        set_sleepclk(WAKE_DURATION);
        closedflag = false;
        led1 = 0;
    }
}

void isr_closeDetect() {
    to_close.attach(&isr_closeConfirm,0.02);
}

void isr_closeConfirm() {
    if(doorSw == DOOR_CLOSED) {
        set_sleepclk(WAKE_DURATION);
        closedflag = true;
        led1 = 1;
    }
}

void isr_sleeptrigger() {
    sleepflag = true;
}