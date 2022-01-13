#include "ThingsBoard.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>   // Universal Telegram Bot Library written by Brian Lough: https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot
#include <ArduinoJson.h>

// Librerias para el sensor de peso
#include <SPI.h>
#include <Wire.h>

// Librerias para el sensor de temperatura y humedad
#include "DHT.h"

// **************************************************

// LEDS 0 2 15
#define led_rojo 12
#define led_verde 14
#define led_azul 27

// Sensor de lluvia
#define rainAnalog 25
#define rainDigital 26

// Sensor de peso 34 35
int celula1, celula2, media_celula;
#define weightAnalog1 34
#define weightAnalog2 35

// Sensor de temperatura y humedad
#define DHTPIN 4     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321

// Sensor de distancia
const int trigPin = 5;
const int echoPin = 18;
#define SOUND_SPEED 0.034
long duration;
long distanceCm;

// Thingsboard token and server
#define TOKEN               "OuYRHyb4l6x0LIWaQDqo"
#define THINGSBOARD_SERVER  "demo.thingsboard.io"

// WiFi Replace with your network credentials
const char* ssid = "SBC";
const char* password = "sbc$2020";

// Initialize Telegram BOT
#define BOTtoken "2070554645:AAFRG44Sy0LYwEdH9abgvKnJxO42R_9-aBM"  // your Bot Token (Get from Botfather)

// Use @myidbot to find out the chat ID of an individual or a group
// Also note that you need to click "start" on a bot before it can
// message you
#define CHAT_ID "1963881901"

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

// Initialize ThingsBoard client
WiFiClient espClient;
// Initialize ThingsBoard instance
ThingsBoard tb(espClient);

// Checks for new messages every 0.5 second.
int botRequestDelay = 1000;
unsigned long lastTimeBotRan;

// Updates Thingsboard data every 1 second
int thingsBoardDelay = 1000;
unsigned long lastTimeThingsBoard;

int ledDelay = 1000;
unsigned long ledTimer = 0;

int papeleraLlenaDelay = 10000;
unsigned long lastTimePapeleraLlena = 0;

bool ledsON = true;

DHT dht(DHTPIN, DHTTYPE);

bool avisarLluvia = true;
bool avisarSeco = false;

// Handle what happens when you receive new messages
void handleNewMessages(int numNewMessages) {
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));

  for (int i=0; i<numNewMessages; i++) {
    // Chat id of the requester
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID){
      bot.sendMessage(chat_id, "Usuario no autorizado", "");
      continue;
    }
    
    // Print the received message
    String text = bot.messages[i].text;
    Serial.println(text);

    String from_name = bot.messages[i].from_name;

    if (text == "/start") {
      String welcome = "Bienvenido, " + from_name + ".\n";
      welcome += "Utiliza los siguientes comandos para conocer el estado de los sensores.\n\n";
      welcome += "/esta_mojado para saber si el suelo de la papelera esta mojado \n";
      welcome += "/th para saber la temperatura y humedad \n";
      welcome += "/distancia para saber el porcentaje de espacio ocupado en la papelera \n";
      welcome += "/peso para saber el peso de la papelera \n";
      welcome += "/estado para recoger el resumen de datos de todos los sensores \n";
      bot.sendMessage(chat_id, welcome, "");
    }

    if (text == "/esta_mojado") {
      if (digitalRead(rainDigital)){
        bot.sendMessage(chat_id, "El suelo NO está mojado", "");
      }
      else{
        bot.sendMessage(chat_id, "El suelo está mojado", "");
      }
    }

    if (text == "/th") {
      float h = dht.readHumidity();
      float t = dht.readTemperature();
      float hic = dht.computeHeatIndex(t, h, false);
      String tA = "Temperatura: ";
      String tB = tA + t + "ºC, ";
      String hA = "Humedad: ";
      String hB = hA + h + "%, ";
      String hicA = "Sensación térmica: ";
      String hicB = hicA + hic + "ºC.\n";
      String sensor_th = tB + hB + hicB;
      bot.sendMessage(chat_id, sensor_th);
    }

    if (text == "/distancia") {
      String distanciaA = "La distancia es de: ";
      String distanciaB = distanciaA + distanceCm + " cm";
      bot.sendMessage(chat_id, distanciaB);
    }

    if (text == "/peso") {
      String pesoA = "La papelera pesa: ";
      String pesoB = pesoA + media_celula + " kg\n";
      bot.sendMessage(chat_id, pesoB);
    }
    
    if (text == "/estado") {
      String estado = "**** SENSOR DE LLUVIA ****\n";
      if (digitalRead(rainDigital)){
        estado += "El suelo NO está mojado.\n\n**** SENSOR DE PESO ****\n";
      }
      else{
        estado += "El suelo está mojado.\n\n**** SENSOR DE PESO ****\n";
      }
      String pesoA = "La papelera pesa: ";
      String pesoB = pesoA + media_celula + " kg\n\n**** SENSOR DE TEMPERATURA Y HUMEDAD ****\n";
      estado += pesoB;
      float h = dht.readHumidity();
      float t = dht.readTemperature();
      float hic = dht.computeHeatIndex(t, h, false);
      String tA = "Temperatura: ";
      String tB = tA + t + "ºC, ";
      String hA = "Humedad: ";
      String hB = hA + h + "%, ";
      String hicA = "Sensación térmica: ";
      String hicB = hicA + hic + "ºC.\n";
      String sensor_th = tB + hB + hicB;
      estado += sensor_th + "\n\n**** SENSOR DE DISTANCIA ****\n";
      String dA = "Distancia: ";
      String dB = dA + distanceCm;
      estado += dB;
      bot.sendMessage(chat_id, estado, "");
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(led_rojo,OUTPUT);
  pinMode(led_verde,OUTPUT);
  pinMode(led_azul,OUTPUT);
  
  pinMode(rainDigital,INPUT);

  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT); // Sets the echoPin as an Input
  
  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  // Print ESP32 Local IP Address
  Serial.println(WiFi.localIP());

  dht.begin();
}

void loop() {
  // **** SENSOR DE LLUVIA ****
  bool estaMojado = false;
  if (digitalRead(rainDigital)) // '0' => Está mojado
  {
    estaMojado = false;
  }
  else estaMojado = true;

  // **** SENSOR DE DISTANCIA ****
  // Clears the trigPin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  // Reads the echoPin, returns the sound wave travel time in microseconds
  duration = pulseIn(echoPin, HIGH);
  // Calculate the distance
  distanceCm = duration * SOUND_SPEED/2;

  // **** SENSOR DE PESO ****
  celula1 = ((analogRead(weightAnalog1)/28-25/7)*0.453592)-4.99;
  celula2 = ((analogRead(weightAnalog1)/28-25/7)*0.453592)-4.99;
  media_celula = (celula1+celula2)/2;

  // **** SENSOR DE TEMPERATURA Y HUMEDAD ****
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }
  float hic = dht.computeHeatIndex(t, h, false);

  if (papeleraLlenaYMojada()) { // Si la papelera está llena y tiene el suelo mojado
    if (ledsElapsed()) {
      if (ledsON) {
        apagarLeds();
        ledsON = false;
        ledTimer = millis();
      } else {
        digitalWrite(led_rojo, HIGH);
        ledsON = true;
        ledTimer = millis();
      }
    }
  } else if (papeleraLlena()) { // Si la papelera está llena
      if (ledsElapsed()) {
        if (ledsON) {
          apagarLeds();
          ledsON = false;
          ledTimer = millis();
        } else {
          digitalWrite(led_rojo, HIGH);
          digitalWrite(led_verde, HIGH);
          ledsON = true;
          ledTimer = millis();
        }
      }
  } else if (papeleraMojada()) { // Si la papelera tiene el suelo mojado
      if (ledsElapsed()) {
        if (ledsON) {
          apagarLeds();
          ledsON = false;
          ledTimer = millis();
        } else {
          digitalWrite(led_azul, HIGH);
          ledsON = true;
          ledTimer = millis();
        }
      }
  } else { // Si la papelera está OK
      if (ledsElapsed()) {
        if (ledsON) {
          apagarLeds();
          ledsON = false;
          ledTimer = millis();
        } else {
          digitalWrite(led_verde, HIGH);
          ledsON = true;
          ledTimer = millis();
        }
      }
  }

  if (millis() > lastTimeThingsBoard + thingsBoardDelay) {
    if (!tb.connected()) {
      // Connect to the ThingsBoard
      Serial.print("Connecting to: ");
      Serial.print(THINGSBOARD_SERVER);
      Serial.print(" with token ");
      Serial.println(TOKEN);
      if (!tb.connect(THINGSBOARD_SERVER, TOKEN)) {
        Serial.println("Failed to connect");
        return;
      }
    }
    Serial.println("Sending data...");
    tb.sendTelemetryBool("rain", estaMojado);
    tb.sendTelemetryInt("weight", media_celula);
    tb.sendTelemetryFloat("humidity", h);
    tb.sendTelemetryFloat("temperature", t);
    tb.sendTelemetryFloat("heat_index", hic);
    tb.sendTelemetryFloat("distance", distanceCm);
    tb.loop();
    lastTimeThingsBoard = millis();
  }
  
  if (millis() > lastTimeBotRan + botRequestDelay)  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while(numNewMessages) {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }
}

void apagarLeds() {
  digitalWrite(led_rojo, LOW);
  digitalWrite(led_verde, LOW);
  digitalWrite(led_azul, LOW);
}

bool ledsElapsed() {
  return millis() > ledTimer + ledDelay;
}

bool papeleraLlenaYMojada() {
    return papeleraLlena() & papeleraMojada();
}

bool papeleraLlena() {
  if (distanceCm < 10) {
    return true;
  } else return false;
}

bool papeleraMojada() {
  if (digitalRead(rainDigital)) // '0' => Está mojado
  {
    return false;
  } else return true;
}
