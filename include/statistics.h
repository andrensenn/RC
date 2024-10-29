
#ifndef _STATISTICS_H_
#define _STATISTICS_H_

#include <stdio.h>
#include <stdlib.h>
#include <time.h>


struct statistic{
    time_t start_time;
    time_t end_time;
    /// 
    int retransmission_total;
    int timeout_total;
    ///
    int bytes_total;
    int bits_total;
    int information_frame_size; // bytes
    ///
    int link_capacity;  // C (Baudrate)
};

int logStatistics(struct statistic a);

void printStatistics(struct statistic a);

#endif // _APPLICATION_LAYER_H_