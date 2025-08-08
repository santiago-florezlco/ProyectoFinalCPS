

import asyncio
import websockets
import json
import random


HOST = '0.0.0.0'
PORT = 8765

# --- Cadena de Markov ---
STATES = ["Normal", "Moderado", "Congestionado"]
# Matriz de transición: fila = estado actual, columna = siguiente estado
# Ejemplo simple, los valores pueden ajustarse según pruebas
TRANSITION_MATRIX = [
    # Normal      Moderado    Congestionado
    [0.7,         0.25,       0.05],      # Desde Normal
    [0.2,         0.6,        0.2],       # Desde Moderado
    [0.05,        0.25,       0.7]        # Desde Congestionado
]

# Estado global inicial
current_state = 0  # 0: Normal, 1: Moderado, 2: Congestionado
stability_counter = 0  # Contador para estabilidad del estado
min_stability_cycles = 3  # Mínimo de ciclos antes de permitir cambio

def evaluar_estado(datos):
    """
    Decide el estado de tráfico con enfoque híbrido Markov-probabilístico.
    Usa probabilidades conservadoras que favorecen mantener el estado actual.
    """
    global current_state, stability_counter
    
    # Extraer datos relevantes
    vehiculos1 = datos.get("vehiculosSem1", 0)
    vehiculos2 = datos.get("vehiculosSem2", 0)
    trafico_total = vehiculos1 + vehiculos2
    co2 = datos.get("co2", 0)
    esNoche = datos.get("esNoche", False)
    peaton1 = datos.get("peaton1", False)
    peaton2 = datos.get("peaton2", False)
    peatones = peaton1 or peaton2
    
    # LÓGICA DETERMINÍSTICA: Calcular el estado ideal basado en condiciones
    estado_ideal = current_state  # Por defecto mantener el actual
    
    # Condiciones para NORMAL (0)
    if trafico_total <= 1 and co2 <= 250 and not peatones:
        estado_ideal = 0
    
    # Condiciones para MODERADO (1)
    elif (trafico_total == 2 or trafico_total == 3) or peatones or (250 < co2 <= 300):
        estado_ideal = 1
    
    # Condiciones para CONGESTIONADO (2)
    elif trafico_total >= 4 or co2 > 300:
        estado_ideal = 2
    
    # Ajustes por condiciones nocturnas (menos agresivo)
    if esNoche and trafico_total <= 2 and estado_ideal == 2:
        estado_ideal = 1  # Reducir de Congestionado a Moderado en noche
    
    print(f"[SENSORES] tráfico={trafico_total}, co2={co2}, noche={esNoche}, peatones={peatones}")
    print(f"[ESTADO] Actual={STATES[current_state]}, Ideal={STATES[estado_ideal]}")
    
    # ENFOQUE HÍBRIDO MARKOV-PROBABILÍSTICO
    # Usar probabilidades más conservadoras que favorezcan mantener el estado actual
    if estado_ideal == current_state:
        # El estado ideal coincide con el actual - alta probabilidad de mantener
        if current_state == 0:  # Normal
            probs = [0.9, 0.08, 0.02]
        elif current_state == 1:  # Moderado
            probs = [0.05, 0.9, 0.05]
        else:  # Congestionado
            probs = [0.05, 0.05, 0.9]
        print(f"[DECISIÓN] Estado ideal coincide - mantener {STATES[current_state]} con alta probabilidad")
    else:
        # El estado ideal es diferente - probabilidades conservadoras de cambio
        probs = [0.6, 0.3, 0.1] if estado_ideal < current_state else [0.4, 0.4, 0.2]
        print(f"[DECISIÓN] Estado ideal diferente ({STATES[estado_ideal]}) - probabilidades conservadoras de cambio")
    
    # Normalizar para que sumen 1
    total = sum(probs)
    probs = [max(0, p/total) for p in probs]
    
    print(f"[PROBABILIDADES] Normal={probs[0]:.2f}, Moderado={probs[1]:.2f}, Congestionado={probs[2]:.2f}")
    
    # Elegir el siguiente estado según las probabilidades
    next_state = random.choices([0,1,2], weights=probs)[0]
    print(f"[RESULTADO] {STATES[current_state]} → {STATES[next_state]}")
    
    current_state = next_state
    return STATES[current_state]
    
    # ===== ENFOQUE DETERMINÍSTICO (COMENTADO PARA PRUEBA) =====
    # # ESTABILIZACIÓN: Solo cambiar si el estado ideal es diferente Y hemos esperado suficientes ciclos
    # if estado_ideal == current_state:
    #     # El estado actual es correcto, incrementar estabilidad
    #     stability_counter += 1
    #     print(f"[DECISIÓN] Mantener {STATES[current_state]} (estable)")
    # else:
    #     # El estado ideal es diferente
    #     if stability_counter >= min_stability_cycles:
    #         # Hemos esperado suficiente, cambiar al estado ideal
    #         print(f"[DECISIÓN] Cambiar {STATES[current_state]} → {STATES[estado_ideal]} (justificado)")
    #         current_state = estado_ideal
    #         stability_counter = 0  # Reiniciar contador
    #     else:
    #         # No hemos esperado suficiente, mantener actual
    #         stability_counter += 1
    #         print(f"[DECISIÓN] Mantener {STATES[current_state]} (esperando estabilidad {stability_counter}/{min_stability_cycles})")
    # 
    # return STATES[current_state]


async def handle_client(websocket):
    print("Cliente conectado")
    try:
        async for message in websocket:
            print(f"Mensaje recibido: {message}")
            try:
                data = json.loads(message)
            except json.JSONDecodeError:
                await websocket.send(json.dumps({"error": "Formato JSON inválido"}))
                continue

            # Evaluar el estado global usando la cadena de Markov
            estado = evaluar_estado(data)
            response = {"estado": estado}
            await websocket.send(json.dumps(response))
            print(f"Respuesta enviada: {response}")

    except Exception as e:
        print(f"Error en conexión WebSocket: {type(e).__name__}: {e}")
        if isinstance(e, websockets.ConnectionClosed):
            print("Cliente desconectado")



async def main():
    async with websockets.serve(handle_client, host=HOST, port=PORT):
        print(f"Servidor WebSocket escuchando en ws://{HOST}:{PORT}")
        await asyncio.Future()  # Ejecutar para siempre

if __name__ == "__main__":
    asyncio.run(main())
