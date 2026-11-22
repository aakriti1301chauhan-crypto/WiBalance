#include "dsa.h"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>
using namespace std;

constexpr int DEFAULT_FINAL_TICK = 50;
constexpr double CONGESTION_LIMIT = 80.0;

class User {
public:
    string id, trafficType, currentAP;
    double requestedMbps = 5, minimumMbps = 1, currentDemandMbps = 1;
    bool mobile = false;
};

class AccessPoint {
public:
    string id;
    double capacityMbps = 100, baseLatencyMs = 8, assignedDemandMbps = 0;
    double loadPercent = 0, channelUtilization = 0, latencyMs = 8, packetLossPercent = 0;
    int maxUsers = 25;
    vector<string> userIds;
};

class WiBalance {
    vector<AccessPoint> accessPoints;
    vector<User> users;
    DSAHashHandle userHash = dsa_hash_create();
    DSAQueueHandle assignmentQueue = dsa_queue_create();
    mt19937 rng{static_cast<unsigned>(time(nullptr))};
    int tick = 0, finalTick = DEFAULT_FINAL_TICK;
    bool hasRun = false;

    static bool yes(const string& s) {
        return s == "y" || s == "Y" || s == "yes" || s == "YES";
    }

    template<class T>
    T readNumber(const string& prompt, T fallback, T minimum, T maximum,
                 const string& error) {
        for (;;) {
            cout << prompt;
            string line;
            getline(cin, line);
            if (line.empty()) return fallback;
            stringstream ss(line);
            T value; char extra;
            if ((ss >> value) && !(ss >> extra) && isfinite(static_cast<double>(value))
                && value >= minimum && value <= maximum) return value;
            cout << error << '\n';
        }
    }

    double readPositiveDouble(const string& p, double d) {
        return readNumber<double>(p, d, 0.000001, 1e100, "Enter a positive number.");
    }
    int readInt(const string& p, int lo, int hi, int d) {
        return readNumber<int>(p, d, lo, hi, "Enter a number from " + to_string(lo) + " to " + to_string(hi) + ".");
    }
    string readLine(const string& prompt, const string& fallback = "") {
        cout << prompt;
        string s; getline(cin, s);
        return s.empty() ? fallback : s;
    }

    string nextId(bool ap) const {
        int largest = ap ? 0 : 100;
        if (ap) {
            for (const auto& a : accessPoints)
                if (a.id.rfind("AP", 0) == 0) try { largest = max(largest, stoi(a.id.substr(2))); } catch (...) {}
            return "AP" + to_string(largest + 1);
        }
        for (const auto& u : users)
            if (!u.id.empty() && u.id[0] == 'U') try { largest = max(largest, stoi(u.id.substr(1))); } catch (...) {}
        return "U" + to_string(largest + 1);
    }

    int findAPIndex(const string& id) const {
        vector<string> ids;
        vector<const char*> ptrs;
        for (const auto& ap : accessPoints) ids.push_back(ap.id);
        for (const auto& s : ids) ptrs.push_back(s.c_str());
        return dsa_linear_search_strings(ptrs.data(), static_cast<int>(ptrs.size()), id.c_str());
    }

    void rebuildUserHash() {
        dsa_hash_clear(userHash);
        for (int i = 0; i < static_cast<int>(users.size()); ++i)
            dsa_hash_put(userHash, users[i].id.c_str(), i);
    }

    int findUserIndex(const string& id) const {
        int i;
        return dsa_hash_get(userHash, id.c_str(), &i) && i >= 0 &&
               i < static_cast<int>(users.size()) && users[i].id == id ? i : -1;
    }

    double demandForType(const User& u) const {
        double factor = 1;
        if (u.trafficType == "Voice") factor = .14;
        else if (u.trafficType == "Messaging") factor = .06;
        else if (u.trafficType == "Web browsing") factor = .30;
        else if (u.trafficType == "Gaming") factor = .22;
        else if (u.trafficType == "Video streaming") factor = .78;
        else if (u.trafficType == "File downloads") factor = .92;
        return min(u.requestedMbps, max(u.minimumMbps, u.requestedMbps * factor));
    }

    void updateTrafficDemand() {
        uniform_real_distribution<double> variation(.85, 1.15);
        for (auto& u : users)
            u.currentDemandMbps = min(u.requestedMbps,
                max(u.minimumMbps, demandForType(u) * variation(rng)));
    }

    double candidateScore(const User& u, const AccessPoint& ap) const {
        double load = ap.capacityMbps > 0
            ? (ap.assignedDemandMbps + u.currentDemandMbps) / ap.capacityMbps * 100 : 1000;
        double clients = ap.userIds.size() + 1.0;
        double clientPct = ap.maxUsers > 0 ? clients / ap.maxUsers * 100 : 1000;
        double latency = ap.baseLatencyMs + (load > 40 ? (load - 40) * .5 : 0);
        double projectedLoad = load / 100;
        double projectedClients = clientPct / 100;
        double latencyScore = latency / 100;
        double overload = max(0.0, projectedLoad - 1) * 3
            + max(0.0, projectedClients - 1) * 2;
        return .62 * projectedLoad + .20 * projectedClients
            + .12 * latencyScore + .06 * overload;
    }

    int bestAPFor(const User& u) const {
        int best = -1; double score = 1e9;
        for (int i = 0; i < static_cast<int>(accessPoints.size()); ++i) {
            double s = candidateScore(u, accessPoints[i]);
            if (s < score) best = i, score = s;
        }
        return best;
    }

    void clearAssignments() {
        for (auto& ap : accessPoints) ap.userIds.clear(), ap.assignedDemandMbps = 0;
        for (auto& u : users) u.currentAP.clear();
    }

    void recalculateAPMetrics() {
        for (auto& ap : accessPoints) {
            ap.loadPercent = ap.capacityMbps > 0 ? ap.assignedDemandMbps / ap.capacityMbps * 100 : 1000;
            ap.loadPercent = max(0.0, ap.loadPercent);
            ap.channelUtilization = min(100.0, ap.loadPercent * .88);
            ap.latencyMs = ap.baseLatencyMs + (ap.loadPercent > 40 ? (ap.loadPercent - 40) * .5 : 0);
            ap.packetLossPercent = min(20.0, ap.loadPercent > 85 ? (ap.loadPercent - 85) * .2 : 0.0);
        }
    }

    void globallyRedistributeUsers() {
        clearAssignments();
        if (users.empty() || accessPoints.empty()) { recalculateAPMetrics(); return; }
        vector<int> indices(users.size());
        vector<double> scores(users.size());
        for (int i = 0; i < static_cast<int>(users.size()); ++i) {
            indices[i] = i;
            scores[i] = users[i].currentDemandMbps;
        }
        dsa_insertion_sort_indices(indices.data(), scores.data(), static_cast<int>(indices.size()));
        dsa_queue_destroy(assignmentQueue);
        assignmentQueue = dsa_queue_create();
        for (int i : indices) dsa_queue_enqueue(assignmentQueue, users[i].id.c_str());

        char id[64];
        while (dsa_queue_dequeue(assignmentQueue, id, sizeof(id))) {
            int i = findUserIndex(id);
            if (i < 0) continue;
            User& u = users[i];
            int target = bestAPFor(u);
            if (target < 0) continue;
            auto& ap = accessPoints[target];
            u.currentAP = ap.id;
            ap.userIds.push_back(u.id);
            ap.assignedDemandMbps += u.currentDemandMbps;
        }
        recalculateAPMetrics();
    }

    string statusOf(const AccessPoint& ap) const {
        if (ap.loadPercent >= CONGESTION_LIMIT || ap.userIds.size() > static_cast<size_t>(ap.maxUsers)) return "CONGESTED";
        return ap.loadPercent >= 55 ? "MODERATE" : "HEALTHY";
    }
    string readTrafficType() {
        cout << "Traffic type: 1.Voice  2.Messaging  3.Web browsing  4.Gaming  5.Video streaming  6.File downloads\n";
        const char* types[] = {"Voice", "Messaging", "Web browsing", "Gaming", "Video streaming", "File downloads"};
        return types[readInt("Choose traffic type [1-6]: ", 1, 6, 1) - 1];
    }
    void askToRunAfterChange() {
        hasRun = false; tick = 0;
        cout << "\nConfiguration changed. Start simulation now? (y/n): ";
        string s; getline(cin, s);
        if (yes(s)) startSimulation();
    }

    void addAP() {
        AccessPoint ap;
        ap.id = readLine("Access Point ID [blank = auto]: ");
        if (ap.id.empty()) ap.id = nextId(true);
        if (findAPIndex(ap.id) >= 0) { cout << "An AP with that ID already exists.\n"; return; }
        ap.capacityMbps = readPositiveDouble("Total capacity (Mbps) [default 100]: ", 100);
        ap.maxUsers = readInt("Maximum supported users [default 25]: ", 1, 10000, 25);
        ap.baseLatencyMs = readPositiveDouble("Base latency (ms) [default 8]: ", 8);
        accessPoints.push_back(ap);
        cout << "\nAdded " << ap.id << ".\n";
        askToRunAfterChange();
    }

    void removeAP() {
        if (accessPoints.empty()) { cout << "No AP exists to remove.\n"; return; }
        string id = readLine("AP ID to remove: ");
        int i = findAPIndex(id);
        if (i < 0) { cout << "AP not found.\n"; return; }
        accessPoints.erase(accessPoints.begin() + i);
        for (auto& u : users) if (u.currentAP == id) u.currentAP.clear();
        cout << "Removed " << id << ". Users will be redistributed when the simulation starts.\n";
        askToRunAfterChange();
    }

    void addUser() {
        User u;
        u.id = readLine("User ID [blank = auto]: ");
        if (u.id.empty()) u.id = nextId(false);
        if (findUserIndex(u.id) >= 0) { cout << "A user with that ID already exists.\n"; return; }
        u.trafficType = readTrafficType();
        u.requestedMbps = readPositiveDouble("Requested bandwidth (Mbps) [default 5]: ", 5);
        u.minimumMbps = readPositiveDouble("Minimum required bandwidth (Mbps) [default 1]: ", 1);
        if (u.minimumMbps > u.requestedMbps) {
            cout << "Minimum bandwidth cannot exceed requested bandwidth. User not added.\n"; return;
        }
        string mobile = readLine("Is this user mobile? (y/n, default n): ");
        u.mobile = yes(mobile);
        u.currentDemandMbps = demandForType(u);
        users.push_back(u); rebuildUserHash();
        cout << "\nAdded " << u.id << ". The algorithm will choose its AP.\n";
        if (accessPoints.empty()) {
            cout << "No APs exist yet. Add an AP before starting the simulation.\n";
            hasRun = false; tick = 0; return;
        }
        askToRunAfterChange();
    }

    void removeUser() {
        string id = readLine("User ID to remove: ");
        int i = findUserIndex(id);
        if (i < 0) { cout << "User not found.\n"; return; }
        users.erase(users.begin() + i); rebuildUserHash();
        cout << "Removed " << id << ". Remaining users will be redistributed when the simulation starts.\n";
        askToRunAfterChange();
    }

    void showAPs() const {
        if (accessPoints.empty()) { cout << "No access points configured.\n"; return; }
        cout << left << setw(8) << "AP" << right << setw(8) << "Users" << setw(11) << "Load%"
             << setw(12) << "Channel%" << setw(12) << "Latency" << setw(10) << "Loss%"
             << setw(11) << "MaxUsers" << "   Status\n" << string(91, '-') << '\n';
        for (const auto& ap : accessPoints) {
            cout << left << setw(8) << ap.id << right << setw(8) << ap.userIds.size()
                 << setw(10) << fixed << setprecision(1) << ap.loadPercent << "%"
                 << setw(11) << ap.channelUtilization << "%" << setw(9) << ap.latencyMs << " ms"
                 << setw(9) << ap.packetLossPercent << "%" << setw(11) << ap.maxUsers
                 << "   " << statusOf(ap) << "\n  Capacity: " << ap.capacityMbps
                 << " Mbps | Base latency: " << ap.baseLatencyMs << " ms\n";
        }
    }

    void showUsers() const {
        if (users.empty()) { cout << "No users configured.\n"; return; }
        cout << left << setw(8) << "User" << setw(12) << "Assigned AP" << setw(12) << "Traffic"
             << right << setw(12) << "Demand"
             << "   Status\n" << string(75, '-') << '\n';
        for (const auto& u : users)
            cout << left << setw(8) << u.id << setw(12) << (u.currentAP.empty() ? "Unassigned" : u.currentAP)
                 << setw(12) << u.trafficType
                 << right << setw(9) << fixed << setprecision(2) << u.currentDemandMbps << " Mbps"
                 << setw(12) << (u.currentAP.empty() ? "Unassigned" : "Assigned")
                 << (u.mobile ? " [Mobile]" : "") << '\n';
    }

    void startSimulation() {
        if (accessPoints.empty()) { cout << "Add at least one access point before starting.\n"; return; }
        if (users.empty()) { cout << "Add at least one user before starting.\n"; return; }
        tick = 0; hasRun = false;
        cout << "\nStarting WiBalance simulation: " << finalTick << " ticks.\n";
        for (int t = 1; t <= finalTick; ++t) {
            tick = t; updateTrafficDemand(); globallyRedistributeUsers();
            if (t == 1 || t % 10 == 0 || t == finalTick) {
                int assigned = 0;
                for (const auto& u : users) if (!u.currentAP.empty()) ++assigned;
                cout << "Tick " << t << "/" << finalTick << " completed | users assigned: "
                     << assigned << "/" << users.size() << '\n';
            }
        }
        hasRun = true;
        cout << "\nSimulation finished at tick " << finalTick
             << ".\n\n--- FINAL ACCESS POINT RESULTS ---\n";
        showAPs();
        cout << "\n--- FINAL USER ASSIGNMENTS ---\n";
        showUsers();
    }

    void showSummary() const {
        cout << "\n========== WIBALANCE SUMMARY ==========\nTicks performed: "
             << (hasRun ? tick : 0) << "/" << finalTick << "\nConfigured APs: "
             << accessPoints.size() << " | Configured users: " << users.size()
             << "\n\n--- Access Point details (last run/current state) ---\n";
        showAPs();
        cout << "\n--- User details (last run/current state) ---\n";
        showUsers();
        cout << "=======================================\n";
    }

    void loadSampleData() {
        uniform_real_distribution<double> bandwidth(5, 15);
        uniform_int_distribution<int> type(0, 5), mobile(0, 1);
        const char* types[] = {"Voice", "Messaging", "Web browsing", "Gaming", "Video streaming", "File downloads"};
        for (int i = 1; i <= 4; ++i) {
            AccessPoint ap; ap.id = "AP" + to_string(i);
            ap.maxUsers = 8; accessPoints.push_back(ap);
        }
        for (int i = 101; i <= 112; ++i) {
            User u; u.id = "U" + to_string(i); u.trafficType = types[type(rng)];
            u.requestedMbps = bandwidth(rng);
            u.mobile = mobile(rng);
            u.currentDemandMbps = demandForType(u); users.push_back(u);
        }
        rebuildUserHash();
    }

public:
    WiBalance() { loadSampleData(); }
    ~WiBalance() { dsa_queue_destroy(assignmentQueue); dsa_hash_destroy(userHash); }

    void menu() {
        cout << "==============================================\n"
             << " WiBalance - C DSA + C++ Classes and Objects\n"
             << "==============================================\nConfigure APs/users, then Start runs all "
             << finalTick << " ticks.\nThis is a software simulation, not a controller for physical Wi-Fi APs.\n";
        for (;;) {
            cout << "\n--- MAIN MENU | Last run: "
                 << (hasRun ? to_string(tick) + "/" + to_string(finalTick) : "not run")
                 << " ---\n1. Add Access Point\n2. Remove Access Point\n3. Add User\n"
                    "4. Remove User\n5. View Access Points\n6. View Users\n"
                    "7. Start Simulation (run until final tick)\n8. Show Summary\n0. Exit\n";
            switch (readInt("Choose option: ", 0, 8, -1)) {
                case 1: addAP(); break;
                case 2: removeAP(); break;
                case 3: addUser(); break;
                case 4: removeUser(); break;
                case 5: showAPs(); break;
                case 6: showUsers(); break;
                case 7: startSimulation(); break;
                case 8: showSummary(); break;
                case 0: cout << "Exiting WiBalance.\n"; return;
                default: cout << "Invalid choice.\n";
            }
        }
    }
};

int main() {
    WiBalance application;
    application.menu();
    return 0;
}
