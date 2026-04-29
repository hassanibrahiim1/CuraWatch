#include "AuthenticationManager.h"
#include <Arduino.h>

extern String closestKnownAPName;

// Storage configuration
const char* AuthenticationManager::PREF_NAMESPACE = "cura";  // Shortened namespace
const char* AuthenticationManager::KEY_EMAIL = "email";
const char* AuthenticationManager::KEY_PASSWORD = "password";
const char* AuthenticationManager::KEY_TOKEN = "token";

/**
 * Constructor - Initialize preferences and load stored token
 */
AuthenticationManager::AuthenticationManager() 
  : tokenSavedTime(0), lastError("") {
  
  Serial.println("[AUTH] Constructor: Initializing preferences...");
  Serial.print("[AUTH] Using namespace: ");
  Serial.println(PREF_NAMESPACE);
  
  // Open preferences namespace (first param: namespace, second: read-only mode)
  bool beginResult = preferences.begin(PREF_NAMESPACE, false);
  Serial.print("[AUTH] preferences.begin() result: ");
  Serial.println(beginResult ? "SUCCESS" : "FAILED");
  
  if (!beginResult) {
    Serial.println("[AUTH] ERROR: Failed to initialize preferences!");
    lastError = "Preferences initialization failed";
  } else {
    Serial.println("[AUTH] ✓ Preferences initialized successfully");
  }
  
  // Debug: Check what keys exist
  Serial.println("[AUTH] DEBUG: Checking existing preferences keys...");
  if (preferences.isKey(KEY_EMAIL)) {
    Serial.println("[AUTH] DEBUG: Email key exists");
  } else {
    Serial.println("[AUTH] DEBUG: Email key does not exist");
  }
  if (preferences.isKey(KEY_PASSWORD)) {
    Serial.println("[AUTH] DEBUG: Password key exists");
  } else {
    Serial.println("[AUTH] DEBUG: Password key does not exist");
  }
  if (preferences.isKey(KEY_TOKEN)) {
    Serial.println("[AUTH] DEBUG: Token key exists");
  } else {
    Serial.println("[AUTH] DEBUG: Token key does not exist");
  }
  
  // Try to load existing token
  currentToken = loadToken();
  
  if (!currentToken.isEmpty()) {
    Serial.println("[AUTH] ✓ Loaded existing token from storage");
  } else {
    Serial.println("[AUTH] No token found in storage");
  }
}

/**
 * Debug method to check preferences state after Serial is initialized
 */
void AuthenticationManager::debugPreferencesState() {
  Serial.println("[AUTH] DEBUG: Checking preferences state...");
  
  // Check if preferences is initialized
  Serial.print("[AUTH] DEBUG: Preferences initialized: ");
  Serial.println(preferences.isKey(KEY_EMAIL) || preferences.isKey(KEY_PASSWORD) || preferences.isKey(KEY_TOKEN) ? "YES" : "NO");
  
  // Check what keys exist
  if (preferences.isKey(KEY_EMAIL)) {
    Serial.println("[AUTH] DEBUG: Email key exists");
    String email = preferences.getString(KEY_EMAIL, "");
    Serial.print("[AUTH] DEBUG: Email value: ");
    Serial.println(email);
  } else {
    Serial.println("[AUTH] DEBUG: Email key does not exist");
  }
  
  if (preferences.isKey(KEY_PASSWORD)) {
    Serial.println("[AUTH] DEBUG: Password key exists");
    String password = preferences.getString(KEY_PASSWORD, "");
    Serial.print("[AUTH] DEBUG: Password length: ");
    Serial.println(password.length());
  } else {
    Serial.println("[AUTH] DEBUG: Password key does not exist");
  }
  
  if (preferences.isKey(KEY_TOKEN)) {
    Serial.println("[AUTH] DEBUG: Token key exists");
    String token = preferences.getString(KEY_TOKEN, "");
    Serial.print("[AUTH] DEBUG: Token length: ");
    Serial.println(token.length());
  } else {
    Serial.println("[AUTH] DEBUG: Token key does not exist");
  }
}

/**
 * Save email and password to persistent storage
 */
void AuthenticationManager::saveCredentials(const char* email, const char* password) {
  Serial.println("[AUTH] DEBUG: saveCredentials called");
  
  if (!email || !password) {
    lastError = "Invalid credentials provided";
    Serial.println("[AUTH] ERROR: Invalid credentials");
    return;
  }
  
  if (strlen(email) > 128 || strlen(password) > 128) {
    lastError = "Credentials too long";
    Serial.println("[AUTH] ERROR: Credentials too long");
    return;
  }
  
  Serial.println("[AUTH] DEBUG: About to save to preferences");
  preferences.putString(KEY_EMAIL, String(email));
  preferences.putString(KEY_PASSWORD, String(password));
  
  // Debug: Verify the save worked
  String testEmail = preferences.getString(KEY_EMAIL, "");
  String testPassword = preferences.getString(KEY_PASSWORD, "");
  
  Serial.print("[AUTH] DEBUG: Verification - testEmail length: ");
  Serial.println(testEmail.length());
  Serial.print("[AUTH] DEBUG: Verification - testPassword length: ");
  Serial.println(testPassword.length());
  
  if (testEmail == email && testPassword == password) {
    Serial.print("[AUTH] ✓ Saved credentials for email: ");
    Serial.println(email);
  } else {
    Serial.println("[AUTH] ⚠ Credentials save verification failed!");
    Serial.print("[AUTH] DEBUG: Expected email: ");
    Serial.println(email);
    Serial.print("[AUTH] DEBUG: Saved email: ");
    Serial.println(testEmail);
  }
  
  lastError = "";
}

/**
 * Load email and password from persistent storage
 */
bool AuthenticationManager::loadCredentials(String& email, String& password) {
  Serial.println("[AUTH] Attempting to load credentials...");
  
  // Debug: Check if preferences is initialized
  if (!preferences.isKey(KEY_EMAIL)) {
    Serial.println("[AUTH] DEBUG: No email key found in preferences");
  } else {
    Serial.println("[AUTH] DEBUG: Email key exists in preferences");
  }
  
  email = preferences.getString(KEY_EMAIL, "");
  password = preferences.getString(KEY_PASSWORD, "");
  
  Serial.print("[AUTH] DEBUG: Loaded email length: ");
  Serial.println(email.length());
  Serial.print("[AUTH] DEBUG: Loaded password length: ");
  Serial.println(password.length());
  
  if (email.isEmpty() || password.isEmpty()) {
    lastError = "No credentials found in storage";
    Serial.println("[AUTH] No credentials found in storage");
    return false;
  }
  
  Serial.print("[AUTH] ✓ Loaded credentials for email: ");
  Serial.println(email);
  lastError = "";
  return true;
}

/**
 * Clear stored credentials
 */
void AuthenticationManager::clearCredentials() {
  preferences.remove(KEY_EMAIL);
  preferences.remove(KEY_PASSWORD);
  Serial.println("[AUTH] ✓ Cleared stored credentials");
  lastError = "";
}

/**
 * Save JWT token to persistent storage
 */
void AuthenticationManager::saveToken(const char* token) {
  if (!token) {
    lastError = "Invalid token provided";
    Serial.println("[AUTH] ERROR: Invalid token");
    return;
  }
  
  if (strlen(token) > 2048) {
    lastError = "Token too long";
    Serial.println("[AUTH] ERROR: Token too long");
    return;
  }
  
  preferences.putString(KEY_TOKEN, String(token));
  currentToken = String(token);
  tokenSavedTime = millis();
  
  Serial.print("[AUTH] ✓ Saved token: ");
  Serial.println(String(token).substring(0, 20) + "...");
  lastError = "";
}

/**
 * Load JWT token from persistent storage
 */
String AuthenticationManager::loadToken() {
  String token = preferences.getString(KEY_TOKEN, "");
  
  if (!token.isEmpty()) {
    tokenSavedTime = millis();
    Serial.print("[AUTH] ✓ Loaded token from storage: ");
    Serial.println(token.substring(0, 20) + "...");
    lastError = "";
    return token;
  }
  
  return "";
}

/**
 * Clear stored token
 */
void AuthenticationManager::clearToken() {
  preferences.remove(KEY_TOKEN);
  currentToken = "";
  tokenSavedTime = 0;
  Serial.println("[AUTH] ✓ Cleared stored token");
  lastError = "";
}

/**
 * Check if device is authenticated (has valid token)
 */
bool AuthenticationManager::isAuthenticated() const {
  return !currentToken.isEmpty();
}

/**
 * Check if token has expired (based on TTL)
 * Note: This is a simple time-based check. The backend may reject expired tokens.
 */
bool AuthenticationManager::tokenExpired() const {
  if (currentToken.isEmpty()) {
    return true;
  }
  
  unsigned long elapsed = millis() - tokenSavedTime;
  return elapsed > TOKEN_TTL;
}

/**
 * Parse login response JSON and extract token
 */
bool AuthenticationManager::parseLoginResponse(const String& jsonResponse, String& token, String& userId, String& fullName) {
  // Create JSON document with appropriate size
  DynamicJsonDocument doc(1024);
  
  // Deserialize JSON
  DeserializationError error = deserializeJson(doc, jsonResponse);
  
  if (error) {
    Serial.print("[AUTH] JSON parse error: ");
    Serial.println(error.c_str());
    lastError = String("JSON parse error: ") + error.c_str();
    return false;
  }
  
  // Extract token
  if (!doc.containsKey("token")) {
    lastError = "Response missing 'token' field";
    Serial.println("[AUTH] ERROR: Response missing 'token' field");
    return false;
  }
  
  token = doc["token"].as<String>();
  
  // Extract user data if available
  if (doc.containsKey("data")) {
    JsonObject data = doc["data"].as<JsonObject>();
    userId = data["id"].as<String>();
    fullName = data["full_name"].as<String>();
    
    Serial.print("[AUTH] Login successful! User: ");
    Serial.println(fullName);
  }
  
  lastError = "";
  return true;
}

/**
 * Perform login with email and password
 */
bool AuthenticationManager::login(const char* email, const char* password, const char* backendURL) {
  if (!email || !password || !backendURL) {
    lastError = "Invalid login parameters";
    Serial.println("[AUTH] ERROR: Invalid login parameters");
    return false;
  }
  
  // Build login URL
  String loginURL = String(backendURL) + "/api/v1/auth/login";
  
  Serial.print("[AUTH] Attempting login to: ");
  Serial.println(loginURL);
  Serial.print("[AUTH] Email: ");
  Serial.println(email);
  
  // Create HTTP client
  HTTPClient http;
  http.begin(loginURL);
  http.setTimeout(10000);  // 10 second timeout
  
  // Set headers
  http.addHeader("Content-Type", "application/json");
  
  Serial.println("[AUTH] DEBUG: HTTP client initialized");
  Serial.print("[AUTH] DEBUG: WiFi connected: ");
  Serial.println(WiFi.status() == WL_CONNECTED ? "YES" : "NO");
  
  // Build request body
  String payload = "{\"email\":\"" + String(email) + "\",\"password\":\"" + String(password) + "\"}";
  Serial.print("[AUTH] DEBUG: Payload length: ");
  Serial.println(payload.length());
  
  // Send POST request
  Serial.println("[AUTH] DEBUG: Sending POST request...");
  int httpResponseCode = http.POST(payload);
  
  Serial.print("[AUTH] DEBUG: HTTP response code: ");
  Serial.println(httpResponseCode);
  
  if (httpResponseCode != 200) {
    Serial.print("[AUTH] ERROR: HTTP ");
    Serial.print(httpResponseCode);
    Serial.println(" response");
    
    // Add specific error messages for common codes
    if (httpResponseCode == -1) {
      Serial.println("[AUTH] DEBUG: Connection failed (-1)");
    } else if (httpResponseCode == -11) {
      Serial.println("[AUTH] DEBUG: Connection timeout or refused (-11)");
    } else if (httpResponseCode == 404) {
      Serial.println("[AUTH] DEBUG: Endpoint not found (404)");
    }
    
    String response = http.getString();
    lastError = "HTTP " + String(httpResponseCode);
    Serial.print("[AUTH] Response: ");
    Serial.println(response);
    http.end();
    return false;
  }
  
  // Get response
  String response = http.getString();
  http.end();
  
  Serial.println("[AUTH] Response received, parsing...");
  
  // Parse response
  String token, userId, fullName;
  if (!parseLoginResponse(response, token, userId, fullName)) {
    return false;
  }
  
  // Save credentials and token
  saveCredentials(email, password);
  saveToken(token.c_str());
  
  Serial.println("[AUTH] ✓ Authentication successful!");
  return true;
}

/**
 * Automatically login using stored credentials
 */
bool AuthenticationManager::autoLogin(const char* backendURL) {
  String email, password;
  
  // Load stored credentials
  if (!loadCredentials(email, password)) {
    Serial.println("[AUTH] No stored credentials available for auto-login");
    lastError = "No stored credentials";
    return false;
  }
  
  // Perform login
  return login(email.c_str(), password.c_str(), backendURL);
}

/**
 * Logout - clear credentials and token
 */
void AuthenticationManager::logout() {
  clearCredentials();
  clearToken();
  Serial.println("[AUTH] ✓ Logged out successfully");
}

/**
 * Create Authorization header with Bearer token
 */
String AuthenticationManager::createAuthHeader() const {
  if (currentToken.isEmpty()) {
    return "";
  }
  return "Bearer " + currentToken;
}

/**
 * Build vitals JSON payload
 */
String AuthenticationManager::buildVitalsJSON(int heartRate, float spo2, int steps, float temperature) const {
  // Validate and clamp values
  if (spo2 < 0) spo2 = 0;
  if (spo2 > 100) spo2 = 100;
  
  // Validate temperature (reasonable range: 20-45°C)
  if (temperature < 20 || temperature > 45) {
    Serial.print("[AUTH] WARNING: Invalid temperature ");
    Serial.print(temperature);
    Serial.println("°C, using 36.6°C");
    temperature = 36.6;
  }
  
  // Create JSON document
  DynamicJsonDocument doc(768);
  
  doc["heart_rate"] = heartRate;
  doc["oxygen"] = round(spo2);  // Round to integer as backend expects 0-100
  doc["steps"] = steps;
  doc["temperature"] = temperature;
  
  String currentLocationName = closestKnownAPName;
  if (!currentLocationName.isEmpty()) {
    JsonObject locations = doc.createNestedObject("locations");
    locations["indoor"] = currentLocationName;
  }
  
  // Add ISO timestamp
  char isoTime[25];
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  strftime(isoTime, sizeof(isoTime), "%Y-%m-%dT%H:%M:%SZ", timeinfo);
  doc["reading_date"] = isoTime;
  
  // Serialize to string
  String json;
  serializeJson(doc, json);
  
  return json;
}

/**
 * Send vitals to backend
 */
bool AuthenticationManager::sendVitals(const char* backendURL, int heartRate, float spo2, int steps, float temperature) {
  if (!isAuthenticated()) {
    lastError = "Not authenticated - no valid token";
    Serial.println("[AUTH] ERROR: Cannot send vitals - not authenticated");
    return false;
  }
  
  if (tokenExpired()) {
    lastError = "Token expired - need to re-login";
    Serial.println("[AUTH] WARNING: Token may have expired");
  }
  
  // Build vitals URL
  String vitalsURL = String(backendURL) + "/api/v1/patients/vitals";
  
  Serial.print("[AUTH] Sending vitals to: ");
  Serial.println(vitalsURL);
  
  // Create HTTP client
  HTTPClient http;
  http.begin(vitalsURL);
  
  // Set headers
  http.addHeader("Content-Type", "application/json");
  String authHeader = createAuthHeader();
  http.addHeader("Authorization", authHeader);
  
  // Build payload
  String payload = buildVitalsJSON(heartRate, spo2, steps, temperature);
  
  Serial.print("[AUTH] Payload: ");
  Serial.println(payload);
  
  // Send POST request
  int httpResponseCode = http.POST(payload);
  
  String response = http.getString();
  http.end();
  
  if (httpResponseCode != 200 && httpResponseCode != 201) {
    Serial.print("[AUTH] ERROR: HTTP ");
    Serial.print(httpResponseCode);
    Serial.println(" response");
    Serial.print("[AUTH] Response: ");
    Serial.println(response);
    lastError = "HTTP " + String(httpResponseCode);
    return false;
  }
  
  Serial.println("[AUTH] ✓ Vitals sent successfully!");
  Serial.print("[AUTH] Response: ");
  Serial.println(response);
  lastError = "";
  return true;
}
