/**
 * DHT22 Temperature and Humidity Reader
 * 
 * Provides compact encoding for LoRaWAN payload (2 bytes total):
 * - Temperature: 1 byte, range -40 to +87.5°C, 0.5°C resolution
 * - Humidity: 1 byte, range 0-100%, 1% resolution
 */

#ifndef _DHT_READER_H_
#define _DHT_READER_H_

#include <stdint.h>

// Initialize the DHT22 sensor
void dht_init(uint8_t pin);

// Read temperature and humidity from sensor
// Returns true if read was successful, false if using cached values
bool dht_read();

// Get last temperature reading in °C
float dht_get_temperature();

// Get last humidity reading in %
float dht_get_humidity();

// Get encoded temperature (1 byte)
// Decode: temp = (value / 2.0) - 40.0
uint8_t dht_get_encoded_temperature();

// Get encoded humidity (1 byte, 0-100)
uint8_t dht_get_encoded_humidity();

#endif
