#ifndef WEBSERVERHANDLER_HH
#define WEBSERVERHANDLER_HH

#include <WebServer.h>
#include <WiFi.h>

class WebServerHandler
{
public:
    WebServerHandler();
    void begin();
    void handleClient();
    String getAuthorizationCode();
    bool isAuthorizationReceived();
    void clearAuthorizationReceived();

private:
    WebServer server;
    String authCode;
    bool authCodeReceived;

    void setupRoutes();
    void handleRoot();
    void handleCallback();
    void handleNotFound();
};

#endif // WEBSERVERHANDLER_HH