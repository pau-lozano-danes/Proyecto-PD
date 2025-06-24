## 🧠 Resumen General
Este programa permite al ESP32-S3 actuar como un lector RFID con interfaz web y pantalla OLED, que puede funcionar en modo cliente WiFi o en modo Access Point (AP). Detecta tarjetas RFID, muestra información en pantalla y sirve una página web con estado y acciones disponibles.

## 📡 Conexión WiFi
Intenta primero conectarse como cliente WiFi usando las credenciales configuradas (clientSSID, clientPassword).

Si no logra conectarse tras varios intentos, activa un punto de acceso (AP) con SSID propio (ESP32-RFID-Reader).

La dirección IP usada se guarda y muestra tanto en el OLED como en la interfaz web.

## 🖥️ Pantalla OLED (SH1107)
Se usa para mostrar:

Estado del sistema (modo actual, IP, etc.).

Mensajes cuando se detecta una tarjeta.

URL asociada a la tarjeta (recortada para mostrar solo una parte).

Indicaciones durante el arranque y conexiones WiFi.

## 🔐 Lector RFID (RC522)
Escanea tarjetas RFID y obtiene el UID (código único).

Compara el UID con una base de datos local de tarjetas (cardDatabase[]).

Si encuentra coincidencia:

Muestra el nombre asociado en la pantalla.

Permite acceder al enlace desde la interfaz web.

Si no está registrada:

Muestra mensaje de "Tarjeta no registrada".

## 🌐 Servidor Web (HTTP)
El ESP32 monta un servidor web accesible desde un navegador, el cual:

Muestra el estado actual (modo, IP, UID detectado).

Muestra botones para:

Abrir el enlace asociado a una tarjeta detectada.

Cambiar entre modo AP y modo cliente.

Acceder a la configuración WiFi (aunque no se guarda permanentemente).

Se actualiza automáticamente cada 10 segundos.

## 🔄 Cambio de Modo (Cliente ↔ AP)
Desde el botón en la web, puedes cambiar de modo:

Si estás en modo AP, puedes intentar reconectar como cliente.

Si estás en modo cliente, puedes pasar al modo AP si hay fallos o por elección.

## 💾 Configuración WiFi
Hay una página para ingresar nuevos datos de WiFi (SSID y contraseña).

Actualmente, estos datos no se guardan en la memoria del ESP32.

Para que persistan después de reiniciar, deberías agregar almacenamiento en EEPROM o SPIFFS.

## 📂 Estructura del Código
Usa las siguientes bibliotecas:

WiFi.h y WebServer.h para red.

MFRC522.h para el lector RFID.

Adafruit_SH110X.h para la pantalla OLED.

Está organizado en funciones como:

connectAsClient(): intenta conectarse a WiFi.

startAPMode(): inicia el modo AP.

handleRoot(): sirve la página principal.

handleWiFiConfig(): sirve la página de configuración WiFi.

updateDisplay(): actualiza la pantalla con mensajes dinámicos.

## 🏷️ Base de Datos de Tarjetas
Es un array de estructuras Card, que contiene:

UID de la tarjeta.

URL asociada.

Nombre descriptivo.

Puedes agregar más tarjetas modificando el arreglo cardDatabase[].

## 🕹️ Interacción Típica
El ESP32 arranca y trata de conectarse como cliente.

Si no puede, crea su propia red WiFi (modo AP).

El usuario accede a la IP mostrada (en pantalla o en consola).

La web permite ver estado y acceder a contenidos de tarjetas RFID reconocidas.# Proyecto-PD
