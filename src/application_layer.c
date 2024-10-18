// Application layer protocol implementation

#include "../include/application_layer.h"
#include "../include/link_layer.h"
#include <stdio.h>
#include <string.h>

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
    {

    LinkLayer args;
    strcpy(args.serialPort, serialPort);
    if(!strcmp(role,"tx")){
        args.role = LlTx;
    }
    else{
        args.role = LlRx;
    }
    args.baudRate = baudRate;
    args.nRetransmissions = nTries;
    args.timeout = timeout;
    int checkLlopen = llopen(args);
    if(checkLlopen!=1){
        printf("error LLopen.\n");
    }
    // TODO
}
