#define HB_TIMEOUT 7500

#include <Arduino.h>
#include <Wire.h>

#define DEBUG_SWITCH_PIN PA11

#define ON_LED_PIN PB12
#define ACTIVE_LED_PIN PB13
#define ERROR_LED_PIN PB14

#define BUTTON_PIN PB8

#define PER_SDA_PIN PB11
#define PER_SCL_PIN PB10

TwoWire WirePeripheral(PER_SDA_PIN, PER_SCL_PIN);
HardwareSerial Serial1(PA10, PA9);
HardwareTimer HeartbeatTimer(TIM3);

bool debug_mode = false;
bool heartbeat_disable_reset_on_arrest = false;

char output_byte = '0';
bool button_state = false;
uint32_t last_heartbeat = 0;

void requestEvent();
void receiveEvent(int);
void heartbeatEvent();

void setup() {
  pinMode(ON_LED_PIN, OUTPUT);
  pinMode(ACTIVE_LED_PIN, OUTPUT);
  pinMode(ERROR_LED_PIN, OUTPUT);
  
  pinMode(DEBUG_SWITCH_PIN, INPUT_PULLDOWN);
  pinMode(BUTTON_PIN, INPUT);

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

  WirePeripheral.begin(0x1F);
  WirePeripheral.onRequest(requestEvent);
  WirePeripheral.onReceive(receiveEvent);

  last_heartbeat = millis();

  HeartbeatTimer.setOverflow(2, HERTZ_FORMAT);
  HeartbeatTimer.attachInterrupt(heartbeatEvent);
  HeartbeatTimer.resume();

  digitalWrite(ON_LED_PIN, HIGH);
}

void loop() {
  bool current_button_state = digitalRead(BUTTON_PIN);

  if(current_button_state != button_state) {
    output_byte = current_button_state ? '1' : '0';

    button_state = current_button_state;

    if(debug_mode) {
      Serial1.print("Button state changed to ");
      Serial1.print(button_state ? "on" : "off");
      Serial1.println(".");
    }
  }
}

void requestEvent() {
  WirePeripheral.write(output_byte);

  if(debug_mode) {
    Serial1.print("Responded to I2C request from controller with ");
    Serial1.println(output_byte);
  }
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