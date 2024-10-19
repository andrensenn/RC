// Application layer protocol implementation

#include "../include/application_layer.h"
#include "../include/link_layer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
    {

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
            int i = sendControlPacket(filename); 
            if (i==-1) return;
            }

            break;
        case LlRx:
            {
            unsigned char *filesize[MAX_PAYLOAD_SIZE];
            unsigned char *filename[MAX_PAYLOAD_SIZE];
            int i = getControlPacket(filename, filesize);
            if (i==-1) 
            {
                return;
            }
            }
            break;

    }
    // TODO
}
