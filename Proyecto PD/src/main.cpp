#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// Configuración de pines para ESP32-S3
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_CS      10  // GPIO10
#define OLED_DC      39  // GPIO39
#define OLED_RST     40  // GPIO40
Adafruit_SH1107 display(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, OLED_DC, OLED_RST, OLED_CS);

#define RST_PIN     6   // GPIO6 para RC522
#define SS_PIN      7   // GPIO7 para RC522
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Pines SPI para RC522
#define SCK_PIN     12  // GPIO12
#define MOSI_PIN    11  // GPIO11
#define MISO_PIN    13  // GPIO13

WebServer server(80);

// Configuración WiFi
const char *apSSID = "ESP32-RFID-Reader";  // Nombre del AP cuando actúa como Access Point
const char *apPassword = "12345678";       // Contraseña del AP (mínimo 8 caracteres)

// Configuración WiFi como cliente (MODIFICA CON TUS DATOS)
const char *clientSSID = "MOVISTAR_451D";   // Nombre de tu red WiFi
const char *clientPassword = "zHz4mkV8VTciyn8P6HyJ"; // Contraseña de tu red WiFi
bool useStaticIP = false;                   // Usar IP estática (false para DHCP)

// IP estática (opcional)
IPAddress local_ip(192, 168, 1, 100);       // Dirección IP que tendrá el ESP32
IPAddress gateway(192, 168, 1, 1);          // Puerta de enlace
IPAddress subnet(255, 255, 255, 0);         // Máscara de subred

// Estructura para almacenar tarjetas y URLs
struct Card {
  String uid;
  String url;
  String name;
};

// Base de datos de tarjetas
Card cardDatabase[] = {
  {"6B695BDE", "https://eseiaat.upc.edu/ca", "ESEIAAT UPC"},
  {"8B6295DB", "https://github.com/pau-lozano-danes", "GitHub Pau Lozano"},
  {"C2981A06", "https://github.com/mauricioudsx", "GitHub Mauricio Urbina"},
  {"CB3B90DB", "https://futur.upc.edu/ManuelLopezPalma", "FUTUR Manuel Palma"}
};
const int cardCount = sizeof(cardDatabase) / sizeof(Card);

String currentUID = "";
unsigned long lastReadTime = 0;
const int displayTime = 5000;
String ipAddress = "";
bool isAPMode = false;  // false = Modo cliente, true = Modo AP

// Declaraciones anticipadas
void handleRoot();
void handleCardPage(int cardIndex);
void handleNotFound();
void handleWiFiConfig();
void handleSwitchMode();
void updateDisplay(String message);
void connectAsClient();
void startAPMode();

void setup() {
  Serial.begin(115200);
  
  // Inicialización SPI para RC522
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  
  // Inicialización pantalla SH1107
  if(!display.begin(1000000)) {  // 1MHz para SPI
    Serial.println(F("Error en pantalla OLED"));
    while(1);
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0,0);
  display.println("Iniciando sistema...");
  display.display();

  // Intentar conectar como cliente primero
  connectAsClient();

  // Si no se pudo conectar como cliente, iniciar modo AP
  if (WiFi.status() != WL_CONNECTED) {
    startAPMode();
  }

  // Inicialización RC522
  mfrc522.PCD_Init();
  Serial.println("Lector RC522 inicializado");

  // Configuración servidor web
  server.on("/", handleRoot);
  server.on("/wifi-config", handleWiFiConfig);
  server.on("/switch-mode", handleSwitchMode);
  for (int i = 0; i < cardCount; i++) {
    server.on("/card" + String(i), [i]() {
      handleCardPage(i);
    });
  }
  server.onNotFound(handleNotFound);
  server.begin();

  updateDisplay(isAPMode ? "Modo AP activo" : "Modo Cliente activo");
}

void connectAsClient() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Conectando como cliente");
  display.println("Red: " + String(clientSSID));
  display.display();
  
  if(useStaticIP) {
    WiFi.config(local_ip, gateway, subnet);
  }
  
  WiFi.begin(clientSSID, clientPassword);
  
  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 20) {
    delay(500);
    display.print(".");
    display.display();
    intentos++;
  }
  
  display.clearDisplay();
  display.setCursor(0,0);
  if(WiFi.status() == WL_CONNECTED) {
    isAPMode = false;
    ipAddress = WiFi.localIP().toString();
    display.println("WiFi conectado!");
    display.println("IP: " + ipAddress);
    Serial.print("Conectado como cliente. IP: ");
    Serial.println(ipAddress);
  } else {
    display.println("Error conexión WiFi");
    display.println("Iniciando modo AP...");
    display.display();
    delay(2000);
    return;
  }
  display.display();
  delay(2000);
}

void startAPMode() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Iniciando modo AP...");
  display.println("SSID: " + String(apSSID));
  display.println("Pass: " + String(apPassword));
  display.display();
  
  WiFi.softAP(apSSID, apPassword);
  isAPMode = true;
  ipAddress = WiFi.softAPIP().toString();
  
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Modo AP activo!");
  display.println("IP: " + ipAddress);
  display.println("Conéctate a:");
  display.println(apSSID);
  display.display();
  
  Serial.print("Modo AP iniciado. IP: ");
  Serial.println(ipAddress);
  delay(2000);
}

void loop() {
  server.handleClient();
  
  if (currentUID != "" && millis() - lastReadTime > displayTime) {
    currentUID = "";
    updateDisplay(isAPMode ? "Modo AP activo" : "Modo Cliente activo");
  }
  
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    String uid = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      uid += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
      uid += String(mfrc522.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();
    
    bool cardFound = false;
    for (int i = 0; i < cardCount; i++) {
      if (uid == cardDatabase[i].uid) {
        cardFound = true;
        currentUID = uid;
        lastReadTime = millis();
        updateDisplay("Acceso: " + cardDatabase[i].name);
        Serial.println("Tarjeta reconocida: " + uid);
        Serial.println("Redirigiendo a: " + cardDatabase[i].url);
        break;
      }
    }
    
    if (!cardFound) {
      updateDisplay("Tarjeta no registrada");
      Serial.println("Tarjeta no registrada: " + uid);
    }
    
    mfrc522.PICC_HaltA();
  }
}

void updateDisplay(String message) {
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("ESP32-S3 RFID Reader");
  display.println("------------------");
  display.println("Modo: " + String(isAPMode ? "AP" : "Cliente"));
  display.println(message);
  
  if (currentUID != "") {
    for (int i = 0; i < cardCount; i++) {
      if (currentUID == cardDatabase[i].uid) {
        display.println("URL: " + cardDatabase[i].url.substring(0, 15) + "...");
        break;
      }
    }
    display.println("IP: " + ipAddress);
  }
  
  display.display();
}

void handleRoot() {
  String html = R"=====(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Lector RFID - ESP32-S3</title>
  <style>
    :root {
      --primary: #2563eb;
      --secondary: #1e40af;
      --bg: #f8fafc;
      --card-bg: #ffffff;
      --text: #1e293b;
      --text-light: #64748b;
      --warning: #d97706;
      --success: #059669;
    }
    
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      line-height: 1.6;
      color: var(--text);
      background-color: var(--bg);
      margin: 0;
      padding: 20px;
      max-width: 800px;
      margin: 0 auto;
    }
    
    header {
      text-align: center;
      margin-bottom: 2rem;
      padding-bottom: 1rem;
      border-bottom: 1px solid #e2e8f0;
    }
    
    h1 {
      color: var(--primary);
      margin-bottom: 0.5rem;
    }
    
    .mode-indicator {
      padding: 0.5rem 1rem;
      border-radius: 9999px;
      font-weight: 600;
      background-color: )=====";
  
  html += isAPMode ? "var(--warning); color: white;" : "var(--success); color: white;";
  
  html += R"=====(
    }
    
    .status-card {
      background: var(--card-bg);
      border-radius: 12px;
      padding: 2rem;
      margin-bottom: 2rem;
      box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
      transition: all 0.3s ease;
    }
    
    .status-card:hover {
      box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.1);
    }
    
    .tag {
      display: inline-block;
      background: #e0f2fe;
      color: #0369a1;
      padding: 0.25rem 0.75rem;
      border-radius: 9999px;
      font-size: 0.875rem;
      font-weight: 500;
    }
    
    .btn {
      display: inline-block;
      background: var(--primary);
      color: white;
      padding: 0.75rem 1.5rem;
      border-radius: 8px;
      text-decoration: none;
      font-weight: 600;
      transition: background 0.2s;
      margin-top: 1rem;
      margin-right: 0.5rem;
      border: none;
      cursor: pointer;
    }
    
    .btn:hover {
      background: var(--secondary);
    }
    
    .btn-warning {
      background: var(--warning);
    }
    
    .btn-warning:hover {
      background: #b45309;
    }
    
    .hidden {
      display: none;
    }
    
    footer {
      text-align: center;
      margin-top: 3rem;
      color: var(--text-light);
      font-size: 0.875rem;
    }
    
    .form-group {
      margin-bottom: 1rem;
    }
    
    label {
      display: block;
      margin-bottom: 0.5rem;
      font-weight: 500;
    }
    
    input {
      width: 100%;
      padding: 0.75rem;
      border: 1px solid #e2e8f0;
      border-radius: 8px;
      font-size: 1rem;
    }
  </style>
</head>
<body>
  <header>
    <h1>Sistema de Acceso RFID</h1>
    <div class="mode-indicator">Modo: )=====";
  html += isAPMode ? "Access Point" : "Cliente WiFi";
  html += R"=====(</div>
    <p>Conectado a: )=====";
  html += ipAddress;
  html += R"=====(</p>
  </header>
  
  <div class="status-card" id="card-status">
    <h2>Estado Actual</h2>
    <p id="status-message">)=====";

  if (currentUID != "") {
    html += "Tarjeta detectada: <span class='tag'>" + currentUID + "</span>";
  } else {
    html += "No se detectó ninguna tarjeta";
  }

  html += R"=====(</p>
  </div>
  
  <div class="status-card" id="card-content" style=")=====";
  html += (currentUID != "") ? "" : "display: none;";
  html += R"=====(">
    <h2>Contenido Disponible</h2>
    <div id="card-links">)=====";

  if (currentUID != "") {
    for (int i = 0; i < cardCount; i++) {
      if (currentUID == cardDatabase[i].uid) {
        html += "<a href='" + cardDatabase[i].url + "' class='btn' target='_blank'>Abrir " + cardDatabase[i].name + "</a>";
        break;
      }
    }
  }

  html += R"=====(</div>
  </div>
  
  <div class="status-card">
    <h2>Configuración WiFi</h2>
    <p>Estado actual: <strong>)=====";
  html += isAPMode ? "Modo Access Point (AP)" : "Conectado a " + String(clientSSID);
  html += R"=====(</strong></p>
    <a href="/wifi-config" class="btn">Configurar WiFi</a>
    <form action="/switch-mode" method="POST" style="display: inline;">
      <button type="submit" class="btn btn-warning">)=====";
  html += isAPMode ? "Intentar conectar como cliente" : "Cambiar a modo AP";
  html += R"=====(</button>
    </form>
  </div>
  
  <footer>
    <p>Sistema RFID con ESP32-S3 &copy; 2023</p>
  </footer>
  
  <script>
    setTimeout(function(){
      window.location.reload();
    }, 10000);
  </script>
</body>
</html>
)=====";

  server.send(200, "text/html", html);
}

void handleWiFiConfig() {
  if (server.method() == HTTP_POST) {
    // Procesar configuración WiFi enviada por el formulario
    String newSSID = server.arg("ssid");
    String newPass = server.arg("password");
    
    // Aquí podrías guardar estos valores en EEPROM o similar
    // Por simplicidad, en este ejemplo solo mostramos un mensaje
    
    String html = R"=====(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Configuración WiFi</title>
  <style>
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background: #f8fafc;
      color: #1e293b;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      height: 100vh;
      margin: 0;
      text-align: center;
    }
    .message {
      background: white;
      padding: 2rem;
      border-radius: 12px;
      box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
      max-width: 500px;
    }
    .btn {
      display: inline-block;
      background: #2563eb;
      color: white;
      padding: 0.75rem 1.5rem;
      border-radius: 8px;
      text-decoration: none;
      font-weight: 600;
      transition: background 0.2s;
      margin-top: 1rem;
    }
    .btn:hover {
      background: #1e40af;
    }
  </style>
</head>
<body>
  <div class="message">
    <h1>Configuración WiFi recibida</h1>
    <p>SSID: )=====";
    html += server.arg("ssid");
    html += R"=====(</p>
    <p>El sistema intentará conectarse con estas credenciales.</p>
    <p><strong>Nota:</strong> En este ejemplo, los cambios no se guardan permanentemente. Debes implementar EEPROM o similar para almacenamiento persistente.</p>
    <a href="/" class="btn">Volver al inicio</a>
  </div>
</body>
</html>
)=====";
    server.send(200, "text/html", html);
    return;
  }

  // Mostrar formulario de configuración WiFi
  String html = R"=====(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Configuración WiFi</title>
  <style>
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      line-height: 1.6;
      color: #1e293b;
      background-color: #f8fafc;
      margin: 0;
      padding: 20px;
      max-width: 800px;
      margin: 0 auto;
    }
    h1 {
      color: #2563eb;
      margin-bottom: 1.5rem;
    }
    .card {
      background: white;
      border-radius: 12px;
      padding: 2rem;
      margin-bottom: 2rem;
      box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
    }
    .form-group {
      margin-bottom: 1.5rem;
    }
    label {
      display: block;
      margin-bottom: 0.5rem;
      font-weight: 500;
    }
    input {
      width: 100%;
      padding: 0.75rem;
      border: 1px solid #e2e8f0;
      border-radius: 8px;
      font-size: 1rem;
    }
    .btn {
      display: inline-block;
      background: #2563eb;
      color: white;
      padding: 0.75rem 1.5rem;
      border-radius: 8px;
      text-decoration: none;
      font-weight: 600;
      transition: background 0.2s;
      border: none;
      cursor: pointer;
    }
    .btn:hover {
      background: #1e40af;
    }
    .btn-secondary {
      background: #64748b;
      margin-left: 0.5rem;
    }
    .btn-secondary:hover {
      background: #475569;
    }
  </style>
</head>
<body>
  <h1>Configuración WiFi</h1>
  
  <div class="card">
    <form action="/wifi-config" method="POST">
      <div class="form-group">
        <label for="ssid">Nombre de la red (SSID)</label>
        <input type="text" id="ssid" name="ssid" placeholder="Nombre de tu red WiFi" value=")=====";
  html += clientSSID;
  html += R"=====(" required>
      </div>
      
      <div class="form-group">
        <label for="password">Contraseña</label>
        <input type="password" id="password" name="password" placeholder="Contraseña de tu red WiFi" value=")=====";
  html += clientPassword;
  html += R"=====(" required>
      </div>
      
      <button type="submit" class="btn">Guardar configuración</button>
      <a href="/" class="btn btn-secondary">Cancelar</a>
    </form>
  </div>
</body>
</html>
)=====";

  server.send(200, "text/html", html);
}

void handleSwitchMode() {
  if (server.method() == HTTP_POST) {
    if (isAPMode) {
      // Intentar conectar como cliente
      connectAsClient();
    } else {
      // Cambiar a modo AP
      startAPMode();
    }
    
    // Redirigir a la página principal
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  }
}

void handleCardPage(int cardIndex) {
  String html = R"=====(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Redireccionando...</title>
  <meta http-equiv="refresh" content="0;url=)=====";
  html += cardDatabase[cardIndex].url;
  html += R"=====(">
  <style>
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background: #f8fafc;
      color: #1e293b;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      height: 100vh;
      margin: 0;
      text-align: center;
    }
    .loader {
      width: 50px;
      height: 50px;
      border: 5px solid #e2e8f0;
      border-top-color: #2563eb;
      border-radius: 50%;
      animation: spin 1s linear infinite;
      margin-bottom: 2rem;
    }
    @keyframes spin {
      to { transform: rotate(360deg); }
    }
    h1 {
      color: #2563eb;
      margin-bottom: 1rem;
    }
    a {
      color: #2563eb;
      text-decoration: none;
      font-weight: 600;
    }
    a:hover {
      text-decoration: underline;
    }
  </style>
</head>
<body>
  <div class="loader"></div>
  <h1>Redireccionando...</h1>
  <p>Estás siendo dirigido a: <strong>)=====";
  html += cardDatabase[cardIndex].name;
  html += R"=====(</strong></p>
  <p>Si no eres redirigido automáticamente, <a href=")=====";
  html += cardDatabase[cardIndex].url;
  html += R"=====(">haz clic aquí</a></p>
  <p><a href="/">← Volver al inicio</a></p>
</body>
</html>
)=====";

  server.send(200, "text/html", html);
}

void handleNotFound() {
  String message = "Página no encontrada\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMétodo: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArgumentos: ";
  message += server.args();
  message += "\n";
  
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  
  server.send(404, "text/plain", message);
}