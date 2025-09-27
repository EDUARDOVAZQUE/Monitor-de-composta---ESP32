# Monitor de Composta Inteligente (ESP32)

> Un sistema de monitoreo de IoT tolerante a fallos y de bajo consumo para analizar y registrar las condiciones de una pila de compostaje.

-----

## 📜 Descripción General

Este proyecto implementa un monitor de composta basado en el microcontrolador ESP32. El dispositivo está diseñado para ser energéticamente eficiente y robusto, capaz de operar en entornos con o sin conexión a internet. Recolecta datos de temperatura, humedad y gas metano, los almacena localmente en su memoria interna, y los sincroniza con una base de datos en la nube (Supabase) cuando hay conexión disponible.

El sistema cuenta con dos modos de operación: un modo de ultra bajo consumo para el registro de datos a largo plazo y un modo de Access Point para mantenimiento y visualización de datos en tiempo real.

-----

## ✨ Características Principales

  * **Monitoreo de Sensores:** Lectura de temperatura (DS18B20), humedad del suelo (FC-28) y gas metano (MQ-4).
  * **Almacenamiento Local:** Guarda un historial de datos en formato CSV en la memoria interna del ESP32 (SPIFFS), asegurando que no se pierdan datos por falta de conexión.
  * **Sincronización en la Nube:** Envía los datos a una base de datos PostgreSQL en **Supabase** para su almacenamiento y análisis a largo plazo.
  * **Modo de Bajo Consumo:** Utiliza **Deep Sleep** para minimizar el consumo de energía, despertando solo en intervalos programados para realizar su trabajo. Ideal para funcionar con baterías.
  * **Modo de Mantenimiento (Access Point):** Al presionar un botón, el dispositivo crea una red WiFi y levanta un servidor web para ver los datos de los sensores en tiempo real desde cualquier dispositivo.
  * **Dashboard Web (PWA):** Incluye un panel de control web adaptable a móviles (Progressive Web App) para visualizar los datos de Supabase, con la capacidad de descargar el historial completo.

-----

## 🛠️ Componentes y Tecnologías

### Hardware

  * Microcontrolador **ESP32 DevKit V1**
  * Sensor de Temperatura **DS18B20**
  * Sensor de Humedad de Suelo **FC-28**
  * Sensor de Gas Metano **MQ-4**
  * Botón Pulsador
  * Resistencia de 4.7kΩ

### Software y Servicios

  * **Arduino IDE** (Lenguaje C++)
  * Sistema de Archivos **SPIFFS** para almacenamiento interno.
  * **Supabase** como backend y base de datos (PostgreSQL).
  * **Vercel** para el despliegue del dashboard web.
  * **PWA (Progressive Web App)** con HTML, CSS y JavaScript para la visualización de datos.

-----

## 🔌 Diagramas de Conexión

### Prototipo Inicial (ESP32-CAM)

En la fase inicial del proyecto se utilizó una placa ESP32-CAM y una tarjeta Micro SD para el almacenamiento de datos.

![Diagrama ESP32-CAM](/img/ESP32CAM-Diagrama.jpg)


### Versión Final (ESP32 DevKit - Bajo Consumo)

La versión final y optimizada utiliza una placa ESP32 DevKit, la memoria interna SPIFFS y un botón externo como fuente de despertar de Deep Sleep.

![Diagrama ESP32-CAM](/img/ESP32-DIAGRAMA.png)

-----

## 🧬 Evolución del Proyecto (Versiones)

El firmware del ESP32 fue desarrollado en varias etapas incrementales:

#### Versión 1.0: MicroSD-ESP32CAM

  * **Objetivo:** Crear una base funcional sin dependencias externas.
  * **Funcionalidad:** El ESP32-CAM crea un Access Point. Los datos se leen y se guardan en la memoria micro sd. Se puede acceder a un servidor web para ver la última lectura y descargar el historial. [ESP32CAM](https://github.com/EDUARDOVAZQUE/Implementaci-n-de-un-monitor-de-composta-inteligente-con-ESP32-CAM/blob/main/Codigos%20ESP32/ESP32-CAM%20v1.0/ESP32CAM.ino)


#### Versión 2: Completamente Offline

  * **Objetivo:** Crear una base funcional sin dependencias externas.
  * **Funcionalidad:** El ESP32 crea un Access Point. Los datos se leen y se guardan en la memoria interna (SPIFFS) usando un ID de registro. Se puede acceder a un servidor web para ver la última lectura y descargar el historial. [ESP32-OFFLINE v1.2](https://github.com/EDUARDOVAZQUE/Implementaci-n-de-un-monitor-de-composta-inteligente-con-ESP32-CAM/blob/main/Codigos%20ESP32/ESP32-OFFLINE%20v1.2/ESP32-COMPOSTA-OFFLINE-v1.2.ino)

#### Versión 2: Conexión Online (NTP)

  * **Objetivo:** Añadir una marca de tiempo precisa.
  * **Funcionalidad:** Se añade la capacidad de conectarse a una red WiFi para obtener la fecha y hora de un servidor NTP. El ID de registro se reemplaza por un timestamp real. Si no hay internet, se usa un timestamp de respaldo basado en el tiempo de actividad del dispositivo. [ESP32-ONLINE_TIME v1.5](https://github.com/EDUARDOVAZQUE/Implementaci-n-de-un-monitor-de-composta-inteligente-con-ESP32-CAM/blob/main/Codigos%20ESP32/ESP32-ONLINE_TIME%20v1.5/ESP32-COMPOSTA-ONLINE_TIME-V1.5.ino)

#### Versión 3: Híbrido con Base de Datos (Supabase)

  * **Objetivo:** Sincronizar los datos con la nube.
  * **Funcionalidad:** Se mantiene todo lo anterior y se añade el envío de datos a Supabase. El sistema es tolerante a fallos: si no hay internet, los datos solo se guardan localmente. [ESP32-ONLINE v2.0](https://github.com/EDUARDOVAZQUE/Implementaci-n-de-un-monitor-de-composta-inteligente-con-ESP32-CAM/blob/main/Codigos%20ESP32/ESP32-ONLINE%20v2.0/ESP32-COMPOSTA-ONLINE-v2.0.ino)

#### Versión 4: Bajo Consumo (Deep Sleep)

  * **Objetivo:** Optimizar el consumo de energía para operación con baterías.
  * **Funcionalidad:** Se reestructura completamente el código para usar **Deep Sleep**. Se crean dos modos de operación (Normal/Bajo Consumo y AP/Mantenimiento) que se pueden cambiar con un botón, el cual también funciona como fuente de despertar. [ESP32-ONLINE v2.1](URL)

-----

## 🚀 Cómo Empezar

1.  **Clonar el Repositorio:** `git clone [https://github.com/EDUARDOVAZQUE/Implementaci-n-de-un-monitor-de-composta-inteligente-con-ESP32-CAM?tab=readme-ov-file]`
2.  **Abrir en Arduino IDE:** Abre el archivo `.ino` de la versión que desees probar.
3.  **Instalar Librerías:** Asegúrate de tener instaladas todas las librerías listadas en la cabecera del archivo.
4.  **Rellenar Credenciales:** En el código, introduce tus credenciales de WiFi y de tu proyecto de Supabase (URL y Anon Key).
5.  **Montar el Circuito:** Conecta los componentes siguiendo el diagrama correspondiente a la versión final o ESP32CAM respectivamente.
6.  **Subir el Código:** Selecciona tu placa ESP32 Dev Module y el puerto correcto, y sube el programa.

-----

## 📝 Licencia

Uso personal, académico y de divulgación permitido. Es obligatorio atribuir la autoría en publicaciones derivadas.