#include <TroykaLight.h>       // Library for the light sensor
#include <MQUnifiedsensor.h>   // Library for the gas sensor
#include <DHT11.h>             // Library for the DHT11 sensor
#include <SPI.h>
#include <mcp2515.h>

// Pin definitions
#define LIGHT_SENSOR_PIN A1    // Analog pin for the light sensor
#define MQ4_PIN A0             // Analog input pin for the MQ-4 gas sensor
#define DHT11_PIN 3            // Digital pin for the DHT11 sensor
#define BUZZER_PIN 6          // Analog pin for the passive buzzer

// Constants for the MQ-4 sensor
#define Board                   ("Arduino UNO")
#define Type                    ("MQ-4") // MQ-4 gas sensor type
#define Voltage_Resolution      (5)
#define ADC_Bit_Resolution      (10)
#define RatioMQ4CleanAir        (4.4) // RS / R0 = 60 ppm 

// Create sensor objects
TroykaLight sensorLight(LIGHT_SENSOR_PIN);
MQUnifiedsensor MQ4(Board, Voltage_Resolution, ADC_Bit_Resolution, MQ4_PIN, Type);
DHT11 dht11(DHT11_PIN);

MCP2515 mcp2515(10);
struct can_frame canMsg;

void setup() {
  // Initialize serial communication
  Serial.begin(9600);

  // Initialize the MQ-4 sensor
  initializeMQ4Sensor();

  // Initialize MCP2515 CAN controller
  mcp2515.reset();
  mcp2515.setBitrate(CAN_125KBPS);
  mcp2515.setNormalMode();
  
  // Set buzzer pin as output
  pinMode(BUZZER_PIN, OUTPUT);
  
  Serial.println("Sensors initialized and CAN bus ready.");
}

void loop() {
  // Variables for sensor data
  int temperature = 0;
  int humidity = 0;
  String lightLevel = "";
  float gasValue = 0;
  float gasPercentage = 0;

  // Read data from sensors
  lightLevel = getLightLevel();
  gasValue = getDataFromMQ4Sensor();
  gasPercentage = calculateGasPercentage(gasValue);
  getDataFromDHT11Sensor(temperature, humidity);

  // Trigger the buzzer if humidity is above 50%
  if (humidity > 70) {
    tone(BUZZER_PIN, 5000);  // Generate a 5kHz tone
  } else {
    noTone(BUZZER_PIN);      // Turn off the buzzer
  }

  // Send sensor data via CAN
  sendCanMessage(lightLevel, gasPercentage, temperature, humidity);

  // Introduce a delay between readings
  delay(500);
}

// Function to initialize the MQ-4 sensor
void initializeMQ4Sensor() {
  // Set math model to calculate the PPM concentration and the value of constants
  MQ4.setRegressionMethod(1); // _PPM =  a*ratio^b
  MQ4.setA(1012.7); 
  MQ4.setB(-2.786); // Configure the equation to calculate CH4 concentration

  // Initialize the MQ-4 sensor
  MQ4.init(); 

  // Calibrate the MQ-4 sensor
  Serial.print("Calibrating please wait.");
  float calcR0 = 0;
  for(int i = 1; i <= 10; i++) {
    MQ4.update(); // Update data, the Arduino will read the voltage from the analog pin
    calcR0 += MQ4.calibrate(RatioMQ4CleanAir);
    Serial.print(".");
  }
  MQ4.setR0(calcR0 / 10);
  Serial.println("  done!.");
  
  // Check for calibration errors
  if (isinf(calcR0)) {
    Serial.println("Warning: Connection issue, R0 is infinite (Open circuit detected), please check your wiring and supply");
    while(1);
  }
  if (calcR0 == 0) {
    Serial.println("Warning: Connection issue found, R0 is zero (Analog pin shorts to ground), please check your wiring and supply");
    while(1);
  }
  MQ4.serialDebug(true);
}

// Function to calculate the gas percentage
float calculateGasPercentage(float gasValue) {
  float minValue = 5.0;
  float maxValue = 5000000.0;

  // Calculate the percentage
  float percentage = ((gasValue - minValue) / (maxValue - minValue)) * 100.0;

  if (percentage < 0) percentage = 0;
  if (percentage > 100) percentage = 100;

  return percentage;
}

// Function to read and return the light level as a string ("dark" or "light")
String getLightLevel() {
  sensorLight.read();  // Read data from the light sensor
  float lightLux = sensorLight.getLightLux(); // Get the light level in Lux

  if (lightLux <= 10) {
    return "dark";
  } else {
    return "light";
  }
}

// Function to read and return the gas value from MQ-4 sensor
float getDataFromMQ4Sensor() {
  MQ4.update(); // Update data, the Arduino will read the voltage from the analog pin
  return MQ4.readSensor(); // Sensor will read PPM concentration using the model, a and b values set previously or from the setup
}

// Function to read temperature and humidity from the DHT11 sensor
void getDataFromDHT11Sensor(int &temperature, int &humidity) {
  int result = dht11.readTemperatureHumidity(temperature, humidity);
  if (result != 0) {
    Serial.println(DHT11::getErrorString(result));
  }
}

// Function to send sensor data via CAN bus
void sendCanMessage(String light, float gas, int temp, int humidity) {
  canMsg.can_id  = 0x0F6;
  canMsg.can_dlc = 8;

  // Convert light level to a byte (0 for dark, 1 for light)
  uint8_t lightByte = (light == "dark") ? 0x00 : 0x01;

  // Pack sensor data into CAN message
  canMsg.data[0] = lightByte;              // Light level (0 or 1)
  canMsg.data[1] = (int(gas) & 0xFF);  // Gas low byte
  canMsg.data[2] = (int(gas) >> 8) & 0xFF; // Gas high byte
  canMsg.data[3] = temp;               // Temperature
  canMsg.data[4] = humidity;           // Humidity
  canMsg.data[5] = 0x00;               // Reserved for future use
  canMsg.data[6] = 0x00;               // Reserved for future use
  canMsg.data[7] = 0x00;               // Reserved for future use

  // Send the CAN message
  if(mcp2515.sendMessage(&canMsg)==MCP2515::ERROR_OK){
    Serial.println("Sensor data sent over CAN bus.");
  }else{
    Serial.println("Message not sent");
  };

  // Print the sensor data to the serial monitor
  Serial.print("Sending CAN Message: ");
  Serial.print("Light: "); Serial.print(light);
  Serial.print(", Gas: "); Serial.print(gas);
  Serial.print(" %, Temp: "); Serial.print(temp);
  Serial.print(" C, Humidity: "); Serial.print(humidity);
  Serial.println(" %");
  Serial.println(" ");
}