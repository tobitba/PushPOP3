#include "../include/serverMetrics.h"

typedef struct Metrics {
    unsigned long currentConextions;
    unsigned long totalConextions;
    unsigned long totalReadBytes;
} Metrics;

Metrics globalMetrics = {0, 0, 0};

void incrementCurrentConnections() {
  globalMetrics.currentConextions++;
}
void decrementCurrentConnections() {
    globalMetrics.currentConextions--;
}
void incrementTotalConnections() {
  globalMetrics.totalConextions++;
}
void incrementTotalReadBytes(unsigned long bytes) {
  globalMetrics.totalReadBytes += bytes;
}

unsigned long getCurrentConnections() {
  return globalMetrics.currentConextions;
}
unsigned long getTotalConnections() {
  return globalMetrics.totalConextions;
}
unsigned long getTotalReadBytes() {
  return globalMetrics.totalReadBytes;
}