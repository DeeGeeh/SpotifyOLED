#ifndef WEBSERVER_HANDLER_H
#define WEBDRIVER_HANDLER_H

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

private:
    WebServer server;
    String authCode;
    bool authCodeReceived;

    void setupRoutes();
    void handleRoot();
    void handleCallback();
    void handleNotFound();
};

#endif // WEBSERVER_HANDLER_H