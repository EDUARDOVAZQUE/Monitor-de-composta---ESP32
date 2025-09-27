/**
 * @file main_online.cpp
 * @brief Monitor de Composta - Versión Online con Sincronización de Hora (NTP).
 *
 * @details Este proyecto se conecta a internet para obtener la fecha y hora
 * exactas y usarlas como timestamp para los registros de datos.
 * - Almacena datos en SPIFFS con un timestamp preciso (ej: '2025-09-24 22:01:27').
 * - Si no hay conexión a internet, utiliza los segundos desde el arranque como timestamp.
 * - Mantiene todas las funcionalidades de la versión offline: servidor web,
 * botón para AP, descarga y borrado de datos.
 *
 * @author Jesús Eduardo Vázquez Martínez
 * @author Kaory Gissel Contreras Álvarez
 *
 * @date 2025-09-22
 * @version 1.2 (ESP32-ONLINE_TIME)
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
#include <FS.h>
#include <SPIFFS.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "time.h" // Librería para la funcionalidad de tiempo (NTP)

// --- 2. CONFIGURACIÓN DE PINES ---
const int PIN_HUMEDAD     = 34; // Sensor de Humedad del suelo (FC-28)
const int PIN_GAS         = 35; // Sensor de Gas Metano (MQ-4)
const int PIN_TEMPERATURA = 22; // Sensor de Temperatura (DS18B20)
const int PIN_BOTON_AP    = 27; // Botón para activar/desactivar el AP

// --- 3. CONFIGURACIÓN DE RED ---
// Red a la que se conectará para obtener la hora (Modo Cliente)
const char* WIFI_SSID = "tu red de internet"; 
const char* WIFI_PASSWORD = "tu constraseña";

// Red que creará el ESP32 (Modo Access Point)
const char* AP_SSID = "ESP32-COMPOSTA";
const char* AP_PASSWORD = "serviciosocial2025";

// Configuración NTP (Protocolo de Tiempo de Red)
const char* NTP_SERVER = "pool.ntp.org";
const long  GMT_OFFSET_SEC = -21600; // UTC-6 (Progreso, Hidalgo). Ajustar si es necesario.
const int   DAYLIGHT_OFFSET_SEC = 0;

// --- 4. VARIABLES GLOBALES ---
WebServer server(80);
OneWire oneWire(PIN_TEMPERATURA);
DallasTemperature sensors(&oneWire);

float temperatura = 0.0, humedad = 0.0, gas_ppm = 0.0;
bool timeIsSynced = false; // Flag para saber si tenemos la hora de internet

bool apActivo = false;
volatile bool botonPresionado = false;
unsigned long ultimaLecturaMillis = 0;
unsigned long ultimoLogMillis = 0;

// ====================================================================================
// --- 5. FUNCIONES DE TIEMPO (NUEVO) ---
// ====================================================================================

void initWiFi() {
    Serial.printf("Conectando a %s ", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    // Esperar hasta 20 segundos para conectar
    int tryCount = 0;
    while (WiFi.status() != WL_CONNECTED && tryCount < 20) {
        delay(1000);
        Serial.print(".");
        tryCount++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(" CONECTADO!");
        // Configurar el cliente NTP
        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
        timeIsSynced = true;
    } else {
        Serial.println(" No se pudo conectar a WiFi. Se usará el tiempo de actividad.");
        timeIsSynced = false;
    }
}

String getTimestamp() {
    if (!timeIsSynced) {
        // Plan de respaldo: devolver segundos desde el arranque
        return String(millis() / 1000);
    }
    
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Error al obtener la hora local");
        return "error_time";
    }
    
    char timeString[20];
    // Formato: AAAA-MM-DD HH:MM:SS
    strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(timeString);
}

// ====================================================================================
// --- 6. GESTIÓN DE DATOS (SPIFFS) ---
// ====================================================================================

void logDataToSPIFFS() {
    String timestamp = getTimestamp();
    File dataFile = SPIFFS.open("/datalog.csv", FILE_APPEND);
    if (dataFile) {
        String dataString = timestamp + "," + String(temperatura, 2) + "," + String(humedad, 2) + "," + String(gas_ppm, 2);
        dataFile.println(dataString);
        dataFile.close();
        Serial.println(">>> Dato guardado en SPIFFS: " + dataString);
    } else {
        Serial.println("Error al abrir datalog.csv para escribir");
    }
}

void borrarRegistros() {
    Serial.println("Borrando archivo de registros...");
    SPIFFS.remove("/datalog.csv");
    // Recrear el archivo con los nuevos encabezados
    File newFile = SPIFFS.open("/datalog.csv", FILE_WRITE);
    if (newFile) {
        newFile.println("timestamp,temperatura_C,humedad_porc,gas_ppm");
        newFile.close();
        Serial.println("Archivo de registros recreado.");
    }
}

// ====================================================================================
// --- 7. SERVIDOR WEB (HTML MODIFICADO) ---
// ====================================================================================
void handleRoot() {
    String html = R"rawliteral(
<!DOCTYPE html><html><head><title>ESP32 Composta Online</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
 body{font-family:Arial,sans-serif;text-align:center;margin-top:20px;background-color:#f4f4f4;}
 .sensor-data{margin:30px;padding:20px;background-color:white;border-radius:10px;box-shadow:0 2px 4px rgba(0,0,0,0.1);}
 .sensor-data p{font-size:22px;margin:10px;} .sensor-data span{font-weight:bold;color:#0056b3;}
 .button-container{display:flex;flex-direction:column;align-items:center;gap:15px;margin-top:30px;}
 .button{display:block;width:80%;max-width:300px;padding:20px;font-size:20px;font-weight:bold;color:white;border:none;border-radius:10px;text-decoration:none; cursor:pointer;}
 .btn-csv{background-color:#4CAF50;} .btn-delete{background-color:#f44336;}
</style></head><body>
<h1>&#x1F331; Monitor de Composta (Online)</h1>
<div class="sensor-data">
 <h2>Datos en Tiempo Real</h2>
 <p>&#x1F552; Hora Actual: <span id="time">--</span></p>
 <p>&#x1F525; Temperatura: <span id="temp">--</span> &deg;C</p>
 <p>&#x1F4A7; Humedad: <span id="hum">--</span> %</p>
 <p>&#x1F32A;&#xFE0F; Gas Metano: <span id="gas">--</span> ppm</p>
</div>
<div class="button-container">
 <a href="/csv" class="button btn-csv" download="datalog.csv">Descargar Historial (CSV)</a>
 <a href="/delete" class="button btn-delete">Borrar Registros</a>
</div>
<script>
 setInterval(function() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
   if (this.readyState == 4 && this.status == 200) {
    var data = JSON.parse(this.responseText);
    document.getElementById("time").innerHTML = data.timestamp;
    document.getElementById("temp").innerHTML = data.temperatura;
    document.getElementById("hum").innerHTML = data.humedad;
    document.getElementById("gas").innerHTML = data.gas_ppm;
   }
  };
  xhttp.open("GET", "/data.json", true);
  xhttp.send();
 }, 1000);
</script></body></html>)rawliteral";
    server.send(200, "text/html", html);
}

void handleDataJson() {
    String json = "{";
    json += "\"timestamp\":\"" + getTimestamp() + "\",";
    json += "\"temperatura\":" + String(temperatura, 2) + ",";
    json += "\"humedad\":" + String(humedad, 2) + ",";
    json += "\"gas_ppm\":" + String(gas_ppm, 2);
    json += "}";
    server.send(200, "application/json", json);
}

void leerSensores() {
    humedad = map(analogRead(PIN_HUMEDAD), 3500, 800, 0, 100);
    humedad = constrain(humedad, 0, 100);
    gas_ppm = (analogRead(PIN_GAS) / 4095.0) * 10000;
    sensors.requestTemperatures();
    temperatura = sensors.getTempCByIndex(0);
}

void handleCsv() {
    File dataFile = SPIFFS.open("/datalog.csv", "r");
    if (dataFile) server.streamFile(dataFile, "text/csv");
    else server.send(404, "text/plain", "Archivo no encontrado");
    dataFile.close();
}

void handleDelete() {
    borrarRegistros();
    server.send(200, "text/html", "<h1>Registros borrados</h1><p>Archivo recreado.</p><a href='/'>Volver</a>");
}

void IRAM_ATTR onBotonPresionado() {
    botonPresionado = true;
}

void toggleAP() {
    apActivo = !apActivo;
    if (apActivo) {
        WiFi.softAP(AP_SSID, AP_PASSWORD);
        server.begin();
        Serial.printf("AP '%s' iniciado. IP: %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
    } else {
        server.stop();
        WiFi.softAPdisconnect(true);
        Serial.println("Access Point desactivado.");
    }
}

// ====================================================================================
// --- 8. SETUP Y LOOP PRINCIPAL ---
// ====================================================================================
void setup() {
    Serial.begin(115200);
    sensors.begin();
    
    if (!SPIFFS.begin(true)) {
        Serial.println("Error al montar SPIFFS.");
        return;
    }
    
    // Si el archivo de log no existe, lo creamos con el nuevo encabezado
    if (!SPIFFS.exists("/datalog.csv")) {
        borrarRegistros();
    }
    
    // Intentar conectar a WiFi para sincronizar la hora
    initWiFi();

    pinMode(PIN_BOTON_AP, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_BOTON_AP), onBotonPresionado, FALLING);

    server.on("/", handleRoot);
    server.on("/data.json", handleDataJson);
    server.on("/csv", handleCsv);
    server.on("/delete", handleDelete);

    Serial.println("Calentando sensor de gas (30 segundos)...");
    delay(30000);
    Serial.println("Sensor listo. Presiona el botón para iniciar el AP.");
    
    ultimaLecturaMillis = millis();
    ultimoLogMillis = millis();
}

void loop() {
    unsigned long currentMillis = millis();

    if (botonPresionado) {
        toggleAP();
        botonPresionado = false;
    }
    
    if (apActivo) {
        server.handleClient();
    }

    if (currentMillis - ultimaLecturaMillis >= 1000) {
        ultimaLecturaMillis = currentMillis;
        leerSensores();
    }

    // Guardar datos en SPIFFS cada hora (3600000 ms)
    if (currentMillis - ultimoLogMillis >= 3600000) {
        ultimoLogMillis = currentMillis;
        logDataToSPIFFS();
    }
}