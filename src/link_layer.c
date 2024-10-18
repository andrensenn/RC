// Link layer protocol implementation

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include "link_layer.h"
#include "serial_port.h"
#include "../include/consts.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

int alarmEnabled = FALSE;
int alarmCount = 0;


void alarmHandler(int signal){
        alarmEnabled = FALSE;
        alarmCount++;
        printf("Alarm #%d\n", alarmCount);
}


////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters){
    if (openSerialPort(connectionParameters.serialPort,
    connectionParameters.baudRate) < 0){
        return -1;
    }
    int nTrys = connectionParameters.nRetransmissions;
    int timeout = connectionParameters.timeout;
    switch(connectionParameters.role){
        case LlTx: {


            // Create string to send
            unsigned char buf[6] = {0};
            buf[0] = FLAG;
            buf[1] = ADDRESS_SENDER;
            buf[2] = CONTROL_SET;
            buf[3] = ADDRESS_SENDER ^ CONTROL_SET;
            buf[4] = FLAG;
            buf[5] = '\n';

            unsigned char buf2[6] = {0}; // +1: Save space for the final '\0' char
            int checkWrite = writeBytesSerialPort(buf,6);
            if (checkWrite==-1) return -1;
            

            //vars for alarm and alarm instalation
            alarm(timeout);
            alarmEnabled = TRUE;
            int STOP = FALSE;
            char a;
            char c;
            while(STOP==FALSE && alarmCount < nTrys){
                printf("var = 0x%02X\n", buf2[0]);
                //reseting alarm and rewriting
                if(alarmEnabled==FALSE){
                    checkWrite = writeBytesSerialPort(buf,6);
                    if (checkWrite==-1) return -1;
                    alarm(timeout);
                    alarmEnabled = TRUE;
                }

                //vars for the loop and state machine
                state curState = Other_RCV;
                int onStateMachine = TRUE;
                while(onStateMachine){
                    int checkRead = readByteSerialPort(buf2);
                    printf("var = 0x%02X\n", buf2[0]);
                    while(!checkRead){
                        if(checkRead==-1) return -1; 
                        printf("%d \n",checkRead);
                        checkRead = readByteSerialPort(buf2);
                    }
                    switch (curState){
                        case Other_RCV:
                            if(buf2[0]==FLAG){
                                curState = FLAG_RCV;
                            }
                            else{
                                onStateMachine = FALSE;
                            }
                            break;
                        case FLAG_RCV:
                            if(buf2[0]==ADDRESS_SENDER){
                                curState = A_RCV;
                                a = buf2[0];
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
                                c = buf2[0];
                            }
                            else if(buf2[0]==FLAG){
                            
                                curState = FLAG_RCV;
                            }
                            else{
                               
                                curState = Other_RCV;
                            }
                            break;
                        case C_RCV:
                            if(buf2[0] == (a^c)){
                                curState = BBC_ok;
                            }
                            else if(buf2[0]==FLAG){
                                curState = FLAG;
                            }
                            else{
                                curState = Other_RCV;
                            }
                            break;
                        case BBC_ok:
                            if(buf2[0]==FLAG){
                                printf("\nConnection established!\n");
                                return 1;
                            }
                            break;
                    default:
                        break;
                    }
                    }
                }
                break;
                }
            case LlRx:
                {
                //connection starting message
                unsigned char buf[6] = {0};
                //vars for the lopp
                int STOP = TRUE;
                //vars to check bcc
                char a;
                char c;
                while(STOP){
                    //vars for state machine
                    int onStateMachine = TRUE;
                    state curState = Other_RCV;
                    while(onStateMachine){
                        int checkRead = readByteSerialPort(buf);
                        while(!checkRead){
                            if(checkRead==-1) return -1; 
                            checkRead = readByteSerialPort(buf);
                        }
                        switch(curState){
                            case Other_RCV:
                                if(buf[0]==FLAG){
                                    curState = FLAG_RCV;
                                }
                                else{
                                    onStateMachine = FALSE;
                                }
                                break;
                            case FLAG_RCV:
                                if(buf[0]==ADDRESS_SENDER){
                                    curState = A_RCV;
                                    a = ADDRESS_SENDER;
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
                                    c = CONTROL_SET;
                                }
                                else if(buf[0]==FLAG){
                                    curState = FLAG_RCV;
                                }
                                else{
                                    curState = Other_RCV;
                                }
                                break;
                            case C_RCV:
                                if(buf[0]==(a^c)){
                                        curState = BBC_ok;
            
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
                                    //STOP = TRUE;
                                    unsigned char buf2[6] = {0};
                                    printf("\nSending UA! \n");
                                    buf2[0] = FLAG;
                                    buf2[1] = ADDRESS_SENDER;
                                    buf2[2] = CONTROL_UA;
                                    buf2[3] = buf2[1]^buf2[2];
                                    buf2[4] = FLAG;
                                    buf2[5] = '\n';

                                    int checkWrite = writeBytesSerialPort(buf2, 6);
                                    if (checkWrite==-1) return -1;
                                    return 1;

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
            break;
            }
        default:
            return -1;

    }
    
    // TODO

    return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO

    return 0;
}

 /*

usar 
caso
llread receba um SET
enviar UA brute force

readByteSerialPort(unsigned char *byte) para ler (-1 se não ler nada)

writeBytesSerialPort(const unsigned char *bytes, int numBytes) (-1 se erro)

Trama size?
maxpayload *2 + 6 = 2006

    chamar llopen


app layer{
    pacotes de controlo e pacotes de dados
    controlo -> TLV
    start (1) 
    T1 -> file size -> 0
    T2 -> file name -> 1
    end (3)
}
        
    TX - F A C BCC1 [DADOS] BCC2 F
    Rx - F A C BCC1

    I0   ->
    RR1  <-
    I1   ->
    REJ1 <-
    I1   ->
    RR0  ->

    -> SET
    UA <-
    -> I0
    <- RR1


    PARA DISCONECTAR FAZER LOOP NORMAL
    GUARDAR C E SE A FLAG ESTIVER BEM O C == DISC
    CHAMAR LLCLOSE
    */

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    /*
    switch(connectionParameters.role){
        case LlTx: {


            // Create string to send
            unsigned char buf[6] = {0};
            buf[0] = FLAG;
            buf[1] = ADDRESS_SENDER;
            buf[2] = CONTROL_SET;
            buf[3] = ADDRESS_SENDER ^ CONTROL_SET;
            buf[4] = FLAG;
            buf[5] = '\n';

            unsigned char buf2[6] = {0}; // +1: Save space for the final '\0' char
            int checkWrite = writeBytesSerialPort(buf,6);
            if (checkWrite==-1) return -1;
            

            //vars for alarm and alarm instalation
            alarm(timeout);
            alarmEnabled = TRUE;
            int STOP = FALSE;
            char a;
            char c;
            while(STOP==FALSE && alarmCount < nTrys){

                //reseting alarm and rewriting
                if(alarmEnabled==FALSE){
                    checkWrite = writeBytesSerialPort(buf,6);
                    if (checkWrite==-1) return -1;
                    alarm(timeout);
                    alarmEnabled = TRUE;
                }

                //vars for the loop and state machine
                state curState = Other_RCV;
                int onStateMachine = TRUE;
                while(onStateMachine){
                    int checkRead = readByteSerialPort(buf2);

                    while(!checkRead){
                        if(checkRead==-1) return -1; 
                        checkRead = readByteSerialPort(buf2);
                    }
                   
                   switch (curState){
                    case Other_RCV:
                            if(buf2[0]==FLAG){
                                curState = FLAG_RCV;
                            }
                            else{
                                onStateMachine = FALSE;
                            }
                            break;
                        case FLAG_RCV:
                            if(buf2[0]==ADDRESS_SENDER){
                                curState = A_RCV;
                                a = buf2[0];
                            }
                            else if(buf2[0]==FLAG){
                                curState = FLAG_RCV;
                            }
                            else{
                                curState = Other_RCV;
                            }
                        case A_RCV:
                            if(buf2[0]==CONTROL_UA){
                                curState = C_RCV;
                                c = buf2[0];
                            }
                            else if(buf2[0]==FLAG){
                                curState = FLAG_RCV;
                            }
                            else{
                                curState = Other_RCV;
                            }
                        case C_RCV:
                            if(buf2[0] == (a^c)){
                                curState = BBC_ok;
                            }
                            else if(buf2[0]==FLAG){
                                curState = FLAG;
                            }
                            else{
                                curState = Other_RCV;
                            }
                        case BBC_ok:
                            if(buf2[0]==FLAG){
                                printf("Connection established!\n");
                                return 1;
                            }
                    default:
                        break;
                    }
                    }
                }
                break;
                }
            case LlRx:
                {
                printf("here\n");
                //connection starting message
                unsigned char buf[6] = {0};
                //vars for the lopp
                int STOP = TRUE;
                //vars to check bcc
                char a;
                char c;
                while(STOP){
                    //vars for state machine
                    int onStateMachine = TRUE;
                    state curState = Other_RCV;
                    while(onStateMachine){
                        int checkRead = readByteSerialPort(buf);
                        while(!checkRead){
                            if(checkRead==-1) return -1; 
                            checkRead = readByteSerialPort(buf);
                        }
                        switch(curState){
                            case Other_RCV:
                                if(buf[0]==FLAG){
                                    curState = FLAG_RCV;
                                }
                                else{
                                    onStateMachine = FALSE;
                                }
                                break;
                            case FLAG_RCV:
                                if(buf[0]==ADDRESS_SENDER){
                                    curState = A_RCV;
                                    a = ADDRESS_SENDER;
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
                                    c = CONTROL_SET;
                                }
                                else if(buf[0]==FLAG){
                                    curState = FLAG_RCV;
                                }
                                else{
                                    curState = Other_RCV;
                                }
                                break;
                            case C_RCV:
                                if(buf[0]==(a^c)){
                                        curState = BBC_ok;
            
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
                                    //STOP = TRUE;
                                    unsigned char buf2[6] = {0};
                                    buf2[0] = FLAG;
                                    buf2[1] = ADDRESS_SENDER;
                                    buf2[2] = CONTROL_UA;
                                    buf2[3] = buf2[1]^buf2[2];
                                    buf2[4] = FLAG;
                                    buf2[5] = '\n';

                                    int checkWrite = writeBytesSerialPort(buf2, 6);
                                    if (checkWrite==-1) return -1;
                                    return 1;

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
            break;
            }
        default:
            return -1;

    }
*/
    int clstat = closeSerialPort();
    return clstat;
}
