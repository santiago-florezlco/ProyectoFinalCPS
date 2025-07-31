#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
 
const char *ssid = "Wokwi-GUEST";
const char *password = "";
 
#define LDR1 13 // LDR Light sensor from traffic light 1 connected in pin 13
#define LDR2 12 // LDR Light sensor from traffic light 2 connected in pin 12
#define CO2 14  // CO2 sensor connected in pin 14
#define P1 1    // Traffic light 1 button connected in pin 1
#define P2 2    // Traffic light 2 button connected in pin 2
#define CNY1 42 // Infrared sensor 1 in traffic light 1 connected in pin 42
#define CNY2 41 // Infrared sensor 2 in traffic light 1 connected in pin 41
#define CNY3 40 // Infrared sensor 3 in traffic light 1 connected in pin 40
#define CNY4 39 // Infrared sensor 4 in traffic light 2 connected in pin 39
#define CNY5 38 // Infrared sensor 5 in traffic light 2 connected in pin 38
#define CNY6 37 // Infrared sensor 6 in traffic light 2 connected in pin 37
#define LR1 5   // Red traffic light 1 connected in pin 5
#define LY1 4   // Yellow traffic light 1 connected in pin 4
#define LG1 6   // Green traffic light 1 connected in pin 6
#define LR2 7   // Red traffic light 2 connected in pin 7
#define LY2 15  // Yellow traffic light 2 connected in pin 15
#define LG2 16  // Green traffic light 2 connected in pin 16
#define ROJO 0
 
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
 
// Variables para sensores
int ldr1Value = 0;
int ldr2Value = 0;
int co2Value = 0;
int p1Value = 0;
int p2Value = 0;
int cny1Value = 0;
int cny2Value = 0;
int cny3Value = 0;
int cny4Value = 0;
int cny5Value = 0;
int cny6Value = 0;
 
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
    case 0:                         // Sem1 rojo, Sem2 verde
        setSemaforo(sem1, 1, 0, 0); // Rojo
        setSemaforo(sem2, 0, 0, 1); // Verde
        if (tdelta >= 6000)
        {
            estado = 1;
            Serial.println("Estado 1: Sem2 amarillo");
            tini = millis();
        }
        break;
    case 1:                         // Sem1 rojo, Sem2 amarillo
        setSemaforo(sem1, 1, 0, 0); // Rojo
        setSemaforo(sem2, 0, 1, 0); // Amarillo
        if (tdelta >= 2000)
        {
            estado = 2;
            Serial.println("Estado 2: Sem1 verde");
            tini = millis();
        }
        break;
    case 2:                         // Sem1 verde, Sem2 rojo
        setSemaforo(sem1, 0, 0, 1); // Verde
        setSemaforo(sem2, 1, 0, 0); // Rojo
        if (tdelta >= 6000)
        {
            estado = 3;
            Serial.println("Estado 3: Sem1 amarillo");
            tini = millis();
        }
        break;
    case 3:                         // Sem1 amarillo, Sem2 rojo
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
 
    // Sensores
    pinMode(LDR1, INPUT);
    pinMode(LDR2, INPUT);
    pinMode(CO2, INPUT);
    pinMode(P1, INPUT_PULLUP);
    pinMode(P2, INPUT_PULLUP);
    pinMode(CNY1, INPUT);
    pinMode(CNY2, INPUT);
    pinMode(CNY3, INPUT);
    pinMode(CNY4, INPUT);
    pinMode(CNY5, INPUT);
    pinMode(CNY6, INPUT);
 
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
    // webSocket.beginSSL("ws.davinsony.com", 443, "/samuel_santiago");
    // webSocket.onEvent(webSocketEvent);
    // webSocket.setReconnectInterval(5000);
}
 
void loop()
{
    // webSocket.loop();
 
    // Lote 1: Lectura de sensores
    ldr1Value = analogRead(LDR1);
    ldr2Value = analogRead(LDR2);
    co2Value = analogRead(CO2);
    p1Value = digitalRead(P1);
    p2Value = digitalRead(P2);
    cny1Value = digitalRead(CNY1);
    cny2Value = digitalRead(CNY2);
    cny3Value = digitalRead(CNY3);
    cny4Value = digitalRead(CNY4);
    cny5Value = digitalRead(CNY5);
    cny6Value = digitalRead(CNY6);
 
    // --- Lote 2: Determinación de estados y condiciones ---
    // 1. Modo día/noche (umbral ejemplo: 400)
    bool esNoche = (ldr1Value < 400 && ldr2Value < 400);
 
    // 2. Conteo de vehículos por carril (CNY1-3: semáforo 1, CNY4-6: semáforo 2)
    int vehiculosSem1 = (cny1Value == 0 ? 1 : 0) + (cny2Value == 0 ? 1 : 0) + (cny3Value == 0 ? 1 : 0);
    int vehiculosSem2 = (cny4Value == 0 ? 1 : 0) + (cny5Value == 0 ? 1 : 0) + (cny6Value == 0 ? 1 : 0);
 
    // 3. Peatones esperando
    bool peaton1 = (p1Value == 0); // Botón presionado
    bool peaton2 = (p2Value == 0);
 
    // 4. Calidad del aire (umbral ejemplo: 200)
    bool co2Alto = (co2Value > 200);
 
    // Mostrar valores y estados por Serial
    Serial.print("LDR1: ");
    Serial.print(ldr1Value);
    Serial.print(" | LDR2: ");
    Serial.print(ldr2Value);
    Serial.print(" | CO2: ");
    Serial.print(co2Value);
    Serial.print(" | P1: ");
    Serial.print(p1Value);
    Serial.print(" | P2: ");
    Serial.print(p2Value);
    Serial.print(" | CNY1: ");
    Serial.print(cny1Value);
    Serial.print(" | CNY2: ");
    Serial.print(cny2Value);
    Serial.print(" | CNY3: ");
    Serial.print(cny3Value);
    Serial.print(" | CNY4: ");
    Serial.print(cny4Value);
    Serial.print(" | CNY5: ");
    Serial.print(cny5Value);
    Serial.print(" | CNY6: ");
    Serial.print(cny6Value);
    Serial.println();
 
    Serial.print("Estado: ");
    Serial.print(esNoche ? "NOCHE" : "DIA");
    Serial.print(" | Vehículos Sem1: ");
    Serial.print(vehiculosSem1);
    Serial.print(" | Vehículos Sem2: ");
    Serial.print(vehiculosSem2);
    Serial.print(" | Peatón1: ");
    Serial.print(peaton1 ? "SI" : "NO");
    Serial.print(" | Peatón2: ");
    Serial.print(peaton2 ? "SI" : "NO");
    Serial.print(" | CO2 alto: ");
    Serial.println(co2Alto ? "SI" : "NO");
 
    // --- WebSocket deshabilitado temporalmente para pruebas locales ---
    // unsigned long now = millis();
    // if (now - lastSent > interval && webSocket.isConnected())
    // {
    //     lastSent = now;
    //     sensorLocal = analogRead(LDR2); // SENSOR_PIN reemplazado por LDR2 para compatibilidad
    //
    //     // Enviar el valor local al remoto
    //     StaticJsonDocument<100> doc;
    //     doc["sensor"] = sensorLocal;
    //     doc["msg"] = sensorLocal;
    //     doc["to"] = "gabriel_tatiana";
    //     String json;
    //     serializeJson(doc, json);
    //     webSocket.sendTXT(json);
    //     Serial.println("Sent: " + json);
    // }
 
    static bool enPrioridad = false;
    static unsigned long tiniAdapt = 0;
    static int estadoAdapt = 0;
    static int tiempoVerde1 = 6000;
    static int tiempoVerde2 = 6000;
    static int tiempoAmarillo = 2000;
    static int tiempoRojo = 0; // No usado explícitamente
 
    // --- Lógica adaptativa de semáforos ---
    // Ajuste de tiempos según tráfico y condiciones
    // Verde más largo si hay más vehículos, más corto si hay pocos
    if (vehiculosSem1 > vehiculosSem2)
    {
        tiempoVerde1 = 8000;
        tiempoVerde2 = 4000;
    }
    else if (vehiculosSem2 > vehiculosSem1)
    {
        tiempoVerde1 = 4000;
        tiempoVerde2 = 8000;
    }
    else
    {
        tiempoVerde1 = 6000;
        tiempoVerde2 = 6000;
    }
    // Si hay CO2 alto, reducir ambos verdes para evitar congestión
    if (co2Alto)
    {
        tiempoVerde1 -= 2000;
        tiempoVerde2 -= 2000;
        if (tiempoVerde1 < 2000)
            tiempoVerde1 = 2000;
        if (tiempoVerde2 < 2000)
            tiempoVerde2 = 2000;
    }
    // Si es de noche, aumentar amarillo
    if (esNoche)
    {
        tiempoAmarillo = 3000;
    }
    else
    {
        tiempoAmarillo = 2000;
    }
 
    // Prioridad peatón: si hay peatón esperando, forzar verde peatonal en el siguiente ciclo
    static bool prioridadPeaton1 = false;
    static bool prioridadPeaton2 = false;
    if (peaton1)
        prioridadPeaton1 = true;
    if (peaton2)
        prioridadPeaton2 = true;
 
    medir();
    unsigned long tdeltaAdapt = millis() - tiniAdapt;
 
    // Estados: 0=Sem1 rojo/Sem2 verde, 1=Sem1 rojo/Sem2 amarillo, 2=Sem1 verde/Sem2 rojo, 3=Sem1 amarillo/Sem2 rojo
    switch (estadoAdapt)
    {
    case 0: // Sem1 rojo, Sem2 verde
        setSemaforo(sem1, 1, 0, 0);
        setSemaforo(sem2, 0, 0, 1);
        if (tdeltaAdapt >= tiempoVerde2 || prioridadPeaton1)
        {
            estadoAdapt = 1;
            tiniAdapt = millis();
        }
        break;
    case 1: // Sem1 rojo, Sem2 amarillo
        setSemaforo(sem1, 1, 0, 0);
        setSemaforo(sem2, 0, 1, 0);
        if (tdeltaAdapt >= tiempoAmarillo)
        {
            estadoAdapt = 2;
            tiniAdapt = millis();
            if (prioridadPeaton1)
                prioridadPeaton1 = false;
        }
        break;
    case 2: // Sem1 verde, Sem2 rojo
        setSemaforo(sem1, 0, 0, 1);
        setSemaforo(sem2, 1, 0, 0);
        if (tdeltaAdapt >= tiempoVerde1 || prioridadPeaton2)
        {
            estadoAdapt = 3;
            tiniAdapt = millis();
        }
        break;
    case 3: // Sem1 amarillo, Sem2 rojo
        setSemaforo(sem1, 0, 1, 0);
        setSemaforo(sem2, 1, 0, 0);
        if (tdeltaAdapt >= tiempoAmarillo)
        {
            estadoAdapt = 0;
            tiniAdapt = millis();
            if (prioridadPeaton2)
                prioridadPeaton2 = false;
        }
        break;
    default:
        estadoAdapt = 0;
        tiniAdapt = millis();
        break;
    }
    actuar();
    delay(2000); // Espera 2 segundos para facilitar la visualización en el monitor serie
}