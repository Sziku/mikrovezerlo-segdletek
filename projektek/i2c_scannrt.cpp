#include <Wire.h>

void scan(TwoWire &bus, const char *name) {
  Serial.print("\nScan: "); Serial.println(name);
  byte found = 0;
  for (byte a = 1; a < 127; a++) {
    bus.beginTransmission(a);
    if (bus.endTransmission() == 0) {
      Serial.print("  found 0x");
      if (a < 16) Serial.print("0");
      Serial.println(a, HEX);
      found++;
    }
  }
  if (!found) Serial.println("  (no devices)");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  // belső felhúzók tesztre (nem mindig elég, de segíthet)
  pinMode(SDA, INPUT_PULLUP);
  pinMode(SCL, INPUT_PULLUP);

  Wire.begin();
  Wire.setClock(100000);
  scan(Wire, "Wire (shield I2C)");

  Wire1.begin();
  Wire1.setClock(100000);
  scan(Wire1, "Wire1 (Qwiic)");
}

void loop() {}
