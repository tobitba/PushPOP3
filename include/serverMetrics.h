#ifndef SERVER_METRICS_H
#define SERVER_METRICS_H

void incrementCurrentConnections();
void decrementCurrentConnections();
void incrementTotalConnections();
void incrementTotalReadBytes(unsigned long bytes);

unsigned long getTotalConnections();
unsigned long getCurrentConnections();
unsigned long getTotalReadBytes();

#endif