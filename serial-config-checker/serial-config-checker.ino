
#define INVERTER_POWER_GPIO_PIN GPIO10
#define ELECTRICAL_METER_DATAREQUEST_GPIO_PIN GPIO7
#define TIMEOUT 10//time in ms

#define SERIAL_BUFFER_SIZE (1024+2)
uint8_t serialBuffer[SERIAL_BUFFER_SIZE];
int size;

void setup() {
  // Initialize Serial1 (USB serial) at a baud rate of 115200
  Serial.begin(115200);

  Serial.println("Serial 1 setup.");

  Serial.println("Serial 2 setup.");
  //Alimentation for the inverter : 3.3V
  pinMode(INVERTER_POWER_GPIO_PIN, OUTPUT);
  digitalWrite(INVERTER_POWER_GPIO_PIN, HIGH);

  /**
    The A1 port is activated (start sending data) by setting 'Data Request' line high (to +5V).
    While receiving data, the requesting OSM must keep the 'Data Request' line activated (set to +5V).
    To stop receiving data the OSM has to drop the 'Data Request' line (set it to 'high impedance' mode); hence,
    the data transfer will stop immediately.
    For backward compatibility reasons, no OSM is allowed to set the 'Data Request' line low (set it to GND or 0V).

    For these reason we are using   OD_HI : Open Drain, Drives High
    3.3V when set high
    Open collector (not GND) when set down.
  **/
  pinMode(ELECTRICAL_METER_DATAREQUEST_GPIO_PIN, OD_HI);
  digitalWrite(ELECTRICAL_METER_DATAREQUEST_GPIO_PIN, LOW);

  /*
    Initialize second Serial (hardware serial) at a baud rate of 115200 (or the baud rate required by your electrical meter)
    Port settings for P1 are 115200 baud, 8N1, Xon/Xoff.
    Serial1.begin(uint32_t baud=115200, uint32_t config=SERIAL_8N1, int rxPin=-1, int txPin=-1, bool invert=false, unsigned long timeout_ms = 20000UL)
  */
  Serial1.begin(115200, SERIAL_8N1, UART_RX2, UART_TX2, false, 20000UL);

  Serial.println("Serial 2 setup.");
}



uint16_t readSerial1Until(char terminator, char *buffer, uint16_t buffer_length){
  /*
  Read Serial1 until the terminator character and up to buffer_lenght characters
  return the number of characters readed.
  */
  uint16_t nb_char_readed = 0;

  // Read characters while there is data available and buffer is not full
  while (nb_char_readed < buffer_length - 1 && Serial1.available()) {
    char c = Serial1.read();  // Read a character from Serial1

    // Check if the character matches the terminator
    if (c == terminator) {
      break;  // Stop reading if the terminator is found
    }

    // Store the character in the buffer
    buffer[nb_char_readed++] = c;

    //Wait up to 1 ms for the next character
    int i = 0;
    while(i<1000)
    {
        if(Serial1.available())
            break;
        delayMicroseconds(1);
        i++;
    }
  }

  // Add null terminator to make the buffer a valid C-string
  buffer[nb_char_readed] = '\0';

  // Return the number of characters read
  return nb_char_readed;
}




bool readOneMeterMessage() {
  /**
    Read a message up to the end.
    Each line end up with '\n'.
    The longest line is in code 0-0:96.13.0.255, up to 1024 character so serialBuffer need to be 1026 characters.
  */

  //request data from the meter
  digitalWrite(ELECTRICAL_METER_DATAREQUEST_GPIO_PIN, HIGH);
  bool message_ended = false;
  uint16_t len = 0;

  if (Serial.available()) {
    // meter start sending data, stop the request (we only want one message)
    digitalWrite(ELECTRICAL_METER_DATAREQUEST_GPIO_PIN, LOW);
    while(!message_ended){

      memset(serialBuffer, 0, sizeof(serialBuffer));
      len = readSerial1Until('\n', (char*)serialBuffer, SERIAL_BUFFER_SIZE);
      if (len < SERIAL_BUFFER_SIZE){
        serialBuffer[len] = '\n';
        serialBuffer[len + 1] = 0;
      }

    }
  }
}


void loop() {
  // Check if there is data available on Serial2
  if (Serial1.available()) {
    // Read data from Serial2
    char incomingByte = Serial1.read();

    // Write the data to Serial1
    Serial.write(incomingByte);
  }

  // size = Serial.read(serialBuffer,TIMEOUT);
  // if(size)
  // {
  //   Serial.printf("rev data size %d : ",size);
  //   Serial.write(serialBuffer,size);
  //   // Serial.write("\r\n");
  // }
}
