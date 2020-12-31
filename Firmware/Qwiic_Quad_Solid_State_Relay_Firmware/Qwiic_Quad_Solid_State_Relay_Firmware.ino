
/*
  An I2C based KeyPad
  By: Elias Santistevan
  SparkFun Electronics
  Date: October 2018
  License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

  Feel like supporting our work? Buy a board from SparkFun!
	<Product Page Here>

  To install support for ATtiny84 in Arduino IDE: https://github.com/SpenceKonde/ATTinyCore/blob/master/Installation.md
  This core is installed from the Board Manager menu
  This core has built in support for I2C S/M and serial
  If you have Dave Mellis' ATtiny installed you may need to remove it from \Users\xx\AppData\Local\Arduino15\Packages

  To support 400kHz I2C communication reliably ATtiny84 needs to run at 8MHz. This requires user to
  click on 'Burn Bootloader' before code is loaded.
*/

#include <Wire.h>

#include <EEPROM.h>

#define LOCATION_I2C_ADDRESS 0x01 //Location in EEPROM where the I2C address is stored
#define I2C_ADDRESS_DEFAULT 8 //0x08 Default address
#define I2C_ADDRESS_JUMPER 9 //0x09 Address if address jumper is closed

#define COMMAND_CHANGE_ADDRESS 0xC7

#define NUM_RELAYS 4

#define RELAY_ONE 0
#define RELAY_TWO 1
#define RELAY_THREE 2
#define RELAY_FOUR 3

// Commands that toggle the relays.
#define RELAY_ONE_TOGGLE 0x01
#define RELAY_TWO_TOGGLE 0x02
#define RELAY_THREE_TOGGLE 0x03
#define RELAY_FOUR_TOGGLE 0x04
#define RELAY_ONE_PWM 0x10
#define RELAY_TWO_PWM 0x11
#define RELAY_THREE_PWM 0x12
#define RELAY_FOUR_PWM 0x13

#define SLOW_PWM_FREQUENCY 1
#define SLOW_PWM_MS SLOW_PWM_FREQUENCY * 1000 //Time for one full PWM cycle
#define SLOW_PWM_RESOLUTION 120.0
#define SLOW_PWM_STEP_TIME SLOW_PWM_MS / SLOW_PWM_RESOLUTION // Time per LSB in ms
#define TURN_ALL_OFF 0xA
#define TURN_ALL_ON 0xB
#define TOGGLE_ALL 0xC

// Commands to request the state of the relay, whether it is currently on or
// off.
#define RELAY_STATUS_ONE  0x05
#define RELAY_STATUS_TWO  0x06
#define RELAY_STATUS_THREE 0x07
#define RELAY_STATUS_FOUR 0x08

// Request firmware version
#define RELAY_FIRMWARE_VERSION_REQUEST 0x30

// Messages for requests that state whether a relay is on or off.
#define RELAY_IS_ON 0xF
#define RELAY_IS_OFF 0x0

volatile uint8_t setting_i2c_address = I2C_ADDRESS_DEFAULT;
volatile uint8_t COMMAND = 0;  //Variable for incoming I2C commands

const uint8_t addrPin = 7;
const uint8_t resetPin = 11;
const uint8_t firmwareVersion = 0x02;

// This 'status' array keeps track of the state of the relays.
uint8_t status[] = {RELAY_IS_OFF, RELAY_IS_OFF, RELAY_IS_OFF, RELAY_IS_OFF};
uint8_t pwmValues[] = {0, 0, 0, 0};
uint8_t relayPins[] = {RELAY_ONE, RELAY_TWO, RELAY_THREE, RELAY_FOUR};
uint32_t pwmOffTime[] = {0, 0, 0, 0};
uint32_t startTime = 0;
uint32_t now = 0;
uint32_t elapsed = 0;
bool beginPWM = true;

void setup(void)
{
  pinMode(addrPin, INPUT_PULLUP); //Default HIGH = 0x6D or 109
  pinMode(resetPin, INPUT_PULLUP);

  for (uint8_t relayNum = 0; relayNum < NUM_RELAYS; relayNum++) {
    pinMode(relayPins[relayNum], OUTPUT); //Set relay pins as output
  }

  readSystemSettings(); //Load all system settings from EEPROM
  startI2C(); //Determine the I2C address we should be using and begin listening on I2C bus
}

void loop(void) {
  now = millis();
  for (uint8_t relayNum = 0; relayNum < NUM_RELAYS; relayNum++)
  {
    if ((pwmValues[relayNum] != 0) && (pwmOffTime[relayNum] == 0))
    {
      if (beginPWM == true)
      {
        startTime = now;
        beginPWM = false;
      }
      digitalWrite(relayNum, HIGH);
      status[relayNum] = RELAY_IS_ON;
      pwmOffTime[relayNum] = startTime + (pwmValues[relayNum] * SLOW_PWM_STEP_TIME);
    }
  }
  // Only continue if we're in a PWM processing cycle
  if (beginPWM == true) return;
  
  elapsed = now - startTime;
  for (uint8_t relayNum = 0; relayNum < NUM_RELAYS; relayNum++)
  {
    // If this relay is using PWM and it's time to turn this one off.
    if (pwmValues[relayNum] > 0 && 
        (SLOW_PWM_STEP_TIME * pwmValues[relayNum]) < elapsed)
    {
      digitalWrite(relayPins[relayNum], LOW);
      status[relayNum] = RELAY_IS_OFF;
    }
  }
  if (elapsed >= SLOW_PWM_MS)
  {
    beginPWM = true;
    for (uint8_t relayNum = 0; relayNum < NUM_RELAYS; relayNum++)
    {
      pwmOffTime[relayNum] = 0;
    }
  }
}

void receiveEvent(int numberOfBytesReceived)
{
  while ( Wire.available() ) {
    //Record uint8_ts to local array
    COMMAND = Wire.read();

    if (COMMAND == COMMAND_CHANGE_ADDRESS) { //Set new I2C address
      if (Wire.available()) {
        setting_i2c_address = Wire.read();

        //Error check
        if (setting_i2c_address < 0x08 || setting_i2c_address > 0x77)
          continue; //Command failed. This address is out of bounds.

        EEPROM.write(LOCATION_I2C_ADDRESS, setting_i2c_address);

        //Our I2C address may have changed because of user's command
        startI2C(); //Determine the I2C address we should be using and begin listening on I2C bus
      }
    }

    else if (COMMAND >= RELAY_ONE_TOGGLE && COMMAND <= RELAY_FOUR_TOGGLE) {
      int relay = COMMAND - RELAY_ONE_TOGGLE;
      // IF we are using PWM, toggling always turns off.
      if (pwmValues[relay] == 0 && status[relay] == RELAY_IS_OFF) {
        digitalWrite(relayPins[relay], HIGH);
        status[relay] = RELAY_IS_ON;
      }
      else if (pwmValues[relay] > 0 || status[relay] == RELAY_IS_ON ) {
        digitalWrite(relayPins[relay], LOW);
        status[relay] = RELAY_IS_OFF;
      }
      pwmValues[relay] = 0;
    }

    else if (COMMAND >= RELAY_ONE_PWM && COMMAND <= RELAY_FOUR_PWM) {
      int relay = COMMAND - RELAY_ONE_PWM;
      if (Wire.available())
      {
        pwmValues[relay] = Wire.read();
        if (pwmValues[relay] == 0.0) {
          digitalWrite(relayPins[relay], LOW);
          status[relay] = RELAY_IS_OFF;
        }
      }
    }

    // Turn all relays off, record all of their states.
    else if (COMMAND == TURN_ALL_OFF) {
      for (int i = 0; i < NUM_RELAYS; i++) {
        digitalWrite(relayPins[i], LOW);
        status[i] = RELAY_IS_OFF;
        pwmValues[i] = 0;
      }
    }

    // Turn it up to ELEVEN, all on! Record their states to the status array.
    else if (COMMAND == TURN_ALL_ON) {
      for (int i = 0; i < NUM_RELAYS; i++) {
        digitalWrite(relayPins[i], HIGH);
        status[i] = RELAY_IS_ON;
        pwmValues[i] = 0;
      }
    }

    // This command will put each relay into it's opposite state: relays that
    // are on will be turned off and vice versa.
    else if (COMMAND == TOGGLE_ALL) {
      for (int i = 0; i < NUM_RELAYS; i++) {
        if (status[i] == RELAY_IS_ON) {
          // Remember that the relays are on pins 0-4, which aligns with the
          // status array. With that, I'll be using the iterator 'i' to write
          // the pins HIGH and LOW.
          digitalWrite(relayPins[i], LOW);
          status[i] = RELAY_IS_OFF;
          pwmValues[i] = 0;
          continue;
        }
        //Would be an else statement, but for clarity....
        else if (status[i] == RELAY_IS_OFF) {
          digitalWrite(relayPins[i], HIGH);
          status[i] = RELAY_IS_ON;
          pwmValues[i] = 0;
        }
      }
    }

  }
}

void requestEvent()
{
  if (COMMAND >= RELAY_STATUS_ONE && COMMAND <= RELAY_STATUS_FOUR) {
    int relay = COMMAND - RELAY_STATUS_ONE;
    Wire.write(status[relay]);
  } else if (COMMAND >= RELAY_ONE_PWM && COMMAND <= RELAY_FOUR_PWM) {
    int relay = COMMAND - RELAY_ONE_PWM;
    Wire.write(pwmValues[relay]);
  } else if (COMMAND == RELAY_FIRMWARE_VERSION_REQUEST) {
    Wire.write(firmwareVersion);
  }
}

void readSystemSettings(void)
{
  //Read what I2C address we should use
  setting_i2c_address = EEPROM.read(LOCATION_I2C_ADDRESS);
  if (setting_i2c_address == 255) {
    setting_i2c_address = I2C_ADDRESS_DEFAULT; //By default, we listen for I2C_ADDRESS_DEFAULT
    EEPROM.write(LOCATION_I2C_ADDRESS, setting_i2c_address);
  }
}

//Begin listening on I2C bus as I2C slave using the global variable setting_i2c_address
void startI2C()
{
  Wire.end(); //Before we can change addresses we need to stop

  if (digitalRead(addrPin) == LOW) //Default HIGH
    Wire.begin(setting_i2c_address); //Start I2C and answer calls using address from EEPROM
  else
    Wire.begin(I2C_ADDRESS_JUMPER);//Closed jumper is LOW

  //The connections to the interrupts are severed when a Wire.begin occurs. So re-declare them.
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
}
