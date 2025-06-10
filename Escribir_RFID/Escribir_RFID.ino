#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN  33  
#define SS_PIN   53 

MFRC522 mfrc522(SS_PIN, RST_PIN);

MFRC522::MIFARE_Key key;

const float PMV_TO_WRITE = 2.0;

/**
 * @brief Imprime un arreglo de bytes en formato hexadecimal por el monitor serial.
 * 
 * @param buffer Puntero al arreglo de bytes a imprimir.
 * @param bufferSize Cantidad de bytes que contiene el arreglo.
 */
void printHexArray(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

/**
 * @brief Inicializa la comunicación serie, el lector RFID y la llave de autenticación.
 */
void setup() {
  Serial.begin(9600);
  while (!Serial) { ; }
  
  SPI.begin();            
  mfrc522.PCD_Init();     

  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  Serial.println(F("Aproxime una tarjeta MIFARE Classic para escribir PMV en el bloque 4"));
}

/**
 * @brief Bucle principal del programa: detecta tarjeta, escribe y lee el valor de PMV en el bloque 4.
 */
void loop() {
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  Serial.print(F("Tarjeta detectada, UID:"));
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
  }
  Serial.println();

  byte blockAddr   = 4;
  byte trailerBlock = 7;

  MFRC522::StatusCode status;
  status = mfrc522.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A,
    trailerBlock,
    &key,
    &(mfrc522.uid)
  );
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("Fallo en PCD_Authenticate(): "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    delay(1000);
    return;
  }

  byte bufferToWrite[16];
  float tempF = PMV_TO_WRITE;
  byte *floatBytes = reinterpret_cast<byte*>(&tempF);
  for (byte i = 0; i < 4; i++) {
    bufferToWrite[i] = floatBytes[i]; 
  }

  for (byte i = 4; i < 16; i++) {
    bufferToWrite[i] = 0x00;
  }

  Serial.print(F("Escribiendo PMV (float "));
  Serial.print(PMV_TO_WRITE);
  Serial.print(F(") en el bloque "));
  Serial.print(blockAddr);
  Serial.println(F(" ..."));
  Serial.print(F("Datos a escribir (hex):"));
  printHexArray(bufferToWrite, 16);
  Serial.println();

  status = mfrc522.MIFARE_Write(blockAddr, bufferToWrite, 16);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("Error en MIFARE_Write(): "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    delay(1000);
    return;
  }
  Serial.println(F("Escritura exitosa."));

  byte bufferRead[18];
  byte sizeRead = sizeof(bufferRead);
  Serial.print(F("Leyendo bloque "));
  Serial.print(blockAddr);
  Serial.println(F(" para verificación..."));

  status = mfrc522.MIFARE_Read(blockAddr, bufferRead, &sizeRead);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("Error en MIFARE_Read(): "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    delay(1000);
    return;
  }

  Serial.print(F("Datos leídos (hex):"));
  printHexArray(bufferRead, 16);
  Serial.println();

  float *readFloatPtr = reinterpret_cast<float*>(bufferRead);
  float pmvLeido = *readFloatPtr;
  Serial.print(F("PMV leído del bloque: "));
  Serial.println(pmvLeido);

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  delay(1000);
}
