

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

