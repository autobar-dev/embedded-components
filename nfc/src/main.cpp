#define HB_TIMEOUT 7500 // WILL BE CHECKED 2 TIMES A SECOND

#include <Arduino.h>
#include <Wire.h>
#include "ST25DVSensor.h"

#define DEBUG_SWITCH_PIN PA11

#define ON_LED_PIN PB12
#define ACTIVE_LED_PIN PB13
#define ERROR_LED_PIN PB14

#define NFC_GPO_PIN PB8
#define NFC_LPD_PIN -1
#define NFC_SDA_PIN PB7
#define NFC_SCL_PIN PB6

#define PER_SDA_PIN PB11
#define PER_SCL_PIN PB10

TwoWire WirePeripheral(PER_SDA_PIN, PER_SCL_PIN);
TwoWire WireNFC(NFC_SDA_PIN, NFC_SCL_PIN);
HardwareSerial Serial1(PA10, PA9);
HardwareTimer HeartbeatTimer(TIM3);

bool debug_mode = false;
bool heartbeat_disable_reset_on_arrest = false;

byte previous_uri_protocol_id = 0x00;
byte uri_protocol_id = 0x00;

char previous_uri[128] = "";
char uri_message[128] = "";

char output_byte = '\0';
uint32_t last_heartbeat = 0;

bool writeUri(byte, String);
void requestEvent();
void receiveEvent(int);
void heartbeatEvent();
String protocolIdToString(byte);
String resultToString(int);

void setup() {
  pinMode(ON_LED_PIN, OUTPUT);
  pinMode(ACTIVE_LED_PIN, OUTPUT);
  pinMode(ERROR_LED_PIN, OUTPUT);
  
  pinMode(DEBUG_SWITCH_PIN, INPUT_PULLDOWN);

  digitalWrite(ON_LED_PIN, LOW);
  digitalWrite(ACTIVE_LED_PIN, LOW);
  digitalWrite(ERROR_LED_PIN, LOW);

  debug_mode = debug_mode || digitalRead(DEBUG_SWITCH_PIN);

  if(debug_mode) {
    for(byte i = 0; i < 3; i++) {
      digitalWrite(ACTIVE_LED_PIN, HIGH);
      delay(250);
      digitalWrite(ACTIVE_LED_PIN, LOW);
      delay(250);
    }

    heartbeat_disable_reset_on_arrest = heartbeat_disable_reset_on_arrest || digitalRead(DEBUG_SWITCH_PIN);
    
    if(heartbeat_disable_reset_on_arrest) {
      for(byte i = 0; i < 2; i++) {
        digitalWrite(ACTIVE_LED_PIN, HIGH);
        delay(250);
        digitalWrite(ACTIVE_LED_PIN, LOW);
        delay(250);
      }
    }
  }

  if(debug_mode) {
    Serial1.begin(115200);
    Serial1.println("Start");

    Serial1.println("Debug mode enabled.");
    
    if(heartbeat_disable_reset_on_arrest) {
      Serial1.println("Heartbeat reset on arrest disabled.");
    } else {
      Serial1.println("Heartbeat reset on arrest enabled.");
    }
  }

  if(st25dv.begin(NFC_GPO_PIN, -1, &WireNFC) != 0) {
    digitalWrite(ERROR_LED_PIN, HIGH);

    if(debug_mode) {
      Serial1.println("Error opening NFC module.");
    }

    while(1);
  }

  WirePeripheral.begin(0x0F);
  WirePeripheral.onRequest(requestEvent);
  WirePeripheral.onReceive(receiveEvent);

  last_heartbeat = millis();

  HeartbeatTimer.setOverflow(2, HERTZ_FORMAT);
  HeartbeatTimer.attachInterrupt(heartbeatEvent);
  HeartbeatTimer.resume();

  digitalWrite(ON_LED_PIN, HIGH);
}

void loop() {
  if(
    strcmp(uri_message, previous_uri) != 0 ||
    previous_uri_protocol_id != uri_protocol_id
  ) {
    if(debug_mode) {
      Serial1.print("URI to write changed (");
      Serial1.print(previous_uri);
      Serial1.print(" -> ");
      Serial1.print(uri_message);
      Serial1.println(").");
      Serial1.print("Protocol to write changed (0x");
      Serial1.print(previous_uri_protocol_id, 16);
      Serial1.print(" -> 0x");
      Serial1.print(uri_protocol_id, 16);
      Serial1.println(").");
    }

    bool is_successful = writeUri(uri_protocol_id, uri_message);

    if(!is_successful) {
      if(debug_mode) {
        Serial1.println("Writing URI unsuccessful.");
      }

      output_byte = 'E';
      digitalWrite(ERROR_LED_PIN, HIGH);
    } else {
      if(debug_mode) {
        Serial1.println("Writing URI successful.");
      }

      output_byte = 'K';
      digitalWrite(ERROR_LED_PIN, LOW);
    }

    if(debug_mode) {
      Serial1.print("Assigning new URI (");
      Serial1.print(uri_message);
      Serial1.print(") to previous URI (prev_uri now equals ");
      Serial1.print(previous_uri);
      Serial1.println(")...");
    }

    strcpy(previous_uri, uri_message);

    if(debug_mode) {
      Serial1.print("Assigned new URI to previous URI (prev_uri now equals ");
      Serial1.print(previous_uri);
      Serial1.println(").");
    }

    previous_uri_protocol_id = uri_protocol_id;
  }
}

bool writeUri(byte protocol_id, String uri) {
  digitalWrite(ACTIVE_LED_PIN, HIGH);

  bool is_successful = true;

  String protocol_string = protocolIdToString(protocol_id);

  if(debug_mode) {
    Serial1.print("Attempting to write URI: ");
    Serial1.print(protocol_string);
    Serial1.println(uri);
  }

  int result = st25dv.writeURI(protocol_string.c_str(), uri, "");

  if(debug_mode) {
    Serial1.print("Result: ");
    Serial1.println(
      String(result) +
      String(" - ") +
      resultToString(result)
    );
  }

  if(result > 0) {
    is_successful = false;
  }

  digitalWrite(ACTIVE_LED_PIN, LOW);

  return is_successful;
}

void requestEvent() {
  WirePeripheral.write(output_byte);

  output_byte = '\0';

  if(debug_mode) {
    Serial1.println("Responded to I2C request from controller.");
  }
}

void receiveEvent(int howMany) {
  if(debug_mode) {
    Serial1.print("Receiving ");
    Serial1.print(howMany);
    Serial1.println(" bytes from controller.");
  }

  String input = "";
  uint8_t command = '\0';
  bool command_set = false;
  bool protocol_set = false;
  bool protocol_error = false;

  while(WirePeripheral.available()) {
    char c = WirePeripheral.read();

    if(debug_mode) {
      Serial1.print("Received byte (0x");
      Serial1.print(c, 16);
      Serial1.println(") from controller.");
    }

    if(c == 0) {
      if(debug_mode) {
        Serial1.println("Received 0x00 byte. Skipping");
      }
      
      continue;
    }

    if(!command_set) {
      command = c;

      if(debug_mode) {
        Serial1.print("Received command (0x");
        Serial1.print(command, 16);
        Serial1.println(") from controller.");
      }

      command_set = true;
    } else {
      if(!protocol_set) {
        String protocol = protocolIdToString(c);

        if(protocol.length() == 0) {
          protocol_error = true;

          if(debug_mode) {
            Serial1.print("Unknown protocol: 0x");
            Serial1.println(c, 16);
          }
        } else {
          uri_protocol_id = (byte) c;
        }

        protocol_set = true;
      } else {
        input += c;
      }
    }
  }

  switch(command) {
    case 0x00: // Skip 0
      break;
    case 0x01: // Heartbeat
      if(debug_mode) {
        Serial1.println("Received heartbeat from controller.");
      }

      last_heartbeat = millis();
      break;
    case 0x02: // Write URL
      if(debug_mode) {
        Serial1.println("Received write URL command from controller.");
      }

      if(protocol_error) {
        if(debug_mode) {
          Serial1.println("There has been a protocol error.");
        }

        output_byte = 'E';
        digitalWrite(ERROR_LED_PIN, HIGH);
      } else {
        strcpy(uri_message, input.c_str());

        if(debug_mode) {
          Serial1.print("Received protocol (0x");
          Serial1.print(uri_protocol_id, 16);
          Serial1.print(") followed by text (");
          Serial1.print(input);
          Serial1.println(") from controller.");
        }
      }
      break;
    default:
      if(debug_mode) {
        Serial1.print("Unknown command: 0x");
        Serial1.println(command, 16);
      }
  }  
}

void heartbeatEvent() {
  uint32_t diff = millis() - last_heartbeat;

  if(diff > HB_TIMEOUT) {
    if(debug_mode && !heartbeat_disable_reset_on_arrest) {
      Serial1.println("Heartbeat arrest.");
    }

    if(!heartbeat_disable_reset_on_arrest) {
      if(debug_mode) {
        Serial1.println("Resetting...");
        delay(50);
      }

      digitalWrite(ERROR_LED_PIN, HIGH);
      HAL_NVIC_SystemReset();
    }
  }
}

String protocolIdToString(byte protocol) {
  switch(protocol) {
    case 0x01:
      return URI_ID_0x01_STRING;
    case 0x02:
      return URI_ID_0x02_STRING;
    case 0x03:
      return URI_ID_0x03_STRING;
    case 0x04:
      return URI_ID_0x04_STRING;
    case 0x05:
      return URI_ID_0x05_STRING;
    case 0x06:
      return URI_ID_0x06_STRING;
    case 0x07:
      return URI_ID_0x07_STRING;
    case 0x08:
      return URI_ID_0x08_STRING;
    case 0x09:
      return URI_ID_0x09_STRING;
    case 0x0A:
      return URI_ID_0x0A_STRING;
    case 0x0B:
      return URI_ID_0x0B_STRING;
    case 0x0C:
      return URI_ID_0x0C_STRING;
    case 0x0D:
      return URI_ID_0x0D_STRING;
    case 0x0E:
      return URI_ID_0x0E_STRING;
    case 0x0F:
      return URI_ID_0x0F_STRING;
    case 0x10:
      return URI_ID_0x10_STRING;
    case 0x11:
      return URI_ID_0x11_STRING;
    case 0x12:
      return URI_ID_0x12_STRING;
    case 0x13:
      return URI_ID_0x13_STRING;
    case 0x14:
      return URI_ID_0x14_STRING;
    case 0x15:
      return URI_ID_0x15_STRING;
    case 0x16:
      return URI_ID_0x16_STRING;
    case 0x17:
      return URI_ID_0x17_STRING;
    case 0x18:
      return URI_ID_0x18_STRING;
    case 0x19:
      return URI_ID_0x19_STRING;
    case 0x1A:
      return URI_ID_0x1A_STRING;
    case 0x1B:
      return URI_ID_0x1B_STRING;
    case 0x1C:
      return URI_ID_0x1C_STRING;
    case 0x1D:
      return URI_ID_0x1D_STRING;
    case 0x1E:
      return URI_ID_0x1E_STRING;
    case 0x1F:
      return URI_ID_0x1F_STRING;
    case 0x20:
      return URI_ID_0x20_STRING;
    case 0x21:
      return URI_ID_0x21_STRING;
    case 0x22:
      return URI_ID_0x22_STRING;
    case 0x23:
      return URI_ID_0x23_STRING;
    default:
      return "";
  }
}

String resultToString(int result) {
  switch(result) {
    case NDEF_OK:
      return "NDEF_OK";
    case NDEF_ERROR:
      return "NDEF_ERROR";
    case NDEF_ERROR_MEMORY_TAG:
      return "NDEF_ERROR_MEMORY_TAG";
    case NDEF_ERROR_MEMORY_INTERNAL:
      return "NDEF_ERROR_MEMORY_INTERNAL";
    case NDEF_ERROR_LOCKED:
      return "NDEF_ERROR_LOCKED";
    case NDEF_ERROR_NOT_FORMATED:
      return "NDEF_ERROR_NOT_FORMATED";
    default:
      return "Unknown";
  }
}