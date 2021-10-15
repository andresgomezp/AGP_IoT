//--------------------------------------------------------------------------------------------------
// Autor: Andres Gomez
// fecha: 15-Jul-2021
// TFM:   IoT para monitoreo de humedad y agua en infraestructuras residenciales
// Microcontrolador: ESP32-wroom-32d

// Version: 1.0
//    1. Se conecta a WiFi
//    2. Se conecta a MQTT, sino, no continua
//    3. Toma medidas de: Humedad, Temperatura, Lluvia
// 31-Jul-2021
//    4. crear nuevo topic keepalive
//    5. generar proceos para enviar keepa (keepalive) cada 2min
//       {ss:1,ka:1} indica que todo esta ok, ka:0 problemas que se deben validar
//    6. Se crea loop para tomar 3 muestras y obtener promedio. 
//    7. Ahorro de energia
//       - Bajo CPU de 240MHz a 80MHz(no se puede mas necesito WiFi). Nooo!!! 
//         las lecturas del sensor DTH son erroneas, hago rollback
//

//--------------------------------------------------------------------------------------------------
//--- Lib/Def x DHT ---
#include "DHT.h"  //DHT22
#define DHTTYPE DHT22 //DHT22  (AM2302), AM2321
const int DHTPin = 14;
DHT dht(DHTPin, DHTTYPE);
float h, temp_h;
float t, temp_t;

//--------------------------------------------------------------------------------------------------
//--- Lib/Def x WiFi  ---
#include <WiFi.h>
const char* ssid     = "123"; 
const char* password = "321";    
WiFiClient espClient;
long lastMsg = 0;
char msg[50];

//--------------------------------------------------------------------------------------------------
//--- Lib/Def x MQTT  ---
#include <PubSubClient.h>
PubSubClient client(espClient);
const char* mqtt_server = "192.168.0.1";
const char* Topico = "sensores";
#define TiempoMuestra 240000  //Tiempo de espera para tomar cada muestra de datos, 240000 = 4min
#define BUFFER_SIZE  55 //Payload size

//Tamaño calculado para 1 ID + 3 Variables {"ss":1002010501,"Hu":53.20,"Te":20.60,"Yu":20.0} =-->49
char CadenaEnviar[BUFFER_SIZE];  
unsigned long now;

//--------------------------------------------------------------------------------------------------
//--- Def x lluvia  ---
int SensorLluviaPin = A0;
int SensorLluviaLectura, Temp_SensorLluviaLectura; 

//--------------------------------------------------------------------------------------------------
//--- Def var de control ---
#include <Wire.h>
char SensorID[BUFFER_SIZE];
char DatoHumedad[BUFFER_SIZE];
char DatoTemperatura[BUFFER_SIZE];
char DatoLluvia[BUFFER_SIZE];
int TotalMuestras = 3, ControlMuestras;

int TimeKeepAliveSend = 120000;  //tiempo en mili segundos, cada 2min
unsigned long TomaTiempo, DiffTiempo;
const char* TopicoKeepAlive = "keepalive";
long TotalKA;

// ******************************************************
// *** Defino KeepAlive para monitoreo de SmartDevice ***
// ******************************************************
char KeepAliveMsg[BUFFER_SIZE] = "{\"ss\":1,\"ka\":1}";
// ******************************************************

//--------------------------------------------------------
void setup(){
  Serial.begin(9600);

  Serial.println("Antes de conectar a wifi");
  delay(500);
  //Inicializa WiFi
  setup_wifi();

  //Inicializa coneccion a MQTT
  client.setServer(mqtt_server, 1883);
  //Inicializa Actuadores
  //client.setCallback(callback);

  //Inicializa pin del sensor de lluvia
  pinMode(SensorLluviaPin, INPUT);

  //Inicializa sensor de Humedad-Temperatura
  dht.begin();
  
  // ***********************************
  // *** Defino el ID del del sensor ***
  // ***********************************
  strncat(SensorID, "\"ss\":1002010501", strlen("\"ss\":1002010501") );
  // *********************************************************************

  //Al iniciar el SmartDevice se realice una primera toma de datos sin tener que esperar
  lastMsg = TiempoMuestra +1;
  DiffTiempo = TimeKeepAliveSend +1;
  TotalKA = 0;
}

//--------------------------------------------------------------------------------------------------
//---  Funcion para establecer conexion WiFi  ---
void setup_wifi(){
  delay(10);
  Serial.println("Conectando a ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("Conectado a WIFI");
  Serial.print("IP: "); Serial.println(WiFi.localIP());
  delay(1000);
}

//--------------------------------------------------------------------------------------------------
//---  Coneccion a MQTT  ---
void reconnect(){
  while (!client.connected()){
    Serial.print("Intentando conexión MQTT_AGP ...");
    if (client.connect("ESP32")){
      Serial.println("conectado");
      //client.subscribe("esp32/out-buzzer");
    } 
    else{
      Serial.print("Error1: failed!!!, rc=");
      Serial.print(client.state());
      Serial.println(" Re-intento en 5 segundos");
      delay(5000);
    }
  }
}

//--------------------------------------------------------------------------------------------------
//--- Funcion para concatenar Var+Valor ---
void  DoubleToString( char *str, double double_num,unsigned int len) { 
  double fractpart, intpart;
  fractpart = modf(double_num, &intpart);
  fractpart = fractpart * (pow(10,len));
  sprintf(str + strlen(str),"%d", (int)(intpart)); //Integer part
  sprintf(str + strlen(str), ".%d", (int)(fractpart)); //Decimal part
}

//--------------------------------------------------------
void loop(){
  //Hasta que no se conecte a MQTT no continua
  if (!client.connected()){
    reconnect();
  }
  client.loop();

  //Se valida si tiempo transcurrido de ultima entrega tiene mas de TiempoMuestra para enviar otra
  now = millis();
  if (now - lastMsg > TiempoMuestra){
    lastMsg = now;
    
    SensorLluviaLectura = 0;
    temp_h = 0;
    temp_t = 0;
    h = 0;
    t = 0;
    for(ControlMuestras=0; ControlMuestras<TotalMuestras; ){
      Temp_SensorLluviaLectura = analogRead(SensorLluviaPin);
      temp_h = dht.readHumidity();
      temp_t = dht.readTemperature(); // Temperatura en Celsius
	  
      //valido datos atipicos, Hu=2147483647.0, Te=2147483647.0
      if(isnan(h) || isnan(t) || temp_h < 100 || temp_t < 101){
        SensorLluviaLectura += map(Temp_SensorLluviaLectura, 4095,0, 0,100);
        h += temp_h;
        t += temp_t;
        ControlMuestras++;
        //Serial.println("1. SsnsorYu: "+String(SensorLluviaLectura) +", h:"+String(h) +", t:"+String(t));
      }
      delay(2500); //Datasheet indica que no debe ser menor a 2 seg
    }
	
    //Promedio de las muestras, controlada por TotalMuestras
    h /= ControlMuestras;
    t /= ControlMuestras;
    SensorLluviaLectura /= ControlMuestras;
    
    Serial.print("2. Yu (Map): " +String(SensorLluviaLectura) );
    Serial.print(" - Hu(" +String(h) +" %)");
    Serial.println(" - Te(" +String(t) +" *C)");

    //Inicia creacion de Json para enviar a broker
    strcpy(CadenaEnviar, "");
    sprintf(DatoHumedad, "%s","\"Hu\":");    DoubleToString(DatoHumedad,h,2);
    sprintf(DatoTemperatura,"%s","\"Te\":"); DoubleToString(DatoTemperatura,t,2);
    sprintf(DatoLluvia,"%s","\"Yu\":"); DoubleToString(DatoLluvia,SensorLluviaLectura,0);
    strncat(CadenaEnviar, "{", strlen("{") );
    strncat(CadenaEnviar, SensorID, strlen(SensorID) );
    strncat(CadenaEnviar, ",", 1 );
    strncat(CadenaEnviar, DatoHumedad, strlen(DatoHumedad) );
    strncat(CadenaEnviar, ",", 1 );
    strncat(CadenaEnviar, DatoTemperatura, strlen(DatoTemperatura) );
    strncat(CadenaEnviar, ",", 1 );
    strncat(CadenaEnviar, DatoLluvia, strlen(DatoLluvia) );
    strncat(CadenaEnviar, "}", 1 );

    //publico en el topico configurado
    client.publish(Topico, CadenaEnviar );
    
    Serial.printf("\nCadena %s , length %d \n",CadenaEnviar, strlen(CadenaEnviar));
    Serial.println("");
  } 
  
  DiffTiempo = millis()-TomaTiempo;
  if(DiffTiempo > TimeKeepAliveSend){
    Serial.print("Paso T x enviar KA:" ); Serial.println(KeepAliveMsg);
    TomaTiempo = millis();
    TotalKA++;

    //Se publica el mensage en el topic
    client.publish(TopicoKeepAlive, KeepAliveMsg );
  }  
}
