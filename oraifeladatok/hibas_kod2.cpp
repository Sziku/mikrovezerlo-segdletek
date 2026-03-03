#include "DHT20.h"
#include <Adafruit_BMP280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x2F

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Sensors
Adafruit_BMP280 bmp;
DHT20 dht20;

const int LED_PIN = LED_BUILTIN;
const int BTN_PIN = 3;
const int POT_PIN = A1;
const int LDR_PIN = A0;

bool alarmMode = false;

float seaLevel_hPa = 1099.325;

unsigned long lastUpdate = 0;
unsigned long periodMs = 200;
lehet

    void
    setup()
{
    Serial.begin(115200);

    Wire1.begin();
    Wire1.setClock(400000);

    pinMode(LED_PIN, OUTPUT);

    pinMode(BTN_PIN, INPUT);

    analogReadResolution(12);

    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("OLED init fail");
    }

    dht20.begin();

    if (!bmp.begin(0x75)) {
        Serial.println("BMP init fail");
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
}

void loop()
{

    if (millis() - lastUpdate < periodMs)
        return;
    lastUpdate = millis();

    int rawLdr = analogRead(LDR_PIN);
    int rawPot = analogRead(POT_PIN);

    int lightPct = map(rawLdr, 0, 1023, 0, 100);
    int potPct = map(rawPot, 0, 1023, 0, 100);

    if (digitalRead(BTN_PIN) == HIGH) {
        alarmMode = !alarmMode;
    }

    float t = dht20.getTemperature();
    float h = dht20.getHumidity();

    // BMP280: nyomás, magasság
    float p_hPa = bmp.readPressure() / 100.0;
    float alt_m = bmp.readAltitude(seaLevel_hPa);

    // LED logika
    int threshold = map(potPct, 0, 100, 10, 90);
    bool dark = (lightPct < threshold);

    if (dark || alarmMode)
        digitalWrite(LED_PIN, HIGH);
    else
        digitalWrite(LED_PIN, LOW);

    // OLED kiírás
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Light: ");
    display.print(lightPct);
    display.print("%  Th:");
    display.println(threshold);
    display.print("Pot:   ");
    display.print(potPct);
    display.println("%");
    display.print("Mode:  ");
    display.println(alarmMode ? "ALARM" : "OK");

    display.print("T: ");
    display.print(t, 1);
    display.println(" C");
    display.print("H: ");
    display.print(h, 1);
    display.println(" %");

    display.print("P: ");
    display.print(p_hPa, 1);
    display.println(" hPa");
    display.print("Alt: ");
    display.print(alt_m, 1);
    display.println(" m");
}
