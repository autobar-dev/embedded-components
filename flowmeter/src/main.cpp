#define HB_TIMEOUT 7500 // WILL BE CHECKED 2 TIMES A SECOND

#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>

#define DEBUG_SWITCH_PIN PA11

#define ON_LED_PIN PB12
#define ACTIVE_LED_PIN PB13
#define ERROR_LED_PIN PB14

#define INPUT_PIN PB1

#define PER_SDA_PIN PB11
#define PER_SCL_PIN PB10

TwoWire WirePeripheral(PER_SDA_PIN, PER_SCL_PIN);
HardwareSerial Serial1(PA10, PA9);
HardwareTimer HeartbeatTimer(TIM3);

bool debug_mode = false;
bool heartbeat_disable_reset_on_arrest = false;
bool calibration_mode = false;

uint32_t volume_per_pulse = 0;

char output_buffer[4] = { 0, 0, 0, 0 };
uint32_t last_heartbeat = 0;

uint32_t total_volume = 0;
uint32_t calibration_counter = 0;

void clearTotalVolume();
void updateVolumePerPulse(uint32_t);
void requestEvent();
void receiveEvent(int);
void inputInterruptHandler();
void heartbeatEvent();

void setup() {
  pinMode(ON_LED_PIN, OUTPUT);
  pinMode(ACTIVE_LED_PIN, OUTPUT);
  pinMode(ERROR_LED_PIN, OUTPUT);
  
  pinMode(DEBUG_SWITCH_PIN, INPUT_PULLDOWN);
  pinMode(INPUT_PIN, INPUT_PULLUP);

  digitalWrite(ON_LED_PIN, LOW);
  digitalWrite(ACTIVE_LED_PIN, LOW);
  digitalWrite(ERROR_LED_PIN, LOW);

  attachInterrupt(INPUT_PIN, inputInterruptHandler, RISING);

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

  EEPROM.get(0, volume_per_pulse);

  if(debug_mode) {
    Serial1.print("Read volume per pulse from EEPROM: ");
    Serial1.println(volume_per_pulse);
  }

  if(volume_per_pulse == 0 || volume_per_pulse == 0xFFFFFFFF) {
    digitalWrite(ERROR_LED_PIN, HIGH);
    digitalWrite(ACTIVE_LED_PIN, HIGH);

    volume_per_pulse = 170;

    if(debug_mode) {
      Serial1.println("Component not calibrated. Using default value for volume per pulse (170).");
    }

    delay(2500);
    digitalWrite(ERROR_LED_PIN, LOW);
    digitalWrite(ACTIVE_LED_PIN, LOW);
  }

  WirePeripheral.begin(0x2F);
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

void clearTotalVolume() {
  total_volume = 0;

  output_buffer[0] = 0;
  output_buffer[1] = 0;
  output_buffer[2] = 0;
  output_buffer[3] = 0;
}

void updateVolumePerPulse(uint32_t new_volume_per_pulse) {
  volume_per_pulse = new_volume_per_pulse;
  EEPROM.put(0, new_volume_per_pulse);

  if(debug_mode) {
    Serial1.print("Saved volume per pulse to EEPROM: ");
    Serial1.println(volume_per_pulse);
  }
}

void requestEvent() {
  WirePeripheral.write(output_buffer, 4);

  if(debug_mode) {
    Serial1.print("Responded to I2C request from controller with value ");
    Serial1.println(total_volume);
  }
}

void receiveEvent(int how_many) {
  if(debug_mode) {
    Serial1.print("Receiving ");
    Serial1.print(how_many);
    Serial1.println(" bytes from controller.");
  }

  char command = '\0';
  String data = "";

  while(WirePeripheral.available()) {
    if(!command) {
      command = (char) WirePeripheral.read();
    } else {
      data += (char) WirePeripheral.read();
    }
  }

  if(debug_mode) {
    Serial1.print("Received command 0x");
    Serial1.println(command, 16);
  }

  switch(command) {
    case 0x00:
      break;
    case 0x01: // Receive heartbeat
      {
        last_heartbeat = millis();

        if(debug_mode) {
          Serial1.println("Received heartbeat.");
        }
      }
      break;
    case 0x02: // Reset total volume
      {
        digitalWrite(ERROR_LED_PIN, LOW);

        clearTotalVolume();

        if(debug_mode) {
          Serial1.println("Reset total volume to 0.");
        }
      }
      break;
    case 0x03: // Set volume per pulse
      {
        if(data.length() != 4) {
          if(debug_mode) {
            Serial1.println("Received invalid data for setting volume per pulse.");
          }

          digitalWrite(ERROR_LED_PIN, HIGH);
          break;
        }

        uint32_t new_volume_per_pulse = 0;
        const char* data_bytes = data.c_str();

        new_volume_per_pulse |= data_bytes[0] << 24;
        new_volume_per_pulse |= data_bytes[1] << 16;
        new_volume_per_pulse |= data_bytes[2] << 8;
        new_volume_per_pulse |= data_bytes[3];

        if(debug_mode) {
          Serial1.print("Received new volume per pulse: ");
          Serial1.println(new_volume_per_pulse);
        }

        updateVolumePerPulse(new_volume_per_pulse);
      }
      break;
    case 0x04: // Enter calibration mode
      {
        digitalWrite(ERROR_LED_PIN, HIGH);
        digitalWrite(ACTIVE_LED_PIN, HIGH);

        if(!calibration_mode) {
          calibration_mode = true;
          calibration_counter = 0;
          
          clearTotalVolume();

          if(debug_mode) {
            Serial1.println("Entered calibration mode.");
          }
        } else {
          if(debug_mode) {
            Serial1.println("Already in calibration mode.");
          }
        }
      }
      break;
    case 0x05: // Finish calibration
      {
        if(calibration_mode) {
          digitalWrite(ERROR_LED_PIN, LOW);
          digitalWrite(ACTIVE_LED_PIN, LOW);

          if(data.length() != 4) {
            if(debug_mode) {
              Serial1.println("Received invalid data for calibration.");
            }
          } else {
            uint32_t volume_calibration_input = 0;
            const char* data_bytes = data.c_str();

            volume_calibration_input |= data_bytes[0] << 24;
            volume_calibration_input |= data_bytes[1] << 16;
            volume_calibration_input |= data_bytes[2] << 8;
            volume_calibration_input |= data_bytes[3];

            uint32_t new_volume_per_pulse = ceil((double) volume_calibration_input / (double) calibration_counter);

            if(debug_mode) {
              Serial1.print("Calculated new volume per pulse: ");
              Serial1.println(new_volume_per_pulse);
            }

            updateVolumePerPulse(new_volume_per_pulse);

            calibration_mode = false;
            calibration_counter = 0;

            clearTotalVolume();

            if(debug_mode) {
              Serial1.println("Finished calibration.");
            }
          }
        } else {
          digitalWrite(ERROR_LED_PIN, HIGH);

          if(debug_mode) {
            Serial1.println("Received finish calibration command, but not in calibration mode.");
          }
        }
      }
      break;
    case 0x06: // Cancel calibration
      {
        if(calibration_mode) {
          digitalWrite(ERROR_LED_PIN, LOW);
          digitalWrite(ACTIVE_LED_PIN, LOW);

          calibration_mode = false;
          calibration_counter = 0;
          
          clearTotalVolume();

          if(debug_mode) {
            Serial1.println("Cancelled calibration.");
          }
        } else {
          digitalWrite(ERROR_LED_PIN, HIGH);

          if(debug_mode) {
            Serial1.println("Received cancel calibration command, but not in calibration mode.");
          }
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

void inputInterruptHandler() {
  total_volume += volume_per_pulse;

  if(calibration_mode) {
    calibration_counter++;
  }

  digitalWrite(ACTIVE_LED_PIN, HIGH);

  unsigned char *total_volume_bytes = (unsigned char *)&total_volume;

  output_buffer[0] = total_volume_bytes[3];
  output_buffer[1] = total_volume_bytes[2];
  output_buffer[2] = total_volume_bytes[1];
  output_buffer[3] = total_volume_bytes[0];

  digitalWrite(ACTIVE_LED_PIN, LOW);
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