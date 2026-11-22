


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

