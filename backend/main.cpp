




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

