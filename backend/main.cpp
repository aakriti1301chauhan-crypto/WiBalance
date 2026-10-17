#include "SimulationData.h"
#include "dsa.h"
#include "WiBalanceAPI.h"
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
using namespace std;

static const int DEFAULT_FINAL_TICK = 50;
static const double CONGESTION_LIMIT = 80.0;

class User {
public:
    string id;
    string trafficType;
    string currentAP;
    double requestedMbps;
    double minimumMbps;
    double currentDemandMbps;
    double x;
    double y;
    int priority;       // 1 = Low, 2 = Normal, 3 = High
    bool mobile;
    double signalDbm;

    User()
        : requestedMbps(5.0), minimumMbps(1.0), currentDemandMbps(1.0),
          x(0.0), y(0.0), priority(2), mobile(false), signalDbm(-100.0) {}
};

class AccessPoint {
public:
    string id;
    double capacityMbps;
    int maxUsers;
    double coverageRadius;
    double baseLatencyMs;
    double x;
    double y;
    vector<string> userIds;
    double assignedDemandMbps;
    double loadPercent;
    double channelUtilization;
    double latencyMs;
    double packetLossPercent;

    AccessPoint()
        : capacityMbps(100.0), maxUsers(25), coverageRadius(100.0),
          baseLatencyMs(8.0), x(0.0), y(0.0), assignedDemandMbps(0.0),
          loadPercent(0.0), channelUtilization(0.0), latencyMs(8.0),
          packetLossPercent(0.0) {}
};

class WiBalance {
private:
    vector<AccessPoint> accessPoints;
    vector<User> users;
    DSAHashHandle userHash;
    DSAQueueHandle assignmentQueue;
    mt19937 randomEngine;
    int tick;
    int finalTick;
    bool hasRun;

    string makeNextAPId() const {
        int largest = 0;
        for (size_t i = 0; i < accessPoints.size(); ++i) {
            const string& id = accessPoints[i].id;
            if (id.size() > 2 && id.substr(0, 2) == "AP") {
                try {
                    int number = stoi(id.substr(2));
                    if (number > largest) largest = number;
                } catch (...) {}
            }
        }
        return "AP" + to_string(largest + 1);
    }

    string makeNextUserId() const {
        int largest = 100;
        for (size_t i = 0; i < users.size(); ++i) {
            const string& id = users[i].id;
            if (id.size() > 1 && id[0] == 'U') {
                try {
                    int number = stoi(id.substr(1));
                    if (number > largest) largest = number;
                } catch (...) {}
            }
        }
        return "U" + to_string(largest + 1);
    }

    int findAPIndex(const string& id) const {
        vector<string> ids;
        vector<const char*> pointers;
        ids.reserve(accessPoints.size());
        pointers.reserve(accessPoints.size());
        for (size_t i = 0; i < accessPoints.size(); ++i) ids.push_back(accessPoints[i].id);
        for (size_t i = 0; i < ids.size(); ++i) pointers.push_back(ids[i].c_str());
        return dsa_linear_search_strings(pointers.data(), static_cast<int>(pointers.size()), id.c_str());
    }

    void rebuildUserHash() {
        dsa_hash_clear(userHash);
        for (int i = 0; i < static_cast<int>(users.size()); ++i) {
            dsa_hash_put(userHash, users[i].id.c_str(), i);
        }
    }

    int findUserIndex(const string& id) const {
        int index = -1;
        if (!dsa_hash_get(userHash, id.c_str(), &index)) return -1;
        if (index < 0 || index >= static_cast<int>(users.size())) return -1;
        if (users[static_cast<size_t>(index)].id != id) return -1;
        return index;
    }

    double distanceBetween(double x1, double y1, double x2, double y2) const {
        double dx = x1 - x2;
        double dy = y1 - y2;
        return sqrt(dx * dx + dy * dy);
    }

    double signalAt(const User& user, const AccessPoint& ap) const {
        double distance = distanceBetween(user.x, user.y, ap.x, ap.y);
        if (ap.coverageRadius <= 0.0) return -100.0;
        double ratio = distance / ap.coverageRadius;
        return -35.0 - 48.0 * ratio;
    }

    double demandForType(const User& user) const {
        double base = user.requestedMbps;
        if (user.trafficType == "Video") base *= 0.78;
        else if (user.trafficType == "Download") base *= 0.92;
        else if (user.trafficType == "Voice") base *= 0.14;
        else if (user.trafficType == "Gaming") base *= 0.22;
        else if (user.trafficType == "Web") base *= 0.30;
        else if (user.trafficType == "Messaging") base *= 0.06;
        if (base < user.minimumMbps) base = user.minimumMbps;
        if (base > user.requestedMbps) base = user.requestedMbps;
        return base;
    }

    void updateTrafficDemand() {
        uniform_real_distribution<double> variation(0.85, 1.15);
        for (size_t i = 0; i < users.size(); ++i) {
            double demand = demandForType(users[i]) * variation(randomEngine);
            if (demand < users[i].minimumMbps) demand = users[i].minimumMbps;
            if (demand > users[i].requestedMbps) demand = users[i].requestedMbps;
            users[i].currentDemandMbps = demand;
        }
    }

    double candidateScore(const User& user, const AccessPoint& ap) const {
        double distance = distanceBetween(user.x, user.y, ap.x, ap.y);
        double signal = signalAt(user, ap);
        if (distance > ap.coverageRadius || signal < -83.0) return -1.0e9;

        double projectedLoad = 0.0;
        if (ap.capacityMbps > 0.0)
            projectedLoad = (ap.assignedDemandMbps + user.currentDemandMbps) / ap.capacityMbps * 100.0;
        else
            projectedLoad = 1000.0;

        double projectedClients = static_cast<double>(ap.userIds.size() + 1);
        double clientPercent = ap.maxUsers > 0 ? projectedClients / ap.maxUsers * 100.0 : 1000.0;
        double predictedLatency = ap.baseLatencyMs + (projectedLoad > 40.0 ? (projectedLoad - 40.0) * 0.50 : 0.0);
        double loadScore = 1.0 - projectedLoad / 140.0;
        double signalScore = (signal + 90.0) / 55.0;
        double latencyScore = 1.0 - predictedLatency / 180.0;
        double clientScore = 1.0 - clientPercent / 140.0;
        double priorityWeight = 1.0 + static_cast<double>(user.priority) * 0.025;
        double loadWeight = user.mobile ? 0.35 : 0.43;
        double signalWeight = user.mobile ? 0.33 : 0.25;

        if (projectedLoad > 100.0) loadScore -= (projectedLoad - 100.0) / 60.0;
        if (projectedClients > ap.maxUsers) clientScore -= (projectedClients - ap.maxUsers) / 5.0;

        return priorityWeight * (loadWeight * loadScore + signalWeight * signalScore
                               + 0.17 * latencyScore + 0.15 * clientScore);
    }

    int bestAPFor(const User& user) const {
        int bestIndex = -1;
        double bestScore = -1.0e9;
        for (int i = 0; i < static_cast<int>(accessPoints.size()); ++i) {
            double score = candidateScore(user, accessPoints[static_cast<size_t>(i)]);
            if (score > bestScore) {
                bestScore = score;
                bestIndex = i;
            }
        }
        return bestIndex;
    }

    void clearAssignments() {
        for (size_t i = 0; i < accessPoints.size(); ++i) {
            accessPoints[i].userIds.clear();
            accessPoints[i].assignedDemandMbps = 0.0;
        }
        for (size_t i = 0; i < users.size(); ++i) {
            users[i].currentAP.clear();
            users[i].signalDbm = -100.0;
        }
    }

    void recalculateAPMetrics() {
        for (size_t i = 0; i < accessPoints.size(); ++i) {
            AccessPoint& ap = accessPoints[i];
            ap.loadPercent = ap.capacityMbps > 0.0
                ? ap.assignedDemandMbps / ap.capacityMbps * 100.0 : 1000.0;
            if (ap.loadPercent < 0.0) ap.loadPercent = 0.0;
            ap.channelUtilization = ap.loadPercent * 0.88;
            if (ap.channelUtilization > 100.0) ap.channelUtilization = 100.0;
            ap.latencyMs = ap.baseLatencyMs;
            if (ap.loadPercent > 40.0) ap.latencyMs += (ap.loadPercent - 40.0) * 0.50;
            ap.packetLossPercent = ap.loadPercent > 85.0 ? (ap.loadPercent - 85.0) * 0.20 : 0.0;
            if (ap.packetLossPercent > 20.0) ap.packetLossPercent = 20.0;
        }
        for (size_t i = 0; i < users.size(); ++i) {
            int apIndex = findAPIndex(users[i].currentAP);
            if (apIndex >= 0) users[i].signalDbm = signalAt(users[i], accessPoints[static_cast<size_t>(apIndex)]);
        }
    }

    void globallyRedistributeUsers() {
        clearAssignments();
        if (users.empty() || accessPoints.empty()) {
            recalculateAPMetrics();
            return;
        }

        /* C insertion sort orders high-priority/heavier-demand users first. */
        vector<int> indices(users.size());
        vector<double> scores(users.size());
        for (int i = 0; i < static_cast<int>(users.size()); ++i) {
            indices[static_cast<size_t>(i)] = i;
            scores[static_cast<size_t>(i)] =
                static_cast<double>(users[static_cast<size_t>(i)].priority) * 1000.0
                + users[static_cast<size_t>(i)].currentDemandMbps;
        }
        dsa_insertion_sort_indices(indices.data(), scores.data(), static_cast<int>(indices.size()));

        dsa_queue_destroy(assignmentQueue);
        assignmentQueue = dsa_queue_create();
        for (size_t i = 0; i < indices.size(); ++i) {
            dsa_queue_enqueue(assignmentQueue, users[static_cast<size_t>(indices[i])].id.c_str());
        }

        char userId[64];
        while (dsa_queue_dequeue(assignmentQueue, userId, static_cast<int>(sizeof(userId)))) {
            int userIndex = findUserIndex(userId);
            if (userIndex < 0) continue;
            User& user = users[static_cast<size_t>(userIndex)];
            int targetIndex = bestAPFor(user);
            if (targetIndex < 0) continue;

            AccessPoint& target = accessPoints[static_cast<size_t>(targetIndex)];
            user.currentAP = target.id;
            user.signalDbm = signalAt(user, target);
            target.userIds.push_back(user.id);
            target.assignedDemandMbps += user.currentDemandMbps;
        }
        recalculateAPMetrics();
    }

    string statusOf(const AccessPoint& ap) const {
        if (ap.loadPercent >= CONGESTION_LIMIT || ap.userIds.size() > static_cast<size_t>(ap.maxUsers))
            return "CONGESTED";
        if (ap.loadPercent >= 55.0) return "MODERATE";
        return "HEALTHY";
    }

    string priorityName(int priority) const {
        if (priority >= 3) return "High";
        if (priority == 2) return "Normal";
        return "Low";
    }

    double readPositiveDouble(const string& prompt, double fallback) {
        while (true) {
            cout << prompt;
            string line;
            getline(cin, line);
            if (line.empty()) return fallback;
            stringstream stream(line);
            double value;
            char extra;
            if ((stream >> value) && !(stream >> extra) && isfinite(value) && value > 0.0)
                return value;
            cout << "Enter a positive number.\n";
        }
    }

    double readCoordinate(const string& prompt, double fallback) {
        while (true) {
            cout << prompt;
            string line;
            getline(cin, line);
            if (line.empty()) return fallback;
            stringstream stream(line);
            double value;
            char extra;
            if ((stream >> value) && !(stream >> extra) && isfinite(value) && value >= -100000.0 && value <= 100000.0)
                return value;
            cout << "Enter a valid coordinate.\n";
        }
    }

    int readInt(const string& prompt, int minimum, int maximum, int fallback) {
        while (true) {
            cout << prompt;
            string line;
            getline(cin, line);
            if (line.empty()) return fallback;
            stringstream stream(line);
            int value;
            char extra;
            if ((stream >> value) && !(stream >> extra) && value >= minimum && value <= maximum)
                return value;
            cout << "Enter a number from " << minimum << " to " << maximum << ".\n";
        }
    }

    string readLine(const string& prompt, const string& fallback = "") {
        cout << prompt;
        string line;
        getline(cin, line);
        return line.empty() ? fallback : line;
    }

    string readTrafficType() {
        cout << "Traffic type: 1.Video  2.Download  3.Voice  4.Gaming  5.Web  6.Messaging\n";
        int choice = readInt("Choose traffic type [1-6]: ", 1, 6, 1);
        if (choice == 1) return "Video";
        if (choice == 2) return "Download";
        if (choice == 3) return "Voice";
        if (choice == 4) return "Gaming";
        if (choice == 5) return "Web";
        return "Messaging";
    }

    int readPriority() {
        cout << "Priority: 1.Low  2.Normal  3.High\n";
        return readInt("Choose priority [1-3, default 2]: ", 1, 3, 2);
    }

    void askToRunAfterChange() {
        hasRun = false;
        tick = 0;
        cout << "\nConfiguration changed. Start simulation now? (y/n): ";
        string answer;
        getline(cin, answer);
        if (answer == "y" || answer == "Y" || answer == "yes" || answer == "YES")
            startSimulation();
    }

    void addAP() {
        AccessPoint ap;
        ap.id = readLine("Access Point ID [blank = auto]: ");
        if (ap.id.empty()) ap.id = makeNextAPId();
        if (findAPIndex(ap.id) >= 0) {
            cout << "An AP with that ID already exists.\n";
            return;
        }
        ap.capacityMbps = readPositiveDouble("Total capacity (Mbps) [default 100]: ", 100.0);
        ap.maxUsers = readInt("Maximum supported users [default 25]: ", 1, 10000, 25);
        ap.coverageRadius = readPositiveDouble("Coverage radius (meters) [default 100]: ", 100.0);
        ap.baseLatencyMs = readPositiveDouble("Base latency (ms) [default 8]: ", 8.0);
        ap.x = readCoordinate("AP location X [default 0]: ", 0.0);
        ap.y = readCoordinate("AP location Y [default 0]: ", 0.0);
        accessPoints.push_back(ap);
        cout << "\nAdded " << ap.id << ".\n";
        askToRunAfterChange();
    }

    void removeAP() {
        if (accessPoints.empty()) {
            cout << "No AP exists to remove.\n";
            return;
        }
        string id = readLine("AP ID to remove: ");
        int index = findAPIndex(id);
        if (index < 0) {
            cout << "AP not found.\n";
            return;
        }
        accessPoints.erase(accessPoints.begin() + index);
        for (size_t i = 0; i < users.size(); ++i) {
            if (users[i].currentAP == id) users[i].currentAP.clear();
        }
        cout << "Removed " << id << ". Users will be redistributed when the simulation starts.\n";
        askToRunAfterChange();
    }

    void addUser() {
        User user;
        user.id = readLine("User ID [blank = auto]: ");
        if (user.id.empty()) user.id = makeNextUserId();
        if (findUserIndex(user.id) >= 0) {
            cout << "A user with that ID already exists.\n";
            return;
        }
        user.trafficType = readTrafficType();
        user.requestedMbps = readPositiveDouble("Requested bandwidth (Mbps) [default 5]: ", 5.0);
        user.minimumMbps = readPositiveDouble("Minimum required bandwidth (Mbps) [default 1]: ", 1.0);
        if (user.minimumMbps > user.requestedMbps) {
            cout << "Minimum bandwidth cannot exceed requested bandwidth. User not added.\n";
            return;
        }
        user.priority = readPriority();
        user.x = readCoordinate("User location X [default 0]: ", 0.0);
        user.y = readCoordinate("User location Y [default 0]: ", 0.0);
        cout << "Is this user mobile? (y/n, default n): ";
        string mobileAnswer;
        getline(cin, mobileAnswer);
        user.mobile = (mobileAnswer == "y" || mobileAnswer == "Y" || mobileAnswer == "yes");
        user.currentDemandMbps = demandForType(user);
        users.push_back(user);
        rebuildUserHash();
        cout << "\nAdded " << user.id << ". The algorithm will choose its AP.\n";
        if (accessPoints.empty()) {
            cout << "No APs exist yet. Add an AP before starting the simulation.\n";
            hasRun = false;
            tick = 0;
            return;
        }
        askToRunAfterChange();
    }

    void removeUser() {
        string id = readLine("User ID to remove: ");
        int index = findUserIndex(id);
        if (index < 0) {
            cout << "User not found.\n";
            return;
        }
        users.erase(users.begin() + index);
        rebuildUserHash();
        cout << "Removed " << id << ". Remaining users will be redistributed when the simulation starts.\n";
        askToRunAfterChange();
    }

    void showAPs() const {
        if (accessPoints.empty()) {
            cout << "No access points configured.\n";
            return;
        }
        cout << left << setw(8) << "AP" << right << setw(8) << "Users"
             << setw(11) << "Load%" << setw(12) << "Channel%"
             << setw(12) << "Latency" << setw(10) << "Loss%"
             << setw(11) << "MaxUsers" << "   Status\n";
        cout << string(91, '-') << "\n";
        for (size_t i = 0; i < accessPoints.size(); ++i) {
            const AccessPoint& ap = accessPoints[i];
            cout << left << setw(8) << ap.id << right << setw(8) << ap.userIds.size()
                 << setw(10) << fixed << setprecision(1) << ap.loadPercent << "%"
                 << setw(11) << ap.channelUtilization << "%"
                 << setw(9) << ap.latencyMs << " ms"
                 << setw(9) << ap.packetLossPercent << "%"
                 << setw(11) << ap.maxUsers << "   " << statusOf(ap) << "\n";
            cout << "  Capacity: " << ap.capacityMbps << " Mbps | Coverage: "
                 << ap.coverageRadius << " m | Base latency: " << ap.baseLatencyMs
                 << " ms | Location: (" << ap.x << ", " << ap.y << ")\n";
        }
    }

    void showUsers() const {
        if (users.empty()) {
            cout << "No users configured.\n";
            return;
        }
        cout << left << setw(8) << "User" << setw(12) << "Assigned AP"
             << setw(12) << "Traffic" << setw(10) << "Priority"
             << right << setw(12) << "Demand" << setw(11) << "Signal"
             << "   Location\n";
        cout << string(88, '-') << "\n";
        for (size_t i = 0; i < users.size(); ++i) {
            const User& user = users[i];
            cout << left << setw(8) << user.id << setw(12)
                 << (user.currentAP.empty() ? "Unassigned" : user.currentAP)
                 << setw(12) << user.trafficType << setw(10) << priorityName(user.priority)
                 << right << setw(9) << fixed << setprecision(2) << user.currentDemandMbps << " Mbps"
                 << setw(8) << user.signalDbm << " dBm"
                 << "   (" << user.x << ", " << user.y << ")"
                 << (user.mobile ? " [Mobile]" : "") << "\n";
        }
    }

    
void startSimulation() {
    if (!beginSimulation()) {
        return;
    }

    cout << "\nStarting WiBalance simulation: "
         << finalTick << " ticks.\n";

    while (advanceOneTick()) {
        if (tick == 1 || tick % 10 == 0 || tick == finalTick) {
            int assigned = 0;

            for (size_t i = 0; i < users.size(); ++i) {
                if (!users[i].currentAP.empty()) {
                    ++assigned;
                }
            }

            cout << "Tick " << tick << "/" << finalTick
                 << " completed | users assigned: "
                 << assigned << "/" << users.size() << "\n";
        }
    }

    cout << "\nSimulation finished at tick " << tick << ".\n";
    cout << "\n--- FINAL ACCESS POINT RESULTS ---\n";
    showAPs();
    cout << "\n--- FINAL USER ASSIGNMENTS ---\n";
    showUsers();
}


    void showSummary() const {
        cout << "\n========== WIBALANCE SUMMARY ==========\n";
        cout << "Ticks performed: " << (hasRun ? tick : 0) << "/" << finalTick << "\n";
        cout << "Configured APs: " << accessPoints.size()
             << " | Configured users: " << users.size() << "\n";
        cout << "\n--- Access Point details (last run/current state) ---\n";
        showAPs();
        cout << "\n--- User details (last run/current state) ---\n";
        showUsers();
        cout << "=======================================\n";
    }

public:
    WiBalance()
        : userHash(dsa_hash_create()), assignmentQueue(dsa_queue_create()),
          randomEngine(static_cast<unsigned int>(time(NULL))), tick(0),
          finalTick(DEFAULT_FINAL_TICK), hasRun(false) {}
          
bool beginSimulation() {
    if (accessPoints.empty() || users.empty()) {
        cout << "Add at least one AP and one user before starting.\n";
        return false;
    }

    tick = 0;
    hasRun = false;
    return true;
}

bool advanceOneTick() {
    if (tick >= finalTick) {
        return false;
    }

    updateTrafficDemand();
    globallyRedistributeUsers();
    ++tick;

    if (tick >= finalTick) {
        hasRun = true;
    }

    return true;
}

int getCurrentTick() const {
    return tick;
}

int getTotalTicks() const {
    return finalTick;
}

int getAccessPointCount() const {
    return static_cast<int>(accessPoints.size());
}

int getUserCount() const {
    return static_cast<int>(users.size());
}

double getAPLoad(int index) const {
    if (index < 0 || index >= static_cast<int>(accessPoints.size())) {
        return 0.0;
    }
    return accessPoints[static_cast<size_t>(index)].loadPercent;
}

std::string getAPId(int index) const {
    if (index < 0 || index >= static_cast<int>(accessPoints.size())) {
        return "";
    }
    return accessPoints[static_cast<size_t>(index)].id;
}

int getAPUserCount(int index) const {
    if (index < 0 || index >= static_cast<int>(accessPoints.size())) {
        return 0;
    }
    return static_cast<int>(
        accessPoints[static_cast<size_t>(index)].userIds.size()
    );
}

WiBalanceStatus getStatus() const {
    WiBalanceStatus status;
    status.currentTick = tick;
    status.totalTicks = finalTick;
    status.accessPointCount = static_cast<int>(accessPoints.size());
    status.userCount = static_cast<int>(users.size());
    status.finished = hasRun;
    return status;
}


WiBalanceAPStatus getAPStatus(int index) const {
    WiBalanceAPStatus status;

    if (index < 0 || index >= static_cast<int>(accessPoints.size())) {
        return status;
    }

    const AccessPoint& ap = accessPoints[static_cast<size_t>(index)];

    status.id = ap.id;
    status.loadPercent = ap.loadPercent;
    status.userCount = static_cast<int>(ap.userIds.size());
    status.capacityMbps = ap.capacityMbps;
    status.assignedDemandMbps = ap.assignedDemandMbps;
    status.channelUtilization = ap.channelUtilization;
    status.latencyMs = ap.latencyMs;
    status.packetLossPercent = ap.packetLossPercent;
    status.maxUsers = ap.maxUsers;
    status.status = statusOf(ap);

    return status;
}



WiBalanceSnapshot getSnapshot() const {
    WiBalanceSnapshot snapshot;
    snapshot.status = getStatus();

    for (int i = 0; i < getAccessPointCount(); ++i) {
        snapshot.accessPoints.push_back(getAPStatus(i));
    }

    for (const User& user : users) {
        WiBalanceUserStatus userStatus;
        userStatus.id = user.id;
        userStatus.assignedAP = user.currentAP;
        userStatus.trafficType = user.trafficType;
        userStatus.demandMbps = user.currentDemandMbps;
        userStatus.signalDbm = user.signalDbm;
        userStatus.priority = user.priority;
        userStatus.mobile = user.mobile;

        snapshot.users.push_back(userStatus);
    }

    return snapshot;
}


void initializeDemoScenario() {
    accessPoints.clear();
    users.clear();

    AccessPoint ap1;
    ap1.id = "AP1";
    ap1.capacityMbps = 100.0;
    ap1.maxUsers = 25;
    ap1.coverageRadius = 100.0;
    ap1.baseLatencyMs = 8.0;
    ap1.x = 0.0;
    ap1.y = 0.0;
    accessPoints.push_back(ap1);

    AccessPoint ap2;
    ap2.id = "AP2";
    ap2.capacityMbps = 80.0;
    ap2.maxUsers = 20;
    ap2.coverageRadius = 100.0;
    ap2.baseLatencyMs = 10.0;
    ap2.x = 100.0;
    ap2.y = 0.0;
    accessPoints.push_back(ap2);

    for (int i = 0; i < 10; ++i) {
        User user;
        user.id = "U" + to_string(101 + i);
        user.trafficType = (i % 2 == 0) ? "Video" : "Web";
        user.requestedMbps = 10.0;
        user.minimumMbps = 1.0;
        user.currentDemandMbps = 5.0;
        user.priority = (i % 3 == 0) ? 3 : 2;
        user.mobile = (i % 2 == 0);
        user.x = (i < 5) ? 15.0 : 85.0;
        user.y = (i % 3) * 10.0;
        users.push_back(user);
    }

    rebuildUserHash();

    tick = 0;
    hasRun = false;

    updateTrafficDemand();
    globallyRedistributeUsers();
}

bool configureScenario(int apCount, int userCount, double capacityMbps) {
    if (apCount < 1 || apCount > 10 ||
        userCount < 1 || userCount > 200 ||
        !isfinite(capacityMbps) ||
        capacityMbps < 1.0 || capacityMbps > 10000.0) {
        return false;
    }

    accessPoints.clear();
    users.clear();

    for (int i = 0; i < apCount; ++i) {
        AccessPoint ap;
        ap.id = "AP" + to_string(i + 1);
        ap.capacityMbps = capacityMbps;
        ap.maxUsers = 25;
        ap.coverageRadius = 100.0;
        ap.baseLatencyMs = 8.0 + i;
        ap.x = (apCount == 1) ? 50.0
                              : i * 100.0 / (apCount - 1);
        ap.y = 0.0;

        accessPoints.push_back(ap);
    }

    const string trafficTypes[] = {
        "Video", "Web", "Gaming", "Voice", "Download", "Messaging"
    };

    for (int i = 0; i < userCount; ++i) {
        User user;
        user.id = "U" + to_string(101 + i);
        user.trafficType = trafficTypes[i % 6];
        user.requestedMbps = 10.0;
        user.minimumMbps = 1.0;
        user.currentDemandMbps = 5.0;
        user.priority = (i % 3) + 1;
        user.mobile = (i % 2 == 0);

        // Distribute users across the simulated area.
        user.x = (i % 100) * 100.0 / 99.0;
        user.y = (i / 100) * 10.0;

        users.push_back(user);
    }

    rebuildUserHash();

    tick = 0;
    hasRun = false;

    updateTrafficDemand();
    globallyRedistributeUsers();

    return true;
}



    ~WiBalance() {
        dsa_queue_destroy(assignmentQueue);
        dsa_hash_destroy(userHash);
    }

    void menu() {
        cout << "==============================================\n";
        cout << " WiBalance - C DSA + C++ Classes and Objects\n";
        cout << "==============================================\n";
        cout << "Configure APs/users, then Start runs all " << finalTick << " ticks.\n";
        cout << "This is a software simulation, not a controller for physical Wi-Fi APs.\n";

        while (true) {
            cout << "\n--- MAIN MENU | Last run: "
                 << (hasRun ? to_string(tick) + "/" + to_string(finalTick) : "not run")
                 << " ---\n"
                 << "1. Add Access Point\n"
                 << "2. Remove Access Point\n"
                 << "3. Add User\n"
                 << "4. Remove User\n"
                 << "5. View Access Points\n"
                 << "6. View Users\n"
                 << "7. Start Simulation (run until final tick)\n"
                 << "8. Show Summary\n"
                 << "0. Exit\n";
            int choice = readInt("Choose option: ", 0, 8, -1);
            switch (choice) {
                case 1: addAP(); break;
                case 2: removeAP(); break;
                case 3: addUser(); break;
                case 4: removeUser(); break;
                case 5: showAPs(); break;
                case 6: showUsers(); break;
                case 7: startSimulation(); break;
                case 8: showSummary(); break;
                case 0: cout << "Exiting WiBalance.\n"; return;
                default: cout << "Invalid choice.\n"; break;
            }
        }
    }
};


#ifndef WIBALANCE_HTTP_SERVER
int main() {
    WiBalance application;
    application.menu();
    return 0;
}
#else


string snapshotToJson(const WiBalance& application) {
    WiBalanceSnapshot snapshot = application.getSnapshot();
    const WiBalanceStatus& status = snapshot.status;

    ostringstream json;
    json << fixed << setprecision(2);

    json << "{\"service\":\"WiBalance\",";
    json << "\"status\":\"online\",";
    json << "\"simulationConnected\":true,";
    json << "\"tick\":" << status.currentTick << ",";
    json << "\"totalTicks\":" << status.totalTicks << ",";
    json << "\"accessPointCount\":" << status.accessPointCount << ",";
    json << "\"userCount\":" << status.userCount << ",";
    json << "\"finished\":" << (status.finished ? "true" : "false") << ",";

    json << "\"accessPoints\":[";
    for (size_t i = 0; i < snapshot.accessPoints.size(); ++i) {
        if (i > 0) json << ",";
        const auto& ap = snapshot.accessPoints[i];

        json << "{";
        json << "\"id\":\"" << ap.id << "\",";
        json << "\"loadPercent\":" << ap.loadPercent << ",";
        json << "\"userCount\":" << ap.userCount << ",";
        json << "\"capacityMbps\":" << ap.capacityMbps << ",";
        json << "\"assignedDemandMbps\":" << ap.assignedDemandMbps << ",";
        json << "\"channelUtilization\":" << ap.channelUtilization << ",";
        json << "\"latencyMs\":" << ap.latencyMs << ",";
        json << "\"packetLossPercent\":" << ap.packetLossPercent << ",";
        json << "\"maxUsers\":" << ap.maxUsers << ",";
        json << "\"status\":\"" << ap.status << "\"";
        json << "}";
    }
    json << "],";

    json << "\"users\":[";
    for (size_t i = 0; i < snapshot.users.size(); ++i) {
        if (i > 0) json << ",";
        const auto& user = snapshot.users[i];

        json << "{";
        json << "\"id\":\"" << user.id << "\",";
        json << "\"assignedAP\":\"" << user.assignedAP << "\",";
        json << "\"trafficType\":\"" << user.trafficType << "\",";
        json << "\"demandMbps\":" << user.demandMbps << ",";
        json << "\"signalDbm\":" << user.signalDbm << ",";
        json << "\"priority\":" << user.priority << ",";
        json << "\"mobile\":" << (user.mobile ? "true" : "false");
        json << "}";
    }
    json << "]}";

    return json.str();
}


void runHttpServer(WiBalance& application) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "Winsock startup failed.\n";
        return;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        cerr << "Socket creation failed.\n";
        WSACleanup();
        return;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(8080);
    address.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&address),
              sizeof(address)) == SOCKET_ERROR) {
        cerr << "Could not bind port 8080. Stop server.exe if it is running.\n";
        closesocket(serverSocket);
        WSACleanup();
        return;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        cerr << "Server listen failed.\n";
        closesocket(serverSocket);
        WSACleanup();
        return;
    }

    cout << "WiBalance HTTP server running at http://127.0.0.1:8080\n";

    while (true) {
        SOCKET client = accept(serverSocket, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;

        char request[4096]{};
        int received = recv(client, request, sizeof(request) - 1, 0);
       string body =
    "{\"error\":\"Unsupported endpoint\","
    "\"availableEndpoints\":["
    "\"GET /api/status\","
    "\"POST /api/start\","
    "\"POST /api/step\"]}";

        if (received > 0) {
            string requestText(request, received);
            istringstream requestStream(requestText);
            string method, path;
            requestStream >> method >> path;

            
if (method == "GET" && path == "/api/status") {
    body = snapshotToJson(application);

} else if (method == "POST" && path == "/api/configure") {
   int apCount = 2;
int userCount = 10;
double capacityMbps = 100.0;

size_t bodyStart = requestText.find("\r\n\r\n");

if (bodyStart != string::npos) {
    string jsonBody = requestText.substr(bodyStart + 4);

    size_t apPos = jsonBody.find("\"apCount\"");
    size_t userPos = jsonBody.find("\"userCount\"");
    size_t capacityPos = jsonBody.find("\"capacityMbps\"");

    if (apPos != string::npos)
        apCount = stoi(jsonBody.substr(jsonBody.find(':', apPos) + 1));

    if (userPos != string::npos)
        userCount = stoi(jsonBody.substr(jsonBody.find(':', userPos) + 1));

    if (capacityPos != string::npos)
        capacityMbps = stod(jsonBody.substr(jsonBody.find(':', capacityPos) + 1));
}

bool configured = application.configureScenario(
    apCount, userCount, capacityMbps
);

    body = configured
        ? snapshotToJson(application)
        : "{\"error\":\"Invalid simulation configuration\"}";

} else if (method == "POST" && path == "/api/start") {
    bool started = application.beginSimulation();
    body = started
        ? snapshotToJson(application)
        : "{\"error\":\"Add access points and users first\"}";

} else if (method == "POST" && path == "/api/step") {
    application.advanceOneTick();
    body = snapshotToJson(application);
}

        }

        string response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type\r\n"
            "Connection: close\r\n"
            "Content-Length: " + to_string(body.size()) + "\r\n\r\n" + body;

        send(client, response.c_str(), static_cast<int>(response.size()), 0);
        shutdown(client, SD_SEND);
        closesocket(client);
    }

    closesocket(serverSocket);
    WSACleanup();
}

int main() {
    WiBalance application;
    application.initializeDemoScenario();
    runHttpServer(application);
    return 0;
}
#endif

