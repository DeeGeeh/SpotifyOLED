#include <Arduino.h>
#include <WebServer.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha256.h>

#include "secrets.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

WebServer server(80);
String serverHeader;
unsigned long currentTime = millis();
unsigned long previousTime = 0;
// 2000ms = 2s
const long timeoutTime = 2000;

String clientId = "cd421c13a2db4851a6b45d4a07f47183";
String redirectUri = "http%3A%2F%2Fhttpbin.org%2Fanything";  // redirect link -> http://httpbin.org/anything
String authCode = "";
String accessToken = "";

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

typedef struct s_trackInfo  {
  String artistName;
  String trackName;
} trackInfo;

void handleRoot() {
    server.send(200, "text/plain", "Server is running!");
}

void handleCallback() {
    if (server.hasArg("code")) {
        authCode = server.arg("code");  // Capture the authorization code
        server.send(200, "text/plain", "Authorization successful! You can close this window.");
        Serial.println("Authorization code received: " + authCode);
    } else {
        server.send(400, "text/plain", "Authorization code not found.");
    }
}

String generateCodeVerifier() {
    String codeVerifier = "";
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~";

    for (int i = 0; i < 128; i++) {  // The code verifier should be between 43 and 128 characters
        codeVerifier += charset[random(0, sizeof(charset) - 1)];
    }
    return codeVerifier;
}

String generateCodeChallenge(String codeVerifier) {
    unsigned char sha256Hash[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, (const unsigned char *)codeVerifier.c_str(), codeVerifier.length());
    mbedtls_sha256_finish(&ctx, sha256Hash);
    mbedtls_sha256_free(&ctx);

    // Base64 encode the hash
    unsigned char base64Hash[45];  // Base64 encoding adds 33% overhead, so 32 bytes becomes 44, plus a null terminator
    size_t base64Len;
    mbedtls_base64_encode(base64Hash, sizeof(base64Hash), &base64Len, sha256Hash, sizeof(sha256Hash));

    // Convert to String and replace characters to make it URL-safe
    String codeChallenge = (char*)base64Hash;
    codeChallenge.replace("+", "-");
    codeChallenge.replace("/", "_");
    codeChallenge.replace("=", "");

    return codeChallenge;
}

String generateAuthURL(String clientId, String redirectUri, String codeChallenge) {
    String authURL = "https://accounts.spotify.com/authorize?";
    authURL += "response_type=code";
    authURL += "&client_id=" + clientId;
    authURL += "&redirect_uri=" + redirectUri;
    authURL += "&code_challenge_method=S256";
    authURL += "&code_challenge=" + codeChallenge;
    authURL += "&scope=user-read-playback-state"; // Add required scopes here
    return authURL;
}

void startServer() {
    Serial.println("");
    Serial.println("IP Address: ");
    Serial.println(WiFi.localIP());
    // Set up URL routes
    server.on("/", handleRoot);  // Handle the root path
    server.on("/callback", handleCallback);  // Handle the callback path
    server.begin();
    Serial.println("Server started");
}

bool connectWIFI(const char* ssid, const char* pass) {
    display.clearDisplay();
    display.println("Connecting...");
    display.display();
    
    WiFi.begin(ssid, pass);
    unsigned long startAttemptTime = millis();

    // Wait for connection with a timeout
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) {
        delay(500); // Poll every 500ms
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("Successfully connected to network: ");
        Serial.println(WiFi.SSID());
        return true;
    } else {
        Serial.println("Failed to connect to a WiFi network.");
        return false;
    }
}


void exchangeCodeForToken(String authCode, String codeVerifier, String clientId, String redirectUri) {
    HTTPClient http;
    http.begin("https://accounts.spotify.com/api/token");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    // Construct POST data
    String postData = "client_id=" + clientId +
                      "&grant_type=authorization_code" +
                      "&code=" + authCode +
                      "&redirect_uri=" + redirectUri +
                      "&code_verifier=" + codeVerifier;

    Serial.println("Sending POST request...");
    Serial.println(postData);

    // Make the HTTP request
    int httpResponseCode = http.POST(postData);
    String response = http.getString();

    if (httpResponseCode == 200) {
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, response);

        if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.c_str());
            return;
        }

        accessToken = doc["access_token"].as<String>();
        String tokenType = doc["token_type"].as<String>();
        int expiresIn = doc["expires_in"].as<int>();

        Serial.println("Access Token retrieved successfully.");
        Serial.println("Token Type: " + tokenType);
        Serial.println("Expires In: " + String(expiresIn));

    } else {
        Serial.printf("Error in HTTP request, code: %d\n", httpResponseCode);
        Serial.println("Response: " + response);
    }

    http.end();
}


void setupOLED() {
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  display.clearDisplay();
}

String fetchCurrentTrack() {

    if (accessToken == "") {
        return "No Access Token";
    }

    HTTPClient http;
    http.begin("https://api.spotify.com/v1/me/player/currently-playing");
    http.addHeader("Authorization", "Bearer " + accessToken);

    int httpResponseCode = http.GET();
    String response = http.getString();

    if (httpResponseCode == 200) {
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, response);

        if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.c_str());
            return "Error";
        }

        String trackName = doc["item"]["name"].as<String>();
        String artistName = doc["item"]["artists"][0]["name"].as<String>();
        return artistName + "\n" + trackName;

    } else {
        Serial.printf("Error in HTTP request, code: %d\n", httpResponseCode);
        return "No Track Playing";
    }

    http.end();
}

void displayCurrentTrack() {
    String currentTrack = fetchCurrentTrack();
    display.clearDisplay();
    display.setCursor(0, 10);
    display.println(currentTrack);  // Display first part of the track
    display.display();
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting device...");

    // Check display init
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;);
    }

    setupOLED();

    // Connect to wifi
    Serial.println("Waiting for Wifi to connect...");
    if (!connectWIFI(SECRET_SSID, SECRET_PASS)) {
        Serial.println("Wifi connection failed.");
        display.clearDisplay();
        display.println("Connection fail.");
        display.display();
        for (;;);
    } 
    else {
        display.clearDisplay();
        display.println("Connected to ");
        display.println(String(WiFi.SSID()));
        display.display();
    }
    startServer();

    String codeVerifier = generateCodeVerifier();
    String codeChallenge = generateCodeChallenge(codeVerifier);
    String authURL = generateAuthURL(clientId, redirectUri, codeChallenge);
    Serial.println("Visit this URL to authorize: " + authURL);

    Serial.println("Enter the authorization code: ");
    while (Serial.available() == 0) {} // Wait for user input
    String authCode = Serial.readStringUntil('\n');
    
    // Display the URL on the OLED screen as well 
    display.clearDisplay();
    display.println("Open URL:");
    display.println(authURL.substring(0, SCREEN_WIDTH / 6));  // Print part of the URL
    display.display();

    // Wait for the user to visit the URL and provide the authorization code manually
    // Replace this part with actual code to input the authorization code
    if (authCode.length() > 0) {
        exchangeCodeForToken(authCode, codeVerifier, clientId, redirectUri);
    }
    display.clearDisplay();
    displayCurrentTrack();
}

void loop() {
    // put your main code here, to run repeatedly:
    server.handleClient();
    if (millis() - previousTime >= 5000) {  // Update every 5 seconds
    displayCurrentTrack();
    previousTime = millis();
    }

}
