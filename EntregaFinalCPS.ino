#include <WiFi.h>
#include <WebSocketsClient_Generic.h>
#include <ArduinoJson.h>

const char *ssid = "Wokwi-GUEST";
const char *password = "";

#define LR1 5 // Red traffic light 1 connected in pin 5
#define LY1 4 // Yellow traffic light 1 connected in pin 4
#define LG1 6 // Green traffic light 1 connected in pin 6

#define LR2 7  // Red traffic light 2 connected in pin 7
#define LY2 15 // Yellow traffic light 2 connected in pin 15
#define LG2 16 // Green traffic light 2 connected in pin 16
#define ROJO 0
#define SENSOR_PIN 12

// Estructura para semáforo
struct Semaforo
{
  int pinR, pinA, pinV;
  bool R, A, V;
};

Semaforo sem1 = {LR1, LY1, LG1, 0, 0, 0};
Semaforo sem2 = {LR2, LY2, LG2, 0, 0, 0};

int estado = 0;
int sensorLocal = 1023;
int sensorRemoto = 1023;

WebSocketsClient webSocket;

unsigned long lastSent = 0;
const unsigned long interval = 2000;

// Handle incoming WebSocket messages
void webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_CONNECTED:
    Serial.println("Connected to WebSocket server");
    break;
  case WStype_DISCONNECTED:
    Serial.println("Disconnected from WebSocket server");
    break;
  case WStype_TEXT:
  {
    Serial.printf("Received: %s\n", payload);
    // Parse the incoming JSON message
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
      Serial.print("JSON parsing failed: ");
      Serial.println(error.c_str());
      break;
    }
    // Leer el valor del sensor remoto
    if (doc.containsKey("msg"))
    {
      sensorRemoto = doc["msg"];
      Serial.print("Sensor remoto: ");
      Serial.println(sensorRemoto);
    }
  }
  break;
  default:
    break;
  }
}

long unsigned tini, tactual, tdelta;

void medir()
{
  tactual = millis();
  tdelta = tactual - tini;
}

void setSemaforo(Semaforo &sem, bool r, bool a, bool v)
{
  sem.R = r;
  sem.A = a;
  sem.V = v;
}

void controlar()
{
  switch (estado)
  {
  case 0:                       // Sem1 rojo, Sem2 verde
    setSemaforo(sem1, 1, 0, 0); // Rojo
    setSemaforo(sem2, 0, 0, 1); // Verde
    if (tdelta >= 6000)
    {
      estado = 1;
      Serial.println("Estado 1: Sem2 amarillo");
      tini = millis();
    }
    break;
  case 1:                       // Sem1 rojo, Sem2 amarillo
    setSemaforo(sem1, 1, 0, 0); // Rojo
    setSemaforo(sem2, 0, 1, 0); // Amarillo
    if (tdelta >= 2000)
    {
      estado = 2;
      Serial.println("Estado 2: Sem1 verde");
      tini = millis();
    }
    break;
  case 2:                       // Sem1 verde, Sem2 rojo
    setSemaforo(sem1, 0, 0, 1); // Verde
    setSemaforo(sem2, 1, 0, 0); // Rojo
    if (tdelta >= 6000)
    {
      estado = 3;
      Serial.println("Estado 3: Sem1 amarillo");
      tini = millis();
    }
    break;
  case 3:                       // Sem1 amarillo, Sem2 rojo
    setSemaforo(sem1, 0, 1, 0); // Amarillo
    setSemaforo(sem2, 1, 0, 0); // Rojo
    if (tdelta >= 2000)
    {
      estado = 0;
      Serial.println("Estado 0: Sem2 verde");
      tini = millis();
    }
    break;
  default:
    break;
  }
}

void actuar()
{
  digitalWrite(sem1.pinR, sem1.R);
  digitalWrite(sem1.pinA, sem1.A);
  digitalWrite(sem1.pinV, sem1.V);
  digitalWrite(sem2.pinR, sem2.R);
  digitalWrite(sem2.pinA, sem2.A);
  digitalWrite(sem2.pinV, sem2.V);
}

void setup()
{
  Serial.begin(115200);
  pinMode(LR1, OUTPUT);
  pinMode(LY1, OUTPUT);
  pinMode(LG1, OUTPUT);
  pinMode(LR2, OUTPUT);
  pinMode(LY2, OUTPUT);
  pinMode(LG2, OUTPUT);

  actuar();
  tini = millis();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nConnected to WiFi");

  // Setup secure WebSocket client (wss)
  webSocket.beginSSL("ws.davinsony.com", 443, "/gabriel_tatiana");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
}

void loop()
{
  webSocket.loop();

  unsigned long now = millis();
  if (now - lastSent > interval && webSocket.isConnected())
  {
    lastSent = now;
    sensorLocal = analogRead(SENSOR_PIN);

    // Enviar el valor local al remoto
    StaticJsonDocument<100> doc;
    doc["sensor"] = sensorLocal;
    doc["msg"] = sensorLocal;
    doc["to"] = "samuel_santiago";
    String json;
    serializeJson(doc, json);
    webSocket.sendTXT(json);
    Serial.println("Sent: " + json);
  }

  static bool enPrioridad = false;
  medir();
  // Lógica de prioridad: si local o remoto < 600, forzar semáforo 2 en verde y 1 en rojo
  if (sensorLocal < 600 || sensorRemoto < 600)
  {
    setSemaforo(sem1, 1, 0, 0); // Semáforo 1 rojo
    setSemaforo(sem2, 0, 0, 1); // Semáforo 2 verde
    enPrioridad = true;
  }
  else
  {
    if (enPrioridad)
    {
      tini = millis(); // Reiniciar temporizador al salir de prioridad
      enPrioridad = false;
    }
    controlar(); // Ciclo normal
  }
  actuar();
}
