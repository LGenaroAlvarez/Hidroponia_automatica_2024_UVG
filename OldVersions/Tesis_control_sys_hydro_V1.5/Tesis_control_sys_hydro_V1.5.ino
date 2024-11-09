/*  Versión 1.5
    Autor: Ing. Luis Genaro Alvarez Sulecio
    Tesis de sistema hidroponico automatico
    Comentarios: 
    - Lectura de módulo RTC DS3231
*/
//---------------------Librerias utilizadas---------------------------
#include <WiFi.h>                 // Libreria para conexion wifi
#include <ArduinoHttpClient.h>    // Librería de arduino para conexiones HTTP
#include <String.h>               // Formato de informacion en tipo String
#include <ArduinoJson.h>          // Formato de informacion en tipo JSON para envio por HTTP
#include <DHT.h>
//#include <time.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_NeoPixel.h>
#include <DS3231.h>
#include <Wire.h>

//--------------Pin Definitions---------
#define DHTPIN 4          // 10k Ohm resistor
#define DHTPIN2 0         //    ||    ||
#define ONE_WIRE_BUS 15   // 4.7k Ohm resistor
#define DHTTYPE DHT11
#define FAN1PIN 12        // 100 Ohm resistor
#define FAN2PIN 14        //    ||    ||
#define NEOPIXELPIN 27

//--------------Constant definitions-------------
#define ACTIVE 1
#define IDLE 0
#define DS18_PRECISION 9
#define NUM_PIXELS 30//+32
#define HTTP_TIMEOUT 1500

//------------Structs--------------
struct timerVar
{
	unsigned long start_time;
	unsigned long time_period;
  unsigned long prev_time; // Pending
};

//--------Conexion a internet (wifi)----------

//const char* SSID = "Blue Dragon";
const char* SSID = "Tesis20168";
const char* password = "12345671";

// Detalles del servidor para http
const char* host = "dweet.io"; // Servidor
const int port = 80; // Puerto de conexión para servidor HTTP
// https://dweet.io/get/dweets/for/hydro_test // URL de servidor para lecturas de sensores
// https://dweet.io/get/dweets/for/hydro_LOG  // URL de servidor para informaci[on sobre el estado del sistema

// Variables para tiempo de reconexion de wifi
const int timeout_period = 20;
int network_timeout = 0;
unsigned long network_retry = 300000;
unsigned long wifi_retry_prev_time = 0;
int network_error_flag = 0;
unsigned long previous_time = 0;
unsigned long interval = 1000;

//------------Definicion para cliente WiFi----------

WiFiClient espClient; // Creacion de cliente para conexion WiFi del ESP32
HttpClient client = HttpClient(espClient, host, port); // Cliente para conexión HTTP3

//----------Variables de sensores-----------
float temp_s1 = 0.0;
float hum_s1 = 0.0;
float temp_s2 = 0.0;
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
int max_retry = 2;
int fan_activate = 0;
int fan_state = IDLE;
unsigned long previous_millis_dht = 0;
unsigned long http_prev_time = 0;
unsigned long current_fan_time = 0;
unsigned long srt_prev_time = 0;
unsigned long nst_prev_time = 0;
unsigned long glt_prev_time = 0;
unsigned long current_day_time = 0;

//-------------Contadores------------
int i = 0;
int time_timeout = 0;
unsigned long active_time = 0;  // Tiempo que han estado activas las luces de crecimiento
int day_timer = 0;
int http_send_fail_counter = 0;

//-----------------------Variables generales---------------
bool first_reading = true;
const int ledPin = LED_BUILTIN;  // the number of the LED pin
int ledState = LOW;  // ledState used to set the LED
int neo_pixel_state = 0;
float growLightTime = 8; // horas
int time_of_day = 1;

int morning_time = 6;                                                   // am - Hora local a la que amanece
int night_time = 17;                                                    // pm - Hora local a la que anochece
float day_length = 23.5;                                                // Cantidad de horas en un dia

bool century = false;
bool h12Flag;
bool pmFlag;

//--------------Inicializacion de modulos del sistema----------------
DHT dht_s1(DHTPIN, DHTTYPE);                                            // Activar sensor DHT11 numero 1
DHT dht_s2(DHTPIN2, DHTTYPE);                                           // Activar sensor DHT11 numero 1
OneWire oneWire(ONE_WIRE_BUS);                                          // Bus de comunicacion para sensores de protocolo onewire
DallasTemperature sensors(&oneWire);                                    // Inicializacion de sensores de temperatura de agua
DeviceAddress tankSensor, growChannel1, growChannel2;                   // Definicion de sensores de temperatura de agua
Adafruit_NeoPixel NeoPixel(NUM_PIXELS, NEOPIXELPIN, NEO_GRB + NEO_KHZ800);  // Definicion de luces led (NEOPIXELS)
DS3231 RTC_CLOCK;


//-----------Prototipos de funciones--------
//void initTimers();                                                    // Activar lectura de tiempo real
void initWifi();                                                        // Iniciar conexion WiFi
void httpPostRequest(int sel);                                          // Envio de datos a servidor WiFi
void getTemperatureHumidity();                                          // Lectura de sensores de temperatura y humedad
String packageData(float val1, float val2, float val3, float val4);     // Empaquetado de datos en formato JSON
String performanceLog();                                                // Empaquetado de datos para log de rendimiento
//void getCurrentTime();                                                  // Lectura de tiempo real (mediante modulo RTC DS3231)
float twoValAverage(float dat1, float dat2);                            // Calculo de valor promedio para dos valores
void fanControl();                                                      // Accionamiento de ventiladores
void ambientAirParamsHandler();                                         // Control de parametros ambientales
void waterTempInit();                                                   // Iniciar sensores de temperatura de agua
float getWaterTemperature(DeviceAddress deviceAddress);                 // Lectura de sensores de temperatura de agua
void setNeopixelState();                                                // Cambiar el estado de los leds segun tiempo recorrido
void dayCounter(); // REVISE                                            // Mantener registro de las horas transcurridas desde inicializacion


void setup() 
{
  // Configuracioens iniciales
  pinMode(FAN1PIN, OUTPUT);
  pinMode(FAN2PIN, OUTPUT);
  pinMode(ledPin, OUTPUT);
  NeoPixel.begin();
  NeoPixel.clear();   // Apagar luces LED
  NeoPixel.show();    // Actualizar luces LED
  neo_pixel_state = 0;// Indicar que las luces led estan apagadas
  Wire.begin();
  initWifi();
  dht_s1.begin();
  dht_s2.begin();
  waterTempInit();
  sensors.begin();
  //qty_sensors = sensors.getDeviceCount();
}

void loop()
{
  struct timerVar srt;                              // srt = sensor read timer
  struct timerVar nst;                              // nst = network send timer
  
  if(first_reading == true)
  {
    //------------------Configurar periodo de muestreo----------------------
    srt.time_period = 6000;                         // Lectura de sensores cada 6 segundos
    nst.time_period = 60000;                        // Envio de datos a la nube cada 60 segundos
  }

  srt.start_time = millis();
  nst.start_time = millis();
  if (srt.start_time - srt_prev_time >= srt.time_period)
  {
    srt_prev_time = srt.start_time;
    dht_retry_cont = 0;
    getTemperatureHumidity();     // Lectura de sensores DHT11
    
    sensors.requestTemperatures();                  // Solicitud de valores de temperatura
    w_temp_s1 = getWaterTemperature(tankSensor);    // Lectura de sensores DS18B20
    w_temp_s2 = getWaterTemperature(growChannel1);  //      |||       |||||
    w_temp_s3 = getWaterTemperature(growChannel2);  //      |||       |||||

    if (first_reading == true)
    {
      first_reading = false;
    }
  }

  //-----------------------Envio de datos a servidor Dweet.io---------------------------
  if (nst.start_time - nst_prev_time >= nst.time_period)
  {
    nst_prev_time = nst.start_time;
    httpPostRequest(0);            // Envio de datos a servidor de Dweet.io
    httpPostRequest(1);            // Envio de datos a servidor de Dweet.io
    /*if (network_error_flag == 0)
    {
      httpPostRequest(0);            // Envio de datos a servidor de Dweet.io
      //delay(1000);                   // Delay para separar envios de datos
      httpPostRequest(1);            // Envio de datos a servidor de Dweet.io
    }*/
  }

  //-----------------Sistemas de control-------------------
  if (first_reading == false)
  {
    // Si ya se cuenta con datos, analizar parametros y ejecutar ciclos de control
    ambientAirParamsHandler();
    fanControl();
  }

  //-----------------Reconeccion a red WIFI--------------------
  struct timerVar wifi_retry;
  wifi_retry.start_time = millis();
  if (wifi_retry.start_time - wifi_retry_prev_time >= network_retry)
  {
    wifi_retry_prev_time = wifi_retry.start_time;
    if (WiFi.status() != WL_CONNECTED)
    {
      network_timeout = 0;
      initWifi();
    }
  }
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
  //-----------------Actualizacion de hora del dia--------------------
  dayCounter();
  //-----------------Actualizacion de luces de crecimiento---------------
  setNeopixelState();
}

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


/*
  Función desarrollada según el ejemplo "DweetPost" del repositorio de github de la librería "ArduinoHttpClient.h"
*/
/*
void httpPostRequest(int sel)
{
  
  struct timerVar timer_http;
  timer_http.time_period = 10000;
  timer_http.start_time = millis();
  if (timer_http.start_time - http_prev_time >= timer_http.time_period)
  {
    String thing_address = "hydro_test";
    String thing_address2 = "hydro_LOG";
    String path = "/dweet/for/" + thing_address;
    String path2 = "/dweet/for/" + thing_address2;
    String content_type = "application/json";
    String post_data = packageData(temp_s1, hum_s1, temp_s2, hum_s2);
    String log_data = performanceLog();

    switch(sel){
      case 0:
        client.post(path, content_type, post_data);
        break;
      case 1:
        client.post(path2, content_type, log_data);
        break;
      default:
        client.post(path, content_type, post_data);
        break;
    }
    
    int status_code = client.responseStatusCode();
    String response = client.responseBody();
    if (status_code >= 0)
    {
      http_send_fail_counter = 0;
    }
    else
    {
      http_send_fail_counter++;
      if (WiFi.status() != WL_CONNECTED)
      {
        network_error_flag = 1;
      }
    }
  }
}*/

void getTemperatureHumidity()
{
  if (dht_retry_cont < max_retry)
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
        temp_s1 = 0.0;
        hum_s1 = 0.0;
        temp_s2 = 0.0;
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

String packageData(float val1, float val2, float val3, float val4, float val5, float val6, float val7)
{

}

/*
String packageData(float val1, float val2, float val3, float val4)
{
  String start_marker = "{";
  String end_marker = "\"}";
  String separator = "\",";
  String percent = "%";

  String data_package = start_marker;
  data_package += "\"t_s1\":\"";
  data_package += val1;
  data_package += separator;
  data_package += "\"h_s1\":\"";
  data_package += val2;
  data_package += percent;
  data_package += separator;
  data_package += "\"t_s2\":\"";
  data_package += val3;
  data_package += separator;
  data_package += "\"h_s2\":\"";
  data_package += val4;
  data_package += percent;
  data_package += separator;
  data_package += "\"wt_s1\":\"";
  data_package += w_temp_s1;
  data_package += separator;
  data_package += "\"wt_s2\":\"";
  data_package += w_temp_s2;
  data_package += separator;
  data_package += "\"wt_s3\":\"";
  data_package += w_temp_s3;
  data_package += separator;
  data_package += "\"GrowLight\":\"";
  data_package += neo_pixel_state;
  data_package += end_marker;
  return data_package;
}

String performanceLog()
{
  double network_RSSI = WiFi.RSSI();

  String start_marker = "{";
  String end_marker = "\"}";
  String separator = "\",";
  String data_log = start_marker;
  data_log += "\"SSID\":\"";
  data_log += SSID;
  data_log += separator;
  data_log += "\"RSSI\":\"";
  data_log += network_RSSI;
  data_log += separator;
  data_log += "\"HTTP_FAIL_CONT\":\"";
  data_log += http_send_fail_counter;
  data_log += end_marker;
  return data_log;
}
/*
void getCurrentTime()
{
  return;
}*/

void ambientAirParamsHandler()
{
  ambient_T_min = 18.0;
  ambient_T_max = 30.0;
  ambient_H_min = 30.0;
  ambient_H_max = 75.0;

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
  if (fan_activate == 1 && fan_state == IDLE)         // La variable fan_state permite evutar que se active el pin constantemente si ya se encendieron los ventiladores
  {
    fan_state = ACTIVE;
    digitalWrite(FAN1PIN, HIGH);
    digitalWrite(FAN2PIN, HIGH);
    struct timerVar timer_fan;
    timer_fan.time_period = 30000;
    timer_fan.start_time = millis();
    //------------------------Activar los ventiladores por n segundos-----------------------------
    if (timer_fan.start_time - current_fan_time > timer_fan.time_period)
    {
      current_fan_time = timer_fan.start_time;
      fan_state = IDLE;
      digitalWrite(FAN1PIN, LOW);
      digitalWrite(FAN2PIN, LOW);
    }
  }
  //--------------------------Desactivar los ventiladores-------------------------------------
  else
  {
    digitalWrite(FAN1PIN, LOW);
    digitalWrite(FAN2PIN, LOW);
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
  
  if (active_time >= growLightTime && neo_pixel_state == 1)
  {
    NeoPixel.clear();   // Apagar luces LED
    NeoPixel.show();    // Actualizar luces LED
    neo_pixel_state = 0;
  }
}

void dayCounter()
{
  struct timerVar dayCycle;
  dayCycle.start_time = millis();
  dayCycle.time_period = 60000;
  if (dayCycle.start_time - current_day_time >= dayCycle.time_period)
  {
    day_timer++;
    current_day_time = dayCycle.start_time;
    if (day_timer >= (morning_time*60))
    {
      time_of_day = 1;
    }
    else if (day_timer >= (night_time*60))
    {
      time_of_day = 0;
      active_time = 0;
    }
    else if (day_timer >= (day_length*60))
    {
      day_timer = 0;
      //current_day_time = 0;
    }
  }
}
