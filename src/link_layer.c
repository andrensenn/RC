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
#include "../include/statistics.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

int alarmEnabled = FALSE;
int alarmCount = 0;
int curInfFram = 0;
int curInfFramR = 0;
int timeout = 0;
int nTrys = 0;
LinkLayerRole role;

extern struct statistic stats;

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

long sendControlPacket(const char *filename){
    FILE *fileCheck = fopen(filename, "rb"); 
    if(fileCheck==NULL){
        printf("error opening file\n");
        return -1;
    }

    fseek(fileCheck, 0, SEEK_END);
    long filesize = ftell(fileCheck);

    size_t length = strlen(filename);
    unsigned char *buf = (unsigned char *)malloc(14 + length);
    unsigned char auxbuf[8];
    //unsigned char buf[14+length] = {0};
    memset(buf, 0, 14+length );
    //start
    buf[0] = 1; 
    //T1 -> filesize
    buf[1] = 0;
    buf[2] = 8;

    sprintf((char *)auxbuf, "%ld", filesize); //this aparently converts a long into a string into a buf (thanks stack overflow)
    int j = 0;
    while(j<8){
        buf[3+j] = auxbuf[j];
        j++;
    }
    //T2 -> filename
    buf[11] = 1;
    buf[12] = length;
    buf[13] = *filename;
    int i =0;
    while(i<length){
        buf[13+i] = filename[i];
        i++;
    }
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
    unsigned char ansBuf[MAX_PAYLOAD_SIZE] = {0};
    int STOP = TRUE;
    state curState = Other_RCV;
    int a;
    int c;
    while(STOP){
        int checkRead = readByteSerialPort(ansBuf);
        if(checkRead>0){
            switch(curState){
                case Other_RCV:
                    if(ansBuf[0]==FLAG){
                        curState = FLAG_RCV;
                    }
                    else{
                        return -1;
                    }
                    break;
                case FLAG_RCV:
                    if(ansBuf[0] == ADDRESS_SENDER){
                        curState = A_RCV;
                        a = ADDRESS_SENDER;
                    }
                    else if(ansBuf[0]==FLAG){
                        curState = FLAG_RCV;
                    }
                    else{
                        curState = Other_RCV;
                    }
                    break;
                case A_RCV:
                    if(ansBuf[0] == RR0){
                        curState = C_RCV;
                        c = RR0;
                    }
                    else if(ansBuf[0]==FLAG){
                        curState = FLAG_RCV;
                    }
                    else{
                        curState = Other_RCV;
                    }
                    break;
                case C_RCV:
                    if(ansBuf[0] == (a^c)){
                        curState = BBC_ok;
                    }
                    else if(ansBuf[0]==FLAG){
                        curState = FLAG_RCV;
                    }
                    else{
                        curState = Other_RCV;
                    }
                    break;
                case BBC_ok:
                    if(ansBuf[0]==FLAG){
                        STOP = FALSE;
                        printf("\nControl packet confirmation ok!\n");
                    }
                    else{
                        curState = Other_RCV;
                    }
                    break;
                default:
                    return -1;
            }
        }
        else if(checkRead==-1){
            return -1;
        }
    }

    return filesize;
}



int getControlPacket(char *filename, char *filesize){
    int STOP = TRUE;
    state curState = Start;
    //curReading -> -1 erro, 0 filesize, 1 filename
    int curReading = -1;
    unsigned char buf[MAX_PAYLOAD_SIZE*2] = {0};
    while(STOP){
        int checkRead =readByteSerialPort(buf);
        if(checkRead>0){
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
                        checkRead = readByteSerialPort(buf);
                        if(checkRead>0){
                            if(curReading==0){
                            filesize[i] = buf[0];
                        }
                            else if(curReading==1){
                            filename[i] = buf[0];
                        }
                            i++;
                        }
                        else if(checkRead==-1){
                            return -1;
                        }
                        
                    }
                    /*
                    if(curReading==0){
                            filesize[i] = "\n";
                        }
                    else if(curReading==1){
                            filename[i] = "\n";
                    }
                    */
                    curState = T;
                    }
                    break;
                default:
                    return -1;
            }
        }
        else if(checkRead==-1){
            return -1;
        }
    }
   
    printf("\nControl packet received!\n");
    unsigned char packAns[5] = {FLAG, ADDRESS_SENDER, RR0, (ADDRESS_SENDER^RR0), FLAG};
    if(writeBytesSerialPort(packAns,5)==-1)return -1; 
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
    nTrys = connectionParameters.nRetransmissions;
    timeout = connectionParameters.timeout;
    role = connectionParameters.role;
    switch(connectionParameters.role){
        case LlTx: {

            signal(SIGALRM, alarmHandler);
            // Create string to send
            unsigned char buf[5] = {0};
            buf[0] = FLAG;
            buf[1] = ADDRESS_SENDER;
            buf[2] = CONTROL_SET;
            buf[3] = (ADDRESS_SENDER ^ CONTROL_SET);
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
                //vars for the loop and state machine
                int onStateMachine = TRUE;
                curState = Other_RCV;
                while(onStateMachine){
                    //reseting alarm and rewriting
                    if(alarmEnabled==FALSE){
                        printf("Trying again...\n");
                        checkWrite = writeBytesSerialPort(buf,5);
                        if (checkWrite==-1) return -1;
                        alarm(timeout);
                        alarmEnabled = TRUE;
                        onStateMachine = FALSE;
                    }
                    int checkRead = readByteSerialPort(buf2);
                    if(checkRead>0){
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
                                    alarmCount = 0;
                                    onStateMachine = FALSE;
                                    STOP = TRUE;
                                    printf("\nUA received!\n");
                                    //return 1;
                                }
                                break;
                        default:
                            break;
                        }
                    }
                    else if(checkRead==-1){
                        return -1;
                    }
                
                }
            }
            if(alarmCount==nTrys) return -1;
                break;
                }
            case LlRx:
                {
                //connection starting message
                unsigned char buf[5] = {0};
                //vars for the lopp
                int STOP = TRUE;
                //vars to check bcc
                int a;
                int c;
                while(STOP){
                    //vars for state machine
                    int onStateMachine = TRUE;
                    state curState = Other_RCV;
                    while(onStateMachine){
                        int checkRead = readByteSerialPort(buf);
                        if(checkRead>0){
                            switch(curState){
                                case Other_RCV:
                                    if(buf[0]==FLAG){
                                        curState = FLAG_RCV;
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
                                        unsigned char bufAnswer[5] = {0};
                                        printf("\nSending UA! \n");
                                        bufAnswer[0] = FLAG;
                                        bufAnswer[1] = ADDRESS_SENDER;
                                        bufAnswer[2] = CONTROL_UA;
                                        bufAnswer[3] = bufAnswer[1]^bufAnswer[2];
                                        bufAnswer[4] = FLAG;
                                        //bufAnswer[5] = '\n';

                                        int checkWrite = writeBytesSerialPort(bufAnswer, 5);
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
                        else if(checkRead==-1){
                            return -1;
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
    
    signal(SIGALRM, alarmHandler);
    //if firstIn = True não vamos ler, vamos logo escrever, se não lêmos primeiro par ver os I(0|1) etc
    unsigned char bufSend[MAX_PAYLOAD_SIZE*2 + 6] = {0}; //creating the buf to send
    //inicio
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

    //unsigned char bufAux[BUF_SIZE] = {0};
    int bcc2 = 0;
    while (index < bufSize) {

        //reading 1 byte and checking for errors
        bcc2 = bcc2^buf[index];
        if(buf[index]==FLAG){
            bufSend[4+index+offest] = ESC;
            bufSend[4+index+offest+1] = 0x5E;
            offest++;
        }
        else if(buf[index]==ESC){
            bufSend[4+index+offest] = ESC;
            bufSend[4+index+offest+1] = 0x5D;
            offest++;
        }
        else{
            bufSend[4+index+offest] = buf[index];
        }
        index++;
    }
    bufSend[4+index+offest] = bcc2;
    bufSend[4+index+offest+1] = FLAG;
    // TODO
    int size = 6 + bufSize + offest;
    int checkWrite = writeBytesSerialPort(bufSend,size);
    if(checkWrite==-1) return -1;
    printf("\nPacket writen!\n");
    //wait for the answer from the reader
    state curState = Other_RCV;
    int a;
    int c;
    unsigned char answer[MAX_PAYLOAD_SIZE] = {0};
    alarm(timeout);
    alarmEnabled = TRUE;
    alarmCount = 0;
    while(alarmCount<nTrys){
        int checkRead = readByteSerialPort(answer);
        if(alarmEnabled==FALSE){
            printf("Trying again...\n");
            checkWrite = writeBytesSerialPort(bufSend,size);
            if (checkWrite==-1) return -1;
            alarm(timeout);
            alarmEnabled = TRUE;
        }
        if(checkRead>0){
            switch (curState){
                case Other_RCV:
                    if(answer[0]==FLAG){
                        curState = FLAG_RCV;
                    }
                    else{
                        return -1;
                    }
                    break;
                case FLAG_RCV:
                    if(answer[0]==ADDRESS_SENDER){
                        curState = A_RCV;
                        a = ADDRESS_SENDER;
                    }
                    else{
                        return -1;
                    }
                    break;
                case A_RCV:
                    if(answer[0]==RR0 || answer[0]==RR1 || answer[0]==REJ0 || answer[0]== REJ1 ){
                        curState = C_RCV;
                        c = answer[0];
                    }
                    else{
                        return -1;
                    }
                    break;
                case C_RCV:
                    if(answer[0]==(a^c)){
                        curState = BBC_ok;
                    }
                    else{
                        return -1;
                    }
                    break;
                case BBC_ok:
                    if(answer[0]==FLAG){
                        alarm(0);
                        alarmEnabled = FALSE;
                        if(curInfFram==0 && c==RR1){
                            printf("packet was confirmed!\n");
                            curInfFram = 1;
                            return checkWrite;
                        }
                        else if(curInfFram==1 && c==RR0){
                            printf("packet was confirmed!\n");
                            curInfFram = 0;
                            return checkWrite;
                        }
                        else if(c==REJ0 && curInfFram==0){
                            printf("rej!\n");
                            return -1;
                        }
                        else if(c==REJ1 && curInfFram==1){
                            printf("rej!\n");
                            return -1;
                        }
                        else{
                            printf("wrong rr/rej\n");
                            return -1;
                        }
                    }
                    else{
                        return -1;
                    }
                    break;
                default:
                    return -1;
            }   
        }
    }
    //fim 
    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{       
    unsigned char buf[MAX_PAYLOAD_SIZE] = {0};
    int STOP = TRUE;
    state curState = Other_RCV;
    int a;
    int c;
    char bcc2 = 0;
    int bcc2Check = 0;
    int i = 0;
    int index = 0;
    while(STOP){
        int checkRead = readByteSerialPort(buf);
        if(checkRead>0){
            switch(curState){
                case Other_RCV:
                    if(buf[0]==FLAG){
                        curState = FLAG_RCV;
                    }
                    break;
                case FLAG_RCV:
                    if(buf[0]==ADDRESS_SENDER){
                        curState = A_RCV;
                        a = buf[0];
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
                        printf("todo: bruteforce UA.\n");
                        return -1;
                    }
                    else if(buf[0]==I0 || buf[0]==I1){
                        curState = C_RCV;
                        c = buf[0];
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
                        curState = Reading_Data;
                        printf("reading data...\n");
                    }
                    else if(buf[0]==FLAG){
                        curState = FLAG_RCV;
                    }
                    else{
                        curState = Other_RCV;
                    }
                    break;
                case Reading_Data:
                    {
                        if(buf[0]==ESC){
                            int STOP = TRUE;
                            while(STOP){
                                int checkByteDes = readByteSerialPort(buf);
                                if(checkByteDes>0){
                                    char toPut;
                                    if(buf[0]==0x5E){
                                        toPut = 0x7E;
                                    }
                                    else if(buf[0]==0x5D){
                                        toPut = 0x7D;
                                    }
                                    else{
                                        return -1;
                                    }
                                    packet[index] = toPut;
                                    bcc2Check = bcc2Check^toPut;
                                    index++;
                                    STOP = FALSE;
                                }
                                else if(checkByteDes==-1){
                                    return -1;
                                }
                            }
                        }
                        else{
                            packet[index] = buf[0];
                            bcc2Check = bcc2Check^buf[0];
                            index++;//aumentar indice no packet
                        }
                        while(index<(MAX_PAYLOAD_SIZE)){
                            int checkReadDate = readByteSerialPort(buf);
                            if(checkReadDate>0){
                                if(buf[0]==ESC){
                                    int STOP = TRUE;
                                    while(STOP){
                                        int checkByteDes = readByteSerialPort(buf);
                                        if(checkByteDes>0){
                                            char toPut;
                                            if(buf[0]==0x5E){
                                                toPut = 0x7E;
                                            }
                                            else if(buf[0]==0x5D){
                                                toPut = 0x7D;
                                            }
                                            else{
                                                return -1;
                                            }
                                            packet[index] = toPut;
                                            bcc2Check = bcc2Check^toPut;
                                            index++;
                                            STOP = FALSE;
                                        }
                                        else if(checkByteDes==-1){
                                            return -1;
                                        }
                                    }
                                }
                                else{
                                    packet[index] = buf[0];
                                    bcc2Check = bcc2Check^buf[0];
                                    index++;//aumentar indice no packet
                                }
                            }
                            else if(checkReadDate==-1) return -1; 
                            
                        }
               
                        curState = Check_BCC;
                        break;
                    }
                case Check_BCC:
                    if(buf[0]==bcc2Check){
                        curState = BBC_ok;
                    }
                    else{
                        curState = BCC_Wrong;
                    }
                    break;
                case BBC_ok:
                    if(buf[0]==FLAG){
                        if(curInfFramR==0){
                            unsigned char answer[5] = {FLAG, ADDRESS_SENDER, RR1, (ADDRESS_SENDER^RR1), FLAG};
                            curInfFramR = 1;
                            int checkWrite = writeBytesSerialPort(answer, 5);
                            if (checkWrite==-1) return -1;
                            
                        }
                        else if(curInfFramR==1){
                            unsigned char answer[5] = {FLAG, ADDRESS_SENDER, RR0, (ADDRESS_SENDER^RR0), FLAG};
                            curInfFramR = 0;
                            int checkWrite = writeBytesSerialPort(answer, 5);
                            if (checkWrite==-1) return -1;
                        
                        }
                        return index;
                    }
                    else{
                        return -1;
                    }
                    break;
                case BCC_Wrong:
                    if(buf[0]==FLAG){
                        printf("\nWrong BCC\n");
                        if(curInfFramR==0){
                            unsigned char answer[5] = {FLAG, ADDRESS_SENDER, REJ0, (ADDRESS_SENDER^REJ0), FLAG};
                            int checkWrite = writeBytesSerialPort(answer, 5);
                            if (checkWrite==-1) return -1;
                            
                        }
                        else if(curInfFramR==1){
                            unsigned char answer[5] = {FLAG, ADDRESS_SENDER, REJ1, (ADDRESS_SENDER^REJ1), FLAG};
                            int checkWrite = writeBytesSerialPort(answer, 5);
                            if (checkWrite==-1) return -1;
                        
                        }
                        return -1;
                    }
                    break;
                default:
                    return -1;
            } 
        }
        else if(checkRead==-1){
            return -1;
        }
    }
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
    switch(role){
        case LlTx:{
            unsigned char buf[5] = {0};
            buf[0] = FLAG;
            buf[1] = ADDRESS_SENDER;
            buf[2] = DISC;
            buf[3] = ADDRESS_SENDER ^ DISC;
            buf[4] = FLAG;
            //buf3[5] = '\n';
            int checkWrite = writeBytesSerialPort(buf,5);
            printf("\nSending disconecting flag\n");
            unsigned char bufAnswer[5] = {0};
            if (checkWrite==-1) return -1;
            int onStateMachine = TRUE;
            state curState = Other_RCV;
            int a, c;
            while(onStateMachine){
                int checkWrite = readByteSerialPort(bufAnswer);
                if(checkWrite>0){
                    switch (curState){
                        case Other_RCV:
                            if(bufAnswer[0]==FLAG){
                                curState = FLAG_RCV;
                            }
                            break;
                        case FLAG_RCV:
                            if(bufAnswer[0]==ADDRESS_SENDER){
                                curState = A_RCV;
                                a = bufAnswer[0];
                            }
                            else if(bufAnswer[0]==FLAG){
                                curState = FLAG_RCV;
                            }
                            else{
                                curState = Other_RCV;
                            }
                            break;
                        case A_RCV:
                            
                            if(bufAnswer[0]==DISC){

                                curState = C_RCV;
                                c = bufAnswer[0];
                            }
                            else if(bufAnswer[0]==FLAG){
                            
                                curState = FLAG_RCV;
                            }
                            else{
                                
                                curState = Other_RCV;
                            }
                            break;
                        case C_RCV:
                            if(bufAnswer[0] == (a^c)){
                                curState = BBC_ok;
                            }
                            else if(bufAnswer[0]==FLAG){
                                curState = FLAG;
                            }
                            else{
                                curState = Other_RCV;
                            }
                            break;
                        case BBC_ok:{
                            unsigned char UA[5] = {0};
                            UA[0] = FLAG;
                            UA[1] = ADDRESS_SENDER;
                            UA[2] = DISC;
                            UA[3] = ADDRESS_SENDER ^ DISC;
                            UA[4] = FLAG;
                            int checkWrite = writeBytesSerialPort(UA, 5);
                            if (checkWrite==-1) return -1;
                            onStateMachine = FALSE;
                            break;
                            
                        }
                        default:
                            break;
                    }
                }
                else if(checkWrite==-1){
                    return -1;
                }        
            }

            // Final Statistics
            time_t ending;
            stats.end_time = time(&ending);
            if(showStatistics==TRUE){
                printStatistics(stats);
            }

            break;
        }
        case LlRx:{
            unsigned char buf[5] = {0};
            int STOP = TRUE;
            while(STOP){
                //vars for state machine
                int onStateMachine = TRUE;
                state curState = Other_RCV;
                int a, c;
                while(onStateMachine){
                    int checkWrite = readByteSerialPort(buf);
                    if(checkWrite>0){
                        switch(curState){
                            case Other_RCV:
                                if(buf[0]==FLAG){
                                    curState = FLAG_RCV;
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
                                if(buf[0]==DISC){
                                    curState = C_RCV;
                                    c = DISC;
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
                                    unsigned char bufAnswer[5] = {0};
                                    printf("Sending Disconecting flag!\n");
                                    bufAnswer[0] = FLAG;
                                    bufAnswer[1] = ADDRESS_SENDER;
                                    bufAnswer[2] = DISC;
                                    bufAnswer[3] = bufAnswer[1]^bufAnswer[2];
                                    bufAnswer[4] = FLAG;
                                    int checkWrite = writeBytesSerialPort(bufAnswer, 5);
                                    if (checkWrite==-1) return -1;
                                    onStateMachine = FALSE;
                                    STOP = FALSE;
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
                    else if(checkWrite==-1){
                        return -1;
                    }
                }
            }
            break;
        }
        default:
            return -1;
            break;
        }
    printf("\ndisconecting...\n");
    int clstat = closeSerialPort();
    if(clstat==-1){
        printf("error on llclose\n");
    }

    return clstat;
}
