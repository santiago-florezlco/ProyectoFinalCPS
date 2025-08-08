#include <Wire.h>              // Para I2C
#include <LiquidCrystal_I2C.h> // Para display LCD I2C
#include <WiFi.h>
#include <WebSocketsClient_Generic.h>
#include <WebSocketsServer_Generic.h>
#include <ArduinoJson.h>
// Inicialización del display LCD I2C (dirección 0x27, 20 columnas, 4 filas)
LiquidCrystal_I2C lcd(0x27, 20, 4);

const char *ssid = "latierrita";
const char *password = "lote1706";

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

WebSocketsClient webSocketMarkov;    // Cliente para servidor Markov
WebSocketsServer webSocketVoz(8766); // Servidor para comandos de voz

unsigned long lastSent = 0;
const unsigned long interval = 2000;

// Variables para adaptación dinámica basada en estado Markov
String estadoMarkov = "Normal";  // Estado recibido del servidor
int tiempoVerde1Adaptado = 6000; // Tiempos adaptativos
int tiempoVerde2Adaptado = 6000;
int tiempoAmarilloAdaptado = 2000;

// Variables para control por voz
String modoControl = "automatico"; // "automatico", "manual", "emergencia", "nocturno"
bool comandoVozActivo = false;
unsigned long tiempoComandoVoz = 0;

// Handle incoming WebSocket messages from Markov server
void webSocketMarkovEvent(WStype_t type, uint8_t *payload, size_t length)
{
    switch (type)
    {
    case WStype_CONNECTED:
        Serial.println("Connected to Markov WebSocket server");
        break;
    case WStype_DISCONNECTED:
        Serial.println("Disconnected from Markov WebSocket server");
        break;
    case WStype_TEXT:
    {
        Serial.printf("Received from Markov: %s\n", payload);
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (error)
        {
            Serial.print("JSON parsing failed: ");
            Serial.println(error.c_str());
            break;
        }

        // LOTE 3: Adaptación dinámica basada en estado Markov
        if (doc.containsKey("estado"))
        {
            estadoMarkov = doc["estado"].as<String>();
            Serial.print("Estado Markov recibido: ");
            Serial.println(estadoMarkov);
            adaptarTiemposSemaforo();
        }

        // Mantener compatibilidad con código anterior
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

// Handle incoming WebSocket messages from Voice Control
void webSocketVozEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
    switch (type)
    {
    case WStype_CONNECTED:
        Serial.printf("Voice client [%u] connected\n", num);
        break;
    case WStype_DISCONNECTED:
        Serial.printf("Voice client [%u] disconnected\n", num);
        break;
    case WStype_TEXT:
    {
        Serial.printf("Voice command received from [%u]: %s\n", num, payload);
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (error)
        {
            Serial.print("Voice JSON parsing failed: ");
            Serial.println(error.c_str());
            break;
        }

        // Manejo de comandos de voz
        if (doc.containsKey("tipo") && doc["tipo"] == "comando_voz")
        {
            String comando = doc["comando"].as<String>();
            Serial.print("🎤 Comando de voz recibido: ");
            Serial.println(comando);
            procesarComandoVoz(comando);
        }
    }
    break;
    default:
        break;
    }
}

// LOTE 3: Función para adaptar tiempos según estado Markov
void adaptarTiemposSemaforo()
{
    if (estadoMarkov == "Normal")
    {
        // Tráfico normal - tiempos estándar
        tiempoVerde1Adaptado = 6000;
        tiempoVerde2Adaptado = 6000;
        tiempoAmarilloAdaptado = 2000;
        Serial.println("[ADAPTACIÓN] Modo Normal: tiempos estándar");
    }
    else if (estadoMarkov == "Moderado")
    {
        // Tráfico moderado - tiempos ligeramente extendidos
        tiempoVerde1Adaptado = 8000;
        tiempoVerde2Adaptado = 8000;
        tiempoAmarilloAdaptado = 2500;
        Serial.println("[ADAPTACIÓN] Modo Moderado: tiempos extendidos");
    }
    else if (estadoMarkov == "Congestionado")
    {
        // Tráfico congestionado - tiempos largos para descongestionar
        tiempoVerde1Adaptado = 10000;
        tiempoVerde2Adaptado = 10000;
        tiempoAmarilloAdaptado = 3000;
        Serial.println("[ADAPTACIÓN] Modo Congestionado: tiempos largos");
    }
}

// NUEVO: Función para procesar comandos de voz
void procesarComandoVoz(String comando)
{
    comandoVozActivo = true;
    tiempoComandoVoz = millis();

    // Comandos de modo
    if (comando == "modo_automatico")
    {
        modoControl = "automatico";
        Serial.println("🎤 Modo automático activado");
    }
    else if (comando == "modo_manual")
    {
        modoControl = "manual";
        Serial.println("🎤 Modo manual activado");
    }
    else if (comando == "modo_emergencia")
    {
        modoControl = "emergencia";
        Serial.println("🎤 ¡MODO EMERGENCIA ACTIVADO!");
        // Poner todos en rojo inmediatamente
        setSemaforo(sem1, 1, 0, 0);
        setSemaforo(sem2, 1, 0, 0);
        actuar();
    }
    else if (comando == "modo_nocturno")
    {
        modoControl = "nocturno";
        Serial.println("🎤 Modo nocturno forzado");
    }

    // Control directo de semáforos
    else if (comando == "sem1_verde")
    {
        modoControl = "manual"; // Activar modo manual automáticamente
        setSemaforo(sem1, 0, 0, 1);
        setSemaforo(sem2, 1, 0, 0);
        actuar();
        Serial.println("🎤 Semáforo 1 puesto en verde (modo manual)");
    }
    else if (comando == "sem2_verde")
    {
        modoControl = "manual"; // Activar modo manual automáticamente
        setSemaforo(sem1, 1, 0, 0);
        setSemaforo(sem2, 0, 0, 1);
        actuar();
        Serial.println("🎤 Semáforo 2 puesto en verde (modo manual)");
    }
    else if (comando == "todos_rojo")
    {
        modoControl = "emergencia"; // Cambiar a modo emergencia para que perdure
        setSemaforo(sem1, 1, 0, 0);
        setSemaforo(sem2, 1, 0, 0);
        actuar();
        Serial.println("🎤 Todos los semáforos en rojo (modo emergencia)");
    }
    else if (comando == "todos_amarillo")
    {
        setSemaforo(sem1, 0, 1, 0);
        setSemaforo(sem2, 0, 1, 0);
        actuar();
        Serial.println("🎤 Todos los semáforos en amarillo");
    }

    // Protocolos especiales
    else if (comando == "ambulancia")
    {
        modoControl = "ambulancia";
        setSemaforo(sem1, 0, 1, 0); // Cambiar a amarillo en lugar de rojo
        setSemaforo(sem2, 0, 1, 0); // Cambiar a amarillo en lugar de rojo
        actuar();
        Serial.println("🎤 🚑 PROTOCOLO AMBULANCIA - Todos en amarillo");
    }
    else if (comando == "reiniciar")
    {
        modoControl = "automatico";
        estadoMarkov = "Normal";
        adaptarTiemposSemaforo();
        Serial.println("🎤 Sistema reiniciado");
    }

    else
    {
        Serial.println("🎤 ❓ Comando de voz no reconocido: " + comando);
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
    // Inicializar LCD
    lcd.init();
    lcd.backlight();
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
    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());

    // Setup WebSocket client para servidor Markov
    webSocketMarkov.begin("192.168.80.24", 8765, "/");
    webSocketMarkov.onEvent(webSocketMarkovEvent);
    webSocketMarkov.setReconnectInterval(5000);

    // Setup WebSocket server para comandos de voz
    webSocketVoz.begin();
    webSocketVoz.onEvent(webSocketVozEvent);
    Serial.println("Voice WebSocket server started on port 8766");
}

void loop()
{
    webSocketMarkov.loop(); // Mantener conexión con servidor Markov
    webSocketVoz.loop();    // Mantener servidor para comandos de voz

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
    bool peaton1 = (p1Value == 1); // Botón presionado
    bool peaton2 = (p2Value == 1);

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

    // --- Envío de datos al servidor Markov cada 2 segundos ---
    unsigned long now = millis();
    if (now - lastSent > interval && webSocketMarkov.isConnected())
    {
        lastSent = now;
        // Enviar todos los datos relevantes al servidor Markov
        StaticJsonDocument<200> doc;
        doc["ldr1"] = ldr1Value;
        doc["ldr2"] = ldr2Value;
        doc["co2"] = co2Value;
        doc["vehiculosSem1"] = (cny1Value == 0 ? 1 : 0) + (cny2Value == 0 ? 1 : 0) + (cny3Value == 0 ? 1 : 0);
        doc["vehiculosSem2"] = (cny4Value == 0 ? 1 : 0) + (cny5Value == 0 ? 1 : 0) + (cny6Value == 0 ? 1 : 0);
        doc["peaton1"] = (p1Value == 1);
        doc["peaton2"] = (p2Value == 1);
        doc["co2Alto"] = (co2Value > 200);
        doc["esNoche"] = (ldr1Value < 400 && ldr2Value < 400);
        String json;
        serializeJson(doc, json);
        webSocketMarkov.sendTXT(json);
        Serial.println("Sent to Markov: " + json);
    }

    static bool enPrioridad = false;
    static unsigned long tiniAdapt = 0;
    static int estadoAdapt = 0;

    // Variables para prioridad peatonal (deben ser accesibles en modo día y noche)
    static bool prioridadPeaton1 = false;
    static bool prioridadPeaton2 = false;

    // MODO NOCTURNO: Parpadeo amarillo para seguridad O comando de voz
    if (esNoche || modoControl == "nocturno")
    {
        // En modo nocturno, ambos semáforos parpadean en amarillo
        static unsigned long tiempoParpadeo = 0;
        static bool estadoParpadeo = false;

        if (millis() - tiempoParpadeo >= 1000) // Parpadeo cada 1 segundo
        {
            estadoParpadeo = !estadoParpadeo;
            tiempoParpadeo = millis();

            if (estadoParpadeo)
            {
                // Ambos amarillos encendidos
                setSemaforo(sem1, 0, 1, 0);
                setSemaforo(sem2, 0, 1, 0);
            }
            else
            {
                // Ambos amarillos apagados
                setSemaforo(sem1, 0, 0, 0);
                setSemaforo(sem2, 0, 0, 0);
            }
            actuar();
        }
    }
    // MODO EMERGENCIA: Todos en rojo
    else if (modoControl == "emergencia")
    {
        setSemaforo(sem1, 1, 0, 0);
        setSemaforo(sem2, 1, 0, 0);
        actuar();

        // Auto-reset después de 60 segundos (aumentado de 30)
        if (millis() - tiempoComandoVoz > 60000)
        {
            modoControl = "automatico";
            Serial.println("🎤 Modo emergencia auto-desactivado");
        }
    }
    // MODO AMBULANCIA: Todos en amarillo
    else if (modoControl == "ambulancia")
    {
        setSemaforo(sem1, 0, 1, 0);
        setSemaforo(sem2, 0, 1, 0);
        actuar();

        // Auto-reset después de 45 segundos
        if (millis() - tiempoComandoVoz > 45000)
        {
            modoControl = "automatico";
            Serial.println("🎤 Protocolo ambulancia completado");
        }
    }
    // MODO MANUAL: No hacer nada automático, esperar comandos
    else if (modoControl == "manual")
    {
        // Los semáforos se controlan solo por comandos de voz
        // No ejecutar lógica automática
    }
    else
    {
        // MODO DIURNO: Lógica normal de semáforos adaptativa

        // LOTE 3: Usar tiempos adaptados por estado Markov en lugar de lógica local
        // Los tiempos ahora vienen del servidor Markov global
        int tiempoVerde1 = tiempoVerde1Adaptado;
        int tiempoVerde2 = tiempoVerde2Adaptado;
        int tiempoAmarillo = tiempoAmarilloAdaptado;

        // --- LÓGICA ADAPTATIVA HÍBRIDA: Markov + Local ---
        // Ajustes finos basados en condiciones locales inmediatas
        // (El estado Markov da la base, los sensores locales hacen ajustes menores)

        // Ajuste fino por diferencia de tráfico entre carriles
        if (vehiculosSem1 > vehiculosSem2 + 1)
        {
            tiempoVerde1 += 1000; // Pequeño incremento para carril más congestionado
            tiempoVerde2 -= 500;  // Pequeña reducción para carril menos congestionado
        }
        else if (vehiculosSem2 > vehiculosSem1 + 1)
        {
            tiempoVerde2 += 1000; // Pequeño incremento para carril más congestionado
            tiempoVerde1 -= 500;  // Pequeña reducción para carril menos congestionado
        }

        // Límites de seguridad
        if (tiempoVerde1 < 3000)
            tiempoVerde1 = 3000; // Mínimo 3 segundos
        if (tiempoVerde2 < 3000)
            tiempoVerde2 = 3000;
        if (tiempoVerde1 > 15000)
            tiempoVerde1 = 15000; // Máximo 15 segundos
        if (tiempoVerde2 > 15000)
            tiempoVerde2 = 15000;

        // Prioridad peatón: si hay peatón esperando, forzar verde peatonal en el siguiente ciclo
        if (peaton1)
            prioridadPeaton1 = true;
        if (peaton2)
            prioridadPeaton2 = true;

        medir();
        unsigned long tdeltaAdapt = millis() - tiniAdapt;

        // Estados: 0=Sem1 rojo/Sem2 verde, 1=Sem1 rojo/Sem2 amarillo, 2=Sem1 verde/Sem2 rojo, 3=Sem1 amarillo/Sem2 rojo
        switch (estadoAdapt)
        {
        case 0: // Sem1 rojo, Sem2 verde - Peatón 2 puede cruzar aquí
            setSemaforo(sem1, 1, 0, 0);
            setSemaforo(sem2, 0, 0, 1);
            if (tdeltaAdapt >= tiempoVerde2 || prioridadPeaton2) // Peatón 2 extiende el verde del Sem2
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
                if (prioridadPeaton2)
                    prioridadPeaton2 = false;
            }
            break;
        case 2: // Sem1 verde, Sem2 rojo - Peatón 1 puede cruzar aquí
            setSemaforo(sem1, 0, 0, 1);
            setSemaforo(sem2, 1, 0, 0);
            if (tdeltaAdapt >= tiempoVerde1 || prioridadPeaton1) // Peatón 1 extiende el verde del Sem1
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
                if (prioridadPeaton1)
                    prioridadPeaton1 = false;
            }
            break;
        default:
            estadoAdapt = 0;
            tiniAdapt = millis();
            break;
        }
        actuar();
    }

    // Actualizar estado de peatones (común para día y noche)
    if (peaton1)
        prioridadPeaton1 = true;
    if (peaton2)
        prioridadPeaton2 = true;

    // --- Lote 4: Mostrar información en el display I2C (con control para evitar errores I2C) ---
    static unsigned long ultimoUpdateLCD = 0;
    if (millis() - ultimoUpdateLCD >= 500) // Actualizar LCD cada 500ms para evitar sobrecarga I2C
    {
        ultimoUpdateLCD = millis();

        lcd.clear();

        // Línea 0: Estado Markov y tráfico
        lcd.setCursor(0, 0);
        lcd.print("Markov:");
        if (estadoMarkov == "Normal")
            lcd.print("NOR");
        else if (estadoMarkov == "Moderado")
            lcd.print("MOD");
        else if (estadoMarkov == "Congestionado")
            lcd.print("CON");
        lcd.print(" S1:");
        lcd.print(vehiculosSem1);
        lcd.print(" S2:");
        lcd.print(vehiculosSem2);

        // Línea 1: Tiempos adaptados y CO2
        lcd.setCursor(0, 1);
        lcd.print("T:");
        lcd.print(tiempoVerde1Adaptado / 1000);
        lcd.print("/");
        lcd.print(tiempoVerde2Adaptado / 1000);
        lcd.print("s CO2:");
        lcd.print(co2Value);

        // Línea 2: Modo día/noche, estado semáforo y CONTROL DE VOZ
        lcd.setCursor(0, 2);
        if (esNoche || modoControl == "nocturno")
        {
            lcd.print("NOCHE PARPADEO ON   ");
        }
        else if (modoControl == "emergencia")
        {
            lcd.print("EMERGENCIA   ");
        }
        else if (modoControl == "ambulancia")
        {
            lcd.print("AMBULANCIA   ");
        }
        else if (modoControl == "manual")
        {
            lcd.print("CONTROL MANUAL  ");
        }
        else
        {
            lcd.print("DIA   Est:");
            lcd.print(estadoAdapt);
            lcd.print(" T:");
            lcd.print((millis() - tiniAdapt) / 1000);
            lcd.print("s");
        }

        // Línea 3: Mensajes inteligentes para peatones
        lcd.setCursor(0, 3);
        if (esNoche)
        {
            lcd.print("MODO NOCTURNO       ");
        }
        else
        {
            // Lógica basada en prioridades peatonales y seguridad de cruce
            if (peaton1 && !prioridadPeaton1 && peaton2 && !prioridadPeaton2)
            {
                lcd.print("P1&P2: Espere       ");
            }
            else if (peaton1 && !prioridadPeaton1)
            {
                lcd.print("P1: Espere verde    ");
            }
            else if (peaton2 && !prioridadPeaton2)
            {
                lcd.print("P2: Espere verde    ");
            }
            else if (prioridadPeaton1 && (estadoAdapt == 0)) // P1 puede cruzar cuando Sem1 está rojo
            {
                lcd.print("P1: Cruce ahora     ");
            }
            else if (prioridadPeaton2 && (estadoAdapt == 2)) // P2 puede cruzar cuando Sem2 está rojo
            {
                lcd.print("P2: Cruce ahora     ");
            }
            else if (prioridadPeaton1)
            {
                lcd.print("P1: Espere cruce    ");
            }
            else if (prioridadPeaton2)
            {
                lcd.print("P2: Espere cruce    ");
            }
            else
            {
                lcd.print("Peatones: ---       ");
            }
        }
    }

    // Espera 2 segundos mostrando en LCD y manteniendo la conexión WebSocket
    unsigned long startDelay = millis();
    while (millis() - startDelay < 2000)
    {
        webSocketMarkov.loop(); // Mantener conexión Markov
        webSocketVoz.loop();    // Mantener servidor de voz
        delay(10);              // Pequeño delay para no saturar el CPU
    }
}