#define HB_TIMEOUT 7500 // WILL BE CHECKED 2 TIMES A SECOND

#include <Arduino.h>
#include <Wire.h>

#define DEBUG_SWITCH_PIN PA11

#define OUTPUT_PIN PB15

#define ON_LED_PIN PB12
#define ACTIVE_LED_PIN PB13
#define ERROR_LED_PIN PB14

#define PER_SDA_PIN PB11
#define PER_SCL_PIN PB10

TwoWire WirePeripheral(PER_SDA_PIN, PER_SCL_PIN);
HardwareSerial Serial1(PA10, PA9);
HardwareTimer HeartbeatTimer(TIM3);

bool debug_mode = false;
bool heartbeat_disable_reset_on_arrest = false;

char output_byte = '\0';
uint32_t last_heartbeat = 0;

void requestEvent();
void receiveEvent(int);
void heartbeatEvent();

void setup() {
  pinMode(ON_LED_PIN, OUTPUT);
  pinMode(ACTIVE_LED_PIN, OUTPUT);
  pinMode(ERROR_LED_PIN, OUTPUT);
  
  pinMode(DEBUG_SWITCH_PIN, INPUT_PULLDOWN);
  
  pinMode(OUTPUT_PIN, OUTPUT);

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

  WirePeripheral.begin(0x3F);
  WirePeripheral.onRequest(requestEvent);
  WirePeripheral.onReceive(receiveEvent);

  last_heartbeat = millis();

  HeartbeatTimer.setOverflow(2, HERTZ_FORMAT);
  HeartbeatTimer.attachInterrupt(heartbeatEvent);
  HeartbeatTimer.resume();

  digitalWrite(ON_LED_PIN, HIGH);
}

void loop() {
}

void requestEvent() {
  WirePeripheral.write(output_byte);

  if(debug_mode) {
    Serial1.print("Responded to I2C request from controller with value ");
    Serial1.println(output_byte, 16);
  }

  output_byte = '\0';
}

void receiveEvent(int how_many) {
  if(debug_mode) {
    Serial1.print("Receiving ");
    Serial1.print(how_many);
    Serial1.println(" bytes from controller.");
  }

  char command = '\0';

  while(WirePeripheral.available()) {
    if(!command) {
      command = (char) WirePeripheral.read();
    } else {
      WirePeripheral.read();
    }
  }

  if(debug_mode) {
    Serial1.print("Received command 0x");
    Serial1.println(command, 16);
  }

  switch(command) {
    case 0x00: // Ignore 0x00
      break;
    case 0x01: // Receive heartbeat 
      {
        last_heartbeat = millis();

        if(debug_mode) {
          Serial1.println("Received heartbeat.");
        }
      }
      break;
    case 0x02: // Turn valve on
      {
        digitalWrite(OUTPUT_PIN, HIGH);
        digitalWrite(ACTIVE_LED_PIN, HIGH);

        if(debug_mode) {
          Serial1.println("Turned valve on.");
        }
      }
      break;
    case 0x03: // Turn valve off 
      {
        digitalWrite(OUTPUT_PIN, LOW);
        digitalWrite(ACTIVE_LED_PIN, LOW);

        if(debug_mode) {
          Serial1.println("Turned valve off.");
        }
      }
      break;
    default:
      {
        digitalWrite(ERROR_LED_PIN, HIGH);

        if(debug_mode) {
          Serial1.print("Unknown command 0x");
          Serial1.println(command, 16);
        }
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