#include "WebServerHandler.hh"

WebServerHandler::WebServerHandler() 
    : server(8080), authCodeReceived(false) {
}

void WebServerHandler::begin() {
    setupRoutes();
    server.begin();
    Serial.println("Server started");
    Serial.println("IP Address:");
    Serial.println(WiFi.localIP());
}

void WebServerHandler::setupRoutes() {
    server.on("/", HTTP_GET, [this]() { handleRoot(); });
    server.on("/callback", HTTP_GET, [this]() { handleCallback(); });
    server.onNotFound([this]() { handleNotFound(); });
}

void WebServerHandler::handleRoot() {
        Serial.println("Handling root route");

    // Simple HTML page for Spotify Authorization
    String html = R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Spotify Authorization</title>
    <style>
        body { 
            font-family: Arial, sans-serif; 
            display: flex; 
            justify-content: center; 
            align-items: center; 
            height: 100vh; 
            margin: 0; 
            background-color: #1DB954;
            color: white;
            text-align: center;
        }
        .container {
            background-color: rgba(0,0,0,0.7);
            padding: 2rem;
            border-radius: 10px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Spotify Authorization</h1>
        <p>Please complete the authorization process on your device.</p>
        <p>This page confirms that you've authorized the application.</p>
    </div>
</body>
</html>
)";
    server.send(200, "text/html", html);
        Serial.println("Root route response sent");

}

void WebServerHandler::handleCallback() {
        Serial.println("Handling callback route");

    if (server.hasArg("code")) {
        authCode = server.arg("code");
        authCodeReceived = true;
        
        // Send a simple confirmation page
        String html = R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Authorization Successful</title>
    <style>
        body { 
            font-family: Arial, sans-serif; 
            display: flex; 
            justify-content: center; 
            align-items: center; 
            height: 100vh; 
            margin: 0; 
            background-color: #1DB954;
            color: white;
            text-align: center;
        }
        .container {
            background-color: rgba(0,0,0,0.7);
            padding: 2rem;
            border-radius: 10px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Authorization Successful!</h1>
        <p>You can now close this window.</p>
    </div>
</body>
</html>
)";
        server.send(200, "text/html", html);
        
        Serial.println("Authorization code received: " + authCode);
    } else {
        String html = R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Authorization Failed</title>
    <style>
        body { 
            font-family: Arial, sans-serif; 
            display: flex; 
            justify-content: center; 
            align-items: center; 
            height: 100vh; 
            margin: 0; 
            background-color: #FF0000;
            color: white;
            text-align: center;
        }
        .container {
            background-color: rgba(0,0,0,0.7);
            padding: 2rem;
            border-radius: 10px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Authorization Failed</h1>
        <p>No authorization code found.</p>
    </div>
</body>
</html>
)";
        server.send(400, "text/html", html);
    }
}

void WebServerHandler::handleNotFound() {
        Serial.println("Handling not found route");

    String html = R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Page Not Found</title>
    <style>
        body { 
            font-family: Arial, sans-serif; 
            display: flex; 
            justify-content: center; 
            align-items: center; 
            height: 100vh; 
            margin: 0; 
            background-color: #FF0000;
            color: white;
            text-align: center;
        }
        .container {
            background-color: rgba(0,0,0,0.7);
            padding: 2rem;
            border-radius: 10px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>404 - Page Not Found</h1>
        <p>The page you are looking for does not exist.</p>
    </div>
</body>
</html>
)";
    server.send(404, "text/html", html);
}

void WebServerHandler::handleClient() {
    server.handleClient();
}

String WebServerHandler::getAuthorizationCode() {
    return authCode;
}

bool WebServerHandler::isAuthorizationReceived() {
    return authCodeReceived;
}