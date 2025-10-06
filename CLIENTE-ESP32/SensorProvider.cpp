#include "SensorProvider.h"

void SensorProvider::begin(const String& deviceName) {
  _device = deviceName;
#ifdef USE_DHT22
  _dht.begin();
#endif
}

EnvSample SensorProvider::read() {
  EnvSample s{};
  s.ts = millis();
  s.device = _device;

#ifdef USE_DHT22
  float h = _dht.readHumidity();
  float t = _dht.readTemperature(); // °C
  if (isnan(h) || isnan(t)) {
    // Fallback rápido
    t = 24.0 + 2.0 * sin((double)millis()/5000.0);
    h = 50.0 + 5.0 * cos((double)millis()/4500.0);
  }
  s.temperatureC = t;
  s.humidity = h;
#else
  // Simulado (sin DHT): señal suave, reproducible
  s.temperatureC = 24.0 + 2.0 * sin((double)millis()/5000.0);
  s.humidity     = 50.0 + 5.0 * cos((double)millis()/4500.0);
#endif

  return s;
}
