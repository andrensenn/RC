// Link layer protocol implementation

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include "link_layer.h"
#include "serial_port.h"
#include "../include/consts.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

int alarmEnabled = FALSE;
int alarmCount = 0;
int curInfFram = 0;


void alarmHandler(int signal){
        alarmEnabled = FALSE;
        alarmCount++;
        printf("Alarm #%d\n", alarmCount);
}

/*pacotes de controlo e pacotes de dados
    controlo -> TLV
    start (1) 
    T1 -> file size -> 0
    T2 -> file name -> 1
    end (3)
    T->"codigo"
    L->tamanho do que se vai escrber
    V-> nome ficheiro//tamanho ficheiro

    */

int sendControlPacket(const char *filename){
    FILE *fileCheck = fopen(filename, "rb"); 
    if(fileCheck==NULL){
        printf("error opening file\n");
        return -1;
    }

    fseek(fileCheck, 0, SEEK_END);
    long filesize = ftell(fileCheck);

    size_t length = strlen(filename);
    unsigned char *buf = (unsigned char *)malloc(14 + length);
    //unsigned char buf[14+length] = {0};
    memset(buf, 0, 14+length );
    //start
    buf[0] = 1; 
    //T1 -> filesize
    buf[1] = 0;
    buf[2] = 8;
    buf[3] = filesize;
    //T2 -> filename
    buf[11] = 1;
    buf[12] = length;
    buf[13] = *filename;
    buf[13+length] = 3; 
    int writeCheck = writeBytesSerialPort(buf,(14+length));
    if(writeCheck==-1) return -1;
    /*
    tamanho buffer vai ser 
    1 byte -> start | 1
    1 byte -> T1    | 1
    1 byte -> espaço para filesize  |8
    8 bytes -> filesize | ....
    1 byte -> T2   | 
    1 byte -> espaço para filename  | 4 ou 8 ->sizeof(size_t)
    8 bytes -> filename | ....
    1 byte -> end | 3
    */
    free(buf);
    fclose(fileCheck);
    printf("\nControl packet sent!\n");
    return 1;
}



int getControlPacket(char *filename, long *filesize){
    int STOP = TRUE;
    state curState = Start;
    //curReading -> -1 erro, 0 filesize, 1 filename
    int curReading = -1;
    unsigned char buf[MAX_PAYLOAD_SIZE*2] = {0};
    while(STOP){
        int checkRead = readByteSerialPort(buf);
        if(checkRead == -1){
            return -1;

        }else if(checkRead == 0){
            continue;
        }
        switch(curState){
            case Start:
                if(buf[0]==1){
                    curState = T;
                }
                else{
                    return -1;
                }
                break;
            case T:
                if(buf[0]==0){
                    curReading = 0;
                }else if(buf[0]==1){
                    curReading = 1;
                }
                else if(buf[0]==3){
                    STOP = FALSE;
                }else{
                    return -1;
                }
                curState = L;
                break;
            case L:{
                int index = buf[0];
                int i = 0;
                while(i<index){
                    int checkRead = readByteSerialPort(buf);
                    if(checkRead == -1){
                        return -1;

                    }else if(checkRead == 0){
                        continue;
                    }
                    if(curReading==0){
                        filesize = buf[0];
                    }
                    else if(curReading==1){
                        filename = buf[0];
                    }
                    i++;
                }
                curState = T;
                }
                break;
            default:
                return -1;

        }
    }
   
    printf("\nControl packet received!\n");
    return 0;
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

            signal(SIGALRM, alarmHandler);
            // Create string to send
            unsigned char buf[5] = {0};
            buf[0] = FLAG;
            buf[1] = ADDRESS_SENDER;
            buf[2] = CONTROL_SET;
            buf[3] = ADDRESS_SENDER ^ CONTROL_SET;
            buf[4] = FLAG;
            //buf[5] = '\n';

            unsigned char buf2[5] = {0}; // +1: Save space for the final '\0' char
            int checkWrite = writeBytesSerialPort(buf,5);
            if (checkWrite==-1) return -1;
            

            //vars for alarm and alarm instalation
            alarm(timeout);
            alarmEnabled = TRUE;
            int STOP = FALSE;
            char a;
            char c;
            state curState = Other_RCV;
            while(STOP==FALSE && alarmCount < nTrys){
                //reseting alarm and rewriting
                if(alarmEnabled==FALSE){
                    printf("Trying again...\n");
                    checkWrite = writeBytesSerialPort(buf,5);
                    if (checkWrite==-1) return -1;
                    alarm(timeout);
                    alarmEnabled = TRUE;
                }

                //vars for the loop and state machine
                int onStateMachine = TRUE;
                while(onStateMachine){
                    int checkRead = readByteSerialPort(buf2);
                    if(checkRead==-1){
                        return -1;
                    }else if(checkRead==0){

                        onStateMachine = FALSE;
                        continue;
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
                                alarm(0);
                                onStateMachine = FALSE;
                                STOP = TRUE;
                                //return 1;
                            }
                            break;
                    default:
                        break;
                    }
                }
            }
            if(alarmCount==nTrys) return -1;
                /*
                    DISCONECTING PART
                */
                /*
                unsigned char buf3[5] = {0};
                buf3[0] = FLAG;
                buf3[1] = ADDRESS_SENDER;
                buf3[2] = DISC;
                buf3[3] = ADDRESS_SENDER ^ DISC;
                buf3[4] = FLAG;
                //buf3[5] = '\n';
                checkWrite = writeBytesSerialPort(buf3,5);
                printf("\nSending disconecting flag\n");
                unsigned char buf4[5] = {0};
                if (checkWrite==-1) return -1;
                int onStateMachine = TRUE;
                state curState = Other_RCV;
               
                while(onStateMachine){
                    int checkRead = readByteSerialPort(buf4);
                    printf("var = 0x%02X\n", buf4[0]);
                    while(!checkRead){
                        if(checkRead==-1) return -1; 
                        checkRead = readByteSerialPort(buf);
                    }
                    switch (curState){
                        case Other_RCV:
                            if(buf4[0]==FLAG){
                                curState = FLAG_RCV;
                            }
                            //else{
                            //    onStateMachine = FALSE;
                            //    printf("error disconecting\n");
                            //}
                            
                            break;
                        case FLAG_RCV:
                            if(buf4[0]==ADDRESS_SENDER){
                                curState = A_RCV;
                                a = buf4[0];
                            }
                            else if(buf4[0]==FLAG){
                                curState = FLAG_RCV;
                            }
                            else{
                                curState = Other_RCV;
                            }
                            break;
                        case A_RCV:
                            
                            if(buf4[0]==DISC){
    
                                curState = C_RCV;
                                c = buf4[0];
                            }
                            else if(buf4[0]==FLAG){
                            
                                curState = FLAG_RCV;
                            }
                            else{
                               
                                curState = Other_RCV;
                            }
                            break;
                        case C_RCV:
                            if(buf4[0] == (a^c)){
                                curState = BBC_ok;
                            }
                            else if(buf4[0]==FLAG){
                                curState = FLAG;
                            }
                            else{
                                curState = Other_RCV;
                            }
                            break;
                        case BBC_ok:
                            if(buf4[0]==FLAG){
                                if(llclose(0)==-1){
                                    return -1;
                                }
                                return 1;
                            }
                            break;
                    default:
                        break;
                    }
                    }
                    */
                break;
                }
            case LlRx:
                {
                //connection starting message
                unsigned char buf[5] = {0};
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
                                /*
                                else{
                                    onStateMachine = FALSE;
                                }
                                */
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
                                    unsigned char buf2[5] = {0};
                                    printf("\nSending UA! \n");
                                    buf2[0] = FLAG;
                                    buf2[1] = ADDRESS_SENDER;
                                    buf2[2] = CONTROL_UA;
                                    buf2[3] = buf2[1]^buf2[2];
                                    buf2[4] = FLAG;
                                    //buf2[5] = '\n';

                                    int checkWrite = writeBytesSerialPort(buf2, 5);
                                    if(checkWrite==-1) return -1;
                                    STOP = FALSE;
                                    onStateMachine = FALSE;

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
                /*
                    DISCONECTING PART
                */
                /*
                unsigned char buf3[5] = {0};
                STOP = TRUE;
                
                while(STOP){
                    //vars for state machine
                    int onStateMachine = TRUE;
                    state curState = Other_RCV;
                    while(onStateMachine){
                        int checkRead = readByteSerialPort(buf3);
                        while(!checkRead){
                            if(checkRead==-1) return -1; 
                            checkRead = readByteSerialPort(buf3);
                        }
                        printf("var = 0x%02X\n", buf3[0]);
                        switch(curState){
                            case Other_RCV:
                                if(buf3[0]==FLAG){
                                    curState = FLAG_RCV;
                                }
                                
                                //else{
                                //    onStateMachine = FALSE;
                                //}
                                
                                break;
                            case FLAG_RCV:
                                if(buf3[0]==ADDRESS_SENDER){
                                    curState = A_RCV;
                                    a = ADDRESS_SENDER;
                                }
                                else if(buf3[0]==FLAG){
                                    curState = FLAG_RCV;
                                }
                                else{
                                    curState = Other_RCV;
                                }
                                break;
                            case A_RCV:
                                if(buf3[0]==DISC){
                                    curState = C_RCV;
                                    c = DISC;
                                }
                                else if(buf3[0]==FLAG){
                                    curState = FLAG_RCV;
                                }
                                else{
                                    curState = Other_RCV;
                                }
                                break;
                            case C_RCV:
                                if(buf3[0]==(a^c)){
                                        curState = BBC_ok;
            
                                    }
                                    else if(buf3[0]==FLAG){
                                        curState = FLAG_RCV;
                                    }
                                    else{
                                        curState = Other_RCV;
                                    }
                                    break;
                            case BBC_ok:
                                if(buf3[0]==FLAG){
                                    //STOP = TRUE;
                                    unsigned char buf4[5] = {0};
                                    printf("Sending Disconecting flag!\n");
                                    buf4[0] = FLAG;
                                    buf4[1] = ADDRESS_SENDER;
                                    buf4[2] = DISC;
                                    buf4[3] = buf4[1]^buf4[2];
                                    buf4[4] = FLAG;
                                    //buf4[5] = '\n';

                                    int checkWrite = writeBytesSerialPort(buf4, 5);
                                    if (checkWrite==-1) return -1;
                                    if(llclose(0)==-1){
                                        return -1;
                                    }
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
                */
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
    unsigned char bufSend[MAX_PAYLOAD_SIZE*2 + 6] = {0}; //creating the buf to send

    bufSend[0] = FLAG;
    bufSend[1] = ADDRESS_SENDER;
    //I0 or I1
    if(curInfFram==0){
        bufSend[2] = I0;
    }
    else{
        bufSend[2] = I1;
    }
    
    bufSend[3] = bufSend[1]^bufSend[2]; //bcc1

    int index = 0;
    int offest = 0;
    unsigned char bufAux[BUF_SIZE] = {0};
    int bcc2 = 0;
    while (index < bufSize) {
        //reading 1 byte and checking for errors
        int checkRead = readByteSerialPort(bufAux);
        while(!checkRead){
            checkRead = readByteSerialPort(bufAux);
        }
        if(bufAux[0]==FLAG){
            bufSend[4+index+offest] = ESC;
            bufSend[4+index+offest+1] = 0x5E;
            offest++;
        }
        else if(bufAux[0]==ESC){
            bufSend[4+index+offest] = ESC;
            bufSend[4+index+offest+1] = 0x5D;
            offest++;
        }
        else{
            bufSend[4+index+offest] = bufAux[0];
        }
        bcc2 = bcc2^bufAux[0];
        index++;
    }
    bufSend[4+index+offest] = bcc2;
    bufSend[4+index+offest+1] = FLAG;
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
    Rx - F A C BCC1 F

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
    printf("\ndisconecting...\n");
    int clstat = closeSerialPort();
    if(clstat==-1){
        printf("error on llclose\n");
    }
    return clstat;
}
