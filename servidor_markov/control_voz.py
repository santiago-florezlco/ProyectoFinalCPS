# control_voz.py - Sistema de Control por Voz para Semáforos
import speech_recognition as sr
import pyttsx3
import websocket
import json
import threading
import time
from datetime import datetime

class ControlVoz:
    def __init__(self):
        # Inicializar reconocimiento de voz
        self.r = sr.Recognizer()
        self.mic = sr.Microphone()
        
        # Inicializar síntesis de voz
        self.tts = pyttsx3.init()
        self.tts.setProperty('rate', 150)  # Velocidad de habla
        self.tts.setProperty('volume', 0.8)  # Volumen
        
        # Configurar voz en español (si está disponible)
        voices = self.tts.getProperty('voices')
        for voice in voices:
            if 'spanish' in voice.name.lower() or 'es' in voice.id.lower():
                self.tts.setProperty('voice', voice.id)
                break
        
        # Diccionario de comandos (simplificado para mejor reconocimiento)
        self.comandos = {
            # Modos del sistema
            "modo automático": self.modo_automatico,
            "automático": self.modo_automatico,
            "modo manual": self.modo_manual,
            "manual": self.modo_manual,
            "modo emergencia": self.modo_emergencia,
            "emergencia": self.modo_emergencia,
            "modo nocturno": self.modo_nocturno,
            "nocturno": self.modo_nocturno,
            
            # Control directo simplificado
            "uno verde": lambda: self.control_semaforo(1, "verde"),
            "dos verde": lambda: self.control_semaforo(2, "verde"),
            "semáforo uno": lambda: self.control_semaforo(1, "verde"),
            "semáforo dos": lambda: self.control_semaforo(2, "verde"),
            "todos rojo": self.todos_rojo,
            "todos rojos": self.todos_rojo,
            "todos amarillo": self.todos_amarillo,
            
            # Emergencias
            "ambulancia": self.modo_ambulancia,
            "reiniciar": self.reiniciar_sistema,
            "reiniciar sistema": self.reiniciar_sistema,
            
            # Control de escucha
            "para": self.detener_escucha,
            "parar": self.detener_escucha,
            "activar": self.activar_escucha,
            "escuchar": self.activar_escucha,
            
            # Reportes simplificados
            "estado": self.reporte_estado,
            "reporte": self.reporte_completo
        }
        
        # Estado del sistema
        self.escuchando = True
        self.ultimo_estado = {}
        
        # Conexión WebSocket
        self.ws = None
        print("🎤 Sistema de Control por Voz Iniciado")
        
        # Intentar conectar al inicio
        if self.conectar_websocket():
            self.hablar("Sistema de control por voz conectado y listo.")
        else:
            print("⚠️ Iniciando sin conexión - se intentará conectar al enviar comandos")
            self.hablar("Sistema de voz iniciado. Verificando conexión con semáforos.")

    def conectar_websocket(self):
        """Conectar al servidor WebSocket del ESP32"""
        try:
            if self.ws:
                try:
                    self.ws.close()
                except:
                    pass
            
            self.ws = websocket.WebSocket()
            self.ws.settimeout(10)  # Timeout de 10 segundos
            self.ws.connect("ws://192.168.80.22:8766")  # Puerto 8766 para comandos de voz
            print("✅ Conectado al ESP32 via WebSocket (puerto 8766)")
            return True
        except Exception as e:
            print(f"❌ Error conectando WebSocket: {e}")
            self.ws = None
            return False

    def hablar(self, texto):
        """Convertir texto a voz de forma no bloqueante"""
        print(f"🔊 TTS: {texto}")
        try:
            # Versión simplificada sin threading para evitar el error
            self.tts.say(texto)
            self.tts.runAndWait()
        except Exception as e:
            print(f"⚠️ Error en síntesis de voz: {e}")

    def enviar_comando(self, comando, parametros=None):
        """Enviar comando al ESP32 con reconexión automática"""
        max_intentos = 3
        
        for intento in range(max_intentos):
            if not self.ws:
                print(f"🔄 Intento {intento + 1}/{max_intentos} - Conectando WebSocket...")
                if not self.conectar_websocket():
                    continue
            
            mensaje = {
                "tipo": "comando_voz",
                "comando": comando,
                "timestamp": datetime.now().isoformat()
            }
            
            if parametros:
                mensaje.update(parametros)
            
            try:
                self.ws.send(json.dumps(mensaje))
                print(f"📤 Comando enviado: {comando}")
                return True
            except Exception as e:
                print(f"❌ Error enviando comando (intento {intento + 1}): {e}")
                self.ws = None
                if intento < max_intentos - 1:
                    time.sleep(1)  # Esperar 1 segundo antes del siguiente intento
        
        print(f"❌ No se pudo enviar comando después de {max_intentos} intentos")
        return False

    # === COMANDOS DE MODO ===
    def modo_automatico(self):
        if self.enviar_comando("modo_automatico"):
            self.hablar("Modo automático activado. Los semáforos funcionan con inteligencia artificial.")

    def modo_manual(self):
        if self.enviar_comando("modo_manual"):
            self.hablar("Modo manual activado. Control directo disponible.")

    def modo_emergencia(self):
        if self.enviar_comando("modo_emergencia"):
            self.hablar("¡Modo emergencia activado! Todos los semáforos en rojo.")

    def modo_nocturno(self):
        if self.enviar_comando("modo_nocturno"):
            self.hablar("Modo nocturno activado. Semáforos en parpadeo amarillo.")

    # === CONTROL DIRECTO ===
    def control_semaforo(self, numero, color):
        comando = f"sem{numero}_{color}"
        if self.enviar_comando(comando):
            self.hablar(f"Semáforo {numero} puesto en {color}")

    def todos_rojo(self):
        if self.enviar_comando("todos_rojo"):
            self.hablar("Todos los semáforos en rojo")

    def todos_amarillo(self):
        if self.enviar_comando("todos_amarillo"):
            self.hablar("Todos los semáforos en amarillo")

    # === REPORTES ===
    def reporte_estado(self):
        # Aquí deberías recibir el estado actual del ESP32
        self.hablar("El sistema está funcionando en modo normal. Markov estado moderado.")

    def reporte_vehiculos(self):
        self.hablar("Semáforo uno: 2 vehículos. Semáforo dos: 1 vehículo detectado.")

    def reporte_co2(self):
        self.hablar("Calidad del aire: 120 partes por millón. Nivel normal.")

    def reporte_completo(self):
        self.hablar("Estado general: Sistema operativo. Modo día. Estado Markov moderado. Dos vehículos en semáforo uno, uno en semáforo dos. Calidad del aire normal.")

    # === EMERGENCIAS ===
    def modo_ambulancia(self):
        if self.enviar_comando("ambulancia"):
            self.hablar("¡Protocolo ambulancia activado! Todos los semáforos en amarillo.")

    def modo_accidente(self):
        if self.enviar_comando("accidente"):
            self.hablar("Protocolo de accidente activado. Reduciendo velocidad de tráfico.")

    def reiniciar_sistema(self):
        if self.enviar_comando("reiniciar"):
            self.hablar("Reiniciando sistema de semáforos.")

    # === CONTROL DE ESCUCHA ===
    def detener_escucha(self):
        self.escuchando = False
        self.hablar("Escucha desactivada")

    def activar_escucha(self):
        self.escuchando = True
        self.hablar("Escucha activada")

    def calibrar_microfono(self):
        """Calibrar micrófono para reducir ruido ambiente"""
        print("🎤 Calibrando micrófono...")
        try:
            with self.mic as source:
                self.r.adjust_for_ambient_noise(source, duration=1)
            print("✅ Micrófono calibrado")
        except Exception as e:
            print(f"⚠️ Error calibrando micrófono: {e}")

    def verificar_conexion(self):
        """Verificar conexión con ESP32"""
        try:
            if not self.ws:
                return False
            
            # Enviar comando de prueba
            mensaje_prueba = {
                "tipo": "ping",
                "timestamp": datetime.now().isoformat()
            }
            self.ws.send(json.dumps(mensaje_prueba))
            return True
        except Exception as e:
            print(f"🔍 Verificación de conexión falló: {e}")
            self.ws = None
            return False

    def escuchar_comando(self):
        """Escuchar y procesar un comando de voz con mejor manejo de errores"""
        if not self.escuchando:
            return
        
        try:
            with self.mic as source:
                print("🎤 Escuchando...")
                # Usar timeout más corto y phrase_time_limit más largo
                audio = self.r.listen(source, timeout=0.5, phrase_time_limit=4)
            
            print("🔄 Procesando audio...")
            # Intentar primero con español mexicano, luego español estándar
            comando = None
            try:
                comando = self.r.recognize_google(audio, language='es-MX').lower()
            except:
                try:
                    comando = self.r.recognize_google(audio, language='es-ES').lower()
                except:
                    comando = self.r.recognize_google(audio, language='es').lower()
            
            print(f"👂 Comando detectado: '{comando}'")
            
            # Buscar comando en el diccionario con coincidencia parcial mejorada
            comando_encontrado = False
            mejor_coincidencia = ""
            max_coincidencias = 0
            
            for trigger, funcion in self.comandos.items():
                # Contar palabras coincidentes
                palabras_trigger = trigger.split()
                palabras_comando = comando.split()
                coincidencias = sum(1 for palabra in palabras_trigger if palabra in palabras_comando)
                
                # Si encontramos una coincidencia exacta o muy buena
                if trigger in comando or comando in trigger:
                    print(f"✅ Ejecutando: {trigger}")
                    funcion()
                    comando_encontrado = True
                    break
                elif coincidencias > max_coincidencias and coincidencias >= len(palabras_trigger) * 0.6:
                    mejor_coincidencia = trigger
                    max_coincidencias = coincidencias
            
            # Si no hay coincidencia exacta pero hay una buena coincidencia parcial
            if not comando_encontrado and mejor_coincidencia:
                print(f"✅ Ejecutando (coincidencia parcial): {mejor_coincidencia}")
                self.comandos[mejor_coincidencia]()
                comando_encontrado = True
            
            if not comando_encontrado:
                print(f"❓ Comando no reconocido: '{comando}'")
                # No hablar cada vez que no reconoce, solo imprimir
                
        except sr.WaitTimeoutError:
            pass  # No hay audio, continuar silenciosamente
        except sr.UnknownValueError:
            print("❓ No se pudo entender el audio")
        except sr.RequestError as e:
            print(f"❌ Error del servicio de reconocimiento: {e}")
            # Solo avisar de errores críticos
        except Exception as e:
            print(f"⚠️ Error inesperado: {e}")

    def iniciar_escucha_continua(self):
        """Iniciar loop de escucha continua con monitoreo de conexión"""
        self.calibrar_microfono()
        
        print("\n🎤 COMANDOS DISPONIBLES:")
        print("   • emergencia - Activar modo emergencia")
        print("   • ambulancia - Protocolo ambulancia")
        print("   • automático - Modo automático")
        print("   • manual - Modo manual")
        print("   • nocturno - Modo nocturno")
        print("   • uno verde - Semáforo 1 en verde")
        print("   • dos verde - Semáforo 2 en verde")
        print("   • todos rojo - Todos en rojo")
        print("   • reiniciar - Reiniciar sistema")
        print("   • estado - Reporte del sistema")
        print("   • parar - Detener escucha")
        print("\n")
        
        self.hablar("Sistema listo. Di 'emergencia' para probar.")
        
        ultimo_check_conexion = 0
        
        while True:
            try:
                # Verificar conexión cada 30 segundos
                ahora = time.time()
                if ahora - ultimo_check_conexion > 30:
                    if not self.verificar_conexion():
                        print("🔄 Reconectando WebSocket...")
                        self.conectar_websocket()
                    ultimo_check_conexion = ahora
                
                if self.escuchando:
                    self.escuchar_comando()
                time.sleep(0.1)
                
            except KeyboardInterrupt:
                print("\n🛑 Sistema detenido por el usuario")
                break
            except Exception as e:
                print(f"⚠️ Error en loop principal: {e}")
                time.sleep(1)  # Pausa antes de continuar

# === FUNCIÓN PRINCIPAL ===
if __name__ == "__main__":
    try:
        control = ControlVoz()
        control.iniciar_escucha_continua()
    except KeyboardInterrupt:
        print("\n🛑 Sistema detenido por el usuario")
    except Exception as e:
        print(f"❌ Error fatal: {e}")
