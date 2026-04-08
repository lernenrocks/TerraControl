#pragma once

#include <Arduino.h>
#include <Stream.h>
#include <ArduinoJson.h>
#include "DataHub.h"

/**
 * @file JsonParser.h
 * @brief JSON-Parsen und Serialisierung für Shelly/terraControll (ESP32)
 *
 * Uses ArduinoJson with StaticJsonDocument to avoid heap fragmentation.
 */

// JsonParser public API im Namespace JsonParser.
namespace JsonParser
{

    /**
     * @brief Parse-Ergebniscodes
     *
     * Diese Codes sind rein lokal für JsonParser und spiegeln in
     * erster Linie deskriptive Resultate wieder.
     */
    enum class ParseResult: uint8_t
    {
        OK = 0,             /**<@brief Parsing war erfolgreich */
        INVALID_JSON,       /**<@brief Ungültige JSON-Syntax */
        NO_MATCHING_ENTRY,  /**<@brief Kein passender Eintrag gefunden */
        FIELD_MISSING,      /**<@brief Pflichtfeld im JSON fehlt */
        BUFFER_TOO_SMALL,   /**<@brief StaticJsonDocument zu klein */
        STREAM_ERROR,       /**<@brief Lese-/Streamfehler */
        NO_MATCHING_TARGET /**<@brief kein passender Zieltyp */
    };

    /**
     * @brief Ziel des Parsings.
     */
    enum class TargetType: uint8_t 
    {
        UNKNOWN = 0,        /**<@brief Zieltyp noch unbekannt */
        WIFI_RELAY_STATUS,   /**<@brief Shelly-Relay-Eintrag */
    };

    /**
     * @brief Parst JSON aus einem Stream für das gewünschte Ziel.
     *
     * @param stream Eingabe-Stream, z.B. WiFiClient.
     * @param target Typ der zu verarbeitenden Daten.
     * @return Resultat-Code (ParseResult).
     */
    ParseResult parseJson(Stream &stream, TargetType target);

} // namespace JsonParser
