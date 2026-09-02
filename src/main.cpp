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
#include "WiFiWorker.h"
#include "DataHub.h"
#include "SensorManager.h"
#include "SensorTypes.h" // @note remove if loading from JSON is implemented
#include "Controller.h"
#include "NvsStorage.h"

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
  NvsStorage::init();

  // FIXME old:
  Serial.println("\n--- Setup abgeschlossen ---");
  Serial.println("Bereit für WiFiManager Neuaufbau\n");
  DataHub::initDataHub();

  DataHub::WifiRelay relay = {};
  relay.inUse = true;
  relay.id = 0;
  relay.relayMode = RelayTypes::RelayMode::FORCED_OFF;
  relay.switchDuration = 60000;
  strncpy(relay.mac, TERRA_CONTOL_000_MAC_STR, sizeof(relay.mac) - 1);
  DataHub::setWifiRelayEntry(relay, 0);
  relay = {};
  relay.inUse = true;
  relay.id = 0;
  relay.relayMode = RelayTypes::RelayMode::FORCED_OFF;
  relay.switchDuration = 60000;
  strncpy(relay.mac, TERRA_CONTOL_001_MAC_STR, sizeof(relay.mac) - 1);
  DataHub::setWifiRelayEntry(relay, 1);
  relay = {};
  relay.inUse = true;
  relay.id = 0;
  relay.relayMode = RelayTypes::RelayMode::FORCED_OFF;
  relay.switchDuration = 60000;
  strncpy(relay.mac, TERRA_CONTOL_002_MAC_STR, sizeof(relay.mac) - 1);
  DataHub::setWifiRelayEntry(relay, 2);

  WiFiManager::initWiFi();
  WiFiManager::updateWifiStatus();
  // DataHub::dataHubToSerial();
  //? Sensoren anmelden und Testen

  if (!SensorManager::init())
  {
    Serial.println("nicht alle Sensoren initialisiert, Logs prüfen");
  }
  else
    Serial.println("Sensoren initialisiert");
  Serial.println("Setup beendet.");
}

void loop()
{
  unsigned long now = millis();
  Controller::update(now);

  /*
    static unsigned long lastStatus = 0;
    if (now - lastStatus >= 30000)
    {
      lastStatus = now;
      DataHub::dataHubStatusToSerial();
    }
      */
}
