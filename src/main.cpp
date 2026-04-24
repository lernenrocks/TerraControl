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
#include "SensorManager.h"
#include "SensorTypes.h" // @note remove if loading from JSON is implemented

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
  /*
  WiFiManager::initWiFi();
  WiFiManager::updateWifiStatus();
  DataHub::WifiRelay wifiRelayEntry = {};
  strncpy(wifiRelayEntry.mac, TERRA_CONTOL_000_MAC_STR, sizeof(wifiRelayEntry.mac) - 1);
  wifiRelayEntry.inUse = true;
  // @note for mac Integration Test 
  // wifiRelayEntry.inUse = false;
  DataHub::setWifiRelayEntry(wifiRelayEntry, 0);
  strncpy(wifiRelayEntry.mac, TERRA_CONTOL_001_MAC_STR, sizeof(wifiRelayEntry.mac) - 1);
  wifiRelayEntry.inUse = true;
  DataHub::setWifiRelayEntry(wifiRelayEntry, 1);
  strncpy(wifiRelayEntry.mac, TERRA_CONTOL_002_MAC_STR, sizeof(wifiRelayEntry.mac) - 1);
  wifiRelayEntry.inUse = true;
  DataHub::setWifiRelayEntry(wifiRelayEntry, 2);
  WiFiManager::findStoredRelays();
  */
  //DataHub::dataHubToSerial();
  //? Sensoren anmelden und Testen

  SensorManager::initSensors();
  Serial.println("Sensoren initialisiert");
  //DHT00
  DataHub::SensorData sensor={};
  sensor.inUse=true;
  sensor.updateInterval=2000;
  sensor.sensorIndex=0;
  strncpy(sensor.name,"DHT000_Temp",NAME_LEN);
  strncpy(sensor.unit,"°C",UNIT_LEN);
  sensor.type=SensorType::DHT22_TEMPERATURE;
  DataHub::registerSensor(sensor);
  sensor={};
  sensor.inUse=true;
  sensor.updateInterval=2000;
  sensor.sensorIndex=0;
  strncpy(sensor.name,"DHT000_Hum",NAME_LEN);
  strncpy(sensor.unit,"%",UNIT_LEN);
  sensor.type=SensorType::DHT22_HUMIDITY;
  DataHub::registerSensor(sensor);
  
  //DHT01
  sensor={};
  sensor.inUse=true;
  sensor.updateInterval=2000;
  sensor.sensorIndex=1;
  strncpy(sensor.name,"DHT001_Temp",NAME_LEN);
  strncpy(sensor.unit,"°C",UNIT_LEN);
  sensor.type=SensorType::DHT22_TEMPERATURE;
  DataHub::registerSensor(sensor);
  sensor={};
  sensor.inUse=true;
  sensor.updateInterval=2000;
  sensor.sensorIndex=1;
  strncpy(sensor.name,"DHT001_Hum",NAME_LEN);
  strncpy(sensor.unit,"%",UNIT_LEN);
  sensor.type=SensorType::DHT22_HUMIDITY;
  DataHub::registerSensor(sensor);
  
  //DHT02
  sensor={};
  sensor.inUse=true;
  sensor.updateInterval=2000;
  sensor.sensorIndex=2;
  strncpy(sensor.name,"DHT002_Temp",NAME_LEN);
  strncpy(sensor.unit,"°C",UNIT_LEN);
  sensor.type=SensorType::DHT22_TEMPERATURE;
  DataHub::registerSensor(sensor);
  sensor={};
  sensor.inUse=true;
  sensor.updateInterval=2000;
  sensor.sensorIndex=2;
  strncpy(sensor.name,"DHT002_Hum",NAME_LEN);
  strncpy(sensor.unit,"%",UNIT_LEN);
  sensor.type=SensorType::DHT22_HUMIDITY;
  DataHub::registerSensor(sensor);
  
  //DHT03
  sensor={};
  sensor.inUse=true;
  sensor.updateInterval=2000;
  sensor.sensorIndex=3;
  strncpy(sensor.name,"DHT003_Temp",NAME_LEN);
  strncpy(sensor.unit,"°C",UNIT_LEN);
  sensor.type=SensorType::DHT22_TEMPERATURE;
  DataHub::registerSensor(sensor);
  sensor={};
  sensor.inUse=true;
  sensor.updateInterval=2000;
  sensor.sensorIndex=3;
  strncpy(sensor.name,"DHT003_Hum",NAME_LEN);
  strncpy(sensor.unit,"%",UNIT_LEN);
  sensor.type=SensorType::DHT22_HUMIDITY;
  DataHub::registerSensor(sensor);

  //Soil Moisture Sensor
  sensor={};
  sensor.inUse=true;
  sensor.updateInterval=2000;
  sensor.sensorIndex=0;
  strncpy(sensor.name,"Soil Hum",NAME_LEN);
  strncpy(sensor.unit,"%",UNIT_LEN);
  sensor.type=SensorType::SOIL_MOISTURE;
  sensor.calMax=1220.0f;
  sensor.calMin=3080.0f;
  DataHub::registerSensor(sensor);
  
Serial.printf("Größe SensorData: %d",sizeof(DataHub::SensorData));
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
  static unsigned long now;
  now=millis();
  static unsigned long lastUpdate =0;
  SensorManager::update(now);
  if(now-lastUpdate>=3000){
  DataHub::sensorDataToSerial();
  lastUpdate=now;
  }
  
  

  /*
  static unsigned long lastRun = -40000UL; // Offset, damit der erste Durchlauf gleich nach dem Start passiert.
  if (millis() - lastRun >= 40000UL)
  {
    lastRun = millis();
    switchRelays();
  }
  static unsigned long lastHeapLog = 0;
  if (millis() - lastHeapLog >= 30000UL)
  {
    lastHeapLog = millis();
    Serial.printf("[HEAP] Frei: %u Bytes\n", ESP.getFreeHeap());
  }
  WiFiManager::checkRelayTimeouts();
  */
}
