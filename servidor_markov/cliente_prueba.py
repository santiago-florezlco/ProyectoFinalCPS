import asyncio
import websockets
import json

async def test():
    uri = "ws://localhost:8765"
    async with websockets.connect(uri) as websocket:
        # Ejemplo de datos simulados (ajusta los valores para probar diferentes escenarios)
        datos = {
            "vehiculosSem1": 2,
            "vehiculosSem2": 3,
            "co2": 350,
            "esNoche": False,
            "peaton1": True,
            "peaton2": False
        }
        await websocket.send(json.dumps(datos))
        print(f"Enviado: {datos}")
        respuesta = await websocket.recv()
        print(f"Respuesta del servidor: {respuesta}")

asyncio.run(test())