#ifndef AUTHENTICATION_MANAGER_H
#define AUTHENTICATION_MANAGER_H

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>

class AuthenticationManager {
public:
  AuthenticationManager();
  
  // Credential management
  void saveCredentials(const char* email, const char* password);
  bool loadCredentials(String& email, String& password);
  void clearCredentials();
  
  // Token management
  void saveToken(const char* token);
  String loadToken();
  void clearToken();
  String getToken() const { return currentToken; }
  
  // Authentication
  bool login(const char* email, const char* password, const char* backendURL);
  bool autoLogin(const char* backendURL);
  void logout();
  
  // Utility
  bool isAuthenticated() const;
  bool tokenExpired() const; // Simple check based on time
  
  // Vitals sending
  bool sendVitals(const char* backendURL, int heartRate, float spo2, int steps, float temperature);
  
  // Public utility functions
  String buildVitalsJSON(int heartRate, float spo2, int steps, float temperature) const;
  
  // Error handling
  String getLastError() const { return lastError; }
  
  // Debug method to check preferences state
  void debugPreferencesState();
  
private:
  Preferences preferences;
  String currentToken;
  unsigned long tokenSavedTime;
  String lastError;
  
  // Storage keys
  static const char* PREF_NAMESPACE;
  static const char* KEY_EMAIL;
  static const char* KEY_PASSWORD;
  static const char* KEY_TOKEN;
  
  // Token TTL (24 hours in milliseconds) - adjust as needed
  static const unsigned long TOKEN_TTL = 24 * 60 * 60 * 1000;
  
  // Helper functions
  bool parseLoginResponse(const String& jsonResponse, String& token, String& userId, String& fullName);
  String createAuthHeader() const;
};

#endif
