/**
 * @file main_offline.cpp
 * @brief Monitor de Composta - Versión Completamente Offline.
 *
 * @details Este proyecto implementa un sistema autónomo para el monitoreo
 * de una composta usando la memoria interna del ESP32 (SPIFFS).
 * - Lee sensores de humedad, temperatura y gas metano (CH4).
 * - Almacena un historial de datos en formato CSV en la memoria SPIFFS cada hora.
 * - Utiliza un ID de registro único que persiste entre reinicios.
 * - Crea un Access Point con un servidor web para visualizar datos en tiempo real.
 * - Un botón físico permite activar y desactivar el Access Point para ahorrar energía.
 * - Permite descargar y borrar el historial de datos desde la página web.
 *
 * @author Jesús Eduardo Vázquez Martínez
 * @author Kaory Gissel Contreras Álvarez
 *
 * @date 2025-09-22
 * @version 1.2 (ESP32-OFFLINE)
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
#include <SPIFFS.h> // Usamos SPIFFS en lugar de SD_MMC
#include <OneWire.h>
#include <DallasTemperature.h>

// --- 2. CONFIGURACIÓN DE PINES (Para ESP32 DevKit) ---
const int PIN_HUMEDAD     = 34; // Sensor de Humedad del suelo (FC-28)
const int PIN_GAS         = 35; // Sensor de Gas Metano (MQ-4)
const int PIN_TEMPERATURA = 22; // Sensor de Temperatura (DS18B20)
const int PIN_BOTON_AP    = 27; // Botón para activar/desactivar el AP

// --- 3. CONFIGURACIÓN DE RED Y CICLOS ---
const char* AP_SSID = "ESP32-COMPOSTA";
const char* AP_PASSWORD = "serviciosocial2025";

// --- 4. VARIABLES GLOBALES ---
WebServer server(80);
OneWire oneWire(PIN_TEMPERATURA);
DallasTemperature sensors(&oneWire);

// Variables para almacenar la última lectura
float temperatura = 0.0, humedad = 0.0, gas_ppm = 0.0;
unsigned long registroID = 0; // ID único para cada registro guardado

// Variables de estado y timers no bloqueantes
bool apActivo = false; // El AP inicia desactivado
volatile bool botonPresionado = false; // Flag para la interrupción del botón
unsigned long ultimaLecturaMillis = 0;
unsigned long ultimoLogMillis = 0;


// ====================================================================================
// --- 5. FUNCIONES DE LECTURA DE SENSORES ---
// ====================================================================================
void leerSensores() {
    humedad = map(analogRead(PIN_HUMEDAD), 3500, 800, 0, 100);
    humedad = constrain(humedad, 0, 100);
    
    gas_ppm = (analogRead(PIN_GAS) / 4095.0) * 10000;

    sensors.requestTemperatures();
    temperatura = sensors.getTempCByIndex(0);
    
    Serial.printf("Lectura: ID=%lu, Temp=%.2f C, Hum=%.2f %%, Gas=%.2f ppm\n", registroID, temperatura, humedad, gas_ppm);
}

// ====================================================================================
// --- 6. GESTIÓN DE DATOS (SPIFFS) ---
// ====================================================================================
// Función para obtener el último ID del archivo CSV al iniciar
void obtenerUltimoID() {
    File file = SPIFFS.open("/datalog.csv", FILE_READ);
    if (!file || file.size() == 0) {
        Serial.println("No se encontró datalog.csv o está vacío. Iniciando ID en 0.");
        registroID = 0;
        file.close();
        // Crear el archivo con encabezados si no existe
        file = SPIFFS.open("/datalog.csv", FILE_WRITE);
        file.println("id,temperatura_C,humedad_porc,gas_ppm");
        file.close();
        return;
    }

    String lastLine = "";
    while (file.available()) {
        lastLine = file.readStringUntil('\n');
    }
    file.close();

    // Extraer el primer valor (el ID) de la última línea
    int firstComma = lastLine.indexOf(',');
    if (firstComma > 0) {
        registroID = lastLine.substring(0, firstComma).toInt();
        Serial.printf("Último ID encontrado: %lu. El siguiente será %lu.\n", registroID, registroID + 1);
    }
}

void logDataToSPIFFS() {
    registroID++; // Incrementar el ID para el nuevo registro
    File dataFile = SPIFFS.open("/datalog.csv", FILE_APPEND);
    if (dataFile) {
        String dataString = String(registroID) + "," + String(temperatura, 2) + "," + String(humedad, 2) + "," + String(gas_ppm, 2);
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
    registroID = 0; // Reiniciar el contador de ID
    // Recrear el archivo con los encabezados
    File newFile = SPIFFS.open("/datalog.csv", FILE_WRITE);
    if (newFile) {
        newFile.println("id,temperatura_C,humedad_porc,gas_ppm");
        newFile.close();
        Serial.println("Archivo de registros recreado.");
    }
}

// ====================================================================================
// --- 7. SERVIDOR WEB ---
// ====================================================================================
void handleRoot() {
    String html = R"rawliteral(
<!DOCTYPE html><html><head><title>ESP32 Composta Offline</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
 body{font-family:Arial,sans-serif;text-align:center;margin-top:20px;background-color:#f4f4f4;}
 h1{color:#333;}
 .sensor-data{margin:30px;padding:20px;background-color:white;border-radius:10px;box-shadow:0 2px 4px rgba(0,0,0,0.1);}
 .sensor-data p{font-size:22px;margin:10px;} .sensor-data span{font-weight:bold;color:#0056b3;}
 .button-container{display:flex;flex-direction:column;align-items:center;gap:15px;margin-top:30px;}
 .button{display:block;width:80%;max-width:300px;padding:20px;font-size:20px;font-weight:bold;color:white;border:none;border-radius:10px;text-decoration:none; cursor:pointer;}
 .btn-csv{background-color:#4CAF50;} .btn-delete{background-color:#f44336;}
</style></head><body>
<h1>&#x1F331; Monitor de Composta (Offline)</h1>
<div class="sensor-data">
 <h2>Datos en Tiempo Real</h2>
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
    json += "\"temperatura\":" + String(temperatura, 2) + ",";
    json += "\"humedad\":" + String(humedad, 2) + ",";
    json += "\"gas_ppm\":" + String(gas_ppm, 2);
    json += "}";
    server.send(200, "application/json", json);
}

void handleCsv() {
    File dataFile = SPIFFS.open("/datalog.csv", "r");
    if (dataFile) {
        server.streamFile(dataFile, "text/csv");
        dataFile.close();
    } else {
        server.send(404, "text/plain", "Archivo no encontrado");
    }
}

void handleDelete() {
    borrarRegistros();
    String html = "<h1>Registros borrados</h1><p>El archivo datalog.csv ha sido eliminado y recreado.</p><a href='/'>Volver</a>";
    server.send(200, "text/html", html);
}

// ====================================================================================
// --- 8. LÓGICA DEL BOTÓN Y AP ---
// ====================================================================================
void IRAM_ATTR onBotonPresionado() {
    botonPresionado = true;
}

void toggleAP() {
    apActivo = !apActivo;
    if (apActivo) {
        Serial.println("Activando Access Point...");
        WiFi.softAP(AP_SSID, AP_PASSWORD);
        server.begin();
        Serial.print("AP iniciado. Conéctate a '");
        Serial.print(AP_SSID);
        Serial.print("'. IP: ");
        Serial.println(WiFi.softAPIP());
    } else {
        Serial.println("Desactivando Access Point...");
        server.stop();
        WiFi.softAPdisconnect(true);
    }
}

// ====================================================================================
// --- 9. SETUP Y LOOP PRINCIPAL ---
// ====================================================================================
void setup() {
    Serial.begin(115200);
    sensors.begin();
    
    // Iniciar SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("Error al montar SPIFFS. Reiniciando...");
        ESP.restart();
    }

    // Obtener el último ID guardado
    obtenerUltimoID();

    // Configurar el botón con interrupción
    pinMode(PIN_BOTON_AP, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_BOTON_AP), onBotonPresionado, FALLING);

    // Configurar rutas del servidor web
    server.on("/", handleRoot);
    server.on("/data.json", handleDataJson);
    server.on("/csv", handleCsv);
    server.on("/delete", handleDelete);

    // Calentamiento del sensor de gas
    Serial.println("Calentando sensor de gas (30 segundos)...");
    delay(30000); // Reducido para pruebas más rápidas
    Serial.println("Sensor listo. Presiona el botón para iniciar el AP.");
    
    // Iniciar timers
    ultimaLecturaMillis = millis();
    ultimoLogMillis = millis();
}

void loop() {
    unsigned long currentMillis = millis();

    // 1. Manejar el botón para activar/desactivar AP
    if (botonPresionado) {
        toggleAP();
        botonPresionado = false; // Resetear la flag de la interrupción
    }
    
    // 2. Si el AP está activo, manejar las peticiones de los clientes
    if (apActivo) {
        server.handleClient();
    }

    // 3. Timer para leer sensores cada segundo
    if (currentMillis - ultimaLecturaMillis >= 1000) {
        ultimaLecturaMillis = currentMillis;
        leerSensores();
    }

    // 4. Timer para guardar datos en SPIFFS cada hora (3600000 ms) 
    if (currentMillis - ultimoLogMillis >= 5000) {
        ultimoLogMillis = currentMillis;
        logDataToSPIFFS();
    }
}