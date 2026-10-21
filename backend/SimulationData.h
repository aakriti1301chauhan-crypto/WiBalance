#ifndef WIBALANCE_SIMULATION_DATA_H
#define WIBALANCE_SIMULATION_DATA_H

#include <string>

struct APData {
    std::string id;
    double capacityMbps = 0.0;
    double loadPercent = 0.0;
    double channelUtilization = 0.0;
    double latencyMs = 0.0;
    double packetLossPercent = 0.0;
    int userCount = 0;
    int maxUsers = 0;
    std::string status = "HEALTHY";
};

struct UserData {
    std::string id;
    std::string assignedAP;
    std::string trafficType;
    double demandMbps = 0.0;
    double signalDbm = -100.0;
    int priority = 2;
};

struct SimulationSnapshot {
    int currentTick = 0;
    int totalTicks = 50;
    bool finished = false;

    int totalUsers = 0;
    int assignedUsers = 0;

    double averageLoad = 0.0;

    std::string networkHealth = "Healthy";
};

#endif

