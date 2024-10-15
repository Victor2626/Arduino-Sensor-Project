#include <SPI.h>
#include <mcp2515.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

// CAN bus setup
struct can_frame canMsg;
MCP2515 mcp2515(10);
unsigned long LastReceived = 0;

// LCD setup
LiquidCrystal_I2C lcd(0x27, 20, 4); // I2C address and screen size: 20 columns, 4 rows

// Keypad setup
const byte ROWS = 4; // four rows
const byte COLS = 3; // three columns
char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};
byte rowPins[ROWS] = {5, 4, 3, 2}; // connect to the row pinouts of the keypad
byte colPins[COLS] = {9, 7, 6};    // connect to the column pinouts of the keypad

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Login system
String inputUser = "";
String inputPassword = "";
String correctUser = "01";
String correctPassword = "1234";
bool userEntry = true; // true for user, false for password
bool loggedIn = false; // Track if the user has successfully logged in

// Variables to store previous sensor values
String prevLight = " ";
int prevGas = -1;
int prevTemperature = -200;
int prevHumidity = -1;

void setup() {
  // Initialize CAN bus
  Serial.begin(9600);
  mcp2515.reset();
  mcp2515.setBitrate(CAN_125KBPS);
  mcp2515.setNormalMode();

  // Initialize the LCD
  Wire.begin();
  lcd.begin(20, 4, LCD_5x8DOTS); // Correct LCD initialization
  lcd.backlight();

  // Print the login prompt on the LCD
  lcd.setCursor(0, 0);
  lcd.print("User:");
  lcd.setCursor(0, 1);
}

void loop() {
  if (!loggedIn) {
    // Handle user and password input
    handleLogin();
  } else {
    // Once logged in, continue displaying CAN bus data
    displayCANData();
  }
}
void handleLogin() {
  char key = keypad.getKey(); //to enter the user and password at every reset
  if (key) {
    if (key == '#') {
      if(userEntry){ ///to enter the user on the second row
        userEntry = false;
        lcd.setCursor(0, 2);
        lcd.print("Password:");
        lcd.setCursor(0, 3);
  } else {
  

  if(inputUser == correctUser && inputPassword == correctPassword){
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Access Granted!");
    delay(1000);
    lcd.clear();
    loggedIn = true;//if the user and the password are correct, then the user can see the data from the sensors
  }

  else{ //if the user or the password is wrong, then you have to write them again
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Wrong User or");
    lcd.setCursor(0, 1);
    lcd.print("Password");
    delay(2000);
    resetLogin();
  }
}
}

  else if(key == '*'){ //buttton pressed for deleting any character from user or from password
    if(userEntry){
      if(inputUser.length() > 0){
        inputUser.remove(inputUser.length() - 1);
        lcd.setCursor(0, 1);
        lcd.print("                    ");
        lcd.setCursor(0, 1);
        lcd.print(inputUser);
      }
    }
    else{
      if(inputPassword.length() > 0){
        inputPassword.remove(inputPassword.length() - 1);
        lcd.setCursor(0, 3);
        lcd.print("                    ");
        lcd.setCursor(0, 3);
        for(unsigned int i = 0; i < inputPassword.length(); i++){
        lcd.print("*");
        }
    }
  }
}
else {
      // Add character to user or password
      if (userEntry) {
        inputUser += key;
        lcd.setCursor(0, 1);
        lcd.print(inputUser);
      } else {
        inputPassword += key;
        lcd.setCursor(0, 3);
        lcd.print("                    ");
        lcd.setCursor(0, 3);
        for (unsigned int i = 0; i < inputPassword.length(); i++) {
          lcd.print("*");
        }
      }
    }
  }
}


void resetLogin(){ //function for reseting the loggin procces if the user or password are incorrect
  inputUser = "";
  inputPassword = "";
  userEntry = true;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("User:");
  lcd.setCursor(0, 1);
}

void displayCANData() {
  // Your original CAN bus data handling code
  uint8_t err = mcp2515.readMessage(&canMsg);

  if (err == MCP2515::ERROR_OK) {
    if (canMsg.can_id == 0x0F6 && canMsg.can_dlc == 8) {
      // Parse the CAN message
      String light = (canMsg.data[0] == 1) ? "Light" : "Dark"; // Light sensor value (0 or 1)
      int gasLowByte = canMsg.data[1];
      int gasHighByte = canMsg.data[2];
      int gas = gasLowByte | (gasHighByte << 8); // Combine two bytes for gas
      int temperature = canMsg.data[3];          // Temperature value
      int humidity = canMsg.data[4];             // Humidity value

      // Convert gas value to percentage
      float gasPercentage = (float)gas / 100.0;

      // Update only if values change
      if (light != prevLight) {
        lcd.setCursor(0, 0);  // First row
        lcd.print("                    ");  // Clear the row
        lcd.setCursor(0, 0);  // Set cursor back to start
        lcd.print("Light: ");
        lcd.print(light);
        prevLight = light;
      }

      if (gas != prevGas) {
        lcd.setCursor(0, 1);  // Second row
        lcd.print("                    ");  // Clear the row
        lcd.setCursor(0, 1);  // Set cursor back to start
        lcd.print("Gas: ");
        lcd.print(gasPercentage, 2);
        lcd.print(" %");
        prevGas = gas;
      }

      if (temperature != prevTemperature) {
        lcd.setCursor(0, 2);  // Third row
        lcd.print("                    ");  // Clear the row
        lcd.setCursor(0, 2);  // Set cursor back to start
        lcd.print("Temperature: ");
        lcd.print(temperature);
        lcd.print(String(char(0b11011111)) + "C");
        prevTemperature = temperature;
      }

      if (humidity != prevHumidity) {
        lcd.setCursor(0, 3);  // Fourth row
        lcd.print("                    ");  // Clear the row
        lcd.setCursor(0, 3);  // Set cursor back to start
        lcd.print("Humidity: ");
        lcd.print(humidity);
        lcd.print(" %");
        prevHumidity = humidity;
      }

      // Update timestamp for last received data
      LastReceived = millis();
    }
  } else {
    // Optional: Display CAN error on LCD
    if (millis() - LastReceived > 1500) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("CAN Error: ");
      lcd.print(err, HEX);
    }
  }
}