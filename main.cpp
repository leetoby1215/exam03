#include "mbed.h"
#include "mbed_rpc.h"
#include "fsl_port.h"
#include "fsl_gpio.h"
#include "math.h"
#define UINT14_MAX        16383
// FXOS8700CQ I2C address
#define FXOS8700CQ_SLAVE_ADDR0 (0x1E<<1) // with pins SA0=0, SA1=0
#define FXOS8700CQ_SLAVE_ADDR1 (0x1D<<1) // with pins SA0=1, SA1=0
#define FXOS8700CQ_SLAVE_ADDR2 (0x1C<<1) // with pins SA0=0, SA1=1
#define FXOS8700CQ_SLAVE_ADDR3 (0x1F<<1) // with pins SA0=1, SA1=1
// FXOS8700CQ internal register addresses
#define FXOS8700Q_STATUS 0x00
#define FXOS8700Q_OUT_X_MSB 0x01
#define FXOS8700Q_OUT_Y_MSB 0x03
#define FXOS8700Q_OUT_Z_MSB 0x05
#define FXOS8700Q_M_OUT_X_MSB 0x33
#define FXOS8700Q_M_OUT_Y_MSB 0x35
#define FXOS8700Q_M_OUT_Z_MSB 0x37
#define FXOS8700Q_WHOAMI 0x0D
#define FXOS8700Q_XYZ_DATA_CFG 0x0E
#define FXOS8700Q_CTRL_REG1 0x2A
#define FXOS8700Q_M_CTRL_REG1 0x5B
#define FXOS8700Q_M_CTRL_REG2 0x5C
#define FXOS8700Q_WHOAMI_VAL 0xC7

I2C i2c(PTD9, PTD8);
RawSerial pc(USBTX, USBRX);
RawSerial xbee(D12, D11);
EventQueue queue(32 * EVENTS_EVENT_SIZE);
EventQueue queue_acc(32 * EVENTS_EVENT_SIZE);
Thread thread;
Thread thread_acc;

int m_addr = FXOS8700CQ_SLAVE_ADDR1;
float t[3];
float v;
float v_x = 0;
float v_y = 0;

void FXOS8700CQ_readRegs(int addr, uint8_t * data, int len);
void FXOS8700CQ_writeRegs(uint8_t * data, int len);
void getAcc(Arguments *in, Reply *out);
void Acc();
void getAddr(Arguments *in, Reply *out);
void xbee_rx_interrupt(void);
void xbee_rx(void);
void reply_messange(char *xbee_reply, char *messange);
void check_addr(char *xbee_reply, char *messenger);
void detect();

RPCFunction rpcAcc(&getAcc, "getAcc");
RPCFunction rpcAddr(&getAddr, "getAddr");

int main() {
    uint8_t data[2] ;
    // Enable the FXOS8700Q
    FXOS8700CQ_readRegs( FXOS8700Q_CTRL_REG1, &data[1], 1);
    data[1] |= 0x01;
    data[0] = FXOS8700Q_CTRL_REG1;
    FXOS8700CQ_writeRegs(data, 2);
    pc.baud(9600);
    char xbee_reply[4];

    // XBee setting
    xbee.baud(9600);
    xbee.printf("+++");
    xbee_reply[0] = xbee.getc();
    xbee_reply[1] = xbee.getc();
    if(xbee_reply[0] == 'O' && xbee_reply[1] == 'K'){
        pc.printf("enter AT mode.\r\n");
        xbee_reply[0] = '\0';
        xbee_reply[1] = '\0';
    }
    xbee.printf("ATMY 0x88\r\n");
    reply_messange(xbee_reply, "setting MY : 0x88");
    xbee.printf("ATDL 0x87\r\n");
    reply_messange(xbee_reply, "setting DL : 0x87");

    xbee.printf("ATID 0x1\r\n");
    reply_messange(xbee_reply, "setting PAN ID : 0x1");
    xbee.printf("ATWR\r\n");
    reply_messange(xbee_reply, "write config");

    xbee.printf("ATCN\r\n");
    reply_messange(xbee_reply, "exit AT mode");
    xbee.getc();
    // start
    pc.printf("start\r\n");
    thread.start(callback(&queue, &EventQueue::dispatch_forever));
    thread_acc.start(callback(&queue_acc, &EventQueue::dispatch_forever));
    queue_acc.call(Acc);
    // Setup a serial interrupt function of receiving data from xbee
    xbee.attach(xbee_rx_interrupt, Serial::RxIrq);
}

void getAcc(Arguments *in, Reply *out) {
    pc.printf(",%.3f,", v);
}

void Acc() {
    int16_t acc16;
    uint8_t res[6];
    while (true) {
        FXOS8700CQ_readRegs(FXOS8700Q_OUT_X_MSB, res, 6);
        acc16 = (res[0] << 6) | (res[1] >> 2);
        if (acc16 > UINT14_MAX/2)
            acc16 -= UINT14_MAX;
        t[0] = ((float)acc16) / 4096.0f * 9.8;
        acc16 = (res[2] << 6) | (res[3] >> 2);
        if (acc16 > UINT14_MAX/2)
            acc16 -= UINT14_MAX;
        t[1] = ((float)acc16) / 4096.0f * 9.8;
        acc16 = (res[4] << 6) | (res[5] >> 2);
        if (acc16 > UINT14_MAX/2)
            acc16 -= UINT14_MAX;
        t[2] = ((float)acc16) / 4096.0f * 9.8;
        if (t[0] > 1 || t[0] < -1) {
            v_x += 0.1 * t[0];
        } else {
            v_x = v_x;
        }
        if (t[1] > 1 || t[1] < -1) {
            v_y += 0.1 * t[1];
        } else {
            v_y = v_y;
        }
        v = sqrt(v_x * v_x + v_y * v_y);
        wait(0.1);
    }
}

void getAddr(Arguments *in, Reply *out) {
    uint8_t who_am_i, data[2];
    FXOS8700CQ_readRegs(FXOS8700Q_WHOAMI, &who_am_i, 1);
    pc.printf("Here is %x", who_am_i);
}

void FXOS8700CQ_readRegs(int addr, uint8_t * data, int len) {
    char t = addr;
    i2c.write(m_addr, &t, 1, true);
    i2c.read(m_addr, (char *)data, len);
}

void FXOS8700CQ_writeRegs(uint8_t * data, int len) {
    i2c.write(m_addr, (char *)data, len);
}

void xbee_rx_interrupt(void) {
    xbee.attach(NULL, Serial::RxIrq); // detach interrupt
    queue.call(&xbee_rx);
}

void xbee_rx(void) {
    char buf[100] = {0};
    char outbuf[100] = {0};
    
    while(xbee.readable()){
        for(int i = 0; i < 100; i++) {
            buf[i] = '\0';
            outbuf[i] = '\0';
        }
        for (int i = 0; ; i++) {
            char recv = xbee.getc();
            if (recv == '\r') {
                break;
            }
            buf[i] = pc.putc(recv);
        }
        RPC::call(buf, outbuf);
        pc.printf("%s\r\n", outbuf);
        wait(0.1);
    }
    xbee.attach(xbee_rx_interrupt, Serial::RxIrq); // reattach interrupt
}

void reply_messange(char *xbee_reply, char *messange) {
    xbee_reply[0] = xbee.getc();
    xbee_reply[1] = xbee.getc();
    xbee_reply[2] = xbee.getc();
    if(xbee_reply[1] == 'O' && xbee_reply[2] == 'K'){
        pc.printf("%s\r\n", messange);
        xbee_reply[0] = '\0';
        xbee_reply[1] = '\0';
        xbee_reply[2] = '\0';
    }
}

void check_addr(char *xbee_reply, char *messenger) {
    xbee_reply[0] = xbee.getc();
    xbee_reply[1] = xbee.getc();
    xbee_reply[2] = xbee.getc();
    xbee_reply[3] = xbee.getc();
    pc.printf("%s = %c%c%c\r\n", messenger, xbee_reply[1], xbee_reply[2], xbee_reply[3]);
    xbee_reply[0] = '\0';
    xbee_reply[1] = '\0';
    xbee_reply[2] = '\0';
    xbee_reply[3] = '\0';
}