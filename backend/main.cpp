



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

