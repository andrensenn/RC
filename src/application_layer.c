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
            long i = sendControlPacket(filename); 
            if (i==-1) return;
            sizeOfFile = i;
            //just to test
            unsigned char test[15] = {0,1,2,4,1,6,8,2,2,1,ESC,FLAG,5,1,1};
            int checkllwrite = llwrite(test,15);
            //test end

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
            unsigned char read[MAX_PAYLOAD_SIZE] = {0};
            int checkRead = llread(read);
            //just to test
            sizeOfFile = atol(size);
            printf("\n%s\n", filenameReceived);
            printf("%ld\n", sizeOfFile);
            
            int i = 0;
            while(i<checkRead){
                printf("var = 0x%02X\n", read[i]);
                i++;
            }
            //end test
            
            }
            break;

    }
}
