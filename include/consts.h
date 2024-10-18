#ifndef _CONTSTS_H_
#define _CONTSTS_H_

#include <termios.h>

typedef enum{
    Other_RCV,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BBC_ok
}state;

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 256
#define FLAG 0x7E

#define ADDRESS_RECEIVER 0x03
#define ADDRESS_SENDER 0x01

#define CONTROL_SET 0x03
#define CONTROL_UA 0x07
#define RR0 0xAA
#define RR1 0xAB
#define REJ0 0x54
#define REJ1 0x55
#define DISC 0x0B
#define I0 0x00 //information frame 0
#define I1 0x80 //information frame 1


#define BBC_CHECK_SET (ADDRESS_SENDER^CONTROL_SET)
#define BBC_CHECK_UA (ADDRESS_SENDER^CONTROL_UA)



#endif