// Write to serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <signal.h>
#include <unistd.h>


// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

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
#define BUF_SIZE 256
#define FLAG 0x7E
#define CONTROL_SET 0x03
#define CONTROL_UA 0x07
#define ADDRESS_RECEIVER 0x03
#define ADDRESS_SENDER 0x01
#define BBC_CHECK_SET (ADDRESS_SENDER^CONTROL_SET)
#define BBC_CHECK_UA (ADDRESS_SENDER^CONTROL_UA)

volatile int STOP = FALSE;

//def of alarm vars
int alarmEnabled = FALSE;
int alarmCount = 0;


void alarmHandler(int signal){
        alarmEnabled = FALSE;
        alarmCount++;
        printf("Alarm #%d\n", alarmCount);
}

int main(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];

    signal(SIGALRM, alarmHandler);

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);       
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0.1; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    // Create string to send
    unsigned char buf[BUF_SIZE] = {0};


    buf[0] = FLAG;
    buf[1] = ADDRESS_SENDER;
    buf[2] = CONTROL_SET;
    buf[3] = buf[1] ^ buf[2];
    buf[4] = FLAG;
    buf[5] = '\n';
    // In non-canonical mode, '\n' does not end the writing.
    // Test this condition by placing a '\n' in the middle of the buffer.
    // The whole buffer must be sent even with the '\n'.

    int buf_a_stor;
    int buf_c_stor;
    // Loop for input
    unsigned char buf2[BUF_SIZE + 1] = {0}; // +1: Save space for the final '\0' char
    int index = 0;
    state curState = Other_RCV;

    //first writting to reader & enabling alarm
    int bytes = write(fd, buf, 5);
    printf("%d bytes written\n", bytes);
    alarm(3);
    alarmEnabled = TRUE;
    
    while (STOP == FALSE && alarmCount<4)
    {   
        //if alarm not enabled, write again
        if(alarmEnabled==FALSE){
            int bytes = write(fd, buf, 5);
            printf("%d bytes written\n", bytes);
            alarm(3);
            alarmEnabled = TRUE;
        }
        //enabling alarm   
        state curState = Other_RCV; 
        int onStateMachine = TRUE;
        while(onStateMachine){
            //printf("here\n");
            int bytes = read(fd, buf2, 1);
            buf2[bytes] = '\0'; // Set end of string to '\0', so we can printf
            if(buf2[0]!=0){
                printf("var = 0x%02X\n", buf2[0]);
            }else{
                continue;
            }
            switch(curState){
                case Other_RCV:
                    if(buf2[0]==FLAG){
                        curState = FLAG_RCV;
                    }
                    else{
                        curState = Other_RCV;
                        onStateMachine = FALSE;
                    }
                    break;
                case FLAG_RCV:
                    if(buf2[0]==ADDRESS_SENDER){
                        curState = A_RCV;
                    }
                    else if(buf2[0]==FLAG){
                        curState = FLAG_RCV;
                    }
                    else{
                        curState = Other_RCV;
                    }
                    break;
                case A_RCV:
                    if(buf2[0]==CONTROL_UA){
                        curState = C_RCV;
                    }
                    else if(buf2[0]==FLAG){
                        curState = FLAG_RCV;
                    }
                    else{
                        curState = Other_RCV;
                    }
                    break;
                case C_RCV:
                    if(buf2[0]==BBC_CHECK_UA){
                            curState = BBC_ok;
   
                        }
                        else if(buf2[0]==FLAG){
                            curState = FLAG_RCV;
                        }
                        else{
                            curState = Other_RCV;
                        }
                        break;
                case BBC_ok:
                    if(buf2[0]==FLAG){
                        printf("tudo certo\n\n");
                        printf("UA OKAY!\n\n");
                        curState = Other_RCV;
                        onStateMachine = FALSE;
                        STOP = TRUE;
                    }
                    else{
                        curState = Other_RCV;
                    }
                    break;
                default:
                    onStateMachine = FALSE;
                    break;
                }
        }
    }

    // Wait until all bytes have been written to the serial port
    sleep(1);

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
