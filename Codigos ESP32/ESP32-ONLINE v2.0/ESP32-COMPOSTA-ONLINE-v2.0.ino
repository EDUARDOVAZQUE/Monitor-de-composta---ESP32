/**
 * @file main_hybrid.cpp
 * @brief Monitor de Composta - Versión Híbrida (SPIFFS + Supabase).
 *
 * @details Solución final tolerante a fallos. El dispositivo funciona de forma
 * autónoma y se sincroniza con la nube cuando es posible.
 * - Guarda datos localmente en SPIFFS cada hora como prioridad principal.
 * - Después de cada guardado local, intenta enviar los mismos datos a Supabase.
 * - Obtiene la hora de un servidor NTP; si falla, usa el tiempo de actividad.
 * - Mantiene todas las funcionalidades anteriores: AP con botón, servidor web, etc.
 * - Está diseñado para no perder datos, incluso con fallos de internet prolongados.
 *
 * @author Jesús Eduardo Vázquez Martínez
 * @author Kaory Gissel Contreras Álvarez
 *
 * @date 2025-09-22
 * @version 2.0 (ESP32-ONLINE)
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
#include "time.h"
#include <WiFiClientSecure.h> // Para peticiones HTTPS
#include <ArduinoJson.h>      // Para construir el cuerpo de la petición
#include <HTTPClient.h>

// --- 2. CONFIGURACIÓN DE PINES ---
const int PIN_HUMEDAD     = 34;
const int PIN_GAS         = 35;
const int PIN_TEMPERATURA = 22;
const int PIN_BOTON_AP    = 27;

// --- 3. CONFIGURACIÓN DE RED Y SERVICIOS ---
// WiFi (Modo Cliente)
const char* WIFI_SSID = "tu red de internet";
const char* WIFI_PASSWORD = "tu contraseña";

// Access Point (Modo AP)
const char* AP_SSID = "ESP32-COMPOSTA";
const char* AP_PASSWORD = "serviciosocial2025";

// NTP
const char* NTP_SERVER = "pool.ntp.org";
const long  GMT_OFFSET_SEC = -21600;
const int   DAYLIGHT_OFFSET_SEC = 0;

// Supabase
const char* SUPABASE_URL = "supa url";
const char* SUPABASE_ANON_KEY = "supa anon key";

// --- 4. VARIABLES GLOBALES ---
WebServer server(80);
OneWire oneWire(PIN_TEMPERATURA);
DallasTemperature sensors(&oneWire);
WiFiClientSecure client; // Cliente seguro para HTTPS

float temperatura = 0.0, humedad = 0.0, gas_ppm = 0.0;
bool timeIsSynced = false;
bool apActivo = false;
volatile bool botonPresionado = false;
unsigned long ultimaLecturaMillis = 0;
unsigned long ultimoLogMillis = 0;

// ====================================================================================
// --- 5. LÓGICA DE SUPABASE ---
// ====================================================================================

void enviarASupabase() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Supabase: No hay WiFi, envío omitido.");
        return;
    }

    // Crear el objeto JSON con los datos a enviar
    StaticJsonDocument<200> jsonDoc;
    jsonDoc["timestamp_dispositivo"] = getTimestamp();
    jsonDoc["temperatura"] = temperatura;
    jsonDoc["humedad"] = humedad;
    jsonDoc["gas_ppm"] = gas_ppm;
    
    String requestBody;
    serializeJson(jsonDoc, requestBody);

    // Configurar la petición HTTP
    String endpoint = String("https://") + SUPABASE_URL + "/rest/v1/composta-data";
    
    // Usar HTTPClient para simplificar las peticiones
    HTTPClient http;
    
    http.begin(client, endpoint); // Usar el cliente seguro
    http.addHeader("apikey", SUPABASE_ANON_KEY);
    http.addHeader("Authorization", "Bearer " + String(SUPABASE_ANON_KEY));
    http.addHeader("Content-Type", "application/json");

    Serial.println("Enviando datos a Supabase...");
    int httpResponseCode = http.POST(requestBody);

    if (httpResponseCode == 201) { // 201 Created es la respuesta de éxito de Supabase
        Serial.println("Dato enviado a Supabase con éxito.");
    } else {
        Serial.print("Error al enviar a Supabase. Código de respuesta: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println("Respuesta del servidor: " + payload);
    }

    http.end();
}

// ====================================================================================
// --- 6. GESTIÓN DE DATOS Y TIEMPO ---
// ====================================================================================
String getTimestamp() {
    if (!timeIsSynced) return String(millis() / 1000);
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return "error_time";
    char timeString[20];
    strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(timeString);
}

void logData() {
    // Paso 1: Siempre guardar en la memoria local primero
    File dataFile = SPIFFS.open("/datalog.csv", FILE_APPEND);
    if (dataFile) {
        String dataString = getTimestamp() + "," + String(temperatura, 2) + "," + String(humedad, 2) + "," + String(gas_ppm, 2);
        dataFile.println(dataString);
        dataFile.close();
        Serial.println(">>> Dato guardado en SPIFFS: " + dataString);
    } else {
        Serial.println("Error al abrir datalog.csv para escribir");
    }

    // Paso 2: Intentar enviar a la nube
    enviarASupabase();
}

void initWiFi() {
    Serial.printf("Conectando a %s ", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int tryCount = 0;
    while (WiFi.status() != WL_CONNECTED && tryCount < 20) {
        delay(1000);
        Serial.print(".");
        tryCount++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(" CONECTADO!");
        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
        timeIsSynced = true;
    } else {
        Serial.println(" No se pudo conectar a WiFi. Se usará el tiempo de actividad.");
        timeIsSynced = false;
    }
}

void borrarRegistros() {
    SPIFFS.remove("/datalog.csv");
    File newFile = SPIFFS.open("/datalog.csv", FILE_WRITE);
    if (newFile) {
        newFile.println("timestamp,temperatura_C,humedad_porc,gas_ppm");
        newFile.close();
        Serial.println("Archivo de registros recreado.");
    }
}

void leerSensores() {
    humedad = map(analogRead(PIN_HUMEDAD), 3500, 800, 0, 100);
    humedad = constrain(humedad, 0, 100);
    gas_ppm = (analogRead(PIN_GAS) / 4095.0) * 10000;
    sensors.requestTemperatures();
    temperatura = sensors.getTempCByIndex(0);
}

void handleRoot() {
    String html = R"rawliteral(
<!DOCTYPE html><html><head><title>ESP32 Composta Híbrido</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
 body{font-family:Arial,sans-serif;text-align:center;margin-top:20px;background-color:#f4f4f4;}
 .sensor-data{margin:30px;padding:20px;background-color:white;border-radius:10px;box-shadow:0 2px 4px rgba(0,0,0,0.1);}
 .sensor-data p{font-size:22px;margin:10px;} .sensor-data span{font-weight:bold;color:#0056b3;}
 .button{display:block;width:80%;max-width:300px;padding:20px;font-size:20px;font-weight:bold;color:white;border:none;border-radius:10px;text-decoration:none; cursor:pointer;}
 .btn-csv{background-color:#4CAF50;} .btn-delete{background-color:#f44336;}
</style></head><body>
<h1>&#x1F331; Monitor de Composta (Hibrido)</h1>
<div class="sensor-data">
 <h2>Datos en Tiempo Real</h2>
 <p>&#x1F552; Hora: <span id="time">--</span></p>
 <p>&#x1F525; Temp: <span id="temp">--</span> &deg;C</p>
 <p>&#x1F4A7; Hum: <span id="hum">--</span> %</p>
 <p>&#x1F32A;&#xFE0F; Gas: <span id="gas">--</span> ppm</p>
</div>
<div class="button-container">
 <a href="/csv" class="button btn-csv" download="datalog.csv">Descargar Historial Local</a>
 <a href="/delete" class="button btn-delete">Borrar Historial Local</a>
</div>
<script>
 setInterval(function(){
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
 }, 1500);
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

void handleCsv() {
    File dataFile = SPIFFS.open("/datalog.csv", "r");
    if (dataFile) server.streamFile(dataFile, "text/csv");
    else server.send(404, "text/plain", "Archivo no encontrado");
    dataFile.close();
}

void handleDelete() {
    borrarRegistros();
    server.send(200, "text/html", "<h1>Registros locales borrados</h1><p>Archivo recreado.</p><a href='/'>Volver</a>");
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
// --- 7. SETUP Y LOOP PRINCIPAL ---
// ====================================================================================
void setup() {
    Serial.begin(115200);
    sensors.begin();
    
    if (!SPIFFS.begin(true)) { Serial.println("Error al montar SPIFFS."); return; }
    
    if (!SPIFFS.exists("/datalog.csv")) {
        borrarRegistros();
    }
    
    // El ESP32 se pone en modo Cliente + AP
    WiFi.mode(WIFI_AP_STA);
    initWiFi(); // Intenta conectar a la red principal
    
    client.setInsecure();
    
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

    if (currentMillis - ultimaLecturaMillis >= 1500) { // Lectura más frecuente
        ultimaLecturaMillis = currentMillis;
        leerSensores();
    }

    // Guardar datos localmente e intentar enviar a la nube cada hora 3600000
    if (currentMillis - ultimoLogMillis >= 3600000) {
        ultimoLogMillis = currentMillis;
        logData();
    }
}