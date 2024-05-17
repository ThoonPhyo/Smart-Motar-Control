#include <SoftwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <EEPROM.h>     //Inclui a biblioteca da memoria EEPROM   0=30000 510=35000  0 to 502
#include <MemoryLib.h>  //Inclui a biblioteca para gerenciar a EEPROM com variaveis dos tipos int e long
Servo myServo;          //for servo
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Sim--------------------------------------------------------------
SoftwareSerial mySerial(2, 3);

// Calibration values obtained from the sketch: volt_ac_cal---------
int adc_max = 778;  // Maximum sensor value during calibration
int adc_min = 238;  // Minimum sensor value during calibration

float volt_multi = 203;  // RMS voltage obtained from a multimeter
float volt_multi_p;      // Peak voltage
float volt_multi_n;      // Negative peak voltage
float volt_rms;
String ac_sms = "";
String water_level_sms = "";  // for water level


//Water level--------------------------------------------------------
const int LOWLEVEL = A0;  // pin that the sensor is attached to LOWLEVEL SENSOR
const int MIDLEVEL = A1;  // pin that the sensor is attached to MIDLEVEL SENSOR
const int HILEVEL = A2;   // pin that the sensor is attached to HILEVEL SENSOR
const int FULLLEVEL = 12;
const int alarm = 13;  // buzzer

const int LOWLEVELLED = 4;
const int MIDLEVELLED = 5;
const int HILEVELLED = 6;
const int FULLLEVELLED = 7;

int LowSts = 0;
int MidSts = 0;
int HiSts = 0;
int FullSts = 0;
int buzzer_count = 0;
int Wlevel = 0;

//Relay---------------------------------------------------------------
const int relay1 = 10;
const int relay2 = 9;
const int relay3 = 8;

//EEPROM for servo----------------------------------------------------
MemoryLib memory(1, 1);  //memory(sizeMemory, typeVar) : sizeMemory = KB  /  typeVar = 1=int | 2=long
long servo_data = 0;

// Button for Mod (EEPROM)
// 2= sim mod , 3 = normal mod
long Me_mod = 0;
int v = 1;
//Button for MOD -----------------------------------------------------
const int buttonPin = 0;  // input pin for a pushbutton

//alarm
int ac_alarm = 0;

// Define variables to store current mode and previous button state
int currentMode = 1;
int previousButtonState = LOW;
unsigned long lastButtonPressTime = 0;
bool modeChanging = false;

#define relay1_high digitalWrite(relay1, HIGH);  //symbolish constant
#define relay2_high digitalWrite(relay2, HIGH);
#define relay3_high digitalWrite(relay3, HIGH);

#define relay1_low digitalWrite(relay1, LOW);
#define relay2_low digitalWrite(relay2, LOW);
#define relay3_low digitalWrite(relay3, LOW);

String str = "";        //to check sms from user
String SMS_state = "";  // for motar state sms
byte state_relay3 = 0;
byte sim_relay_state = 0;
byte relay1_state = 0;
byte servo_state = 0;
byte sms_ok_state = 0;

void setup() {

  Serial.begin(9600);

  // Sim-------------------------------------------------
  mySerial.begin(9600);

  // Servo-----------------------------------------------
  myServo.attach(11);
  servo_data = memory.read(0);
  if (servo_data == 0) {
    servo_dc_off();
    servo_state = 0;
  }  // servo DC power off

  // AC Voltage
  volt_multi_p = volt_multi * 1.4142;  // Peak voltage = RMS voltage * 1.4142 (Single-phase current)
  volt_multi_n = -volt_multi_p;        // Negative peak voltage

  // Water level-----------------------------------------

  pinMode(LOWLEVEL, INPUT_PULLUP);  // Sensors
  pinMode(MIDLEVEL, INPUT_PULLUP);
  pinMode(HILEVEL, INPUT_PULLUP);
  pinMode(FULLLEVEL, INPUT_PULLUP);

  pinMode(LOWLEVELLED, OUTPUT);  // LEDs
  pinMode(MIDLEVELLED, OUTPUT);
  pinMode(HILEVELLED, OUTPUT);
  pinMode(FULLLEVELLED, OUTPUT);

  pinMode(alarm, OUTPUT);


  digitalWrite(LOWLEVELLED, LOW);
  digitalWrite(MIDLEVELLED, LOW);
  digitalWrite(HILEVELLED, LOW);
  digitalWrite(FULLLEVELLED, LOW);

  //Relays---------------------------------------------------
  pinMode(relay1, OUTPUT);
  pinMode(relay2, OUTPUT);
  pinMode(relay3, OUTPUT);

  digitalWrite(relay1, LOW);
  digitalWrite(relay2, LOW);
  digitalWrite(relay3, LOW);

  //Button for MOD -----------------------------------------
  // Set up push button pin
  pinMode(buttonPin, INPUT);
  digitalWrite(buttonPin, LOW);

  //......................................................
  lcd.init();  // initialize the lcd
  lcd.init();
  // Print a message to the LCD.
  lcd.backlight();

  AC_Sensor();
  Water_level();
  //Button_Mod();
  lcd.clear();

  if (volt_rms > 100) {
    lcd_acvolt();
    lcd_on_off_state(state_relay3);

    //sim set connection

    if (Me_mod == 1) {
      if (servo_state == 0) {  //if dc off , to on dc
        servo_dc_on();
        memory.write(0, 1);  //servo stage for on^off button
        servo_data = memory.read(0);
        servo_state = 1;
      }



      if (sim_relay_state == 0) {
        relay2_high;
        sim_relay_state = 1;  //sim on
        delay(10000);
      }
      ac_alarm = 1;
    }
  }

  else if (volt_rms < 100) {
    ac_alarm = 0;
    lcd_Wlevel();
    if (Me_mod == 1) {
      lcd.setCursor(0, 1);
      lcd.print("sim - mod");

      if (servo_data == 1) {
        if (sim_relay_state == 0) {
          relay2_high;  //sim on
          sim_relay_state = 1;
          delay(10000);
        }
      }

    } else if (Me_mod == 2) {
      lcd.setCursor(0, 1);
      lcd.print("normalmod");
    }
  }


  ac_sms = volt_rms;        // lcd ac volt
  Me_mod = memory.read(1);  //mod data
  digitalWrite(alarm, LOW);

  if (sim_relay_state == 1 && Me_mod == 1) {
    Serial.println("Initializing...");
    delay(1000);

    mySerial.println("AT");  //Once the handshake test is successful, it will back to OK
    updateSerial();
    mySerial.println("AT+CSQ");  //Signal quality test, value range is 0-31 , 31 is the best
    updateSerial();
    mySerial.println("AT+CCID");  //Read SIM information to confirm whether the SIM is plugged
    updateSerial();
    mySerial.println("AT+CREG?");  //Check whether it has registered in the network
    updateSerial();
  }
}





void loop() {

  //sim loop
  if (sim_relay_state == 1 && Me_mod == 1) { updateSerial(); }


  //====================================================   BUTTON     ==============================================================
  int buttonState = digitalRead(buttonPin);



  // Check if the button is pressed and was not pressed previously

  if (buttonState == HIGH && previousButtonState == LOW) {

    v = 0;  //for system lcd off
    lcd.clear();

    lcd.print("Mode: ");
    if (Me_mod == 1) {  //lcd.print(currentMode);
      lcd.print("sim mod");
    } else if (Me_mod == 2) {
      lcd.print("nor mod");
    }
      else if (Me_mod == 3) {
    lcd.print("aut mod");
  }
}

    if (!modeChanging) {
      modeChanging = true;
      lastButtonPressTime = millis();  // Record the time of the first press
      // Turn off LED during mode changing process
      // digitalWrite(ledPin, LOW);
    } else {
      // Increment current mode
      Me_mod = (Me_mod % 3) + 1;  //currentMode%2 = raminder+1
      // Display current mode on LCD
      lcd.setCursor(6, 0);
      lcd.print("       ");  // Clear previous mode number
      if (Me_mod == 1) {
        lcd.setCursor(6, 0);
        lcd.print("sim mod ");
        memory.write(1, 1);
        Me_mod = memory.read(1);
        // Record the time of this press
        lastButtonPressTime = millis();
      } else if (Me_mod == 2) {
        lcd.setCursor(6, 0);
        lcd.print("nor mod");
        memory.write(1, 2);
        Me_mod = memory.read(1);
        // Record the time of this press
        lastButtonPressTime = millis();
      }else if (Me_mod == 3) {
        lcd.setCursor(6, 0);
        lcd.print("aut mod");
        memory.write(1, 3);
        Me_mod = memory.read(1);
        // Record the time of this press
       lastButtonPressTime = millis();
    }

    // Delay for button debounce
    delay(100);
  }

  // Update previous button state
  previousButtonState = buttonState;

  // Check if mode changing is in progress
  if (modeChanging) {
    // Check if no button press for around 5 seconds
    if (millis() - lastButtonPressTime > 5000) {
      modeChanging = false;  // Exit mode changing process
                             // Clear LCD and display current mode
      lcd.clear();
      lcd.print("verified");
      v = 1;  // verify
      delay(500);
      // Turn on LED again
      //digitalWrite(ledPin, HIGH);
      lcd.clear();
    }
  }





  if (v == 1) {

    AC_Sensor();
    ac_sms = volt_rms;
    Water_level();
    delay(500);

    if (volt_rms > 100) {
      lcd_acvolt();
      lcd_on_off_state(state_relay3);
    } else if (volt_rms < 100) {
      lcd_Wlevel();
      if (Me_mod == 1) {
        lcd.setCursor(0, 1);
        lcd.print("sim - mod ");
      } else if (Me_mod == 2) {
        lcd.setCursor(0, 1);
        lcd.print("normal-mod");
      }
      else if (Me_mod == 3) {
        lcd.setCursor(0, 1);
        lcd.print("auto - mod");
      }
    }


    //....................Auto motar off when water full
    if (Wlevel == 100 && state_relay3 == 1) {
      relay3_low;
      state_relay3 = 0;
      lcd_on_off_state(state_relay3);

      if (Me_mod == 1) {
        send_message(water_level_sms);
        delay(4000);
        RecieveMessage();

        send_message("Motar off");  //from sim module to phone
        delay(5000);
        RecieveMessage();
      }
    }





    //'''''''''''''''''''''''''''''''''''''''''''''''''''''AC on
    if (volt_rms > 100 && Me_mod == 1) {
      lcd_acvolt();
      lcd_on_off_state(state_relay3);
      if (servo_state == 0) {  //if dc off , to on dc
        servo_dc_on();
        memory.write(0, 1);  //servo stage for on^off button
        servo_data = memory.read(0);
        servo_state = 1;
      }
      if (relay1_state == 0) {
        relay1_high;
        relay1_state = 1;
      }  // DC off protect for more v while ac come

      // sim check;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
      if (sms_ok_state == 0) {

        if (sim_relay_state == 0) {
          relay2_high;
          sim_relay_state = 1;
          delay(10000);
        }

        updateSerial();
        delay(1000);
        if (Wlevel < 100) {
          CallNumber();
          delay(6000);
        }
        send_message(" !AC Power is OK : ");  //
        delay(4000);
        RecieveMessage();
        send_message(ac_sms);
        delay(4000);
        RecieveMessage();
        send_message(water_level_sms);
        delay(4000);
        RecieveMessage();
        SMS_state = Motar_state(state_relay3);
        send_message(SMS_state);
        delay(4000);
        sms_ok_state = 1;
        RecieveMessage();
      }
      // Motar by sms..............................

      // SMS string checking
      if (mySerial.available()) {
        str = mySerial.readString();
        Serial.println(str);
      }

      // Motar-on SMS
      if (str.indexOf("Mo") > -1) {
        if (state_relay3 == 0) {
          str = "";
          relay3_high;
          state_relay3 = 1;
          lcd_on_off_state(state_relay3);
          //lcd.clear();
          send_message("OK! Motar on");  //from sim module to phone
          delay(5000);
          RecieveMessage();
        } else if (state_relay3 == 1) {
          str = "";
          send_message("Motar is already on");  //from sim module to phone
          delay(5000);
          RecieveMessage();
        }
      }

      // Motar-off SMS
      if (str.indexOf("Mf") > -1) {
        if (state_relay3 == 1) {
          str = "";
          relay3_low;
          state_relay3 = 0;
          lcd_on_off_state(state_relay3);
          // lcd.clear();
          send_message("!OK Motar off");  //from sim module to phone
          delay(5000);
          RecieveMessage();
        } else if (state_relay3 == 0) {
          str = "";
          send_message("Motar is already off");  //from sim module to phone
          delay(5000);
          RecieveMessage();
        }
      }


      if (str.indexOf("St") > -1) {  // motar state sms
        str = "";
        SMS_state = Motar_state(state_relay3);
        send_message(SMS_state);
        delay(5000);
        RecieveMessage();
      }
      if (str.indexOf("L") > -1) {  //Water level sms
        str = "";
        send_message(water_level_sms);
        delay(5000);
        RecieveMessage();
      }
      if (str.indexOf("Ac") > -1) {  //AC power sms
        str = "";
        send_message(ac_sms);
        delay(5000);
        RecieveMessage();
      }


    }  //''''''''''''''''''''''''''''''''''''''''''''''''AC off
    else if (servo_data == 1 && volt_rms < 100 && Me_mod == 1) {

      if (sms_ok_state == 0) {
        if (sim_relay_state == 0) {
          relay2_high;
          sim_relay_state = 1;
          delay(10000);
        }

        updateSerial();
        send_message("AC Power is OFF : ");
        delay(4000);
        RecieveMessage();
        send_message(water_level_sms);
        delay(4000);
        RecieveMessage();

        if (state_relay3 == 1) {
          relay3_low;
          state_relay3 == 0;
        }
        SMS_state = Motar_state(state_relay3);
        send_message(SMS_state);
        delay(4000);
        sms_ok_state = 1;
        RecieveMessage();

        relay2_low;
        sim_relay_state = 0;
        servo_dc_off();
        servo_state = 0;     // servo DC power off
        memory.write(0, 0);  //servo stage for on^off button
        servo_data = memory.read(0);
      }
    }

    //''''''''''''''''''''''''''''''''''''''''''''''''Button for Mod
    if (Me_mod == 2) {

       

      // dc on off
      if (volt_rms > 100) {

            if ( state_relay3 == 1) {
          relay3_low;
          state_relay3 = 0;
            }

        if (servo_state == 0) {  //if dc off , to on dc
          servo_dc_on();
          memory.write(0, 1);  //servo stage for on^off button
          servo_data = memory.read(0);
          servo_state = 1;
        }
        if (relay1_state == 0) {
          relay1_high;
          relay1_state = 1;
        }  // DC off protect for more v while ac come


        //sim off
        relay2_low;
        sim_relay_state = 0;
        servo_dc_off();
        servo_state = 0;     // servo DC power off
        memory.write(0, 0);  //servo stage for on^off button
        servo_data = memory.read(0);


      } else if (servo_data == 1 && volt_rms < 100) {
        servo_dc_off();
        servo_state = 0;     // servo DC power off
        memory.write(0, 0);  //servo stage for on^off button
        servo_data = memory.read(0);
        ac_alarm = 0;
      }
    
    }
  

 //''''''''''''''''''''''''''''''''''''''''''''''''''''''''''auto mod
    if (Me_mod == 3) {



      // dc on off
      if (volt_rms > 100) {
        if (servo_state == 0) {  //if dc off , to on dc
          servo_dc_on();
          memory.write(0, 1);  //servo stage for on^off button
          servo_data = memory.read(0);
          servo_state = 1;
        }
        if (relay1_state == 0) {
          relay1_high;
          relay1_state = 1;
        }  // DC off protect for more v while ac come


        //sim off
        relay2_low;
        sim_relay_state = 0;
        servo_dc_off();
        servo_state = 0;     // servo DC power off
        memory.write(0, 0);  //servo stage for on^off button
        servo_data = memory.read(0);
        
  //'''ac alarm     
      if (ac_alarm == 0) {
    
        for (int c = 0; c < 15; c++) {
          digitalWrite(alarm, HIGH);
          delay(300);
          digitalWrite(alarm, LOW);
          delay(300);
        }
        ac_alarm = 1;
  }

 //-----auto motar on
          if (Wlevel < 100 && state_relay3 == 0 && volt_rms > 160 ) {
      relay3_high;
      state_relay3 = 1;
      lcd_on_off_state(state_relay3);
      }
    else  
        if (Wlevel == 100 && buzzer_count == 0 && relay1_state == 1) {

  // water full
    digitalWrite(alarm, HIGH);
    delay(5000);
    digitalWrite(alarm, LOW);
    buzzer_count = 1;
  
  //motar off auto
          relay3_low;
      state_relay3 = 0;
      lcd_on_off_state(state_relay3);
  }


      } else if (servo_data == 1 && volt_rms < 100) {
        servo_dc_off();
        servo_state = 0;     // servo DC power off
        memory.write(0, 0);  //servo stage for on^off button
        servo_data = memory.read(0);
        ac_alarm = 0;

        //motar off to save
         if ( state_relay3 == 1) {
          relay3_low;
          state_relay3 = 0;
            }
      }
    }

    
  }  


  //water alarm
  if (Wlevel == 100 && buzzer_count == 0 && Me_mod == 2) {
    digitalWrite(alarm, HIGH);
    delay(5000);
    digitalWrite(alarm, LOW);
    buzzer_count = 1;
  }


  // ac alarm

  if (volt_rms < 100) {
    ac_alarm = 0;
  }

  if (ac_alarm == 0 && volt_rms > 100 && Me_mod == 2) {

    for (int c = 0; c < 15; c++) {
      digitalWrite(alarm, HIGH);
      delay(300);
      digitalWrite(alarm, LOW);
      delay(300);
    }
    ac_alarm = 1;
  }
 
}
