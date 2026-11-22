



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

