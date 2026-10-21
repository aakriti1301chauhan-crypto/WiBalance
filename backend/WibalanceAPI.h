#ifndef WIBALANCE_API_H
#define WIBALANCE_API_H

#include <string>
#include <vector>

struct WiBalanceStatus {
    int currentTick = 0;
    int totalTicks = 50;
    int accessPointCount = 0;
    int userCount = 0;
    bool finished = false;
};

struct WiBalanceAPStatus {
    std::string id;
    double loadPercent = 0.0;
    int userCount = 0;

    double capacityMbps = 0.0;
    double assignedDemandMbps = 0.0;
    double channelUtilization = 0.0;
    double latencyMs = 0.0;
    double packetLossPercent = 0.0;

    int maxUsers = 0;
    std::string status = "HEALTHY";
};
