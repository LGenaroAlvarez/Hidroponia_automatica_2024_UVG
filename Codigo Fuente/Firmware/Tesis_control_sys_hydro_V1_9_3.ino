/*  Version 1.9.3
    Autor: Ing. Luis Genaro Alvarez Sulecio
    Tesis de sistema hidroponico automatico
    Comentarios: 
    - Limpieza de codigo eliminando lineas de prueba, variables obsoletas,
      funciones descontinuadas y completando la documentacion del codigo.
*/
//---------------------Librerias utilizadas---------------------------
//#define USE_PULSE_OUT
#include <WiFi.h>               // Libreria para conexion wifi
#include <WiFiMulti.h>          // Libreria para gestionar multiples redes WiFi
#include <ArduinoHttpClient.h>  // Librería de arduino para conexiones HTTP > https://github.com/arduino-libraries/ArduinoHttpClient/blob/master/README.md
#include <String.h>             // Formato de informacion en tipo String
#include <ArduinoJson.h>        // Formato de informacion en tipo JSON para envio por HTTP
#include <DHT.h>                // Lectura de sensores DHT11
#include <OneWire.h>            // Protocolo de comunicacion de datos OneWire, utilizada para sensores DS18B20
#include <DallasTemperature.h>  // Lectura de sensores DS18B20, procesamiento de datos recividos
#include <Adafruit_NeoPixel.h>  // Control de luces de crecimiento (neopixeles WS2812)
#include <Wire.h>               // Comunicacion I2C con modulo RTC
#include <RTClib.h>             // Formateo de datos recibidos del modulo RTC: https://learn.adafruit.com/metro-minimalist-clock/code-it
#include <ESP32Servo.h>         // Control de servomotores utilizando pines PWM del ESP 32

// Recoleccion de datos de sensor de pH de Atlas Scientific
#ifdef USE_PULSE_OUT
  #include "ph_iso_surveyor.h"  // Libreria a utilizar si se emplea salia digital del modulo de filtrado de ruido
  Surveyor_pH_Isolated pH = Surveyor_pH_Isolated(A0);         
#else
  #include "ph_surveyor.h"      // Libreria para lectura analogica de sensor de pH
  Surveyor_pH pH = Surveyor_pH(A0);   
#endif

//----------------Definiciones de pines-------------
#define DHTPIN 4                // Resistencia pull-up de 10k Ohms (5V a datos)
#define DHTPIN2 13              //          ||          ||
#define DHTTYPE DHT11           // Tipo de sensor DHT instalado (entre DHT11 y DHT22)
#define ONE_WIRE_BUS 15         // Resistencia pull-up de 4.7k Ohms (5V a datos)
#define FAN1PIN 12              // Resistencia de 100 Ohms a pin colector de 2N2222A
#define FAN2PIN 14              //    ||          ||               ||        ||
#define NEOPIXELPIN 27          // Pin de control de luces neopixel (agregar pin 18 o 19 para control individual de cada nivel de luces)
#define NEOPIXELPIN2 18         //    ||                ||
#define SERVO_PH_UP 32          // Pin para control de servomotor para aumentar pH
#define SERVO_PH_DOWN 33        //    ||                ||          || disminuir pH

//--------------Definiciones constantes---------------
#define ACTIVE 1                // Estado activo (ventiladores)
#define IDLE 0                  // Estado inactivo     ||
#define DS18_PRECISION 9        // Presicion de sensores DS18B20
#define NUM_PIXELS 60           // Cantidad de luces led en tira NeoPixel (122 en sistema completo)
#define HTTP_TIMEOUT 5000       // Tiempo de espera para respuesta de servidor HTTP
#define HTTP_MAX_RETRY 3        // Cantidad maxima de intentos para envio de datos por HTTP
#define JSON_BUFFER_SIZE 256    // Tamano del buffer para empaquetado de datos en formato JSON
#define GLOBAL_PERIOD 30000     // Periodo de evaluacion de variables global de 30 segundos
#define TIME_24_HOUR true       // Configuracion de RTC en formato de 24H
#define OPEN 35                 // Valor de grados para abrir valvula de control de pH
#define CLOSE 0                 // Valor de grados para cerrar valvula de control de pH

//------------Structs--------------
// Struct para crear temporizadores no bloqueantes utilizando funcion Millis()
struct timerVar {
  unsigned long start_time;     // Tiempo de inicio para funcion Millis
  unsigned long time_period;    // Intervalo de tiempo para funcion Millis
};

//--------Conexion a internet (wifi)----------
const char* SSID_1 = "";                                                          // SSID de red WiFi #1
const char* SSID_2 = "";                                                          // SSID de red WiFi #2
const char* password_1 = "";                                                      // Contrasena de red WiFi
const char* SSID_3 = "";                                                          // SSID de red WiFi #3
const char* password_2 = "";                                                      // Contrasena de red WiFi

// Detalles del servidor para http
const char* host = "dweet.io";                                                    // Servidor de Dweet.io
const int port = 80;                                                              // Puerto de conexión para servidor HTTP
char jsonBuffer[JSON_BUFFER_SIZE];                                                // Buffer para envio de datos a servidor
int network_error_flag = 0;                                                       // Bandera indicadora de un error en la conexion WiFi
// https://dweet.io/get/dweets/for/hydro_test                                     // URL de servidor para lecturas de sensores
// https://dweet.io/get/dweets/for/hydro_LOG                                      // URL de servidor para informaci[on sobre el estado del sistema

WiFiClient espClient;  // Creacion de cliente para conexion WiFi del ESP32

//----------Variables de sensores-----------
float temp_s1 = 0.0;        // Variable para almacenar valor de temperatura en sensor DHT11 numero 1
float hum_s1 = 0.0;         // Variable para almacenar valor de humedad en sensor DHT11 numero 1
float temp_s2 = 0.0;        // Variable para almacenar valor de temperatura en sensor DHT11 numero 2
float hum_s2 = 0.0;         // Variable para almacenar valor de humedad en sensor DHT11 numero 2
float w_temp_s1 = 0.0;      //      ||          ||      ||   || temperatura en sonda DS18B20 numero 1
float w_temp_s2 = 0.0;      //      ||          ||      ||   ||      ||     en sonda DS18B20 numero 2
float w_temp_s3 = 0.0;      //      ||          ||      ||   ||      ||     en sonda DS18B20 numero 3
float ambient_T_min = 0.0;  //      ||          ||      ||   ||      ||     ambiental minima admisible
float ambient_T_max = 0.0;  //      ||          ||      ||   ||      ||     ambiental maxima admisible
float ambient_H_min = 0.0;  //      ||          ||      ||   || humedad ambiental minima admisible
float ambient_H_max = 0.0;  //      ||          ||      ||   ||    ||   ambiental maxima admisible
float setup_temp = 0.0;     //      ||          ||      ||   || temperatura promedio en el sistema
float setup_hum = 0.0;      //      ||          ||      ||   || humedad promedio en el sistema
float phMaxVal = 0.0;       //      ||          ||      ||   || pH maximo admisible
float phMinVal = 0.0;       //      ||          ||      ||   || pH minimo admisible
float ph_val = 0.0;         //      ||          ||      ||   || de pH del sensor de Atlas Scientific

int ambient_temperature_flag = 0;          // 0 = valor normal, 1 = valor bajo, 2 = valor alto
int ambient_humidity_flag = 0;             // 0 = valor normal, 1 = valor bajo, 2 = valor alto
int dht_retry_cont = 0;                    // Cantidad de intentos utilizados para obtener datos de sensores DHT
int dht_max_retry = 2;                     // Cantidad maxima de intentos permitidos para obtener datos de sensores DHT
int fan_activate = 0;                      // Bandera para activar ventiladores
int fan_state = IDLE;                      // Bandera para registrar estado actual de los ventiladores
int control_active_flag = 0;               // Indicador de estado de control (0 = control inactivo <parametros normales> | 1 = control activo)
unsigned long previous_millis_dht = 0;     // 
unsigned long glt_prev_time = 0;           // Estado anterior del temporizador de luz de crecimiento (Grow Light Timer)
unsigned long global_timer_prev_time = 0;  // Estado anterior de temporizador global
unsigned long previous_dispense_time = 0;  // Estado anterior de temporizador de liberador de soluciones reguladoras de pH

//-------------Contadores------------
double active_time = 0;                    // Tiempo que han estado activas las luces de crecimiento
int http_send_fail_counter = 0;            // Contador de fallos de envio de datos utilizando HTTP
int sre_cont = 0;                          // Contador para eventos de lectura de sensores (Sensor Reading Event Counter)
int dse_cont = 0;                          // Contador para eventos de envío de datos (Data Send Event Counter)
int cse_cont = 0;                          // Contador para eventos del sistema de control (Control System Event Counter)
int wre_cont = 0;                          // Contador para evento de reconexión a red WiFi (WiFi Recconect Event Counter)
int wifi_retry_cont = 0;                   // Contador de cantidad de veces que se ha reintentado la conexion a red WiFi

//-----------------------Variables generales---------------
const int ledPin = 2;                      // Pin para luz led interna del ESP32
int ledState = LOW;                        // Estado para la luz led del ESP32
int neo_pixel_state = 0;                   // Estado para la luz de crecimiento (Neopixel)
double growLightTime = 14;                 // Valor en horas (revisar pagina 14, tesis hidroponia)
int time_of_day = 0;                       // Hora del dia (1 = dia, 0 = noche)
uint8_t day_timer = 0;                     // Registro de horas para determinar la hora actual y activar luces de crecimiento a la hora establecida
uint8_t morning_time = 6;                  // am - Hora local a la que amanece
uint8_t night_time = 17;                   // pm - Hora local a la que anochece

//--------------Inicializacion de modulos del sistema----------------
DHT dht_s1(DHTPIN, DHTTYPE);                                                      // Activar sensor DHT11 numero 1
DHT dht_s2(DHTPIN2, DHTTYPE);                                                     // Activar sensor DHT11 numero 1
OneWire oneWire(ONE_WIRE_BUS);                                                    // Bus de comunicacion para sensores de protocolo onewire
DallasTemperature sensors(&oneWire);                                              // Inicializacion de sensores de temperatura de agua
DeviceAddress tankSensor, growChannel1, growChannel2;                             // Definicion de sensores de temperatura de agua
Adafruit_NeoPixel NeoPixel(NUM_PIXELS, NEOPIXELPIN, NEO_GRB + NEO_KHZ800);        // Definicion de luces led (NEOPIXELS)
Adafruit_NeoPixel NeoPixel2(NUM_PIXELS + 1, NEOPIXELPIN2, NEO_GRB + NEO_KHZ800);  // Definicion de luces led (NEOPIXELS) de la repisa superior
RTC_DS3231 rtc;                                                                   // RTC conectado a pines SCL y SDA: 22 y 21
Servo phUpServo, phDownServo;                                                     // Servomotores para control de valvulas de regulacion de pH
WiFiMulti wifiMulti;                                                              // Inicializacion de objeto para manejar multiples redes WiFi

//---------------------------------Prototipos de funciones---------------------------------
void multiWifi();                                                                                                    // Establecer conexion WiFi
bool httpPostRequest(int sel);                                                                                       // Envio de datos a servidor WiFi
void getTemperatureHumidity();                                                                                       // Lectura de sensores de temperatura y humedad
String packageData(float val1, float val2, float val3, float val4, float val5, float val6, float val7, float val8);  // Empaquetado de dat648213os en formato JSON
String performanceLog();                                                                                             // Empaquetado de datos para log de rendimiento
String getCurrentTime(int sel);                                                                                      // Lectura de tiempo real (mediante modulo RTC DS3231)
float twoValAverage(float dat1, float dat2);                                                                         // Calculo de valor promedio para dos valores
void fanControl();                                                                                                   // Accionamiento de ventiladores
void ambientAirParamsControl();                                                                                      // Control de parametros ambientales
void waterTempInit();                                                                                                // Iniciar sensores de temperatura de agua
float getWaterTemperature(DeviceAddress deviceAddress);                                                              // Lectura de sensores de temperatura de agua
void setNeopixelState();                                                                                             // Cambiar el estado de los leds segun tiempo recorrido
void dayCounter();                                                                                                   // Determinar hora del dia y actualizarla segun requerido
void phRegulationControl();                                                                                          // Regulacion de pH

/*
=====================================================================================================================================================================================
============================================================================INICIA PROGRAMA==========================================================================================
=====================================================================================================================================================================================
*/

void setup() {
  // Configuracioens iniciales
  Serial.begin(57600);
  pinMode(FAN1PIN, OUTPUT);
  pinMode(FAN2PIN, OUTPUT);
  pinMode(ledPin, OUTPUT);
  //----------------Inicializacion Luces de Crecimiento-----------------
  NeoPixel.begin();                       // Inicializar luces de nivel inferior
  NeoPixel2.begin();                      // Inicializar luces de nivel superior
  NeoPixel.clear();                       // Apagar luces LED
  NeoPixel2.clear();                      // Apagar luces LED
  NeoPixel.show();                        // Actualizar luces LED
  NeoPixel2.show();                       // Actualizar luces LED
  neo_pixel_state = 0;                    // Indicar que las luces led estan apagadas
  //--------------------------------------------------------------------
  Wire.begin();                           // Inicializacion one wire para sensores DS18B20
  rtc.begin();                            // Inicializacion de moduo RTC
  wifiMulti.addAP(SSID_1, password_1);    // Incluir SSID de primera red WiFi a lista de redes disponibles
  wifiMulti.addAP(SSID_2, password_1);    // Incluir SSID de segunda red WiFi a lista de redes disponibles
  wifiMulti.addAP(SSID_3, password_2);    // Incluir SSID de tercera red WiFi a lista de redes disponibles
  multiWifi();                            // Iniciar modulo para conexiones multiples de WiFi
  dht_s1.begin();                         // Iniciar primer sensor DHT11
  dht_s2.begin();                         // Iniciar segundo sensor DHT11
  waterTempInit();                        // Iniciar modulo para obtener temperatura de sensores DS18B20
  phUpServo.setPeriodHertz(50);           // Frecuencia de servo de pH (solucion alcalina)
  phUpServo.attach(SERVO_PH_UP, 500, 2400); // Definir anchos de pulsos para servo de pH (solucion alcalina)
  phDownServo.setPeriodHertz(50);         // Frecuencia de servo de pH (solucion acida)
  phDownServo.attach(SERVO_PH_DOWN, 500, 2400); // Definir anchos de pulsos para servo de pH (solucion acida)
  pH.begin();                             // Iniciar sensor de pH
  delay(200);                             // Delay antes de iniciar el sistema
}

/*
=====================================================================================================================================================================================
===========================================================================CICLO DE PROGRAMA=========================================================================================
=====================================================================================================================================================================================
*/

void loop() {
  //-----------------Lectura de la hora del dia--------------------
  dayCounter();

  //-----------------Actualizacion de luces de crecimiento---------------
  setNeopixelState();

  //-------------------------Inicializacion de variables---------------------------------
  int sre_period = (control_active_flag == 0) ? 5 : 2;   // Leer sensores cada 2.5 minutos o cada minuto (si el control esta activo)
  int dse_period = (control_active_flag == 0) ? 10 : 6;  // Envio de datos cada 5 minutos o cada 3 minutos (si el control esta activo)
  int cse_period = (control_active_flag == 0) ? 10 : 4;  // Actualizar ciclo de control cada 5 minutos o cada 2 minutos (si el control esta activo)
  int wre_period = 16;                                   // Intentar reconexion a red WiFi cada 8 minutos

  struct timerVar global_timer;                         // Temporizador global para eventos | utilizando periodo de 30 segundos
  global_timer.start_time = millis();
  if (global_timer.start_time - global_timer_prev_time >= GLOBAL_PERIOD)  // Ingresar a ciclo cada 30 segundos
  {
    global_timer_prev_time = global_timer.start_time;   // Actualizar tiempo del temporizador

    //--------------------------Incrementar contadores de eventos----------------------------------
    sre_cont++;  // Contador de Evento de Lectura de Sensores (Sensor Reading Event Counter)
    dse_cont++;  // Contador de Evento de Envio de Datos (Data Send Event Counter)
    cse_cont++;  // Contador de Evento de Sistemas de Control (Control System Event Counter)
    wre_cont++;  // Contador de Evento de Reconexion de WiFi (Wifi Reconnect Event COunter)

    //---------------------------Evento de Lectura de Datos------------------------------------
    if (sre_cont >= sre_period) {
      sre_cont = 0;                                   // Reiniciar contador
      dht_retry_cont = 0;                             // Reiniciar contador de intentos de lectura de sensor DHT
      getTemperatureHumidity();                       // Lectura de sensores DHT11

      sensors.requestTemperatures();                  // Solicitud de valores de temperatura
      w_temp_s1 = getWaterTemperature(tankSensor);    // Lectura de sensores DS18B20  (Sonda en deposito de agua)
      w_temp_s2 = getWaterTemperature(growChannel1);  //      |||       |||||         (Sonda en canal de crecimiento 1)
      w_temp_s3 = getWaterTemperature(growChannel2);  //      |||       |||||         (Sonda en canal de crecimiento 2)
      ph_val = pH.read_ph();                          // Lectura de sensor de pH
    }

    //--------------------------------Evento de Sistemas de Control--------------------------------
    if (cse_cont >= cse_period) {
      cse_cont = 0;                                   // Reiniciar contador

      // Analizar parametros y ejecutar ciclos de control
      ambientAirParamsControl();                      // Control de parametros del aire
      phRegulationControl();                          // Control de pH
      fanControl();                                   // Actuacion de ventiladores
    }

    //---------------------------------Evento de Envio de Datos------------------------------------
    if (dse_cont >= dse_period) {
      dse_cont = 0;                                   // Reiniciar contador
      if (httpPostRequest(0)) {                       // Si el envio de datos a servidor 1 es exitoso
        http_send_fail_counter = 0;                   // Mantener en 0 la bandera de fallo de envio de datos por HTTP
      } else {                                        // De lo contrario:
        network_error_flag = 1;                       // Activar bandera de fallo de envio de datos a servidor
      }
      if (httpPostRequest(1)) {                       // Si el envio de datos a servidor 2 es exitoso
        http_send_fail_counter = 0;                   // Mantener en 0 la bandera de fallo de envio de datos por HTTP
      } else {                                        // De lo contrario:
        network_error_flag = 1;                       // Activar bandera de fallo de envio de datos a servidor
      }
    }

    //---------------------------------Evento de Reconexion WiFi--------------------------------
    if (wre_cont >= wre_period) 
    {
      wre_cont = 0;                                   // Reiniciar contador
      if (network_error_flag == 1 || WiFi.status() != WL_CONNECTED) // Intentar reconexion WiFi si el sistema esta desconectado o existio un error
      {
        wifi_retry_cont++;                            // Incrementar contador de intentos de reconexion
        multiWifi();                                  // Funcion para conexion a red WiFi
      } 
      else
      {
        network_error_flag = 0;                       // Mantener bandera en cero si existe conexion WiFi
      }
    }
  }

  //-----------------------------------Revisar Conexion WiFi-------------------------------
  if (WiFi.status() == WL_CONNECTED && digitalRead(ledPin) != HIGH) // Si se esta conectado a red WiFi y el led indicador esta apagado
  {
    ledState = HIGH;                                  // Cambiar estado del led indicador a Alto (encendido)
    digitalWrite(ledPin, ledState);                   // Actualizar led indicador
    network_error_flag = 0;                           // Limpiar bandera de error de conexion
  } 
  else if (WiFi.status() != WL_CONNECTED && digitalRead(ledPin) != LOW) // Si no esta conectado a red WiFi y el led indicador esta encendido
  {
    ledState = LOW;                                   // Cambiar estado del led indicador a Bajo (apagado)
    digitalWrite(ledPin, ledState);                   // Actualizar led indicador
    network_error_flag = 1;                           // Activar bandera de error de conexion
  }
}

/*
=====================================================================================================================================================================================
========================================================================FUNCIONES DEL SISTEMA========================================================================================
=====================================================================================================================================================================================
*/

void multiWifi()  // Gestion de multiples redes WiFi y conexion a red con mejor senal
/*
Utiliza el metodo .run() para conectar a la red WiFi en la lista definida en la funcion setup().
La funcion analiza la intensidad de la sennal de cada red WiFi disponible (RSSI) 
y conecta automaticamente a la de mayor intensidad.
*/
{
  // Reiniciar ESP32 si es de noche y se perdio la conexion a red WiFi
  if(time_of_day == 0 && network_error_flag == 1)
  {
    ESP.restart();                                    // Reiniciar ESP32
  }
  
  if (wifiMulti.run() == WL_CONNECTED)                // Si se establecio correctamente la conexion WiFi
  {
    network_error_flag = 0;                           // Limpiar bandera de error de conexion
    wifi_retry_cont = 0;                              // Reiniciar contador de intentos de conexion WiFi
    digitalWrite(ledPin, HIGH);                       // Encender led indicador
  } else {                                            // De lo contrario
    network_error_flag = 1;                           // Activar bandera de error de conexion
  }
}

bool httpPostRequest(int sel) // Solicitud de Publicacion a Servidor HTTP para envio de datos en formato JSON
/*
Realiza envio de datos a dos servidores diferentes segun la seleccion realizada por el usuario mediante el parametro <sel>.
Utiliza formato JSON para el envio de datos tomando dos paquetes, uno con datos de los sensores y de estado del sistema
y otro con informacion sobre el estado de las conexiones, la red de internet utilizada, y demas banderas que sean indicadoras
de posibles errores en el sistema.
*/
{
  String thing_address = (sel == 0) ? "hydro_test" : "hydro_LOG"; // Nombre del servidor HTTP al que se eviaran los datos (seleciona entre servidor principal o de registro)
  String path = "/dweet/for/" + thing_address;        // Composicion de URL para servidor
  String content_type = "application/json";           // Definicion de formato de datos como formato JSON
  String post_data = (sel == 0) ? packageData(temp_s1, hum_s1, temp_s2, hum_s2, w_temp_s1, w_temp_s2, w_temp_s3, neo_pixel_state) : performanceLog(); // Formateo de datos por paquete

  int retries = 0;                                    // Contador de intentos
  bool success = false;                               // Condicion de ejecucion de envio de datos

  while (retries < HTTP_MAX_RETRY && success != true) // Mientras la cantidad de intentos no supere el maximo definido y no se hayan enviado datos corretamente
  {
    if (WiFi.status() != WL_CONNECTED)                // Revisar estado de conexion WiFi
    {
      network_error_flag = 1;                         // Si no esta conectado, activar bandera de error de conexion
      return false;                                   // Indicar fallo en envio de datos a servidor HTTP
    }
    HttpClient client = HttpClient(espClient, host, port);  // Cliente para conexión HTTP3
    client.setTimeout(10000);                               // Definir tiempo maximo de espera de 10 segundos

    int status_code = client.post(path, content_type, post_data); // Obtener codigo de estado de la comunicacion con el servidor HTTP
    //-------------------------Recopilacion de fecha y hora del servidor HTTP-------------------------
    String response = client.responseBody();          // Obtener el contenido de la respuesta enviada por el servidor
    JsonDocument doc1;                                // Crear objeto JSON
    deserializeJson(doc1, response);                  // Convertir respuesta del servidor HTTP a formato JSON
    String with = doc1["with"];                       // Obtener el contenido del campo "with" (contiene informacion del servidor)
    JsonDocument doc2;                                // Crear nuevo objeto JSON
    deserializeJson(doc2, with);                      // Convertir contenido a formato JSON
    String date_time = doc2["created"];               // Acceder datos de hora de respuesta
    double year = date_time.substring(0, 4).toDouble(); // Obtener anno
    int month = date_time.substring(5, 7).toInt();    // mes
    int day = date_time.substring(8, 10).toInt();     // dia
    int hour = date_time.substring(11, 13).toInt();   //hora 
    int minute = date_time.substring(14, 16).toInt(); // minuto
    int second = date_time.substring(17, 19).toInt(); // segundo

    // Ajuste de hora en caso de que la hora recibida sea menor al desfase entre la hora de Guatemala y la hora del servidor (GMT).
    // 6 horas https://www.worldtimebuddy.com/gmt-to-guatemala-guatemala-city 
    if (hour < 6) 
    {
      int time_adjust = 6 - hour;                     // Si la hora del servidor es menor a 6 (media noche en Guatemala) restarle dicho valor al factor de correcion
      hour = 24 - time_adjust;                        // Ajustar la hora utilizando el nuevo factor de correccion iniciando en 24 horas (debido al formato de 24 horas utilizado)
    } 
    else                                              // Si la hora del servidor es mayor a 6
    {
      hour = hour - 6;                                // Restar 6 (desfase) a la hora del servidor
    }

    if (status_code >= 0 && status_code < 400)        // Si se obtuvo un codigo de error entre 0 y 400 (menor a 0 = error de formato)
    {
      http_send_fail_counter = 0;                     // Limpiar contador de error de envio de datos al servidor
      if (rtc.lostPower())                            // Revisar si el modulo RTC (reloj de tiempo real) se ha desconfigurado
      {
        rtc.adjust(DateTime(year, month, day, hour, minute, second)); // Ajustar la hora con los datos del servidor ajustados si se desconfiguro el RTC
      }
      success = true; // Indicar que el envio y lectura de datos fue exitoso
    }
    else                                              // Si el codigo de error es negativo o mayor o igual a 400 | Revisar el codigo de error recibido
    { // Codigos de error HTTP: https://developer.mozilla.org/en-US/docs/Web/HTTP/Status#server_error_responses
      if (status_code < 0);                           // Volver a intentar conexion
      else if (status_code >= 500);                   // Volver a intentar conexion | Error interno del servidor
      else if (status_code >= 400 && status_code < 500) // Imposible establecer conexion | Error del cliente (relacionado a formato, direccion, datos enviados, etc)
      {
        return false;                                 // Terminar intentos de conexion
      }

      retries++;                                      // Aumentar contador de intentos
      if (retries < HTTP_MAX_RETRY)                   // Si aun no se ha llegado a la cantidad maxima de intentos
      {
        delay(HTTP_TIMEOUT);                          // Esperar 1 segundo antes de intentar nuevamente (una frecuencia de intentos elevada ocasiona error en servidor)
      }
    }
    client.stop();                                    // Detener rutina de envio de datos HTTP
  }
  if (success != true)                                // Si no fue posible enviar datos al servidor HTTP
  {
    http_send_fail_counter++;                         // Incrementar contador de intentos fallidos
  }
  return success;                                     // Regresar estado del envio
}

void getTemperatureHumidity() // Lectura de sensores DHT11 (sensores de temperatura y humedad)
/*
Utiliza un periodo de 2000 mS para intentar obtener datos de los sensores DHT11 para obtener
datos de temperatura y humedad ambiental. Los sensores DHT11 cuentan con una resolucion de 0.5 grados C y 1% de humedad.
*/
{
  if (dht_retry_cont < dht_max_retry)                 // Mientras no se haya llegado a la cantidad maxima de intentos
  {
    struct timerVar timer_dht_s1;                     // Inicializar temporizador para lectura de sensores
    timer_dht_s1.time_period = 2000;                  // Establecer periodo de lectura de sensores (2 segundos) https://www.adafruit.com/product/386
    timer_dht_s1.start_time = millis();               // Iniciar cuenta de temporizador para lectura de sensores
    if (timer_dht_s1.start_time - previous_millis_dht >= timer_dht_s1.time_period) // Si ya han pasado 2 segundos
    {
      previous_millis_dht = timer_dht_s1.start_time;  // Actualizar valor pasado del temporizador
      temp_s1 = dht_s1.readTemperature();             // Leer valores de los sensores de temperatura y humedad
      hum_s1 = dht_s1.readHumidity();                 //  ||
      temp_s2 = dht_s2.readTemperature();             //  ||
      hum_s2 = dht_s2.readHumidity();                 //  ||
      if (isnan(hum_s1) || isnan(temp_s1) || isnan(hum_s2) || isnan(temp_s2)) // Revisar si alguno de los valores es NAN (valor vacio)
      {
        dht_retry_cont++;                             // Aumentar contador de intentos de lectura de sensores DHT11
        temp_s1 = 0.0;                                // Setear valores de sensores a 0 (con tal de evitar error en envio o precesamiento de datos)
        hum_s1 = 0.0;                                 // ||
        temp_s2 = 0.0;                                // ||
        hum_s2 = 0.0;                                 // ||
        return;                                       // Salir de funcion de lectura de sensores
      } 
      else                                            // Lectura exitosa
      {
        dht_retry_cont = 0;                           // Limpiar contador de intentos de lectura de sensores DHT11
      }
    }
  }
}

String getCurrentTime(int sel) // Obtener hora y fecha actual del modulo de reloj de tiempo real (RTC)
/*
Devuelve la hora o la fecha actual obtenida del modulo RTC segun la seleccion realizada mediante el parametro <sel>.
La fecha en string utiliza el formato: aa/mm/dd (anno/mes/dia) mientras que la hora en string utiliza el formato: hh:mm:ss (hora:minuto:segundo)
*/
{
  DateTime now = rtc.now();                           // Obtener informacion de modulo RTC
  String date = String(now.year()) + "/" + String(now.month()) + "/" + String(now.day());       // Concatenar fecha en formato string
  String time = String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second());   // Concatenar hora en formato string
  return (sel == 0) ? date : time;                    // Devolver fecha u hora segun seleccion en parametro de la funcion
}

String packageData(float val1, float val2, float val3, float val4, float val5, float val6, float val7, int val8) // Empaquetado de datos de sensores en formato JSON
/*
Utiliza la libreria ArduinoJSON para almacenar los datos de los sensores en un objeto JSON, el cual se convierte en string para luego ser enviado
por medio de HTTP al servidor de Dweet.io. Este paso permite organizar los datos de manera mas concisa, evitando la concatenacion extensa de datos.
Recibe como parametros el valor de temperatura y humedad ambiente, los valores de temperatura del agua y el estado de las luces de crecimiento. 
El resto de los valores se obtienen de variables globales.
*/
{
  StaticJsonDocument<JSON_BUFFER_SIZE> doc;           // Creacion de objeto JSON para organizacion de datos
  doc["Date"] = getCurrentTime(0);                    // Annadir datos de fecha y hora del sistema para compararlos con los datos del servidor
  doc["Time"] = getCurrentTime(1);            
  doc["t_s1"] = String(val1) + "°C";                  // Annadir datos de sensores de temperatura y humedad, tanto ambiental como del agua
  doc["h_s1"] = String(val2) + "%";
  doc["t_s2"] = String(val3) + "°C";
  doc["h_s2"] = String(val4) + "%";
  doc["wt_s1"] = String(val5) + "°C";
  doc["wt_s2"] = String(val6) + "°C";
  doc["wt_s3"] = String(val7) + "°C";
  doc["ph_val"] = String(ph_val);                     // Annadir dato de nivel de pH del agua
  doc["Growlight_Sate"] = val8;                       // Annadir estado de las luces de crecimiento
  doc["Fan_State"] = fan_state;                       // Annadir estado de ventiladores
  doc["Temp_Warn"] = ambient_temperature_flag;        // Annadir bandera de temperatura ambiente
  doc["Hum_Warn"] = ambient_humidity_flag;            // Annadir bandera de humedad ambiental

  serializeJson(doc, jsonBuffer);                     // Transformar a formato String
  return String(jsonBuffer);                          // Devolver paquete de datos
}

String performanceLog() // Empaquetar variables relacionadas al funcionamiento del sistema en formato JSON
/*
Compila los datos relacionados a la conexion WiFi, asi como otras banderas de estado directamente relacionadas
al funcionamiento del sistema digital. El objeto JSON creado es serealizado para enviarlo al servidor de Dweet.io.
Esta informacion es utilizada para debuggear procesos propios del sistema y determinar puntos de fallo.
*/
{
  StaticJsonDocument<JSON_BUFFER_SIZE> doc;           // Objeto JSON con espacio limitado segun tamano de buffer establecido
  doc["Date"] = getCurrentTime(0);                    // Annadir datos de fecha y hora del sistema para compararlos con los datos del servidor
  doc["Time"] = getCurrentTime(1);
  doc["SSID"] = WiFi.SSID();                          // Agregar nombre de la red WiFi utilizada
  doc["RSSI"] = WiFi.RSSI();                          // Agregar indicador de intensidad de señal WiFi recivida por el sistema
  doc["HTTP_FAIL_CONT"] = http_send_fail_counter;     // Agregar variables de monitoreo del sistema como contador de intentos de envio fallidos pasados
  doc["TIME_OF_DAY"] = time_of_day;                   // Estado de tiempo (dia o noche) segun el sistema
  doc["active_time"] = active_time;                   // Tiempo durante el cual han estado encendidas las luces de crecimiento
  doc["day_timer"] = day_timer;                       // Hora del dia (unicamente las horas) en formato de 24 horas
  doc["Wifi_Retry"] = wifi_retry_cont;                // Contador de intentos para conexion a red WiFi

  serializeJson(doc, jsonBuffer);                     // Transformar a formato String
  return String(jsonBuffer);                          // Devolver paquete de datos
}

void ambientAirParamsControl()  // Analiza valores de temperatura y humedad ambienta y los compara con rangos preestablecidos
/*
Cambia el estado de la bandera "fan_active" si valor promedio de temperatura o humedad se encuentra fuera del rango admisible.
Calcula el promedio entre los dos sensores disponibles en el sistema, y compara dicho valor con los limites superiores e 
inferiores establecidos. Estos valores se pueden encontrar en el documento de tesis bajo la seccion de parametros de crecimiento del cilantro.
*/
{
  // Valores optimos para el entorno del sistema hidroponico (seccion de parametros de la solucion de cercimiento del cilantro en tesis hidroponia)
  ambient_T_min = 18.0;  // ⚠️VALOR FIJO POR CODIGO⚠️
  ambient_T_max = 23.5;  // ⚠️VALOR FIJO POR CODIGO⚠️
  ambient_H_min = 30.0;  // ⚠️VALOR FIJO POR CODIGO⚠️
  ambient_H_max = 75.0;  // ⚠️VALOR FIJO POR CODIGO⚠️

  //Logica de correcciones de temperatura y humedad
  setup_temp = twoValAverage(temp_s1, temp_s2);       // Promedio de temperatura en sistema entre ambos sensores DHT11
  setup_hum = twoValAverage(hum_s1, hum_s2);          // Promedio de humedad en sistema entre ambos sensores DHT11
  if (setup_temp < ambient_T_min)                     // Evaluar limite inferior admisible de temperatura ambiente
  {
    ambient_temperature_flag = 1;                     // Bandera de temperatura ambiente en 1 implica valores exceden limite inferior admisible
    // Espacio reservado para control de temperatura hacia arriba
  }
  else if (setup_hum < ambient_H_min)                 // Evaluar limite inferior admisible de humedad ambiental
  {
    ambient_humidity_flag = 1;                        // Bandera de humedad ambiental en 1 implica valores exceden limite inferior admisible
    // Espacio reservado para control de humedad hacia arriba
  }
  else if (setup_temp >= ambient_T_max)               // Evaluar limite superior admisible de temperatura ambiente 
  {
    ambient_temperature_flag = 2;                     // Bandera de temperatura ambiente en 2 implica valores exceden limite superior admisible
    fan_activate = 1;                                 // Activar indicador para activacion de ventiladores
  }
  else if (setup_hum >= ambient_H_max)                // Evaluar limite superior admisible de humedad ambiental
  {
    ambient_humidity_flag = 2;                        // Bandera de humedad ambiental en 2 implica valores exceden limite superior admisible
    fan_activate = 1;                                 // Activar indicador para activacion de ventiladores
  }
  else                                                // Temperatura y humedad en rangos aceptables
  {
    ambient_temperature_flag = 0;                     // Bandera de temperatura ambiente en 0 implica valores nominales
    ambient_humidity_flag = 0;                        // Bandera de humedad ambiente en 0 implica valores nominales
    fan_activate = 0;                                 // Desactivar indicador para activacion de ventiladores 
  }
}

float twoValAverage(float dat1, float dat2) // Funcion para calcular el promedio de dos datos de tipo float
/*
Admite dos datos tipo float como paramaetros y regresa el promedio entre ambos datos utilizando la formula:
y = (x1+x2)/2
*/
{
  float average = 0.0;                                // Se utiliza variable privada para evitar el uso de mas variables globales
  average = (dat1 + dat2) / 2;
  return average;
}

void fanControl() // Funcion para activar o desactivar los ventailadores
/*
Analiza constantemente el estado de los ventiladores activos o estaticos (ACTIVE o IDLE respectivamenet)
junto con el estado de la bandera "fan_activate". Si los ventiladores estan desactivados y la bandera
"fan_activate" esta en 1, se encienten los ventiladores, de lo contrario, se apagan. Esto se logra mediante
dos reles de 5V a 120VAC controlados por dos transistores 2N2222A conectados al ESP32.
*/
{
  if (fan_activate == 1 && fan_state == IDLE)         // La variable fan_state permite evitar que se active el pin constantemente si ya se encendieron los ventiladores
  {
    fan_state = ACTIVE;                               // Cambiar el estado de los ventiladores a activos
    control_active_flag = 1;                          // Indicar que el ciclo de control esta activo
    //-----Activar ventiladores-----
    digitalWrite(FAN1PIN, HIGH);
    digitalWrite(FAN2PIN, HIGH);
  }
  else if (fan_activate == 0 && fan_state == ACTIVE) 
  {
    fan_state = IDLE;                                 // Cambiar el estado de los ventiladores a estaticos
    control_active_flag = 0;                          // Indicar que el ciclo de control no esta activo
    //-----Desactivar ventiladores-----
    digitalWrite(FAN1PIN, LOW);
    digitalWrite(FAN2PIN, LOW);
  }
}

void waterTempInit() // Inicializacion de sensores de temperatura del agua
/*
Asigna los nombres: tankSensor, growChannel1 y growChannel2 a los sensores segun el orden de 
sus direcciones (basicamente su orden de conexion) iniciando con el sensor mas cercano.
Adicionalmente, establece una resolucion para los sensores de 9 Bits.
*/
{
  sensors.begin();                                    // Iniciar sensores de temperatura de agua}
  //-----------------Asignacion de nombres segun direccion-----------------
  if (!sensors.getAddress(tankSensor, 0));
  if (!sensors.getAddress(growChannel1, 1));
  if (!sensors.getAddress(growChannel2, 2));

  //-----------------Definicion de resolucion-----------------
  sensors.setResolution(tankSensor, DS18_PRECISION);
  sensors.setResolution(growChannel1, DS18_PRECISION);
  sensors.setResolution(growChannel2, DS18_PRECISION);
}

float getWaterTemperature(DeviceAddress deviceAddress) // Lectura de sensor especificado de temperatura del agua
/*
Utiliza la dirección del sensor de temperatura para solicitar los datos de temperatura en un momento dado. 
En caso de que el sensor esté desconectado, se devuelve un valor de 255.0 para determinar claramente cuando
uno de los sensores de temperatura han dejado de funcionar, puesto que este valor poco probable de obtener.
*/
{
  float tempC = sensors.getTempC(deviceAddress);      // Guardar la temperatura en una variable privada
  if (tempC == DEVICE_DISCONNECTED_C)                 // Revisar estado de conexion del sensor
  {
    tempC = 255.0;                                    // Asignar valor de alerta
    return tempC;
  } 
  else 
  {
    return tempC;                                     // Regresar valor de temperatura
  }
}

void setNeopixelState() // Encendido y apagado de luces de crecimiento segun bandera de dia
/*
Mientras que sea de dia, la funcion calcula el tiempo qen minutos que han estado encendidas las luces de crecimiento.
Las luces se encienden utilizando una secuencia alternante entre rojo, azul y fusia (3*n, 3*n-1 y 3*n-2 respectivamente).
Los colores utilizados buscan aprovechar el rango de 400 a 500 nm (luz azul) y 600 a 700 nm (luz roja) incluyendo un porcentaje
de luz verde (rango de 500 a 600 nm). Al incrementar el porcentaje de luz roja, se logra una mejor absorción de luz por parte
de las plantas.
*/
{
  if (time_of_day == 1) 
  {
    if (active_time <= (growLightTime * 60)) 
    {
      struct timerVar lightingTime;                   // Definir timer para monitorear el tiempo que han estado encendidas las luces
      lightingTime.start_time = millis();
      lightingTime.time_period = 60000;               // Establecer un periodo de un minuto para el contador de tiempo activo (active_time) de las luces de crecimiento
      if (lightingTime.start_time - glt_prev_time >= lightingTime.time_period) 
      {
        active_time++;                                // Aumentar el contador de tiempo activo
        glt_prev_time = lightingTime.start_time;      // Actualizar timer de monitoreo de tiempo activo
      }

      if (neo_pixel_state == 0)                       // Determinar si las luces estan encendidas o no
      {
        neo_pixel_state = 1;                          // Definir el estado de las luces de crecimiento como encendidas
        NeoPixel.clear();                                                            // Limpiar la configuracion anterior de las luces (repisa inferior)
        NeoPixel2.clear();                                                           // ||            ||            ||            ||   (repisa superior)
        for (int pixel = 0; pixel < NUM_PIXELS; pixel++) {                           // Iterar cada pixel definido (ambas repisas con una cantidad de 30)
          NeoPixel.setPixelColor(3 * pixel, NeoPixel.Color(255, 0, 0));              // Rojo
          NeoPixel2.setPixelColor(3 * pixel, NeoPixel2.Color(255, 0, 0));            // Rojo (repisa superior)
          NeoPixel.setPixelColor((3 * pixel) - 1, NeoPixel.Color(0, 0, 255));        // Azul
          NeoPixel2.setPixelColor((3 * pixel) - 1, NeoPixel2.Color(0, 0, 255));      // Azul (repisa superior)
          NeoPixel.setPixelColor((3 * pixel) - 2, NeoPixel.Color(255, 110, 255));    // Amarilo = 250, 255, 0 | Fusia = 255, 110, 255
          NeoPixel2.setPixelColor((3 * pixel) - 2, NeoPixel2.Color(255, 110, 255));  // Amarilo | Fusia = 255, 110, 255 (repisa superior)
        }
        NeoPixel.show();                              // Actualizar el color y estado de los neopixeles en las tiras de luces
        NeoPixel2.show();                             // Actualizar el color y estado de los neopixeles en las tiras de luces (repisa superior)
      }
    }
    else if (active_time > (growLightTime * 60) && neo_pixel_state == 1)  // Si el tiempo activo es mayor al tiempo establecido para e las luces de crecimiento y las luces estan encendidas
    {
      //------------Apagar luces y actualizar su estado------------
      NeoPixel.clear();
      NeoPixel2.clear();                              // (repisa superior)
      NeoPixel.show();
      NeoPixel2.show();                               // (repisa superior)
      neo_pixel_state = 0;                            // Definir el estado de las luces de crecimiento como apagadas
    }
  } else if (time_of_day == 0)                        // Si no es de dia
  {
    //------------Apagar luces y actualizar su estado------------
    NeoPixel.clear();
    NeoPixel2.clear();                                // (repisa superior)
    NeoPixel.show();
    NeoPixel2.show();                                 // (repisa superior)
    neo_pixel_state = 0;                              // Definir el estado de las luces de crecimiento como apagadas
  }
}

void dayCounter() // Evaluacion de la hora del dia
/*
Utiliza la hora, en formato de 24H, dada por el modulo RTC para mantener regitro de la hora del dia. Mientras 
que la hora se mantenta entre la hora establecida del amanecer y la hora establecida del anochecer (morning_time
y night_time respectivamente) la bandera "time_of_day" se mantiene en 1, de lo contrario, cuando la hora sea
mayor que la hora del anochecer o menor a la hora del amanecer, la bandera "time_of_day" se mantiene en 0.
*/
{
  DateTime now_c = rtc.now();  // El sufijo "_c" en la variable "now" es utilizado para identificar esta variable como un contador,
  //diferente de la variable usada en la funcion getCurrentTime
  day_timer = now_c.hour();
  if (day_timer >= morning_time && day_timer < night_time) {
    time_of_day = 1;
  } else if (day_timer >= night_time || day_timer < morning_time) {
    time_of_day = 0;
    active_time = 0;
  }
}

void phRegulationControl()  // Funcion para regular el valor de pH en la solucion de crecimiento del sistema
/*
Compara el valor de pH, obtenido por el sensor, con un valor maximo y uno minimo, los cuales establecen el rango adecuado de pH
para el crecimiento de las plantas. Utiliza un periodo de 500 milisegundos para activar los servomotores que accionan las valculas
de bola  para cada dispensador de solucion correctora. Activa la bandera de control activo al abrir la valvula, y la desactiva
una vez el valor de pH se encuentre en su rango admisible, siempre y cuando los ventiladores no esten activos. Esta ultima condicion
asegura que se mantenga el estado de control activo si los ventiladores fueron los que activaron dicha bandera. 
*/
{
  phMaxVal = 6.5; // ⚠️VALOR FIJO POR CODIGO⚠️       // Valor maximo de pH admisible para el crecimiento del cilantro (pag. 15, tesis hidroponia)
  phMinVal = 5.5; // ⚠️VALOR FIJO POR CODIGO⚠️       // Valor minimo de pH admisible para el crecimiento del cilantro (pag. 15, tesis hidroponia)
  struct timerVar dispense_timer;                     // Inicializacion de temporizador para dispensar solucion de pH
  dispense_timer.time_period = 500;                   // Periodo del temporizador
  if (ph_val > phMaxVal)                              // Evaluar limite superior de pH
  {
    control_active_flag = 1;                          // Activar bandera de control activo
    phDownServo.write(OPEN);                          // Abrir valvula (cambiar angulo del servo) para correjir pH hacia abajo
    dispense_timer.start_time = millis();             // Iniciar temporizador
    if(dispense_timer.start_time - previous_dispense_time == dispense_timer.time_period)  // Al finalizar el periodo
    {
      previous_dispense_time = dispense_timer.start_time;  // Actualizar valor del temporizador
      phDownServo.write(CLOSE);                       // Cerrar valvula (cambiar angulo del servo a 0 grados)
    }
  }
  else if (ph_val < phMinVal)                         // Evaluar limite inferior de pH
  {
    control_active_flag = 1;                          // Activar bandera de control activo
    phUpServo.write(OPEN);                            // Abrir valvula (cambiar angulo del servo) para correjir pH hacia arriba
    dispense_timer.start_time = millis();             // Iniciar temporizador
    if(dispense_timer.start_time - previous_dispense_time == dispense_timer.time_period)  // Al finalizar el periodo
    {
      previous_dispense_time = dispense_timer.start_time; // Actualizar valor del temporizador
      phUpServo.write(CLOSE);                         // Cerrar valvula (cambair angulo del servo a 0 grados)
    }
  }
  else                                                // Si el valor de pH se encuentra en el rango admisible
  {
    if(fan_state != ACTIVE)                           // Revisar estado de los ventiladores
    {
      control_active_flag = 0;                        // Desactivar bandera de control activo
    }
    //------Asegurar que ambas vlvulas esten cerradas------
    phUpServo.write(CLOSE);
    phDownServo.write(CLOSE);
  }
}
