#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ===== OLED =====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ===== PINEK =====
const uint8_t POT_PIN = A0;
const uint8_t LED_PIN = 6;     // PWM
const uint8_t BTN_PIN = 4;     // INPUT_PULLUP, gomb a GND-re

// ===== 1 gomb: debounce + click + long press esemény =====
struct OneButton {
    uint8_t pin;

    bool stableState = true;     // HIGH alapból (felhúzva)
    bool lastReading = true;
    uint32_t lastChangeMs = 0;

    bool down = false;
    uint32_t downSinceMs = 0;

    bool clickEvent = false;
    bool longPressEvent = false;
    bool longPressFired = false;

    void begin() {
        pinMode(pin, INPUT_PULLUP);
        stableState = digitalRead(pin);
        lastReading = stableState;
        down = (stableState == LOW);
        if (down) downSinceMs = millis();
    }

    void update(uint32_t nowMs) {
        clickEvent = false;
        longPressEvent = false;

        bool reading = digitalRead(pin);
        if (reading != lastReading) {
            lastChangeMs = nowMs;
            lastReading = reading;
        }

        // debounce 25ms
        if ((nowMs - lastChangeMs) > 25) {
            if (stableState != reading) {
                stableState = reading;

                if (stableState == LOW) {          // lenyomás
                    down = true;
                    downSinceMs = nowMs;
                    longPressFired = false;
                } else {                           // felengedés
                    // ha nem volt long press, akkor click
                    if (!longPressFired) clickEvent = true;
                    down = false;
                }
            }
        }

        // long press detektálás (lenyomva tartva)
        if (down && !longPressFired && (nowMs - downSinceMs >= 1000)) {
            longPressEvent = true;
            longPressFired = true; // ne tüzeljen többször
        }
    }

    bool click() const { return clickEvent; }
    bool longPress() const { return longPressEvent; }
    bool isDown() const { return down; }
};

OneButton button{BTN_PIN};

// ===== MÓDOK =====
enum Mode : uint8_t { MODE_MONITOR, MODE_DIMMER, MODE_GRAPH, MODE_GAME };
Mode mode = MODE_MONITOR;

const char* modeName(Mode m) {
    switch (m) {
        case MODE_MONITOR: return "MONITOR";
        case MODE_DIMMER:  return "DIMMER";
        case MODE_GRAPH:   return "GRAPH";
        case MODE_GAME:    return "GAME";
        default:           return "?";
    }
}

// ===== ÁLLAPOT =====
uint16_t potRaw = 0;
uint16_t potFiltered = 0;
bool ledEnabled = true;

// ===== A 3 FELADATFÜGGVÉNY (CSAK EZEKBE ÍRJ!) =====
uint16_t task1_filterPot(uint16_t raw);
void task2_graphMode(uint16_t potValue, bool click, uint32_t nowMs);
void task3_reactionGameMode(uint16_t potValue, bool click, uint32_t nowMs);

// ===== SEGÉDEK =====
uint8_t potToPwm(uint16_t pot) { return (uint32_t)pot * 255 / 1023; }
uint8_t potToPercent(uint16_t pot) { return (uint32_t)pot * 100 / 1023; }

void drawHeader() {
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print(modeName(mode));
    display.setCursor(70, 0);
    display.print(potToPercent(potFiltered));
    display.print("%");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
}

void drawMonitor(bool click) {
    // click: LED ki/be (bináris jelzés)
    if (click) ledEnabled = !ledEnabled;

    // LED: pot küszöb jelzés
    if (ledEnabled) {
        digitalWrite(LED_PIN, (potFiltered > 512) ? HIGH : LOW);
    } else {
        digitalWrite(LED_PIN, LOW);
    }

    display.setCursor(0, 14);
    display.print("RAW: ");
    display.print(potRaw);

    display.setCursor(0, 26);
    display.print("FILT:");
    display.print(potFiltered);

    display.setCursor(0, 38);
    display.print("LED: ");
    display.print(ledEnabled ? "ON" : "OFF");
    display.print("  (pot>50%)");

    display.setCursor(0, 54);
    display.print("CLICK=LED  LONG=mode");
}

void drawDimmer(bool click) {
    if (click) ledEnabled = !ledEnabled;

    uint8_t pwm = ledEnabled ? potToPwm(potFiltered) : 0;
    analogWrite(LED_PIN, pwm);

    display.setCursor(0, 14);
    display.print("LED: ");
    display.print(ledEnabled ? "ON" : "OFF");

    display.setCursor(0, 28);
    display.print("PWM: ");
    display.print(pwm);

    display.setCursor(0, 42);
    display.print("Pot -> brightness");

    display.setCursor(0, 54);
    display.print("CLICK=LED  LONG=mode");
}

void setup() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    button.begin();

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        while (true) { delay(10); }
    }
    display.setRotation(2);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("UNO R4 SensorKit demo");
    display.println("1 button: CLICK/LONG");
    display.println("Tasks: GRAPH + GAME");
    display.display();

    randomSeed(analogRead(POT_PIN));
    delay(800);
}

void loop() {
    uint32_t nowMs = millis();
    button.update(nowMs);

    // hosszú nyomás: módváltás
    if (button.longPress()) {
        mode = (Mode)((mode + 1) % 4);
        display.clearDisplay();
    }

    // pot olvasás + (FELADAT 1) szűrés
    potRaw = analogRead(POT_PIN);
    potFiltered = task1_filterPot(potRaw);

    bool click = button.click();

    display.clearDisplay();
    drawHeader();

    switch (mode) {
        case MODE_MONITOR:
            drawMonitor(click);
            break;

        case MODE_DIMMER:
            drawDimmer(click);
            break;

        case MODE_GRAPH:
            // (FELADAT 2)
            task2_graphMode(potFiltered, click, nowMs);
            break;

        case MODE_GAME:
            // (FELADAT 3)
            task3_reactionGameMode(potFiltered, click, nowMs);
            break;
    }

    display.display();
    delay(20);
}

// ============================================================
// ===================== FELADAT 1 ===================
// Potméter szűrés / simítás
// Minimum:
// - vezess be egyszerű szűrést (mozgóátlag VAGY IIR)
// - kimenet: 0..1023
// Tipp: static változó(ka)t használj.
// ============================================================
uint16_t task1_filterPot(uint16_t raw) {
    // TODO: IDE JÖN A MEGOLDÁS
    return raw; // alap: nincs szűrés
}

// ============================================================
// =================== FELADAT 2 =====================
// GRAPH mód: potméter grafikon (utolsó >=100 minta)
// Minimum:
// - körkörös pufferben tárold a mintákat
// - rajzolj görbét a kijelzőre
// - CLICK: freeze (megáll/folytat)
// - freeze alatt a LED villogjon 2 Hz (250ms váltás)
// ============================================================
void task2_graphMode(uint16_t potValue, bool click, uint32_t nowMs) {
    // TODO: IDE JÖN A MEGOLDÁS

    // alap (kiinduló) képernyő:
    display.setCursor(0, 16);
    display.println("TASK 2: GRAPH");
    display.println("Need history + draw");
    display.println("CLICK=freeze");
    display.print("pot=");
    display.println(potValue);

    analogWrite(LED_PIN, 0);
}

// ============================================================
// ===================== FELADAT 3 =====================
// GAME mód: reakcioido jatek (allapotgep, millis alapon)
// Szabalyok (javasolt):
// - Kezdo: "CLICK to start"
// - Start utan varj veletlen ideig (potValue=nehezseg)
// - GO: LED ON + "GO!"
// - Kattintasig merni ms-ben (reakcioido)
// - Ha GO elott kattint: "Too soon!"
// - Legjobb ido (best) tarolasa es kijelzese
// Kotelező: nincs delay() a logikaban
// ============================================================
void task3_reactionGameMode(uint16_t potValue, bool click, uint32_t nowMs) {
    // TODO: IDE JÖN A MEGOLDÁS

    display.setCursor(0, 16);
    display.println("TASK 3: GAME");
    display.println("State machine needed");
    display.println("CLICK to start/react");
    display.print("diff=");
    display.print((int)potToPercent(potValue));
    display.println("%");

    analogWrite(LED_PIN, 0);
}
