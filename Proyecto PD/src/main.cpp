#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SS_PIN 5
#define RST_PIN 22
#define BUTTON_PIN 0

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

MFRC522 mfrc522(SS_PIN, RST_PIN);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

WebServer server(80);

bool writeMode = false;
String targetUrl = "https://openai.com";  // URL por defecto
String lastReadUrl = "";

void mostrarModo() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Modo actual:");
  display.setCursor(0, 20);
  display.setTextSize(2);
  display.println(writeMode ? "ESCRITURA" : "LECTURA");
  display.display();
}

void escribirNFC() {
  if (!Serial.available()) return;

  targetUrl = Serial.readStringUntil('\n');
  targetUrl.trim();
  if (!targetUrl.startsWith("http://") && !targetUrl.startsWith("https://")) {
    targetUrl = "https://" + targetUrl;
  }

  Serial.print("Escribiendo: ");
  Serial.println(targetUrl);

  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) return;

  byte blockNum = 4;
  byte prefix = 0x04; // "https://"
  byte urlPart[32];
  int len = targetUrl.length();
  len = len > 30 ? 30 : len;

  memcpy(urlPart, targetUrl.c_str() + (targetUrl.startsWith("https://") ? 8 : 0), len);
  byte payload[16] = { 0xD1, 0x01, (byte)(len + 1), 0x55, prefix };
  memcpy(payload + 5, urlPart, len);

  MFRC522::MIFARE_Key key;
  for (int i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  MFRC522::StatusCode status = mfrc522.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockNum, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) return;

  status = mfrc522.MIFARE_Write(blockNum, payload, 16);
  if (status == MFRC522::STATUS_OK) {
    Serial.println("URL escrita exitosamente.");
  } else {
    Serial.println("Error al escribir.");
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

String getUrlPrefix(byte code) {
  switch (code) {
    case 0x00: return ""; case 0x01: return "http://www.";
    case 0x02: return "https://www."; case 0x03: return "http://";
    case 0x04: return "https://";
    default: return "";
  }
}

void leerNFC() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) return;

  byte block[18];
  byte size = 18;
  MFRC522::MIFARE_Key key;
  for (int i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  if (mfrc522.PCD_Authenticate(
        MFRC522::PICC_CMD_MF_AUTH_KEY_A, 4, &key, &(mfrc522.uid)) != MFRC522::STATUS_OK) return;

  if (mfrc522.MIFARE_Read(4, block, &size) != MFRC522::STATUS_OK) return;

  if (block[0] == 0xD1 && block[3] == 'U') {
    int len = block[2];
    byte prefix = block[4];
    lastReadUrl = getUrlPrefix(prefix);
    for (int i = 5; i < 5 + len - 1; i++) {
      lastReadUrl += (char)block[i];
    }
    Serial.println("URL leÃ­da: " + lastReadUrl);
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

void handleRoot() {
  server.send(200, "text/html", "<h1>ESP32 NFC</h1><a href='/open'>Abrir enlace</a>");
}

void handleOpenLink() {
  if (lastReadUrl != "") {
    server.sendHeader("Location", lastReadUrl, true);
    server.send(302, "text/plain", "");
  } else {
    server.send(404, "text/plain", "No se ha leÃ­do ningÃºn enlace");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  SPI.begin();
  mfrc522.PCD_Init();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED no encontrada");
    while (true);
  }
  mostrarModo();

  WiFi.softAP("ESP32-NFC", "12345678");
  Serial.println("AP Iniciado");
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/open", handleOpenLink);
  server.begin();
}

void loop() {
  static bool lastButtonState = HIGH;
  bool currentButtonState = digitalRead(BUTTON_PIN);

  if (lastButtonState == HIGH && currentButtonState == LOW) {
    writeMode = !writeMode;
    mostrarModo();
    delay(300);  // Antirrebote
  }
  lastButtonState = currentButtonState;

  if (writeMode) {
    escribirNFC();
  } else {
    leerNFC();
  }

  server.handleClient();
}