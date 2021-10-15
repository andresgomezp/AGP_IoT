//--------------------------------------------------------------------------------------------------
//Autor: Andres Gomez
//fecha: 08-Jul-2021
// TFM:   IoT para monitoreo de humedad y agua en infraestructuras residenciales
// Microcontrolador: Heltec CubeCell_ASR6501

// Version: 1.0
//    1. Lee datos del sensor DHT22 
//    2. Envia datos (SensorID, Humedad, Temperatura) en formato Json, frecuencia 915
//    3. Destello led Verde x indicar envio de datos
//    4. Espera = 240000 (4min), para obtener aprox 15 medidas x hora
//    5. Se define variable para ID del SmartDevice

//--------------------------------------------------------------------------------------------------
//--- Lib/Def x DHT ---
#include "DHT.h"
#define DHTPIN GPIO2  // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
float h;
float t;

//--------------------------------------------------------------------------------------------------
//--- Lib/Def x LoRa ---
#include "LoRaWan_APP.h"
#define RF_FREQUENCY                 915000000 // Hz
#define TX_OUTPUT_POWER              14        // dBm
#define LORA_BANDWIDTH               0         // [0: 125 kHz,  1: 250 kHz,  2: 500 kHz,  3: Reserved]
#define LORA_SPREADING_FACTOR        7         // [SF7..SF12]
#define LORA_CODINGRATE              1         // [1: 4/5,  2: 4/6,  3: 4/7,  4: 4/8]
#define LORA_PREAMBLE_LENGTH         8         // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT          0         // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON   false
#define LORA_IQ_INVERSION_ON         false
#define RX_TIMEOUT_VALUE             1000
#define BUFFER_SIZE                  30        // Payload size

//--------------------------------------------------------------------------------------------------
//--- Def var de control ---
char CadenaEnviar[42];   //Tamaño calculado para 1 ID + 2 Variables, {"ss":1000010501,"Hu":53.20,"Te":19.50}
char SensorID[BUFFER_SIZE];
char DatoHumedad[BUFFER_SIZE];
char DatoTemperatura[BUFFER_SIZE];
static RadioEvents_t RadioEvents;
#define Espera  240000   //Tiempo de espera para tomar cada muestra de datos, 240000 = 4min

//--------------------------------------------------------------------------------------------------
//--- Funcion para concatenar Var+Valor ---
void  DoubleToString( char *str, double double_num,unsigned int len) { 
  double fractpart, intpart;
  fractpart = modf(double_num, &intpart);
  fractpart = fractpart * (pow(10,len));
  sprintf(str + strlen(str),"%d", (int)(intpart)); //Integer part
  sprintf(str + strlen(str), ".%d", (int)(fractpart)); //Decimal part
}

//--------------------------------------------------------------------------------------------------
void setup() {
  Serial.begin(9600);
  Serial.println("Iniciando Setup");
  dht.begin();

  Radio.Init( &RadioEvents );
  Radio.SetChannel( RF_FREQUENCY );
  Radio.SetTxConfig( MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                     LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                     LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                     true, 0, 0, LORA_IQ_INVERSION_ON, 3000 ); 

  // ***********************************
  // *** Defino el ID del del sensor ***
  // ***********************************
  strncat(SensorID, "\"ss\":1000010501", strlen("\"ss\":1000010501") );
  // *********************************************************************
}

//--------------------------------------------------------------------------------------------------
void loop() {
  h = dht.readHumidity();
  t = dht.readTemperature(); // Temperatura en Celsius

  // Valida muestra de datos
  if (isnan(h) || isnan(t)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  Serial.print("Humedad: "); Serial.print(h); Serial.print("% - ");
  Serial.print(F("Temperatura: ")); Serial.print(t); Serial.print(F("°C"));

  sprintf(CadenaEnviar, "");
  sprintf(DatoHumedad, "%s","\"Hu\":"); DoubleToString(DatoHumedad,h,2);
  sprintf(DatoTemperatura,"%s","\"Te\":"); DoubleToString(DatoTemperatura,t,2);

  strncat(CadenaEnviar, "{", strlen("{") );
  strncat(CadenaEnviar, SensorID, strlen(SensorID) );
  strncat(CadenaEnviar, ",", 1 );
  strncat(CadenaEnviar, DatoHumedad, strlen(DatoHumedad) );
  strncat(CadenaEnviar, ",", 1 );
  strncat(CadenaEnviar, DatoTemperatura, strlen(DatoTemperatura) );
  strncat(CadenaEnviar, "}", 1 );
  
  Serial.printf("\r\nHumedad %s , length %d\r\n",DatoHumedad, strlen(DatoHumedad));
  Serial.printf("\rTemperatura %s , length %d\r\n",DatoTemperatura, strlen(DatoTemperatura));
  Serial.printf("\rCadena %s , length %d\r\n",CadenaEnviar, strlen(CadenaEnviar));
  Serial.printf("--------------------------------------------------------------\n");
  Radio.Send( (uint8_t *)CadenaEnviar, strlen(CadenaEnviar) ); //Se envia paquete 

  turnOnRGB(COLOR_RECEIVED,200); //Destello de rgb en color verde indicando el envio de datos   
  turnOffRGB();
 
  delay(Espera);
}
