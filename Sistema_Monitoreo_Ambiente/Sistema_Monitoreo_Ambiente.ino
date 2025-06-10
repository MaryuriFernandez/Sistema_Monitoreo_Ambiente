#include <LiquidCrystal.h>
#include <Keypad.h>
#include <DHT.h>
#include "AsyncTaskLib.h"
#include "StateMachineLib.h"
#include <SPI.h>
#include <MFRC522.h>

// ----------------- DEFINICIONES DE HARDWARE -----------------
#define DHTPIN 22
#define DHTTYPE DHT11
#define LED_ROJO 46
#define LED_VERDE 48
#define LED_AZUL 49
#define BUZZER_PIN 7
#define VENTILADOR_PIN 6
#define LUZ_PIN A0
#define RST_PIN 33
#define SS_PIN 53

//--------------------------------------------


MFRC522 mfrc522(SS_PIN, RST_PIN);
float pmv = 0.0;

/**
 * @brief Lee el valor PMV (Predicted Mean Vote) desde una tarjeta RFID.
 * 
 * Esta función detecta la presencia de una tarjeta RFID, realiza autenticación 
 * con una clave predeterminada, lee un bloque de datos del sector correspondiente 
 * y convierte los primeros cuatro bytes del bloque en un valor de tipo `float` 
 * representando el valor PMV. Finalmente, detiene la comunicación con la tarjeta.
 * 
 * @param valorPMV Referencia a una variable float donde se almacenará el PMV leído.
 * @return true Si se leyó y procesó correctamente el valor PMV desde la tarjeta.
 * @return false Si no se detectó tarjeta, falló la autenticación o la lectura del bloque.
*/
bool leerPMVdeTarjeta(float &valorPMV) {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return false; // No hay tarjeta presente
  }

  byte blockAddr = 4;
  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF; // clave por defecto

  MFRC522::StatusCode status;
  byte buffer[18];
  byte size = sizeof(buffer);

  // Autenticación
  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockAddr, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("Fallo en autenticación: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }

  // Leer bloque
  status = mfrc522.MIFARE_Read(blockAddr, buffer, &size);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("Fallo en lectura del bloque: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }

  // Convertir los primeros 4 bytes a float (formato Little Endian estándar)
  float *pmvPtr = reinterpret_cast<float*>(buffer);
  valorPMV = *pmvPtr;

  // Detener la comunicación
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  return true;
}
//------------------------------------------------------------------
// ----------------- OBJETOS -----------------
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// Teclado
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {30, 32, 34, 36};
byte colPins[COLS] = {38, 40, 42, 44};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ----------------- VARIABLES DE SENSOR Y CONTROL -----------------
float temperatura = 0;
float humedad = 0;
int luz = 0;
int alarmasConsecutivas = 0;

//Variables para la clave
char bufferClave[5] = "";
byte posClave = 0;
byte intentosClave = 0;
const char claveCorrecta[] = "1234";


// ----------------- ENUMERACIONES -----------------
enum State {
  INICIO = 0,
  BLOQUEO,
  MON_AMB,
  ALARMA,
  COM_TERM_ALTO,
  COM_TERM_BAJO
};

enum Input {
  TIMEOUT = 0,
  TEMP_LUZ,
  KEY,
  BLOCK,
  ACCESS,
  TRY,
  PMV_MAYOR,
  PMV_MENOR,
  Unknown
};

Input input = Unknown;
StateMachine stateMachine(6, 10);

// ----------------- TAREAS ASÍNCRONAS -----------------
AsyncTask Task_TimeoutClave(7000, false, []() {
  posClave = 0;
  memset(bufferClave, 0, sizeof(bufferClave));
  lcd.clear();
  lcd.print("Ingrese clave:");
  lcd.setCursor(0, 1);
});


AsyncTask Task_MonitoreoAmbiente(2000, true, []() {
  temperatura = dht.readTemperature();
  humedad = dht.readHumidity();
  luz = analogRead(LUZ_PIN);

  Serial.print("T: "); Serial.print(temperatura);
  Serial.print(" H: "); Serial.print(humedad);
  Serial.print(" L: "); Serial.println(luz);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T:"); lcd.print(temperatura);
  lcd.print(" H:"); lcd.print(humedad);
  lcd.setCursor(0, 1);
  lcd.print("L: "); lcd.print(luz);

  // Leer PMV desde RFID
  float nuevoPMV;
  if (leerPMVdeTarjeta(nuevoPMV)) {
    pmv = nuevoPMV;
    Serial.print("PMV leído: ");
    Serial.println(pmv);
  } else {
    Serial.println("Esperando lectura de PMV...");
  }

  if(nuevoPMV > 1){
    input = PMV_MAYOR;
  }else if(nuevoPMV < -1){
    input = PMV_MENOR;
  }

  if (temperatura > 40 && luz < 10) {
    input = TEMP_LUZ;
  }
});

AsyncTask Task_Timeout_Alarma(3000, false, []() { input = TIMEOUT; });

bool ledAlarma = false;
AsyncTask Task_LED_Alarma(800, true, []() {
  digitalWrite(LED_ROJO, ledAlarma ? LOW : HIGH);
  ledAlarma = !ledAlarma;
  Task_LED_Alarma.SetIntervalMillis(ledAlarma ? 800 : 200);

  Serial.print("Alarmas consecutivas: ");
  Serial.println(alarmasConsecutivas);
  if (alarmasConsecutivas >= 3) {
    input = TRY;
  }
});

bool ledBloqueo = false;
AsyncTask Task_LED_Bloqueo(200, true, []() {
  digitalWrite(LED_ROJO, ledBloqueo ? LOW : HIGH);
  ledBloqueo = !ledBloqueo;
  Task_LED_Bloqueo.SetIntervalMillis(ledBloqueo ? 200 : 100);
  char key = keypad.getKey();
  if(key == '*'){
    input = KEY;
  }
});

bool buzzerState = false;  
AsyncTask Task_Buzzer(200, true, []() {
    digitalWrite(BUZZER_PIN, buzzerState ? LOW : HIGH);
    buzzerState = !buzzerState; 
    Task_Buzzer.SetIntervalMillis(buzzerState ? 200 : 100); 
});

AsyncTask Task_Timeout_ComTermAlto(5000, false, []() {
  digitalWrite(VENTILADOR_PIN, LOW);
  input = TIMEOUT;
});

AsyncTask Task_LED_Verde(2000, false, []() {
  digitalWrite(LED_VERDE, LOW);
});

AsyncTask Task_Timeout_ComTermBajo(2000, false, []() {
  input = TIMEOUT;
  Task_LED_Verde.Start();
});

AsyncTask Task_LED_Azul(1000, false, []() {
  digitalWrite(LED_AZUL, LOW); 
});

AsyncTask Task_VerificarIntentos(100, false, []() {
  intentosClave++;
  if (intentosClave >= 3) {
    input = BLOCK;
  } else {
    posClave = 0;
    memset(bufferClave, 0, sizeof(bufferClave));
    lcd.clear();
    lcd.print("Ingrese clave:");
  }
});

AsyncTask Task_DemoraAntesDeVerificar(1000, false, []() {
  Task_VerificarIntentos.Start();
});

AsyncTask Task_ProcesarClave(100, false, []() {
  if (strcmp(bufferClave, claveCorrecta) == 0) {
    lcd.clear();
    lcd.print("Clave correcta");
    digitalWrite(LED_VERDE, HIGH);
    Task_LED_Verde.Start();
    input = ACCESS;
  } else {
    lcd.clear();
    lcd.print("Incorrecta");
    digitalWrite(LED_AZUL, HIGH);
    Task_LED_Azul.Start();
    Task_DemoraAntesDeVerificar.Start();  }
});

AsyncTask Task_LeerClave(100, true, []() {
  char key = keypad.getKey();
  if (key) {
    // Reinicia el temporizador de inactividad
    Task_TimeoutClave.Stop();
    Task_TimeoutClave.Start();

    if (key >= '0' && key <= '9' && posClave < 4) {
      bufferClave[posClave++] = key;
      lcd.setCursor(posClave - 1, 1);
      lcd.print("*");
    } else if (key == '#') {
      bufferClave[posClave] = '\0';
      Task_ProcesarClave.Start();
    } else if (key == '*') {
      posClave = 0;
      memset(bufferClave, 0, sizeof(bufferClave));
      lcd.clear();
      lcd.print("Ingrese clave:");
      lcd.setCursor(0, 1);
    }
  }
});

// ----------------- FUNCIONES DE ESTADO -----------------
/**
 * @brief Estado de inicio del sistema.
 * 
 * Inicializa la pantalla LCD con el mensaje para que el usuario ingrese una clave.
 * Reinicia las variables asociadas a la verificación de clave, y comienza la tarea 
 * asincrónica para capturar la entrada del teclado.
*/
void outputINICIO() {
  Serial.println("outputINICIO");
  lcd.clear();
  lcd.print("Ingrese clave:");
  lcd.setCursor(0, 1);
  posClave = 0;
  intentosClave = 0;
  memset(bufferClave, 0, sizeof(bufferClave));
  Task_LeerClave.Start();
}

/**
 * @brief Estado de bloqueo del sistema.
 * 
 * El sistema muestra en la pantalla que está bloqueado y enciende 
 * el led rojo para indicar una alerta visual.
 * En este estado, se puede ingresar '*' desde el teclado 
 * para salir del bloqueo e ir al estado inicial.
*/
void outputBLOQUEO() {
  alarmasConsecutivas = 0;
  Serial.println("outputBLOQUEO");
  lcd.clear(); lcd.print("SISTEMA BLOQUEADO");
  Task_LED_Bloqueo.Start();
}

/**
 * @brief Estado de monitoreo ambiental.
 * 
 * Se encarga de medir periódicamente la temperatura, humedad y luz del entorno. 
 * Muestra esta información en la pantalla LCD y evalúa el valor de PMV leído 
 * desde una tarjeta RFID para determinar si deben activarse otros estados, 
 * como alarmas o control térmico.
*/
void outputMON_AMB() {
  Serial.println("outputMON_AMB");
  Task_MonitoreoAmbiente.Start();
}

/**
 * @brief Estado de alarma.
 * 
 * Este estado indica una condición crítica en el ambiente: 
 * temperatura elevada (temperatura > 40) y luz baja (luz < 10).
 * Activa el led rojo de manera intermitente, sonido con buzzer 
 * y cuenta el número de veces consecutivas que se ha activado 
 * la alarma, si es mayor o igual a 3 pasa a estado de bloqueo.
 */
void outputALARMA() {
  Serial.println("outputALARMA");
  lcd.clear(); lcd.print("ALARMA");
  Task_Buzzer.Start();
  Task_LED_Alarma.Start();
  Task_Timeout_Alarma.Start();
  alarmasConsecutivas++;
}

/**
 * @brief Estado de comfort térmico alto.
 * 
 * Activa el ventilador cuando el PMV es alto (PMV > 1).
 * Después de 5 segundos, se detiene automáticamente 
 * y cambia al estado encargado del monitoreo del ambiente.
 */
void outputCOM_TERM_ALTO() {
  Serial.println("outputCOM_TERM_ALTO");
  lcd.clear(); lcd.print("Ventilador ON");
  digitalWrite(VENTILADOR_PIN, HIGH);
  Task_Timeout_ComTermAlto.Start();
}

/**
 * @brief Estado de comfort térmico bajo.
 * 
 * Reacciona a una condición de bajo PMV (PMV < -1) 
 * encendiendo un LED verde para simular una señal de confort.
 * Permanece activo durante 4 segundos antes de pasar al estado 
 * encargado del monitoreo del ambiente.
 */
void outputCOM_TERM_BAJO() {
  Serial.println("outputCOM_TERM_BAJO");
  lcd.clear(); lcd.print("Calefactor ON");
  digitalWrite(LED_VERDE, HIGH);
  Task_Timeout_ComTermBajo.Start();
}

/**
 * @brief Detiene la tarea de lectura de clave.
 * Esta función se llama al salir del estado INICIO.
*/
void leaveINICIO() {
  Task_LeerClave.Stop();
  Task_TimeoutClave.Stop();
}

/**
 * @brief Detiene la tarea de LED de bloqueo y apaga el LED rojo.
 * Esta función se invoca al salir del estado BLOQUEO.
*/
void leaveBLOQUEO() {
  Task_LED_Bloqueo.Stop(); 
  digitalWrite(LED_ROJO, LOW); 
}

/**
 * @brief Detiene la alarma, el buzzer el timeout y apaga el led rojo y buzzer.
 * Esta función se ejecuta al salir del estado ALARMA.
*/
void leaveALARMA() {
  Task_LED_Alarma.Stop();
  Task_Buzzer.Stop();
  Task_Timeout_Alarma.Stop();
  digitalWrite(LED_ROJO, LOW);
  digitalWrite(BUZZER_PIN, LOW);
}

/**
 * @brief Detiene la tarea de monitoreo ambiental.
 * Se llama al salir del estado MON_AMB.
*/
void leaveMON_AMB() {
  Task_MonitoreoAmbiente.Stop();
}

/**
 *@brief Detiene la tarea encargada de controlar el comfort térmico alto.
 * Esta función se ejecuta al salir del estado COM_TERM_ALTO.
*/
void leaveCOM_TERM_ALTO() {
  Task_Timeout_ComTermAlto.Stop();
}

/**
 * @brief Detiene la tarea encargada de controlar el comfort térmico bajo.
 * Esta función se llama al salir del estado COM_TERM_BAJO.
*/
void leaveCOM_TERM_BAJO() {
  Task_Timeout_ComTermBajo.Stop();
}

// ----------------- SETUP Y LOOP -----------------
/**
 * @brief Configura la máquina de estados del sistema.
 *
 * Define todas las transiciones entre estados, así como las funciones
 * que se ejecutan al entrar y salir de cada estado.
 */
void setupStateMachine() {
  stateMachine.AddTransition(INICIO, BLOQUEO, []() { return input == BLOCK; });
  stateMachine.AddTransition(BLOQUEO, INICIO, []() { return input == KEY; });
  stateMachine.AddTransition(INICIO, MON_AMB, []() { return input == ACCESS; });
  stateMachine.AddTransition(MON_AMB, ALARMA, []() { return input == TEMP_LUZ; });
  stateMachine.AddTransition(ALARMA, MON_AMB, []() { return input == TIMEOUT; });
  stateMachine.AddTransition(ALARMA, BLOQUEO, []() { return input == TRY; });
  stateMachine.AddTransition(MON_AMB, COM_TERM_ALTO, []() { return input == PMV_MAYOR; });
  stateMachine.AddTransition(COM_TERM_ALTO, MON_AMB, []() { return input == TIMEOUT; });
  stateMachine.AddTransition(MON_AMB, COM_TERM_BAJO, []() { return input == PMV_MENOR; });
  stateMachine.AddTransition(COM_TERM_BAJO, MON_AMB, []() { return input == TIMEOUT; });

  stateMachine.SetOnEntering(INICIO, outputINICIO);
  stateMachine.SetOnEntering(BLOQUEO, outputBLOQUEO);
  stateMachine.SetOnEntering(MON_AMB, outputMON_AMB);
  stateMachine.SetOnEntering(ALARMA, outputALARMA);
  stateMachine.SetOnEntering(COM_TERM_ALTO, outputCOM_TERM_ALTO);
  stateMachine.SetOnEntering(COM_TERM_BAJO, outputCOM_TERM_BAJO);

  stateMachine.SetOnLeaving(INICIO, leaveINICIO);
  stateMachine.SetOnLeaving(BLOQUEO, leaveBLOQUEO);
  stateMachine.SetOnLeaving(ALARMA, leaveALARMA);
  stateMachine.SetOnLeaving(MON_AMB, leaveMON_AMB);
  stateMachine.SetOnLeaving(COM_TERM_ALTO, leaveCOM_TERM_ALTO);
  stateMachine.SetOnLeaving(COM_TERM_BAJO, leaveCOM_TERM_BAJO);
}

/**
 * @brief Inicializa el sistema.
 *
 * Configura la comunicación SPI, el lector RFID, el puerto serial,
 * el LCD, el sensor DHT y los pines del sistema. Luego inicializa
 * la máquina de estados y establece el estado inicial.
 */
void setup() {
  SPI.begin();
  mfrc522.PCD_Init();
  
  Serial.begin(9600);
  lcd.begin(16, 2);
  dht.begin();

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); 
  
  pinMode(LED_ROJO, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_AZUL, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(VENTILADOR_PIN, OUTPUT);

  digitalWrite(LED_ROJO, LOW);
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_AZUL, LOW);

  setupStateMachine();
  stateMachine.SetState(INICIO, false, true);
}

/**
 * @brief Lee la entrada del usuario desde el puerto serial.
 *
 * Interpreta caracteres individuales para determinar una señal
 * de entrada válida del sistema y los convierte al tipo `Input`.
 *
 * @return Un valor del tipo Input correspondiente a la entrada leída.
 */
int readInput() {
  Input currentInput = Input::Unknown;
  if (Serial.available()) {
    char incomingChar = Serial.read();
    
    switch (incomingChar) {
      case 'T': currentInput = TIMEOUT; break;
      case 'L': currentInput = TEMP_LUZ; break;
      case 'K': currentInput = KEY; break;
      case 'B': currentInput = BLOCK; break;
      case 'A': currentInput = ACCESS; break;
      case 'Y': currentInput = TRY; break;
      case 'H': currentInput = PMV_MAYOR; break;
      case 'W': currentInput = PMV_MENOR; break;
    }
    Serial.println(incomingChar);
  }
  return currentInput;
}

/**
 * @brief Función principal de ejecución del sistema.
 *
 * Ejecuta la actualización de tareas activas.
 * Procesa las entradas para determinar transiciones de estados.
 */
void loop() {
  if (Task_MonitoreoAmbiente.IsActive()) Task_MonitoreoAmbiente.Update();
  if (Task_LED_Bloqueo.IsActive()) Task_LED_Bloqueo.Update();
  if (Task_LED_Alarma.IsActive()) Task_LED_Alarma.Update();
  if (Task_Buzzer.IsActive()) Task_Buzzer.Update();
  if (Task_Timeout_Alarma.IsActive()) Task_Timeout_Alarma.Update();
  if (Task_Timeout_ComTermAlto.IsActive()) Task_Timeout_ComTermAlto.Update();
  if (Task_Timeout_ComTermBajo.IsActive()) Task_Timeout_ComTermBajo.Update();
  if (Task_LED_Verde.IsActive()) Task_LED_Verde.Update();
  if (Task_LED_Azul.IsActive()) Task_LED_Azul.Update();
  if (Task_LeerClave.IsActive()) Task_LeerClave.Update();
  if (Task_ProcesarClave.IsActive()) Task_ProcesarClave.Update();
  if (Task_VerificarIntentos.IsActive()) Task_VerificarIntentos.Update();
  if (Task_DemoraAntesDeVerificar.IsActive()) Task_DemoraAntesDeVerificar.Update();
  if (Task_TimeoutClave.IsActive()) Task_TimeoutClave.Update();

  if (Serial.available()) {
    input = static_cast<Input>(readInput());
  }

  stateMachine.Update();
  
  input = Unknown;
}