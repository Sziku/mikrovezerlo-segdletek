#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- OLED kijelzo beallitasok ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1  // nincs reset pin
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Bemenetek/Kimenetek ---
const int POT_PIN = A0;
const int BTN_PIN = 4;
const int LED_PIN = 6;

// --- Menü szintek ---
enum MenuId {
  MENU_MAIN = 0,
  MENU_SETTINGS,
  MENU_SPEED
};

// --- Menüpontok szövegei ---

const char* const mainItems[] = {
  "LED KI/BE",
  "LED villogas KI/BE",
  "Beallitasok"
};
const int mainItemCount = sizeof(mainItems) / sizeof(mainItems[0]);

const char* const settingsItems[] = {
  "Villogasi sebesseg",
  "Vissza fo menube"
};
const int settingsItemCount = sizeof(settingsItems) / sizeof(settingsItems[0]);

const char* const speedItems[] = {
  "Lassu",
  "Kozepes",
  "Gyors",
  "Vissza beallit.."
};
const int speedItemCount = sizeof(speedItems) / sizeof(speedItems[0]);

// Menü címsorok
const char* const menuTitles[] = {
  "FO MENU",
  "BEALLITASOK",
  "VILLOGASI SEBESSEG"
};

// --- Menürendszer állapot ---
byte currentMenu = MENU_MAIN;
int selectedIndex = 0;

int lastSelectedIndex = -1;
byte lastMenu = 255;

// görgetéshez
const int maxVisibleItems = 5;  // egyszerre ennyi sor latszik
int viewOffset = 0;             // melyik index van legfelul
int lastViewOffset = -1;

// --- Gombkezelés (debounce) ---
bool lastButtonState = HIGH; // PULLUP miatt
unsigned long lastButtonTime = 0;
const unsigned long debounceDelay = 50;

// --- LED logika ---
bool ledOn = false;
bool blinkMode = false;
unsigned long lastBlinkTime = 0;
unsigned long blinkInterval = 500;  // ms

// --- Függvény prototípusok ---
int getMenuItemCount(byte menuId);
const char* const* getMenuItems(byte menuId);
void redrawMenuIfNeeded();
void handleButtonPress();
void handleBlinking();
void updateSelectionFromPot();

// =====================================================================
// SETUP / LOOP
// =====================================================================

void setup() {
  Serial.begin(9600);

  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // OLED inicializálás (cím általában 0x3C,
  // ha nem jo, probald 0x3D-vel)
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED hiba! Ellenorizd a bekotest es az I2C cimet.");
    while (true) { delay(1); }
  }
  display.setRotation(2);
  display.clearDisplay();
  display.display();

  Serial.println("OLED menu rendszer indul...");

  redrawMenuIfNeeded(); // elso kirajzolas
}

void loop() {
  updateSelectionFromPot();
  redrawMenuIfNeeded();

  // gomb olvasasa + debounce
  int reading = digitalRead(BTN_PIN);
  unsigned long now = millis();

  if (reading != lastButtonState) {
    lastButtonTime = now;
  }

  if ((now - lastButtonTime) > debounceDelay) {
    static int stableState = HIGH;
    if (reading != stableState) {
      stableState = reading;
      if (stableState == LOW) { // lenyomaskor
        handleButtonPress();
      }
    }
  }

  lastButtonState = reading;

  handleBlinking();
}

// =====================================================================
//                              MENÜ LOGIKA
// =====================================================================

int getMenuItemCount(byte menuId) {
  switch (menuId) {
    case MENU_MAIN:     return mainItemCount;
    case MENU_SETTINGS: return settingsItemCount;
    case MENU_SPEED:    return speedItemCount;
  }
  return 0;
}

const char* const* getMenuItems(byte menuId) {
  switch (menuId) {
    case MENU_MAIN:     return mainItems;
    case MENU_SETTINGS: return settingsItems;
    case MENU_SPEED:    return speedItems;
  }
  return nullptr;
}

// OLED menü rajzolása, csak ha valami változott
void redrawMenuIfNeeded() {
  if (currentMenu == lastMenu &&
      selectedIndex == lastSelectedIndex &&
      viewOffset == lastViewOffset) {
    return; // semmi nem változott
  }

  int count = getMenuItemCount(currentMenu);
  if (count <= 0) return;
  if (selectedIndex < 0) selectedIndex = 0;
  if (selectedIndex >= count) selectedIndex = count - 1;

  // Görgetés logika: gondoskodjunk róla, hogy
  // a kijelolt sor benne legyen a lathato savban
  if (selectedIndex < viewOffset) {
    viewOffset = selectedIndex;
  } else if (selectedIndex >= viewOffset + maxVisibleItems) {
    viewOffset = selectedIndex - maxVisibleItems + 1;
  }

  int maxOffset = count - maxVisibleItems;
  if (maxOffset < 0) maxOffset = 0;
  if (viewOffset < 0) viewOffset = 0;
  if (viewOffset > maxOffset) viewOffset = maxOffset;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Címsor
  display.setCursor(0, 0);
  display.print(menuTitles[currentMenu]);

  // Menüsorok
  const char* const* items = getMenuItems(currentMenu);
  int y = 12; // elso sor y pozicioja

  for (int i = viewOffset; i < count && i < viewOffset + maxVisibleItems; i++) {
    // Jelölés -> nyíl
    display.setCursor(0, y);
    if (i == selectedIndex) {
      display.print(">");
    } else {
      display.print(" ");
    }

    // Menüsor szövege
    display.setCursor(10, y);
    display.print(items[i]);

    y += 10; // kovetkezo sor (kb. 10 pixellel lejjebb)
  }

  // Alsó státuszsor (LED állapot + villogás)
  display.setCursor(0, 54);
  display.print("LED:");
  display.print(ledOn ? "ON " : "OFF");
  display.print(" BLINK:");
  display.print(blinkMode ? "ON" : "OFF");

  display.display();

  lastMenu = currentMenu;
  lastSelectedIndex = selectedIndex;
  lastViewOffset = viewOffset;
}

// Potméter -> kiválasztott sor index
void updateSelectionFromPot() {
  int count = getMenuItemCount(currentMenu);
  if (count <= 0) return;

  int potValue = analogRead(POT_PIN); // 0..1023
  int newIndex = map(potValue, 0, 1023, 0, 5); // itt fix 5 elemre van állítva

  if (newIndex < 0) newIndex = 0;
  if (newIndex >= count) newIndex = count - 1;

  if (newIndex != selectedIndex) {
    selectedIndex = newIndex;
    // a redrawMenuIfNeeded majd újrarajzol
  }
}

// Gomb lenyomás kezelése
void handleButtonPress() {
  Serial.print("Gomb, menu=");
  Serial.print(currentMenu);
  Serial.print(" index=");
  Serial.println(selectedIndex);

  if (currentMenu == MENU_MAIN) {
    switch (selectedIndex) {
      case 0: // LED KI/BE
        ledOn = !ledOn;
        blinkMode = false;
        digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
        Serial.println(ledOn ? "LED BE" : "LED KI");
        break;

      case 1: // LED villogas KI/BE
        blinkMode = !blinkMode;
        if (!blinkMode) {
          digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
          Serial.println("Villogas KI");
        } else {
          Serial.println("Villogas BE");
          lastBlinkTime = millis();
        }
        break;

      case 2: // Beallitasok
        currentMenu = MENU_SETTINGS;
        selectedIndex = 0;
        viewOffset = 0;
        break;
    }
  }
  else if (currentMenu == MENU_SETTINGS) {
    switch (selectedIndex) {
      case 0: // Villogasi sebesseg
        currentMenu = MENU_SPEED;
        selectedIndex = 0;
        viewOffset = 0;
        break;

      case 1: // Vissza fo menube
        currentMenu = MENU_MAIN;
        selectedIndex = 0;
        viewOffset = 0;
        break;
    }
  }
  else if (currentMenu == MENU_SPEED) {
    switch (selectedIndex) {
      case 0: // Lassu
        blinkInterval = 800;
        Serial.println("Sebesseg: LASSU");
        break;
      case 1: // Kozepes
        blinkInterval = 400;
        Serial.println("Sebesseg: KOZEPES");
        break;
      case 2: // Gyors
        blinkInterval = 150;
        Serial.println("Sebesseg: GYORS");
        break;
      case 3: // Vissza beallitasokhoz
        currentMenu = MENU_SETTINGS;
        selectedIndex = 0;
        viewOffset = 0;
        break;
    }
  }

  // garantalt ujrarajzolas
  lastSelectedIndex = -1;
  lastMenu = 255;
  lastViewOffset = -1;
}

// LED villogás logika
void handleBlinking() {
  if (!blinkMode) return;

  unsigned long now = millis();
  if (now - lastBlinkTime >= blinkInterval) {
    lastBlinkTime = now;
    int current = digitalRead(LED_PIN);
    digitalWrite(LED_PIN, current == HIGH ? LOW : HIGH);
  }
}
