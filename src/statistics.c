#include "statistics.h"

int logStatistics(struct statistic a){
    // stat Count
    double elapsed_time = difftime(a.end_time, a.start_time); // in seconds
    double received_bitrate = (elapsed_time > 0) ? a.bits_total/elapsed_time : 0.0; // in bits per second
    double efficiency = (a.link_capacity > 0) ? received_bitrate/a.link_capacity : 0.0; 
    

    // log Stats in .txt File
    FILE *stats;
    const char *filename = "statistics.txt";


    // Open the file in append mode to add new attempt entry
    stats = fopen(filename, "a");
    if(stats == NULL){
        perror("Error opening file");
        return 1;
    }

    // Write the new attempt statistics
    fprintf(stats,"\n\n");
    fprintf(stats, "========================\n\n");
    fprintf(stats, "Protocol Start Time: %s\n", ctime(&a.start_time));
    fprintf(stats, "Protocol End Time: %s\n", ctime(&a.end_time));
    fprintf(stats, "---------\n");

    fprintf(stats, "Total Number of Retransmissions: %d\n", a.retransmission_total);
    fprintf(stats, "Total Number of Timeouts: %d\n", a.timeout_total);
    fprintf(stats, "---------\n");

    fprintf(stats, "Total Number of Bytes: %d\n", a.bytes_total);
    fprintf(stats, "Total Number of Bits: %d\n", a.bits_total);
    fprintf(stats, "Information Frame Size: %d\n", a.information_frame_size);
    fprintf(stats, "---------\n");

    fprintf(stats, "Link Capacity (BaudRate): %d\n", a.link_capacity);
    fprintf(stats, "---------\n");

    ///
    fprintf(stats, "\n");
    fprintf(stats, "Elapsed Time: %.5f seconds\n", elapsed_time);
    fprintf(stats, "Received Bitrate (R): %.5f bits per second\n", received_bitrate);
    fprintf(stats, "Efficiency (S): %.5f\n", efficiency);
    fprintf(stats, "\n");
    ///
    fprintf(stats, "Note → All values rounded to a maximum of 5 decimal points\n");
    fprintf(stats, "\n");

    fprintf(stats, "====== Attempt End ======\n");

    fclose(stats);
    
    return 0;
}

void printStatistics(struct statistic a){
    // stat Count
    double elapsed_time = difftime(a.end_time, a.start_time); // in seconds
    double received_bitrate = (elapsed_time > 0) ? a.bits_total/elapsed_time : 0.0; // in bits per second
    double efficiency = (a.link_capacity > 0) ? received_bitrate/a.link_capacity : 0.0; 
    
    // Write statistics
    printf("\n\n");
    printf("========================\n\n");
    printf("Protocol Start Time: %s\n", ctime(&a.start_time));
    printf("Protocol End Time: %s\n", ctime(&a.end_time));
    printf("---------\n");

    printf("Total Number of Retransmissions: %d\n", a.retransmission_total);
    printf("Total Number of Timeouts: %d\n", a.timeout_total);
    printf("---------\n");

    printf("Total Number of Bytes: %d\n", a.bytes_total);
    printf("Total Number of Bits: %d\n", a.bits_total);
    printf("Information Frame Size: %d\n", a.information_frame_size);
    printf("---------\n");

    printf("Link Capacity (BaudRate): %d\n", a.link_capacity);
    printf("---------\n");

    ///
    printf("\n");
    printf("Elapsed Time: %.5f seconds\n", elapsed_time);
    printf("Received Bitrate (R): %.5f bits per second\n", received_bitrate);
    printf("Efficiency (S): %.5f\n", efficiency);
    printf("\n");
    ///
    printf("Note → All values rounded to a maximum of 5 decimal points\n");
    printf("\n");

    printf("========================\n\n");
}