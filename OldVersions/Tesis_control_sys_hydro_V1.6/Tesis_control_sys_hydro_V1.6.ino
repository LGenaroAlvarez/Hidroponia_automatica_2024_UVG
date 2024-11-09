/*  Versión 1.6
    Autor: Ing. Luis Genaro Alvarez Sulecio
    Tesis de sistema hidroponico automatico
    Comentarios: 
    - Lectura de módulo RTC DS3231
    - Cambio de uso de concatenacion 
    para paquetes de datos a formato JSON
    - Cambio de estructura de funcion loop()
    - Inclusion de modulo RTC DS3231
*/
//---------------------Librerias utilizadas---------------------------
#include <WiFi.h>                 // Libreria para conexion wifi
#include <ArduinoHttpClient.h>    // Librería de arduino para conexiones HTTP> https://github.com/arduino-libraries/ArduinoHttpClient/blob/master/README.md
#include <String.h>               // Formato de informacion en tipo String
#include <ArduinoJson.h>          // Formato de informacion en tipo JSON para envio por HTTP
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <RTClib.h>               // https://learn.adafruit.com/metro-minimalist-clock/code-it


//--------------Pin Definitions---------
#define DHTPIN 4                                                        // 10k Ohm resistor
#define DHTPIN2 0                                                       //    ||    ||
#define DHTTYPE DHT11                                                   // Tipo de sensor DHT instalado (entre DHT11 y DHT22)
#define ONE_WIRE_BUS 15                                                 // 4.7k Ohm resistor
#define FAN1PIN 12                                                      // 100 Ohm resistor to 2N2222A collector pin
#define FAN2PIN 14                                                      //    ||    ||            ||        ||
#define NEOPIXELPIN 27                                                  // Pin de control de luces neopixel

//--------------Constant definitions-------------
#define ACTIVE 1                                                        // Estado activo (ventiladores)
#define IDLE 0                                                          // Estado inactivo     ||
#define DS18_PRECISION 9                                                // Presicion de sensores DS18B20
#define NUM_PIXELS 60                                                   // Cantidad de luces led en tira NeoPixel (121 en sistema completo)
#define HTTP_TIMEOUT 5000                                               // Tiempo de espera para respuesta de servidor HTTP
#define HTTP_MAX_RETRY 3                                                // Cantidad maxima de intentos para envio de datos por HTTP
#define JSON_BUFFER_SIZE 256                                            // Tamano del buffer para empaquetado de datos en formato JSON
#define GLOBAL_PERIOD 30000                                             // 30 segundos
#define TIME_24_HOUR true

//------------Structs--------------
struct timerVar
{
	unsigned long start_time;
	unsigned long time_period;
  unsigned long prev_time; // Pending
};

//--------Conexion a internet (wifi)----------

//const char* SSID = "Blue Dragon";
const char* SSID = "Tesis20168";                                        // SSID de red WiFi
const char* password = "12345671";                                      // Contrasena de red WiFi

// Detalles del servidor para http
const char* host = "dweet.io";                                          // Servidor
const int port = 80;                                                    // Puerto de conexión para servidor HTTP
char jsonBuffer[JSON_BUFFER_SIZE];                                      // Buffer para envio de datos a servidor
// https://dweet.io/get/dweets/for/hydro_test                           // URL de servidor para lecturas de sensores
// https://dweet.io/get/dweets/for/hydro_LOG                            // URL de servidor para informaci[on sobre el estado del sistema

// Variables para tiempo de reconexion de wifi
const int timeout_period = 20;
int network_timeout = 0;
int network_error_flag = 0;
unsigned long previous_time = 0;
unsigned long interval = 1000;

//------------Definicion para cliente WiFi----------

WiFiClient espClient;                                                   // Creacion de cliente para conexion WiFi del ESP32

//----------Variables de sensores-----------
float temp_s1 = 0;
float hum_s1 = 0.0;
float temp_s2 = 0;
float hum_s2 = 0.0;
float w_temp_s1 = 0.0;
float w_temp_s2 = 0.0;
float w_temp_s3 = 0.0;
float ambient_T_min = 0.0;
float ambient_T_max = 0.0;
float ambient_H_min = 0.0;
float ambient_H_max = 0.0;
float setup_temp = 0.0;
float setup_hum = 0.0;

int ambient_temperature_flag = 0; //0 = normal, 1 = low, 2 = high
int ambient_humidity_flag = 0; //0 = normal, 1 = low, 2 = high
int dht_retry_cont = 0;
int dht_max_retry = 2;
int fan_activate = 0;
int fan_state = IDLE;
int control_active_flag = 0;
unsigned long previous_millis_dht = 0;
unsigned long http_prev_time = 0;
unsigned long current_fan_time = 0;
unsigned long glt_prev_time = 0;
unsigned long current_day_time = 0;
unsigned long global_timer_prev_time = 0;

//-------------Contadores------------
int time_timeout = 0;
unsigned long active_time = 0;                                          // Tiempo que han estado activas las luces de crecimiento
int day_timer = 0;
int http_send_fail_counter = 0;

int sre_cont = 0;                                                       // Sensor Reading Event Counter
int dse_cont = 0;                                                       // Data Send Event Counter
int cse_cont = 0;                                                       // Control System Event Counter
int wre_cont = 0;                                                       // WiFi Recconect Event Counter

//-----------------------Variables generales---------------
const int ledPin = LED_BUILTIN;                                         // the number of the LED pin
int ledState = LOW;                                                     // ledState used to set the LED
int neo_pixel_state = 0;                                                // Neopixel light state (growlight state)
float growLightTime = 7;                                                // Valor en horas
int time_of_day = 1;

int morning_time = 6;                                                   // am - Hora local a la que amanece
int night_time = 17;                                                    // pm - Hora local a la que anochece

// RTC variables
char days[7][12] = {"Domingo", "Lunes", "Martes", "Miercoles", "Jueves", "Viernes", "Sabado"};

//--------------Inicializacion de modulos del sistema----------------
DHT dht_s1(DHTPIN, DHTTYPE);                                            // Activar sensor DHT11 numero 1
DHT dht_s2(DHTPIN2, DHTTYPE);                                           // Activar sensor DHT11 numero 1
OneWire oneWire(ONE_WIRE_BUS);                                          // Bus de comunicacion para sensores de protocolo onewire
DallasTemperature sensors(&oneWire);                                    // Inicializacion de sensores de temperatura de agua
DeviceAddress tankSensor, growChannel1, growChannel2;                   // Definicion de sensores de temperatura de agua
Adafruit_NeoPixel NeoPixel(NUM_PIXELS, NEOPIXELPIN, NEO_GRB + NEO_KHZ800);  // Definicion de luces led (NEOPIXELS)
RTC_DS3231 rtc;                                                         // RTC conectado a pines SCL y SDA: 22 y 21

//-----------Prototipos de funciones--------
//void initTimers();                                                    // Activar lectura de tiempo real
void initWifi();                                                        // Iniciar conexion WiFi
bool httpPostRequest(int sel);                                          // Envio de datos a servidor WiFi
void getTemperatureHumidity();                                          // Lectura de sensores de temperatura y humedad
String packageData(float val1, float val2, float val3, float val4, float val5, float val6, float val7, float val8);     // Empaquetado de datos en formato JSON
String performanceLog();                                                // Empaquetado de datos para log de rendimiento
String getCurrentTime(int sel);                                                  // Lectura de tiempo real (mediante modulo RTC DS3231)
float twoValAverage(float dat1, float dat2);                            // Calculo de valor promedio para dos valores
void fanControl();                                                      // Accionamiento de ventiladores
void ambientAirParamsHandler();                                         // Control de parametros ambientales
void waterTempInit();                                                   // Iniciar sensores de temperatura de agua
float getWaterTemperature(DeviceAddress deviceAddress);                 // Lectura de sensores de temperatura de agua
void setNeopixelState();                                                // Cambiar el estado de los leds segun tiempo recorrido
void dayCounter(); // REVISE                                            // Mantener registro de las horas transcurridas desde inicializacion

/*
=====================================================================================================================================================================================
============================================================================PROGRAM START============================================================================================
=====================================================================================================================================================================================
*/

void setup() 
{
  // Configuracioens iniciales
  Serial.begin(57600);
  pinMode(FAN1PIN, OUTPUT);
  pinMode(FAN2PIN, OUTPUT);
  pinMode(ledPin, OUTPUT);
  NeoPixel.begin();
  NeoPixel.clear();                                                               // Apagar luces LED
  NeoPixel.show();                                                                // Actualizar luces LED
  neo_pixel_state = 0;                                                            // Indicar que las luces led estan apagadas
  Wire.begin();
  rtc.begin();
  initWifi();
  dht_s1.begin();
  dht_s2.begin();
  waterTempInit();
  sensors.begin();
}

void loop()
{
  //-----------------Actualizacion de hora del dia--------------------
  dayCounter();

  //-------------------------Inicializacion de variables---------------------------------
  int sre_period = (control_active_flag == 0) ? 5 : 2;                            // Read sensors every 2.5 minutes or every minute (if control system is active)
  int dse_period = 10;                                                            // Send data every 5 minutes
  int cse_period = (control_active_flag == 0) ? 10 : 5;                           // Update control systems every 5 minutes or every 2.5 minutes (if control system is active)
  int wre_period = 60;                                                            // Attempt wifi recconect every 30 minutes
  
  struct timerVar global_timer;                                                   // Contador global para eventos
  global_timer.start_time = millis();
  if (global_timer.start_time - global_timer_prev_time >= GLOBAL_PERIOD)          // Enter if condition every minute
  {
    global_timer_prev_time = global_timer.start_time;

    //--------------------------Increase Event Counters----------------------------------
    sre_cont++;                                                                   // Sensor Reading Event Counter
    dse_cont++;                                                                   // Data Send Event Counter
    cse_cont++;                                                                   // Control System Event Counter

    //---------------------------Sensor Reading Event------------------------------------
    if (sre_cont >= sre_period)
    {
      sre_cont = 0;
      dht_retry_cont = 0;
      getTemperatureHumidity();                       // Lectura de sensores DHT11

      sensors.requestTemperatures();                  // Solicitud de valores de temperatura
      w_temp_s1 = getWaterTemperature(tankSensor);    // Lectura de sensores DS18B20
      w_temp_s2 = getWaterTemperature(growChannel1);  //      |||       |||||
      w_temp_s3 = getWaterTemperature(growChannel2);  //      |||       |||||
    }

    //--------------------------------Control System Event--------------------------------
    if (cse_cont >= cse_period)
    {
      cse_cont = 0;
      // Analizar parametros y ejecutar ciclos de control
      ambientAirParamsHandler();
      fanControl();
    }

    //---------------------------------Data Send Event------------------------------------
    if (dse_cont >= dse_period)
    {
      dse_cont = 0;
      if (httpPostRequest(0))
      {
        http_send_fail_counter = 0;
      }
      else
      {
        network_error_flag = 1;
      }
      if (httpPostRequest(1))
      {
        http_send_fail_counter = 0;
      }
      else
      {
        network_error_flag = 1;
      }
    }

    //---------------------------------WiFi Reconnect Event--------------------------------
    if(network_error_flag == 1 && wre_cont >= wre_period) // && time_of_day == 0
    {
      if(WiFi.status()!= WL_CONNECTED)
      {
        initWifi();
      }
      else
      {
        network_error_flag = 0;
      }
    }
  }

  //-----------------------------------Check WiFi Connection-------------------------------
  if (WiFi.status() == WL_CONNECTED && digitalRead(ledPin) != HIGH)
  {
    ledState = HIGH;
    digitalWrite(ledPin, ledState);
    network_error_flag = 0;
  }
  else if (WiFi.status() != WL_CONNECTED && digitalRead(ledPin) != LOW)
  {
    ledState = LOW;
    digitalWrite(ledPin, ledState);
    network_error_flag = 1;
  }

  //-----------------Actualizacion de luces de crecimiento---------------
  setNeopixelState();
}

/*
=====================================================================================================================================================================================
==========================================================================SYSTEM FUNCTIONS===========================================================================================
=====================================================================================================================================================================================
*/

void initWifi() // Conexion a red WiFi utilizando credenciales PEAP no de empresa
{
  if (network_error_flag == 1 && time_of_day == 0) //If couln't connect to wifi after retry and it's night time, restart ESP32 module
  {
    ESP.restart();
  }

  //WiFi.begin(ssid, WPA2_AUTH_PEAP, EAP_IDENTITY, EAP_USERNAME, EAP_PASSWORD); // Para conexion a red wifi de la universidad
  
  if (network_error_flag != 1)  // Network error flag initialized at 0, this runs only on connection after controller reboot.
  {
    WiFi.disconnect(true);  //disconnect form wifi to set new wifi connection
    WiFi.mode(WIFI_STA);
  }

  WiFi.begin(SSID, password); // Para conexion a red de dispositivo movil

  while (WiFi.status() != WL_CONNECTED && network_timeout < timeout_period) 
  {
    struct timerVar current_time;
    current_time.start_time = millis();
    if (current_time.start_time - previous_time >= interval)  
    {
      previous_time = current_time.start_time;
      if (ledState == LOW) 
      {
      ledState = HIGH;
      } 
      else 
      {
        ledState = LOW;
      }
      network_timeout++;
      digitalWrite(ledPin, ledState);
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) // If connection is successfull
  {
    network_error_flag = 0;
    digitalWrite(ledPin, HIGH);
  }
  else if(WiFi.status() != WL_CONNECTED && network_timeout >= timeout_period) // If couldn't connect and exceeded connection timeout, enable network error flag
  {
    network_error_flag = 1;
  }
}


bool httpPostRequest(int sel)
{
  String thing_address = (sel == 0) ? "hydro_test" : "hydro_LOG";
  String path = "/dweet/for/" + thing_address;
  String content_type = "application/json";
  String post_data = (sel == 0) ? packageData(temp_s1, hum_s1, temp_s2, hum_s2, w_temp_s1, w_temp_s2, w_temp_s3, neo_pixel_state) : performanceLog();

  int retries = 0;
  bool success = false;

  while (retries < HTTP_MAX_RETRY && success != true)
  {
    if(WiFi.status() != WL_CONNECTED)
    {
      network_error_flag = 1;
      return false;
    }
    HttpClient client = HttpClient(espClient, host, port);  // Cliente para conexión HTTP3
    client.setTimeout(10000);                               // Definir timeout de 10 segundos

    int status_code = client.post(path, content_type, post_data);
    String response = client.responseBody();
    JsonDocument doc1;
    deserializeJson(doc1, response);
    String with = doc1["with"];
    JsonDocument doc2;
    deserializeJson(doc2, with);
    String date_time = doc2["created"];
    double year = date_time.substring(0,4).toDouble();
    int month = date_time.substring(5,7).toInt();
    int day = date_time.substring(8,10).toInt();
    int hour = date_time.substring(11,13).toInt() - 6;
    int minute = date_time.substring(14,16).toInt();
    int second = date_time.substring(17,19).toInt();

    if(status_code >= 0 && status_code < 400)
    {
      http_send_fail_counter = 0;
      if (rtc.lostPower())
      {
        rtc.adjust(DateTime(year, month, day, hour, minute, second));
      }
      success = true;
    }
    else
    {
      if(status_code < 0);                                  // Volver a intentar conexion
      else if(status_code >= 500);                          // Volver a intentar conexion
      else if(status_code >= 400 && status_code < 500)
      {
        return false;                                       // Terminar intentos de conexion
      }

      retries++;
      if(retries < HTTP_MAX_RETRY)
      {
        delay(HTTP_TIMEOUT);
      }
    }
    client.stop();                                          // Detener rutina de envio de datos HTTP
  }
  if (success != true)
  {
    http_send_fail_counter++;
  }
  return success;                                           // Regresar estado del envio
}

void getTemperatureHumidity()
{
  if (dht_retry_cont < dht_max_retry)
  {
    struct timerVar timer_dht_s1;
    timer_dht_s1.time_period = 2000;
    timer_dht_s1.start_time = millis();
    if (timer_dht_s1.start_time - previous_millis_dht >= timer_dht_s1.time_period)
    {
      previous_millis_dht = timer_dht_s1.start_time;
      temp_s1 = dht_s1.readTemperature();
      hum_s1 = dht_s1.readHumidity();
      temp_s2 = dht_s2.readTemperature();
      hum_s2 = dht_s2.readHumidity();
      if (isnan(hum_s1) || isnan(temp_s1) || isnan(hum_s2) || isnan(temp_s2)) 
      {
        dht_retry_cont++;
        temp_s1 = 30.0;
        hum_s1 = 0.0;
        temp_s2 = 31.0;
        hum_s2 = 0.0;
        return;
      }
      else
      {
        dht_retry_cont = 0;
      }
    }
  }
}

String getCurrentTime(int sel)
{
  DateTime now = rtc.now();
  String date = String(now.year()) + "/" + String(now.month()) + "/" + String(now.day());
  String time = String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second());
  return (sel == 0) ? date : time;
}

String packageData(float val1, float val2, float val3, float val4, float val5, float val6, float val7, int val8)
{
  StaticJsonDocument<JSON_BUFFER_SIZE> doc;

  doc["Date"] = getCurrentTime(0);
  doc["Time"] = getCurrentTime(1);
  doc["t_s1"] = String(val1) + "°C";
  doc["h_s1"] = String(val2) + "%";
  doc["t_s2"] = String(val3) + "°C";
  doc["h_s2"] = String(val4) + "%";
  doc["wt_s1"] = String(val5) + "°C";
  doc["wt_s2"] = String(val6) + "°C";
  doc["wt_s3"] = String(val7) + "°C";
  doc["Growlight_Sate"] = val8;
  doc["Fan_State"] =  fan_state;
  doc["Temp_Warn"] = ambient_temperature_flag;
  doc["Hum_Warn"] = ambient_humidity_flag;

  serializeJson(doc, jsonBuffer);
  return String(jsonBuffer);
}

String performanceLog()
{
  StaticJsonDocument<JSON_BUFFER_SIZE> doc;
  double network_RSSI = WiFi.RSSI();

  doc["Date"] = getCurrentTime(0);
  doc["Time"] = getCurrentTime(1);
  doc["SSID"] = SSID;
  doc["RSSI"] = network_RSSI;
  doc["POST_FAIL_CONT"] = http_send_fail_counter;
  //doc["Error"] = error_log_buffer;

  serializeJson(doc, jsonBuffer);
  return String(jsonBuffer);
}

void ambientAirParamsHandler() // Checks ambient temperature and humidity values, changes state of fan_active flag if average values outside established limits
{
  ambient_T_min = 18.0;
  ambient_T_max = 28.0;
  ambient_H_min = 30.0;
  ambient_H_max = 78.0;

  setup_temp = twoValAverage(temp_s1, temp_s2);
  setup_hum = twoValAverage(hum_s1, hum_s2);
  if (setup_temp < ambient_T_min)
  {
    ambient_temperature_flag = 1;
  }
  else if (setup_hum < ambient_H_min)
  {
    ambient_humidity_flag = 1;
  }
  else if (setup_temp > ambient_T_max || setup_hum > ambient_H_max)
  {
    ambient_temperature_flag = 2;
    fan_activate = 1;
  }
  else
  {
    fan_activate = 0;
  }
}

float twoValAverage(float dat1, float dat2)
{
  float average = 0.0;
  average = (dat1+dat2)/2;
  return average;
}

void fanControl()
{
  if (fan_activate == 1 && fan_state == IDLE)         // La variable fan_state permite evitar que se active el pin constantemente si ya se encendieron los ventiladores
  {
    fan_state = ACTIVE;
    control_active_flag = 1;
    digitalWrite(FAN1PIN, HIGH);
    digitalWrite(FAN2PIN, HIGH);
    struct timerVar timer_fan;
    timer_fan.time_period = 30000;  // n = 30 segundos
    timer_fan.start_time = millis();
    //--------------------------Keep fans ative for n seconds---------------------------------
    if (timer_fan.start_time - current_fan_time > timer_fan.time_period)
    {
      current_fan_time = timer_fan.start_time;
      fan_state = IDLE;
      digitalWrite(FAN1PIN, LOW);
      digitalWrite(FAN2PIN, LOW);
    }
  }
  //--------------------------Desactivar los ventiladores-------------------------------------
  else if (fan_activate == 0 && fan_state == ACTIVE)
  {
    fan_state = IDLE;
    digitalWrite(FAN1PIN, LOW);
    digitalWrite(FAN2PIN, LOW);
    control_active_flag = 0;
  }
}

void waterTempInit()
{
  sensors.begin();
  if (!sensors.getAddress(tankSensor, 0));
  if (!sensors.getAddress(growChannel1, 1));
  if (!sensors.getAddress(growChannel2, 2));

  sensors.setResolution(tankSensor, DS18_PRECISION);
  sensors.setResolution(growChannel1, DS18_PRECISION);
  sensors.setResolution(growChannel2, DS18_PRECISION);
}

float getWaterTemperature(DeviceAddress deviceAddress)
{
  float tempC = sensors.getTempC(deviceAddress);
  if(tempC == DEVICE_DISCONNECTED_C)
  {
    tempC = 0.0;
    return tempC;
  }
  return tempC;
}

void setNeopixelState()
{
  if (time_of_day == 1)       // Conversion de horasa minutos para el tiempo de luz
  {
    if (active_time < (growLightTime*60))
    {
      struct timerVar lightingTime;
      lightingTime.start_time = millis();
      lightingTime.time_period = 60000;
      if (lightingTime.start_time - glt_prev_time >= lightingTime.time_period)
      {
        active_time++;
        glt_prev_time = lightingTime.start_time;
      }
      if (neo_pixel_state == 0)
      {
        neo_pixel_state = 1;
        NeoPixel.clear();  // 
        for (int pixel = 0; pixel < NUM_PIXELS; pixel++) {           // for each pixel
          NeoPixel.setPixelColor(3*pixel, NeoPixel.Color(255, 0, 0));  // Rojo
          NeoPixel.setPixelColor((3*pixel)-1, NeoPixel.Color(0, 0, 255));  // Azul
          NeoPixel.setPixelColor((3*pixel)-2, NeoPixel.Color(255, 110, 255));  // Fusia
        }
        NeoPixel.show();  // update to the NeoPixel Led Strip
      }
    }
  }
  
  if (active_time >= (growLightTime*60) && neo_pixel_state == 1)
  {
    NeoPixel.clear();   // Apagar luces LED
    NeoPixel.show();    // Actualizar luces LED
    neo_pixel_state = 0;
  }
}

void dayCounter()
{
  DateTime now = rtc.now();
  day_timer = now.hour();
  if (day_timer >= morning_time)
  {
    time_of_day = 1;
  }
  else if (day_timer >= night_time)
  {
    time_of_day = 0;
    active_time = 0;
  }
}

