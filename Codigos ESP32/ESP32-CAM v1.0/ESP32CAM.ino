/**
 * @file main.cpp
 * @brief Implementación de un monitor de composta inteligente con ESP32-CAM.
 *
 * @details Este proyecto implementa una máquina de estados para un sistema de
 * monitoreo de composta. Realiza las siguientes funciones:
 * - Lectura de sensores de humedad, temperatura y gas metano (CH4).
 * - Almacenamiento de un historial de datos en formato CSV en una tarjeta SD.
 * - Creación de un servidor web para visualizar la última lectura.
 * - Proporciona opciones para descargar y borrar el historial desde la web.
 *
 * @author Jesús Eduardo Vázquez Martínez
 * @author Kaory Gissel Contreras Álvarez
 *
 * @date 2025-09-22
 * @version 1.0
 *
 * @note Proyecto desarrollado para el 7º semestre de la carrera de TICS en el
 * Instituto Tecnológico Superior del Occidente del Estado de Hidalgo (ITSOEH).
 *
 * @license Uso personal, académico y de divulgación permitido.
 * Es obligatorio atribuir la autoría en publicaciones derivadas.
 */
 
// --- 1. LIBRERÍAS ---
#include <WiFi.h>
#include <WebServer.h>
#include "FS.h"
#include "SD_MMC.h"
#include <OneWire.h>
#include <DallasTemperature.h>

// --- 2. CONFIGURACIÓN DE PINES (Unificada y clara) ---
const int PIN_HUMEDAD     = 13; // Sensor de Humedad del suelo (FC-28)
const int PIN_GAS         = 12; // Sensor de Gas Metano (MQ-4)
const int PIN_TEMPERATURA = 16; // Sensor de Temperatura (DS18B20)
const int PIN_LED_ROJO    = 33; // LED integrado en la placa ESP32-CAM

// --- 3. CONFIGURACIÓN DE RED Y CICLOS ---
const char* ssid = "ESP32-COMPOSTA";
const char* password = "serviciosocial2025";

// Tiempos para la máquina de estados (en milisegundos)
const long readingDuration = 120000; // 2 minutos en modo lectura
const long serverDuration  = 180000; // 3 minutos en modo servidor
const long sleepDuration   = 300000; // 5 minutos en modo reposo

// --- 4. MÁQUINA DE ESTADOS Y VARIABLES GLOBALES ---
enum State { STATE_READING, STATE_WEBSERVER, STATE_SLEEPING };
State currentState = STATE_READING;
unsigned long stateStartTime = 0;

// Objetos para el servidor y los sensores
WebServer server(80);
OneWire oneWire(PIN_TEMPERATURA);
DallasTemperature sensors(&oneWire);

// Variables para almacenar la última lectura VÁLIDA y CONVERTIDA
float temperatura = 0.0, humedad = 0.0, gas_ppm = 0.0;

// ====================================================================================
// --- 5. FUNCIONES DE LECTURA DE SENSORES ---
// ====================================================================================

float leerHumedad() {
  int valorCrudo = analogRead(PIN_HUMEDAD);
  // Calibración: ajusta 3500 (seco) y 800 (húmedo)
  float valorMapeado = map(valorCrudo, 3500, 800, 0, 100);
  return constrain(valorMapeado, 0, 100);
}

float leerTemperatura() {
  sensors.requestTemperatures(); 
  return sensors.getTempCByIndex(0);
}

float leerGasMetano() {
  int valorCrudoGas = analogRead(PIN_GAS);
  // Aproximación lineal. Para mayor precisión, se requiere calibración.
  return (valorCrudoGas / 4095.0) * 10000;
}

void readAllSensors() {
  humedad = leerHumedad();
  gas_ppm = leerGasMetano();
  temperatura = leerTemperatura();
  
  // Imprime en el monitor serie los datos ya convertidos
  Serial.printf("Lectura actual: Temp=%.2f C, Hum=%.2f %%, Gas=%.2f ppm\n", temperatura, humedad, gas_ppm);
}

// ====================================================================================
// --- 6. FUNCIONES DE GESTIÓN DE DATOS (SD CARD) ---
// ====================================================================================

void logDataToSD() {
  File dataFile = SD_MMC.open("/datalog.csv", FILE_APPEND);
  if (dataFile) {
    // Se guardan los datos ya convertidos y legibles
    String dataString = String(millis() / 1000) + "," + String(temperatura, 2) + "," + String(humedad, 2) + "," + String(gas_ppm, 2);
    dataFile.println(dataString);
    dataFile.close();
    Serial.println(">>> Datos guardados en SD: " + dataString);
  } else {
    Serial.println("Error al abrir datalog.csv para escribir");
  }
}

void deleteLogs() {
  Serial.println("Borrando archivo de registros...");
  SD_MMC.remove("/datalog.csv");
  File newFile = SD_MMC.open("/datalog.csv", FILE_WRITE);
  if (newFile) {
    newFile.println("tiempo_seg,temperatura_C,humedad_porc,gas_ppm");
    newFile.close();
    Serial.println("Archivo de registros recreado.");
  }
}

// ====================================================================================
// --- 7. FUNCIONES DEL SERVIDOR WEB (HTML ACTUALIZADO) ---
// ====================================================================================

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html><html><head><title>ESP32-COMPOSTA</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
    body{font-family:Arial,sans-serif;text-align:center;margin-top:20px;background-color:#f4f4f4;}
    .sensor-data{margin:30px;padding:20px;background-color:white;border-radius:10px;box-shadow:0 2px 4px rgba(0,0,0,0.1);}
    .sensor-data p{font-size:22px;margin:10px;}
    .sensor-data span{font-weight:bold;color:#0056b3;}
    .button-container{display:flex;flex-direction:column;align-items:center;gap:15px;margin-top:30px;}
    .button{display:block;width:80%;max-width:300px;padding:20px;font-size:20px;font-weight:bold;color:white;border:none;border-radius:10px;text-decoration:none;}
    .btn-csv{background-color:#4CAF50;}
    .btn-delete{background-color:#f44336;}
</style></head><body>
<h1>Panel de Control - Composta</h1>
<div class="sensor-data">
    <h2>&Uacute;ltima Lectura Guardada</h2>
    <p>&#x1F525; Temperatura: <span>)rawliteral";
  html += String(temperatura, 2) + R"rawliteral(</span> &deg;C</p>
    <p>&#x1F4A7; Humedad: <span>)rawliteral";
  html += String(humedad, 2) + R"rawliteral(</span> %</p>
    <p>&#x1F32A;&#xFE0F; Gas Metano: <span>)rawliteral";
  html += String(gas_ppm, 2) + R"rawliteral(</span> ppm</p>
</div>
<div class="button-container">
    <a href="/csv" class="button btn-csv" download="datalog.csv">Descargar Historial (CSV)</a>
    <a href="/delete" class="button btn-delete">Borrar Registros</a>
</div></body></html>)rawliteral";
  server.send(200, "text/html", html);
}

void handleCsv() {
  File dataFile = SD_MMC.open("/datalog.csv");
  if (dataFile) {
    server.streamFile(dataFile, "text/csv");
    dataFile.close();
  } else {
    server.send(404, "text/plain", "Archivo no encontrado");
  }
}

void handleDelete() {
  deleteLogs();
  String html = "<h1>Registros borrados</h1><p>El archivo datalog.csv ha sido eliminado y recreado.</p><a href='/'>Volver</a>";
  server.send(200, "text/html", html);
}

// ====================================================================================
// --- 8. SETUP Y LOOP PRINCIPAL ---
// ====================================================================================

void setup() {
  Serial.begin(115200);
  sensors.begin();
  
  pinMode(PIN_LED_ROJO, OUTPUT);

  // --- Montar la tarjeta SD ---
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("Error al montar la tarjeta SD. Reiniciando...");
    ESP.restart();
  }
  
  File dataFile = SD_MMC.open("/datalog.csv");
  if (!dataFile) {
    deleteLogs(); // Crea el archivo con encabezados si no existe
  }
  dataFile.close();

  // --- Configurar rutas del servidor web ---
  server.on("/", handleRoot);
  server.on("/csv", handleCsv);
  server.on("/delete", handleDelete);

  // --- Precalentamiento del sensor de gas ---
  Serial.println("Calentando el sensor de gas MQ-4 (1 minuto)...");
  digitalWrite(PIN_LED_ROJO, LOW); // Encender LED rojo durante el calentamiento
  delay(60000); // Espera 1 minuto para estabilizar el sensor
  digitalWrite(PIN_LED_ROJO, HIGH);
  Serial.println("Sensor de gas listo.");

  // --- Iniciar la máquina de estados ---
  currentState = STATE_READING;
  stateStartTime = millis();
  WiFi.mode(WIFI_OFF);
  Serial.println("Iniciando ciclo en MODO_LECTURA...");
}

void loop() {
  unsigned long currentTime = millis();

  switch (currentState) {
    case STATE_READING:
      digitalWrite(PIN_LED_ROJO, LOW); // LED encendido = leyendo
      readAllSensors();
      
      if (currentTime - stateStartTime >= readingDuration) {
        logDataToSD();
        currentState = STATE_WEBSERVER;
        stateStartTime = currentTime;
        Serial.println("Cambiando a MODO_SERVIDOR...");
        WiFi.softAP(ssid, password);
        server.begin();
      }
      delay(5000); // Tomar una lectura cada 5 segundos dentro del modo lectura
      break;

    case STATE_WEBSERVER:
      digitalWrite(PIN_LED_ROJO, HIGH); // LED apagado = servidor activo
      server.handleClient();
      
      if (currentTime - stateStartTime >= serverDuration) {
          currentState = STATE_SLEEPING;
          stateStartTime = currentTime;
          Serial.println("Cambiando a MODO_REPOSO...");
          server.stop();
          WiFi.mode(WIFI_OFF);
      }
      break;

    case STATE_SLEEPING:
      // LED parpadea lentamente para indicar modo reposo
      if ( (currentTime / 500) % 2 == 0 ) {
        digitalWrite(PIN_LED_ROJO, LOW);
      } else {
        digitalWrite(PIN_LED_ROJO, HIGH);
      }

      if (currentTime - stateStartTime >= sleepDuration) {
        currentState = STATE_READING;
        stateStartTime = currentTime;
        Serial.println("Fin del reposo. Volviendo a MODO_LECTURA...");
      }
      break;
  }
}