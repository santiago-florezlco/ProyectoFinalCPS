

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

def evaluar_estado(datos):
    """
    Decide la probabilidad de transición según los datos recibidos.
    Modifica la matriz de transición de forma simple para reflejar condiciones.
    """
    global current_state
    # Extraer datos relevantes
    vehiculos1 = datos.get("vehiculosSem1", 0)
    vehiculos2 = datos.get("vehiculosSem2", 0)
    co2 = datos.get("co2", 0)
    esNoche = datos.get("esNoche", False)
    peaton1 = datos.get("peaton1", False)
    peaton2 = datos.get("peaton2", False)

    # Copiar la matriz base
    probs = TRANSITION_MATRIX[current_state][:]

    # Ajustar probabilidades según condiciones
    # Si hay mucho tráfico o CO2 alto, aumentar probabilidad de Congestionado
    if vehiculos1 + vehiculos2 >= 4 or co2 > 300:
        probs[2] += 0.15  # Congestionado
        probs[0] -= 0.1   # Normal
        probs[1] -= 0.05  # Moderado
    # Si es de noche y poco tráfico, favorecer Normal
    elif esNoche and vehiculos1 + vehiculos2 <= 2:
        probs[0] += 0.1
        probs[2] -= 0.1
    # Si hay peatones esperando, favorecer Moderado
    if peaton1 or peaton2:
        probs[1] += 0.1
        probs[0] -= 0.05
        probs[2] -= 0.05

    # Normalizar para que sumen 1
    total = sum(probs)
    probs = [max(0, p/total) for p in probs]

    # Elegir el siguiente estado según las probabilidades
    next_state = random.choices([0,1,2], weights=probs)[0]
    current_state = next_state
    return STATES[current_state]


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
