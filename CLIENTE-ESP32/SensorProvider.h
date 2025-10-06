#pragma once
#include <Arduino.h>

#define USE_DHT22
#ifdef USE_DHT22
#include <DHT.h>
#define DHT_PIN  15
#define DHT_TYPE DHT22
#endif

struct EnvSample {
  float temperatureC;
  float humidity;
  uint32_t ts;
  String device;
};

class SensorProvider {
public:
  void begin(const String& deviceName);
  EnvSample read();

private:
  String _device;
#ifdef USE_DHT22
  DHT _dht = DHT(DHT_PIN, DHT_TYPE);
#endif
};
