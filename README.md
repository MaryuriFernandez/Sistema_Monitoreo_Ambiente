# Sistema de Control por Estados

Este sistema utiliza un enfoque basado en estados para gestionar el comportamiento de un sistema embebido que monitorea el ambiente, verifica claves de acceso, y activa alarmas en caso necesario.

## Estados del sistema

- **INICIO:** Espera la introducción de la clave para dar acceso.
- **BLOQUEO:** Indica intento fallido de acceso, mostrando advertencias mediante LED. También indica advertencia si se pasa 3 veces consecutivas por el estado de ALARMA.
- **ALARMA:** Activa alarmas visuales y sonoras por una condición crítica (temperatura>40 y luz<10).
- **MON_AMB:** Monitoreo del ambiente (temperatura, humedad y luz).
- **COM_TERM_ALTO:** Estado encargado del comfort termico. Muestra un mensaje indicando que el ventilador fue encendido y lo activa cuando se obtiene un PMV>1.
- **COM_TERM_BAJO:** Estado encargado del comfort termico. Muestra un mensaje indicando que el calefactor fue encendido, y enciende el LED para simular una señal de comfort cuando se obtiene un PMV<-1.

## Funciones de salida de estados (`leave`)

Cada función detiene las tareas y apaga los dispositivos activos al abandonar un estado.

| Función | Acción |
|--------|--------|
| `leaveINICIO()` | Detiene la lectura de clave |
| `leaveBLOQUEO()` | Detiene LED de bloqueo y apaga LED rojo |
| `leaveALARMA()` | Detiene LED de alarma, buzzer, timeout y apaga ambos dispositivos |
| `leaveMON_AMB()` | Detiene monitoreo del ambiente |
| `leaveCOM_TERM_ALTO()` | Detiene tarea encargada del encendido del ventilador |
| `leaveCOM_TERM_BAJO()` | Detiene tarea encargada de encender el led verde |

## Requisitos

- Tener instaladas las siguientes librerías:
  - **LiquidCrystal** – Para controlar pantallas LCD.
  - **Keypad** – Para leer entradas de teclados matriciales.
  - **DHT** – Para sensores de temperatura y humedad DHT11.
  - **AsyncTaskLib** – Para tareas asíncronas temporizadas.
  - **StateMachineLib** – Para implementar la máquina de estados.
  - **SPI** – Protocolo de comunicación necesario para el lector RFID.
  - **MFRC522** – Librería específica para el lector RFID RC522.

- Tener los pines definidos: `DHTPIN`, `DHTTYPE`, `LED_ROJO`, `LED_VERDE`, `LED_AZUL`, `BUZZER_PIN`, `VENTILADOR_PIN`, `LUZ_PIN`, `RST_PIN`, `SS_PIN`.

---

**Autores:**  
Laura Isabel Molano Bermúdez  
Maryuri Fernández Salazar  
Juan Fernando Portilla Collazos  
