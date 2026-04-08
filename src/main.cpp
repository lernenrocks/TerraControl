/*
 * WiFiManager Test - Minimal Setup
 * Zweiter ESP32 ohne angeschlossene Hardware
 * Komplett clean - nur WiFiManager
 */

#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>
#include "wifi_config.h"
#include "WiFiManager.h"
#include "DataHub.h"

#define APP_VERSION "0.1"

void setup()
{
  Serial.begin(115200);
  delay(2000); // Zeit zum Öffnen des Serial Monitors

  /** @note Boot Counter für Debugging */
  /*
  Preferences prefs;
  prefs.begin("system", false);
  uint32_t bootCount = prefs.getUInt("bootCount", 0);
  bootCount++;
  prefs.putUInt("bootCount", bootCount);
  prefs.end();
*/
  // esp_reset_reason_t rr = esp_reset_reason();
  // Serial.printf("\n=== BOOT START === (#%u, reset=%d)\n", bootCount, (int)rr);
  Serial.printf("Version: %s\n", APP_VERSION);
  Serial.printf("Freier Heap: %u Bytes\n", ESP.getFreeHeap());

  Serial.println("\n--- Setup abgeschlossen ---");
  Serial.println("Bereit für WiFiManager Neuaufbau\n");
  DataHub::initDataHub();
  WiFiManager::initWiFi();
  WiFiManager::updateWifiStatus();
  DataHub::WifiRelay wifiRelayEntry = {};
  strncpy(wifiRelayEntry.mac, TERRA_CONTOL_000_MAC_STR, sizeof(wifiRelayEntry.mac) - 1);
  wifiRelayEntry.inUse = true;
  DataHub::setWifiRelayEntry(wifiRelayEntry, 0);
  strncpy(wifiRelayEntry.mac, TERRA_CONTOL_001_MAC_STR, sizeof(wifiRelayEntry.mac) - 1);
  wifiRelayEntry.inUse = true;
  DataHub::setWifiRelayEntry(wifiRelayEntry, 1);
  strncpy(wifiRelayEntry.mac, TERRA_CONTOL_002_MAC_STR, sizeof(wifiRelayEntry.mac) - 1);
  wifiRelayEntry.inUse = true;
  DataHub::setWifiRelayEntry(wifiRelayEntry, 2);
  WiFiManager::findStoredRelays();
  DataHub::dataHubToSerial();
  Serial.println("Setup beendet.");
}

void switchRelays()
{
  DataHub::WifiRelay relays[RELAY_SIZE] = {};
  DataHub::getWifiRelayArray(relays);
  if (relays[0].online)
  {
    WiFiManager::switchRelay(relays[0].mac, relays[0].id, true, 10);
  }
  if (relays[1].online)
  {
    WiFiManager::switchRelay(relays[1].mac, relays[1].id, true, 20);
  }
  if (relays[2].online)
  {
    WiFiManager::switchRelay(relays[2].mac, relays[2].id, true, 30);
  }
  //DataHub::dataHubToSerial();
  DataHub::dataHubStatusToSerial();
}

void loop()
{
  static unsigned long lastRun = -40000UL;//Offset, damit der erste Durchlauf gleich nach dem Start passiert.
  if (millis() - lastRun >= 40000UL)
  {
    lastRun = millis();
    switchRelays();
  }
  WiFiManager::checkRelayTimeouts();
}
