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
#include <qrcode_gen.h>

#include "secrets.h"
#include "WebServerHandler.hh"


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

WebServerHandler webServerHandler;

unsigned long currentTime = millis();
unsigned long previousTime = 0;
const long timeoutTime = 2000;

String redirectUri;
String authCode = "";
String accessToken = "";

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

typedef struct s_trackInfo  {
  String artistName;
  String trackName;
} trackInfo;

String generateCodeVerifier() 
{
    String codeVerifier = "";
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~";

    for (uint8_t i = 0; i < 128; i++) {  // The code verifier should be between 43 and 128 characters
        codeVerifier += charset[random(0, sizeof(charset) - 1)];
    }
    return codeVerifier;
}

String generateCodeChallenge(String codeVerifier) 
{
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

String generateAuthURL(String clientId, String redirectUri, String codeChallenge) 
{
    String authURL = "https://accounts.spotify.com/authorize?";
    authURL += "response_type=code";
    authURL += "&client_id=" + clientId;
    authURL += "&redirect_uri=" + redirectUri;
    authURL += "&code_challenge_method=S256";
    authURL += "&code_challenge=" + codeChallenge;
    authURL += "&scope=user-read-playback-state"; // Add required scopes here
    return authURL;
}

bool connectWIFI(const char* ssid, const char* pass)
{
    display.clearDisplay();
    display.println("Connecting...");
    display.display();
    
    WiFi.begin(ssid, pass);
    unsigned long startAttemptTime = millis();
    unsigned int timeout = 15000;

    // Wait for connection with a timeout
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeout) {
        delay(500);
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
    unsigned int httpResponseCode = http.POST(postData);
    String response = http.getString();

    if (httpResponseCode == 200) {
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, response);

        if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.c_str());
            return;
        }

        accessToken = doc["access_token"].as<String>();
        String tokenType = doc["token_type"].as<String>();
        unsigned int expiresIn = doc["expires_in"].as<int>();

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
    if (accessToken.isEmpty()) {
        return "No Access Token";
    }

    HTTPClient http;
    http.begin("https://api.spotify.com/v1/me/player/currently-playing");
    http.addHeader("Authorization", "Bearer " + accessToken);

    unsigned int httpResponseCode = http.GET();
    String response = http.getString();

    if (httpResponseCode == 200) {
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, response);

        if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.c_str());
            return "Error";
        }

        const char* trackName = doc["item"]["name"] | "Unknown Track";
        const char* artistName = doc["item"]["artists"][0]["name"] | "Unknown Artist";
        
        return String(artistName) + "\n" + String(trackName);
    } else {
        Serial.printf("HTTP Error: %d\n", httpResponseCode);
        return "No Track Playing";
    }
}

void displayCurrentTrack() {
    String currentTrack = fetchCurrentTrack();
    display.clearDisplay();
    display.setCursor(0, 10);
    display.println(currentTrack);  // Display first part of the track
    display.display();
}

void printQRCode(String url) {
    // The structure to manage the QR code
    QRCode qrcode;

    // Allocate a chunk of memory to store the QR code
    uint8_t qrcodeBytes[qrcode_getBufferSize(11)];
        
    // Initialize QR code with the URL
    qrcode_initText(&qrcode, qrcodeBytes, 11, ECC_LOW, url.c_str());

    // Clear the display
    display.clearDisplay();
    display.fillScreen(WHITE);

    // Calculate the size of each QR code module (pixel)
    // This ensures the QR code fits on the screen
    unsigned int moduleSize = min(SCREEN_WIDTH / qrcode.size, SCREEN_HEIGHT / qrcode.size);

    // Calculate offsets to center the QR code
    uint8_t offsetX = (SCREEN_WIDTH - (qrcode.size * moduleSize)) / 2;
    uint8_t offsetY = (SCREEN_HEIGHT - (qrcode.size * moduleSize)) / 2;

    // Render the QR code
    for (uint8_t y = 0; y < qrcode.size; y++) {
        for (uint8_t x = 0; x < qrcode.size; x++) {
            // Check if the module is black
            if (qrcode_getModule(&qrcode, x, y)) {
                // Draw a filled square for black modules
                display.fillRect(
                    offsetX + x * moduleSize, 
                    offsetY + y * moduleSize, 
                    moduleSize, 
                    moduleSize, 
                    BLACK
                );
            }
        }
    }

    // Update the display87
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

    // Start web server
    webServerHandler.begin();

    IPAddress ip = WiFi.localIP();
    redirectUri = "http%3A%2F%2F" + ip.toString();
    redirectUri += "%2Fcallback";

    String codeVerifier = generateCodeVerifier();
    String codeChallenge = generateCodeChallenge(codeVerifier);
    String authURL = generateAuthURL(SECRET_CLIENT_ID, redirectUri, codeChallenge);

    printQRCode(authURL);
    Serial.println("Visit this URL to authorize: " + authURL);
    Serial.println("OR");
    Serial.println("Scan QR code to authorize.");


     // Wait for authorization
    while (authCode.length() == 0) {
        webServerHandler.handleClient();
        if (webServerHandler.isAuthorizationReceived()) {
            authCode = webServerHandler.getAuthorizationCode();
            break;
        }
        delay(100);  // Prevent watchdog reset
    }

    // Only proceed once we have the auth code
    if (authCode.length() > 0) {
        exchangeCodeForToken(authCode, codeVerifier, SECRET_CLIENT_ID, redirectUri);
    }
    display.clearDisplay();
    displayCurrentTrack();
}

void loop() {
    // Only update display if we're authenticated
    if (accessToken.length() > 0) {
        webServerHandler.handleClient();
        
        static unsigned long previousTime = 0;
        if (millis() - previousTime >= 5000) {  
            displayCurrentTrack();
            previousTime = millis();
        }
    }
}
