// Application layer protocol implementation

#include "../include/application_layer.h"
#include "../include/link_layer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../include/consts.h"

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
    {
    long sizeOfFile;
    LinkLayer args;
    strcpy(args.serialPort, serialPort);
    LinkLayerRole roleThis;
    if(!strcmp(role,"tx")){
        args.role = LlTx;
        roleThis = LlTx;
    }
    else{
        args.role = LlRx;
        roleThis  = LlRx;
    }
    args.baudRate = baudRate;
    args.nRetransmissions = nTries;
    args.timeout = timeout;
    int checkLlopen = llopen(args);
    if(checkLlopen!=1){
        printf("error LLopen.\n");
        exit(1);
    }
    // printf("var = 0x%02X\n", buf2[0]);
    printf("\nConnection established!\n");
    
    switch(roleThis){
        case LlTx:{
            //control packet waits for positive answer
            long i = sendControlPacket(filename); 
            if (i==-1) return;
            sizeOfFile = i;

            /*
            open the file
            ready byte by byte to a buf
            send buff on llwrite all bufs
            close file
            disc message
            llclose
            */
            //just to test
            //unsigned char test[15] = {0,1,2,4,1,6,8,2,2,1,ESC,FLAG,5,1,1};
            //int checkllwrite = llwrite(test,15);
            //test end
            
            FILE *fileCheck = fopen(filename, "r"); 
            if(fileCheck==NULL){
                printf("error opening file\n");
                return -1;
            }

            //fseek(fileCheck, 0, SEEK_END);
            
            int STOP = TRUE;
            int offset = 0 ;
            while(STOP){
                int index = 0;
                int nextChar;
                unsigned char bufllwrite[MAX_PAYLOAD_SIZE] = {0};
                while(index<MAX_PAYLOAD_SIZE){
                    nextChar = getc(fileCheck);
                    if(nextChar==EOF){
                        bufllwrite[index] = nextChar;
                        bufllwrite[index+1] = '\n';
                        STOP = FALSE;
                        break;
                    }
                    else{
                        bufllwrite[index] = nextChar;
                        index++;
                    }
                }
                int checkllwrite = llwrite(bufllwrite,MAX_PAYLOAD_SIZE);
                if(checkllwrite==-1) return -1;
                offset = offset + MAX_PAYLOAD_SIZE;
                }
            fclose(fileCheck);
            }
            break;
        case LlRx:
            {
            unsigned char filenameReceived[MAX_PAYLOAD_SIZE] = {0};
            unsigned char size[MAX_PAYLOAD_SIZE] = {0};
            
            int checkControlPacket = getControlPacket(filenameReceived, size);
            if (checkControlPacket==-1) 
            {
                return;
            }
            int STOP = TRUE;
            FILE *newFile = fopen("penguin-received.gif", "wb");
            sizeOfFile = atol(size);
            if(newFile==NULL){
                printf("error opening file\n");
                return -1;
            }
            int total = 0;
            while(STOP){
                unsigned char read[MAX_PAYLOAD_SIZE] = {0};
                int checkRead = llread(read);
                if(checkRead==-1) return -1;
                printf("writting to file\n");
                size_t bytesWritten = fwrite(read, 1, checkRead, newFile);
                if(bytesWritten!=checkRead) {
                    printf("error writting to the file\n");
                    fclose(newFile);
                    return -1;
                }
                total = total + checkRead;
                if(total >= sizeOfFile) STOP = FALSE;
            }
            fclose(newFile);
            //just to test
            /*
            sizeOfFile = atol(size);
            printf("\n%s\n", filenameReceived);
            
            
            int i = 0;
            while(i<checkRead){
                printf("var = 0x%02X\n", read[i]);
                i++;
            }
            */
            //end test
            
            }
            break;

    }
}
