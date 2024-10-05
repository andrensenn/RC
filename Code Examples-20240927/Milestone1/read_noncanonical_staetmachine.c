// Read from serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

typedef enum{
    Other_RCV,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BBC_ok
}state;

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 256
#define FLAG 0x7E
#define CONTROL_SET 0x03
#define CONTROL_UA 0x07
#define ADDRESS_RECEIVER 0x03
#define ADDRESS_SENDER 0x01

volatile int STOP = FALSE;

int main(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing and not as controlling tty
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
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 5;  // Blocking read until 5 chars received

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
    int buf_a_stor;
    int buf_c_stor;
    // Loop for input
    unsigned char buf[BUF_SIZE + 1] = {0}; // +1: Save space for the final '\0' char
    int index = 0;
    state curState = Other_RCV;
    while (STOP == FALSE)//while to read the SET ~A
    {
        
        // Returns after 5 charsADDRESS_SENDER have been input
        int bytes = read(fd, buf, 1);
        buf[bytes] = '\0'; // Set end of string to '\0', so we can printf
        printf("var = 0x%02X\n", buf[0]);
        
        switch(curState){
            case Other_RCV:
                if(buf[0]==FLAG){
                    curState = FLAG_RCV;
                }
                else{
                    curState = Other_RCV;
                }
                break;
            case FLAG_RCV:
                if(buf[0]==ADDRESS_SENDER){
                    curState = A_RCV;
                    buf_a_stor = buf[0];
                }
                else if(buf[0]==FLAG){
                    curState = FLAG_RCV;
                }
                else{
                    curState = Other_RCV;
                }
                break;
            case A_RCV:
                if(buf[0]==CONTROL_SET){
                    curState = C_RCV;
                    buf_c_stor = buf[0];
                }
                else if(buf[0]==FLAG){
                    curState = FLAG_RCV;
                }
                else{
                    curState = Other_RCV;
                }
                break;
            case C_RCV:
                if(buf[0]==(buf_a_stor^buf_c_stor)){
                        curState = BBC_ok;
                        buf_c_stor = buf[0];
                    }
                    else if(buf[0]==FLAG){
                        curState = FLAG_RCV;
                    }
                    else{
                        curState = Other_RCV;
                    }
                    break;
            case BBC_ok:
                if(buf[0]==FLAG){
                    STOP = TRUE;
                    printf("tudo certo\n");
                }
                else{
                    curState = Other_RCV;
                }
                break;
        }
    }
    
    

    // The while() cycle should be changed in order to respect the specifications
    // of the protocol indicated in the Lab guide


    //sending UA ~A
    int fd2 = open(serialPortName, O_RDWR | O_NOCTTY);

    if (fd2 < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    unsigned char buf2[BUF_SIZE] = {0};
    buf2[0] = FLAG;
    buf2[1] = ADDRESS_SENDER;
    buf2[2] = CONTROL_UA;
    buf2[3] = buf2[1]^buf2[2];
    buf2[4] = FLAG;
    buf2[5] = '\n';

    int bytes = write(fd, buf2, BUF_SIZE);
    printf("%d bytes written\n", bytes);
    
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