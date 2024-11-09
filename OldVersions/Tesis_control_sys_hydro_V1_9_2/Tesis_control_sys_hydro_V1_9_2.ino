/*  Versión 1.9.2
    Autor: Ing. Luis Genaro Alvarez Sulecio
    Tesis de sistema hidroponico automatico
    Comentarios: 
    - 
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
#define CALI_BTN_1 5            // Pin para boton de seleccion de modo de calibracion del sensor de pH

//--------------Constant definitions-------------
#define ACTIVE 1              // Estado activo (ventiladores)
#define IDLE 0                // Estado inactivo     ||
#define DS18_PRECISION 9      // Presicion de sensores DS18B20
#define NUM_PIXELS 60         // Cantidad de luces led en tira NeoPixel (122 en sistema completo)
#define HTTP_TIMEOUT 5000     // Tiempo de espera para respuesta de servidor HTTP
#define HTTP_MAX_RETRY 3      // Cantidad maxima de intentos para envio de datos por HTTP
#define JSON_BUFFER_SIZE 256  // Tamano del buffer para empaquetado de datos en formato JSON
#define GLOBAL_PERIOD 30000   // 30 segundos
#define TIME_24_HOUR true     // Configuracion de RTC en formato de 24H
#define OPEN 45               // Valor de grados para abrir valvula de control de pH
#define CLOSE 0               // Valor de grados para cerrar valvula de control de pH

//------------Structs--------------
struct timerVar {
  unsigned long start_time;   // Tiempo de inicio para funcion Millis
  unsigned long time_period;  // Intervalo de tiempo para funcion Millis
  unsigned long prev_time;    // Pending
};

//--------Conexion a internet (wifi)----------

const char* SSID_1 = "Blue Dragon";     // SSID de red WiFi #1
const char* SSID_2 = "Tesis20168";      // SSID de red WiFi #2
const char* password_1 = "12345671";    // Contrasena de red WiFi
const char* SSID_3 = "CLARO1_84DF73";   // SSID de red WiFi #3
const char* password_2 = "VSRU5ORKXQ";  // Contrasena de red WiFi

// Detalles del servidor para http
const char* host = "dweet.io";      // Servidor
const int port = 80;                // Puerto de conexión para servidor HTTP
char jsonBuffer[JSON_BUFFER_SIZE];  // Buffer para envio de datos a servidor
// https://dweet.io/get/dweets/for/hydro_test                                     // URL de servidor para lecturas de sensores
// https://dweet.io/get/dweets/for/hydro_LOG                                      // URL de servidor para informaci[on sobre el estado del sistema

// Variables para tiempo de reconexion de wifi
const int timeout_period = 20;    // Cantidad maxima de intentos para establecer conexion a red WiFi
int network_timeout = 0;          // Contador de intentos para establecer conexion WiFi
int network_error_flag = 0;       // Bandera indicadora de un error en la conexion WiFi
unsigned long previous_time = 0;  // Tiempo anterior (ciclo de intento de conexion WiFi)
unsigned long interval = 1000;    // Tiempo entre intentos para establecer conexion WiFi

//------------Definicion para cliente WiFi----------

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

int ambient_temperature_flag = 0;          // 0 = valor normal, 1 = valor bajo, 2 = valor alto
int ambient_humidity_flag = 0;             // 0 = valor normal, 1 = valor bajo, 2 = valor alto
int dht_retry_cont = 0;                    //
int dht_max_retry = 2;                     //
int fan_activate = 0;                      //
int fan_state = IDLE;                      //
int control_active_flag = 0;               //
unsigned long previous_millis_dht = 0;     //
unsigned long http_prev_time = 0;          //
unsigned long current_fan_time = 0;        //
unsigned long glt_prev_time = 0;           //
unsigned long current_day_time = 0;        //
unsigned long global_timer_prev_time = 0;  //
unsigned long previous_dispense_time = 0;

float phMaxVal = 0.0;
float phMinVal = 0.0;
float ph_val = 0.0;
      
uint8_t user_bytes_received = 0;                
const uint8_t bufferlen = 32;                   
char user_data[bufferlen];
int ph_cali_flag = 1;

int btn_state;
int lbtn_state = HIGH;
unsigned long ldb_time = 0;
unsigned long btn_delay = 50;


//-------------Contadores------------
double active_time = 0;          // Tiempo que han estado activas las luces de crecimiento
int http_send_fail_counter = 0;  //
int i = 0;
int sre_cont = 0;  // Contador para eventos de lectura de sensores (Sensor Reading Event Counter)
int dse_cont = 0;  // Contador para eventos de envío de datos (Data Send Event Counter)
int cse_cont = 0;  // Contador para eventos del sistema de control (Control System Event Counter)
int wre_cont = 0;  // Contador para evento de reconexión a red WiFi (WiFi Recconect Event Counter)
int wifi_retry_cont = 0;

//-----------------------Variables generales---------------
const int ledPin = 2;  // Pin para luz led interna del ESP32
int ledState = LOW;              // Estado para la luz led del ESP32
int neo_pixel_state = 0;         // Estado para la luz de crecimiento (Neopixel)
double growLightTime = 14;       // Valor en horas (basado en pagina 14, tesis hidroponia)
int time_of_day = 0;             // 1 = dia, 0 = noche
uint8_t day_timer = 0;
uint8_t morning_time = 6;  // am - Hora local a la que amanece
uint8_t night_time = 17;   // pm - Hora local a la que anochece

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
WiFiMulti wifiMulti;
//Adafruit_ADS1115 ads;

//-----------Prototipos de funciones--------
int scanWiFi();
//void initWifi();                                                                  // Iniciar conexion WiFi
void multiWifi();
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
void parse_cmd(char* string);
void readButtonEvent(int button_pin);
//void phRegulationControl();                                                       //

/*
=====================================================================================================================================================================================
============================================================================PROGRAM START============================================================================================
=====================================================================================================================================================================================
*/

void setup() {
  // Configuracioens iniciales
  Serial.begin(57600);
  pinMode(FAN1PIN, OUTPUT);
  pinMode(FAN2PIN, OUTPUT);
  pinMode(ledPin, OUTPUT);
  //pinMode(CALI_BTN_1, INPUT_PULLUP);
  //----------------Growlight init-----------------
  NeoPixel.begin();
  NeoPixel2.begin();
  NeoPixel.clear();     // Apagar luces LED
  NeoPixel2.clear();    // Apagar luces LED
  NeoPixel.show();      // Actualizar luces LED
  NeoPixel2.show();     // Actualizar luces LED
  neo_pixel_state = 0;  // Indicar que las luces led estan apagadas
  //----------------------------------------------
  Wire.begin();
  rtc.begin();
  wifiMulti.addAP(SSID_1, password_1);
  wifiMulti.addAP(SSID_2, password_1);
  wifiMulti.addAP(SSID_3, password_2);
  multiWifi();
  dht_s1.begin();
  dht_s2.begin();
  waterTempInit();
  phUpServo.setPeriodHertz(50);// Standard 50hz servo
  phUpServo.attach(SERVO_PH_UP, 500, 2400);
  phDownServo.setPeriodHertz(50);
  phDownServo.attach(SERVO_PH_DOWN, 500, 2400);
  delay(200);
  Serial.println(F("Use commands \"CAL,7\", \"CAL,4\", and \"CAL,10\" to calibrate the circuit to those respective values"));
  Serial.println(F("Use command \"CAL,CLEAR\" to clear the calibration"));
  if (pH.begin())
  {                                     
    Serial.println("\n Loaded EEPROM");
  }
}

/*
=====================================================================================================================================================================================
=============================================================================PROGRAM LOOP============================================================================================
=====================================================================================================================================================================================
*/

void loop() {
  //-----------------Lectura de la hora del dia--------------------
  dayCounter();

  //-----------------Actualizacion de luces de crecimiento---------------
  setNeopixelState();
/*
  //--------------------Calibracion de sensor de pH-----------------------
  //readButtonEvent(CALI_BTN_1);
  while (ph_cali_flag == 1)
  {
    if(i == 0)
    {
      i = 1;
      Serial.println("\nSensor Calibration Started!");
    }
    if (Serial.available() > 0)
    {                                                      
      user_bytes_received = Serial.readBytesUntil(13, user_data, sizeof(user_data));   
    }

    if (user_bytes_received)
    {                                                      
      parse_cmd(user_data);                                                          
      user_bytes_received = 0;                                                        
      memset(user_data, 0, sizeof(user_data));                                         
    }
  }
*/
  //-------------------------Inicializacion de variables---------------------------------
  int sre_period = (control_active_flag == 0) ? 5 : 2;   // Read sensors every 2.5 minutes or every minute (if control system is active)
  int dse_period = (control_active_flag == 0) ? 10 : 6;  // Send data every 5 minutes or every 3 minutes (if control system is active)
  int cse_period = (control_active_flag == 0) ? 10 : 4;  // Update control systems every 5 minutes or every 2 minutes (if control system is active)
  int wre_period = 16;                                   // Attempt wifi recconect every 8 minutes

  struct timerVar global_timer;  // Temporizador global para eventos | utilizando periodo de 30 segundos
  global_timer.start_time = millis();
  if (global_timer.start_time - global_timer_prev_time >= GLOBAL_PERIOD)  // Enter if condition every 30 seconds
  {
    global_timer_prev_time = global_timer.start_time;

    //--------------------------Increase Event Counters----------------------------------
    sre_cont++;  // Sensor Reading Event Counter
    dse_cont++;  // Data Send Event Counter
    cse_cont++;  // Control System Event Counter
    wre_cont++;  // Wifi Reconnect Event COunter

    //---------------------------Sensor Reading Event------------------------------------
    if (sre_cont >= sre_period) {
      sre_cont = 0;
      dht_retry_cont = 0;
      getTemperatureHumidity();                       // Lectura de sensores DHT11

      sensors.requestTemperatures();                  // Solicitud de valores de temperatura
      w_temp_s1 = getWaterTemperature(tankSensor);    // Lectura de sensores DS18B20
      w_temp_s2 = getWaterTemperature(growChannel1);  //      |||       |||||
      w_temp_s3 = getWaterTemperature(growChannel2);  //      |||       |||||
      ph_val = pH.read_ph();                          // Lectura de sensor de pH
    }

    //--------------------------------Control System Event--------------------------------
    if (cse_cont >= cse_period) {
      cse_cont = 0;
      // Analizar parametros y ejecutar ciclos de control
      ambientAirParamsControl();
      //phRegulationControl();
      fanControl();
    }

    //---------------------------------Data Send Event------------------------------------
    if (dse_cont >= dse_period) {
      dse_cont = 0;
      if (httpPostRequest(0)) {
        http_send_fail_counter = 0;
      } else {
        network_error_flag = 1;
      }
      if (httpPostRequest(1)) {
        http_send_fail_counter = 0;
      } else {
        network_error_flag = 1;
      }
    }

    //---------------------------------WiFi Reconnect Event--------------------------------
    if (wre_cont >= wre_period) 
    {
      wre_cont = 0;
      if (network_error_flag == 1 && WiFi.status() != WL_CONNECTED)
      {
        wifi_retry_cont++;
        network_timeout = 0;
        multiWifi();
      } 
      else
      {
        network_error_flag = 0;
      }
    }
  }

  //-----------------------------------Check WiFi Connection-------------------------------
  if (WiFi.status() == WL_CONNECTED && digitalRead(ledPin) != HIGH) {
    ledState = HIGH;
    digitalWrite(ledPin, ledState);
    network_error_flag = 0;
  } else if (WiFi.status() != WL_CONNECTED && digitalRead(ledPin) != LOW) {
    ledState = LOW;
    digitalWrite(ledPin, ledState);
    network_error_flag = 1;
  }
}

/*
=====================================================================================================================================================================================
==========================================================================SYSTEM FUNCTIONS===========================================================================================
=====================================================================================================================================================================================
*/

void multiWifi() 
{
  // Reiniciar ESP32 si es de noche y se perdio la conexion a red WiFi
  if(time_of_day == 0 && network_error_flag == 1)
  {
    ESP.restart();
  }
  
  if (wifiMulti.run() == WL_CONNECTED) {
    network_timeout = 0;
    network_error_flag = 0;
    wifi_retry_cont = 0;
    digitalWrite(ledPin, HIGH);
  } else {
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

  while (retries < HTTP_MAX_RETRY && success != true) {
    if (WiFi.status() != WL_CONNECTED) {
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
    double year = date_time.substring(0, 4).toDouble();
    int month = date_time.substring(5, 7).toInt();
    int day = date_time.substring(8, 10).toInt();
    int hour = date_time.substring(11, 13).toInt();
    int minute = date_time.substring(14, 16).toInt();
    int second = date_time.substring(17, 19).toInt();

    if (hour < 6) {
      int time_adjust = 6 - hour;
      hour = 24 - time_adjust;
    } else {
      hour = hour - 6;
    }

    if (status_code >= 0 && status_code < 400) {
      http_send_fail_counter = 0;
      if (rtc.lostPower()) {
        rtc.adjust(DateTime(year, month, day, hour, minute, second));
      }
      success = true;
    } else {
      if (status_code < 0)
        ;  // Volver a intentar conexion
      else if (status_code >= 500)
        ;  // Volver a intentar conexion
      else if (status_code >= 400 && status_code < 500) {
        return false;  // Terminar intentos de conexion
      }

      retries++;
      if (retries < HTTP_MAX_RETRY) {
        delay(HTTP_TIMEOUT);
      }
    }
    client.stop();  // Detener rutina de envio de datos HTTP
  }
  if (success != true) {
    http_send_fail_counter++;
  }
  return success;  // Regresar estado del envio
}

void getTemperatureHumidity() 
{
  if (dht_retry_cont < dht_max_retry) {
    struct timerVar timer_dht_s1;
    timer_dht_s1.time_period = 2000;
    timer_dht_s1.start_time = millis();
    if (timer_dht_s1.start_time - previous_millis_dht >= timer_dht_s1.time_period) {
      previous_millis_dht = timer_dht_s1.start_time;
      temp_s1 = dht_s1.readTemperature();
      hum_s1 = dht_s1.readHumidity();
      temp_s2 = dht_s2.readTemperature();
      hum_s2 = dht_s2.readHumidity();
      if (isnan(hum_s1) || isnan(temp_s1) || isnan(hum_s2) || isnan(temp_s2)) {
        dht_retry_cont++;
        temp_s1 = 0.0;
        hum_s1 = 0.0;
        temp_s2 = 0.0;
        hum_s2 = 0.0;
        return;
      } else {
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
  doc["ph_val"] = String(ph_val);
  doc["Growlight_Sate"] = val8;
  doc["Fan_State"] = fan_state;
  doc["Temp_Warn"] = ambient_temperature_flag;
  doc["Hum_Warn"] = ambient_humidity_flag;

  serializeJson(doc, jsonBuffer);
  return String(jsonBuffer);
}

String performanceLog() 
{
  StaticJsonDocument<JSON_BUFFER_SIZE> doc;

  doc["Date"] = getCurrentTime(0);
  doc["Time"] = getCurrentTime(1);
  doc["SSID"] = WiFi.SSID();
  doc["RSSI"] = WiFi.RSSI();
  doc["HTTP_FAIL_CONT"] = http_send_fail_counter;
  doc["TIME_OF_DAY"] = time_of_day;
  doc["active_time"] = active_time;
  doc["day_timer"] = day_timer;
  doc["Wifi_Retry"] = wifi_retry_cont;
  //doc["Errors"] = error_log_buffer;

  serializeJson(doc, jsonBuffer);
  return String(jsonBuffer);
}

void ambientAirParamsControl()  // Checks ambient temperature and humidity values, changes state of fan_active flag if average values outside established limits
{
  ambient_T_min = 18.0;  // ⚠️HARD CODED VALUE⚠️
  ambient_T_max = 23.5;  // ⚠️HARD CODED VALUE⚠️
  ambient_H_min = 30.0;  // ⚠️HARD CODED VALUE⚠️
  ambient_H_max = 75.0;  // ⚠️HARD CODED VALUE⚠️

  setup_temp = twoValAverage(temp_s1, temp_s2);
  setup_hum = twoValAverage(hum_s1, hum_s2);
  if (setup_temp < ambient_T_min) {
    ambient_temperature_flag = 1;
  } else if (setup_hum < ambient_H_min) {
    ambient_humidity_flag = 1;
  } else if (setup_temp >= ambient_T_max) {
    ambient_temperature_flag = 2;
    fan_activate = 1;
  } else if (setup_hum >= ambient_H_max) {
    ambient_humidity_flag = 2;
    fan_activate = 1;
  } else  // Temperatura y humedad en rangos aceptables
  {
    ambient_temperature_flag = 0;
    ambient_humidity_flag = 0;
    fan_activate = 0;
  }
}

float twoValAverage(float dat1, float dat2) 
{
  float average = 0.0;
  average = (dat1 + dat2) / 2;
  return average;
}

void fanControl() 
{
  if (fan_activate == 1 && fan_state == IDLE)  // La variable fan_state permite evitar que se active el pin constantemente si ya se encendieron los ventiladores
  {
    fan_state = ACTIVE;
    control_active_flag = 1;
    digitalWrite(FAN1PIN, HIGH);
    digitalWrite(FAN2PIN, HIGH);
  }
  //--------------------------Desactivar los ventiladores-------------------------------------
  else if (fan_activate == 0 && fan_state == ACTIVE) {
    fan_state = IDLE;
    control_active_flag = 0;
    digitalWrite(FAN1PIN, LOW);
    digitalWrite(FAN2PIN, LOW);
  }
}

void waterTempInit() 
{
  sensors.begin();
  if (!sensors.getAddress(tankSensor, 0))
    ;
  if (!sensors.getAddress(growChannel1, 1))
    ;
  if (!sensors.getAddress(growChannel2, 2))
    ;

  sensors.setResolution(tankSensor, DS18_PRECISION);
  sensors.setResolution(growChannel1, DS18_PRECISION);
  sensors.setResolution(growChannel2, DS18_PRECISION);
}

float getWaterTemperature(DeviceAddress deviceAddress) 
{
  float tempC = sensors.getTempC(deviceAddress);
  if (tempC == DEVICE_DISCONNECTED_C) {
    tempC = 255;
    return tempC;
  } else {
    return tempC;
  }
}

void setNeopixelState() 
{
  if (time_of_day == 1) {
    if (active_time <= (growLightTime * 60)) {
      struct timerVar lightingTime;
      lightingTime.start_time = millis();
      lightingTime.time_period = 60000;
      if (lightingTime.start_time - glt_prev_time >= lightingTime.time_period) {
        active_time++;
        glt_prev_time = lightingTime.start_time;
      }

      if (neo_pixel_state == 0) {
        neo_pixel_state = 1;
        NeoPixel.clear();                                                            //
        NeoPixel2.clear();                                                           // (repisa superior)
        for (int pixel = 0; pixel < NUM_PIXELS; pixel++) {                           // for each pixel
          NeoPixel.setPixelColor(3 * pixel, NeoPixel.Color(255, 0, 0));              // Rojo
          NeoPixel2.setPixelColor(3 * pixel, NeoPixel2.Color(255, 0, 0));            // Rojo (repisa superior)
          NeoPixel.setPixelColor((3 * pixel) - 1, NeoPixel.Color(0, 0, 255));        // Azul
          NeoPixel2.setPixelColor((3 * pixel) - 1, NeoPixel2.Color(0, 0, 255));      // Azul (repisa superior)
          NeoPixel.setPixelColor((3 * pixel) - 2, NeoPixel.Color(255, 110, 255));      // Amarilo = 250, 255, 0 | Fusia = 255, 110, 255
          NeoPixel2.setPixelColor((3 * pixel) - 2, NeoPixel2.Color(255, 110, 255));  // Amarilo | Fusia = 255, 110, 255
        }
        NeoPixel.show();   // update to the NeoPixel Led Strip
        NeoPixel2.show();  // update to the NeoPixel Led Strip (repisa superior)
      }
    } else if (active_time > (growLightTime * 60) && neo_pixel_state == 1) {
      NeoPixel.clear();   // Apagar luces LED
      NeoPixel2.clear();  // Apagar luces LED (repisa superior)
      NeoPixel.show();    // Actualizar luces LED
      NeoPixel2.show();   // Actualizar luces LED (repisa superior)
      neo_pixel_state = 0;
    }
  } else if (time_of_day == 0) {
    NeoPixel.clear();
    NeoPixel2.clear();
    NeoPixel.show();
    NeoPixel2.show();
    neo_pixel_state = 0;
  }
}

void dayCounter() 
{
  DateTime now_c = rtc.now();  // Added "_c" as "counter" identifier to diferentiate from variable used in getCurrentTime function
  day_timer = now_c.hour();
  if (day_timer >= morning_time && day_timer < night_time) {
    time_of_day = 1;
  } else if (day_timer >= night_time || day_timer < morning_time) {
    time_of_day = 0;
    active_time = 0;
  }
}

void readButtonEvent(int button_pin)
{
  int btn_reading = digitalRead(button_pin);

  if (btn_reading != lbtn_state)
  {
    ldb_time = millis();
  }

  if ((millis() - ldb_time) > btn_delay)
  {
    if (btn_reading != btn_state)
    {
      btn_state = btn_reading;
      if (btn_state == HIGH)
      {
        ph_cali_flag = 1;
      }
    }
  }
  lbtn_state = btn_reading;
}

// Funcion para calibración de sensor de pH desarrollada por Atlas Scientific
void parse_cmd(char* string) 
{                   
  strupr(string);                                
  if (strcmp(string, "CAL,7") == 0) {       
    pH.cal_mid();                                
    Serial.println("MID CALIBRATED");
  }
  else if (strcmp(string, "CAL,4") == 0) {            
    pH.cal_low();                                
    Serial.println("LOW CALIBRATED");
  }
  else if (strcmp(string, "CAL,10") == 0) {      
    pH.cal_high();                               
    Serial.println("HIGH CALIBRATED");
    ph_cali_flag = 0;
  }
  else if (strcmp(string, "CAL,CLEAR") == 0) { 
    pH.cal_clear();                              
    Serial.println("CALIBRATION CLEARED");
    ph_cali_flag = 0;
  }
}


void phRegulationControl()
{
  phMaxVal = 6.5; // ⚠️HARD CODED VALUE⚠️                                               // Valor maximo de pH admisible para el crecimiento del cilantro (pag. 15, tesis hidroponia)
  phMinVal = 5.5; // ⚠️HARD CODED VALUE⚠️                                               // Valor minimo de pH admisible para el crecimiento del cilantro (pag. 15, tesis hidroponia)
  struct timerVar dispense_timer;
  dispense_timer.time_period = 500;
  if (ph_val > phMaxVal)
  {
    control_active_flag = 1;
    phDownServo.write(OPEN);
    dispense_timer.start_time = millis();
    if(dispense_timer.start_time - previous_dispense_time == dispense_timer.time_period)
    {
      previous_dispense_time = dispense_timer.start_time;
      phDownServo.write(CLOSE);
    }
  }
  else if (ph_val < phMinVal)
  {
    control_active_flag = 1;
    phUpServo.write(OPEN);
    dispense_timer.start_time = millis();
    if(dispense_timer.start_time - previous_dispense_time == dispense_timer.time_period)
    {
      previous_dispense_time = dispense_timer.start_time;
      phUpServo.write(CLOSE);
    }
  }
  else
  {
    if(fan_state != ACTIVE)
    {
      control_active_flag = 0;
    }
    phUpServo.write(CLOSE);
    phDownServo.write(CLOSE);
  }
}
