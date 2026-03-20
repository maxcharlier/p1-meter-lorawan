/**
 * DHT22 Temperature and Humidity Reader
 * 
 * Encoding format for LoRaWAN (2 bytes total):
 * 
 * Temperature (1 byte):
 *   encoded = (temp + 40) * 2
 *   Range: -40°C to +87.5°C
 *   Resolution: 0.5°C
 *   Decode: temp = (byte / 2.0) - 40.0
 * 
 * Humidity (1 byte):
 *   Direct value 0-100%
 *   Resolution: 1%
 */

#include <Arduino.h>
#include <DHT.h>
#include "dht_reader.h"

static DHT* dht_sensor = nullptr;
static float last_temperature = 0.0f;
static float last_humidity = 0.0f;

void dht_init(uint8_t pin) {
  if (dht_sensor != nullptr) {
    delete dht_sensor;
  }
  dht_sensor = new DHT(pin, DHT22);
  dht_sensor->begin();
  Serial.println("DHT22 sensor initialized.");
}

bool dht_read() {
  if (dht_sensor == nullptr) {
    Serial.println("DHT22: not initialized!");
    return false;
  }

  float h = dht_sensor->readHumidity();
  float t = dht_sensor->readTemperature();

  if (!isnan(h) && !isnan(t)) {
    last_temperature = t;
    last_humidity = h;
    Serial.printf("DHT22: %.1f°C, %.1f%%\n", t, h);
    return true;
  } else {
    Serial.println("DHT22: read failed, using cached values");
    return false;
  }
}

float dht_get_temperature() {
  return last_temperature;
}

float dht_get_humidity() {
  return last_humidity;
}

uint8_t dht_get_encoded_temperature() {
  // Encode: (temp + 40) * 2
  // Range: -40°C (0) to +87.5°C (255)
  int16_t encoded = (int16_t)((last_temperature + 40.0f) * 2.0f);
  if (encoded < 0) encoded = 0;
  if (encoded > 255) encoded = 255;
  return (uint8_t)encoded;
}

uint8_t dht_get_encoded_humidity() {
  // Direct 0-100% with rounding
  uint8_t encoded = (uint8_t)(last_humidity + 0.5f);
  if (encoded > 100) encoded = 100;
  return encoded;
}
