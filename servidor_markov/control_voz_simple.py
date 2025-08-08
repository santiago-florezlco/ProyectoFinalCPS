# control_voz_simple.py - Versión simplificada sin PyAudio
import pyttsx3
import websocket
import json
import time
from datetime import datetime

class ControlVozSimple:
    def __init__(self):
        # Solo síntesis de voz (no reconocimiento)
        self.tts = pyttsx3.init()
        self.tts.setProperty('rate', 150)
        self.tts.setProperty('volume', 0.8)
        
        # Comandos disponibles por teclado
        self.comandos = {
            "1": ("modo_automatico", "Modo automático activado"),
            "2": ("modo_manual", "Modo manual activado"),
            "3": ("modo_emergencia", "¡Modo emergencia activado!"),
            "4": ("modo_nocturno", "Modo nocturno activado"),
            "5": ("sem1_verde", "Semáforo 1 puesto en verde"),
            "6": ("sem2_verde", "Semáforo 2 puesto en verde"),
            "7": ("todos_rojo", "Todos los semáforos en rojo"),
            "8": ("todos_amarillo", "Todos los semáforos en amarillo"),
            "9": ("ambulancia", "¡Protocolo ambulancia activado!"),
            "0": ("reiniciar", "Sistema reiniciado"),
        }
        
        # Conexión WebSocket
        self.ws = None
        self.conectar_websocket()
        
        print("🎤 Sistema de Control Simplificado Iniciado")
        self.hablar("Sistema de control activado. Usa el teclado para comandos.")

    def conectar_websocket(self):
        """Conectar al servidor WebSocket del ESP32"""
        try:
            self.ws = websocket.WebSocket()
            self.ws.connect("ws://192.168.80.22:8766")  # Puerto 8766 para comandos de voz
            print("✅ Conectado al ESP32 via WebSocket (puerto 8766)")
        except Exception as e:
            print(f"❌ Error conectando WebSocket: {e}")
            self.hablar("Error de conexión")

    def hablar(self, texto):
        """Convertir texto a voz"""
        print(f"🔊 TTS: {texto}")
        try:
            self.tts.say(texto)
            self.tts.runAndWait()
        except:
            print("⚠️ Error en síntesis de voz")

    def enviar_comando(self, comando):
        """Enviar comando al ESP32"""
        if not self.ws:
            print("❌ Sin conexión al sistema")
            return False
        
        mensaje = {
            "tipo": "comando_voz",
            "comando": comando,
            "timestamp": datetime.now().isoformat()
        }
        
        try:
            self.ws.send(json.dumps(mensaje))
            print(f"📤 Comando enviado: {comando}")
            return True
        except Exception as e:
            print(f"❌ Error enviando comando: {e}")
            return False

    def mostrar_menu(self):
        """Mostrar menú de comandos"""
        print("\n" + "="*50)
        print("🎮 CONTROL DE SEMÁFOROS - MENÚ")
        print("="*50)
        print("1 - Modo Automático")
        print("2 - Modo Manual") 
        print("3 - Modo Emergencia")
        print("4 - Modo Nocturno")
        print("5 - Semáforo 1 Verde")
        print("6 - Semáforo 2 Verde")
        print("7 - Todos en Rojo")
        print("8 - Todos en Amarillo")
        print("9 - Protocolo Ambulancia")
        print("0 - Reiniciar Sistema")
        print("Q - Salir")
        print("="*50)

    def iniciar_control(self):
        """Iniciar loop de control por teclado"""
        while True:
            self.mostrar_menu()
            
            try:
                tecla = input("\n🎮 Selecciona una opción: ").strip().lower()
                
                if tecla == 'q':
                    self.hablar("Sistema detenido")
                    break
                elif tecla in self.comandos:
                    comando, mensaje = self.comandos[tecla]
                    if self.enviar_comando(comando):
                        self.hablar(mensaje)
                else:
                    print("❓ Opción no válida")
                    
                time.sleep(1)
                
            except KeyboardInterrupt:
                print("\n🛑 Sistema detenido")
                break

# === FUNCIÓN PRINCIPAL ===
if __name__ == "__main__":
    try:
        control = ControlVozSimple()
        control.iniciar_control()
    except Exception as e:
        print(f"❌ Error fatal: {e}")
