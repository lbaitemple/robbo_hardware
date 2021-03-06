#include <EEPROM.h>

#define LEFT_MOTOR 9
#define RIGHT_MOTOR 5
#define FORWARD 0
#define BACKWARDS 1

#define SERIAL_ADDRESS 0
#define MOTOR_RATIO_ADDRESS 100
#define SERIAL_LENGTH 20

// EEPROM VARIABLES
// The serial number of the robot, length 50 chars, EEPROM address: 0
char robSerial[SERIAL_LENGTH];// = "000000000000000000000000000000000000000000000000000"; 
// The speed ratio as left / right, EEPROM address: 100  
float motorSpeedRatio = 1;

// MOTOR VARIBLES
int leftMotorSpeed = 255;
int rightMotorSpeed = 255;
int isEncodersEnabled = 0;
volatile int rightStepCount = 0;
volatile int leftStepCount = 0;
// Cumulative time spent on high and low (microseconds) 
volatile long rightTime[2];
volatile long leftTime[2];
// Microsecond time stamps for last two high and low transitions
volatile long rightTimeStamp[2];
volatile long leftTimeStamp[2];
// Last state
volatile bool rightIsHigh;
volatile bool leftIsHigh;

int rightStepLimit = 0;
int leftStepLimit = 0;

int ledOn = 0;

/*
 * Setup the robot.
 */
void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  // INPUTS
  pinMode(2, INPUT);
  pinMode(3, INPUT);
  // OUTPUTS
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(9, OUTPUT);
  pinMode(10, OUTPUT);
  pinMode(13, OUTPUT);

  // GET EEPROM VARIABLES
  EEPROM.get(SERIAL_ADDRESS, robSerial);
  setMotorSpeeds(255);
  
  // DEBUG EEPROM
  Serial.print("SERIAL: ");
  Serial.println(robSerial);
  
  // BLUETOOTH
  delay(1000);
  setBluetoothName();
  
  // START BUTTON
  while(digitalRead(A0) == HIGH) {
    digitalWrite(13, ledOn = !ledOn);
    delay(500);
  }
  // SET A0 LOW TO ENABLE WD TIMER HEARTBEAT 
  pinMode(A0, OUTPUT);
  digitalWrite(A0, LOW);
}

/*
 * The main loop.
 */
void loop(){
  move8();
}

void move8() {
  enableEncoders(1);
  changeMotorState(RIGHT_MOTOR, 0, 255);
  changeMotorState(LEFT_MOTOR, 0, 255);
  while(1) {
    enableEncoders(1);
    delay(400);
    enableEncoders(0);
    // right
    Serial.print(rightTime[0]);
    Serial.print("/");
    Serial.print(rightTime[1]);
    Serial.print(" ms (");
    Serial.print(rightStepCount * 0.55 / 0.4, 0);
    Serial.print(" cm/s)");
    // left
    Serial.print("\t\t");
    Serial.print(leftTime[0]);
    Serial.print("/");
    Serial.print(leftTime[1]);
    Serial.print(" ms (");
    Serial.print(leftStepCount * 0.55 / 0.4, 0);
    Serial.print(" cm/s)");
    
    Serial.println(); 
  }
}

/*
 * Sets the name of the bluetooth module RN-42 to match the serial numbet of the robot.
 */
void setBluetoothName() {
  // NOTE: max length is 20 chars
  Serial.print("$$$");
  delay(15);
  Serial.print("SN,");
  Serial.println(robSerial);
  delay(15);
  Serial.println("---");
  delay(15);
  // flush BT responses (CMD, AOK, END)
  while(Serial.available()) { 
    Serial.read();
  }
}



/*
 * Sets the speed of both motors accoring to the motorSpeedRatio.
 */
void setMotorSpeeds(int motorSpeed) {
  motorSpeed = max(min(motorSpeed, 255), 0);
  leftMotorSpeed = motorSpeedRatio >= 1 ? motorSpeed : motorSpeed * motorSpeedRatio;
  rightMotorSpeed = motorSpeedRatio <= 1 ? motorSpeed : motorSpeed / motorSpeedRatio;
}

/*
 * Enables or diasbles encoder interrupts and sets the isEncodersEnabled flag.
 */
void enableEncoders(bool doEnable) {
  if (doEnable) {
    rightStepCount = 0;
    rightTime[0] = 0;
    rightTime[1] = 0;
    rightTimeStamp[0] = 0;
    rightTimeStamp[1] = 0;
    leftStepCount = 0;
    leftTime[0] = 0;
    leftTime[1] = 0;
    leftTimeStamp[0] = 0;
    leftTimeStamp[1] = 0;
    attachInterrupt(digitalPinToInterrupt(2), incRight, CHANGE);
    attachInterrupt(digitalPinToInterrupt(3), incLeft, CHANGE);
    isEncodersEnabled = 1;
  }
  else {
    detachInterrupt(digitalPinToInterrupt(2));
    detachInterrupt(digitalPinToInterrupt(3));
    isEncodersEnabled = 0;
  }
}

/*
 * The interrupt method for the right motor. Increses step count by one.
 */
void incRight() {
  rightStepCount++;
  rightIsHigh = digitalRead(2);
  rightTimeStamp[rightIsHigh] = millis();
  if (rightStepCount > 1)
    rightTime[rightIsHigh] += rightTimeStamp[rightIsHigh] - rightTimeStamp[!rightIsHigh];
}

/*
 * The interrupt method for the left motor. Increses step count by one.
 */
void incLeft() {
  leftStepCount++;
  leftIsHigh = digitalRead(3);
  leftTimeStamp[leftIsHigh] = millis();
  if (leftStepCount > 1)
    leftTime[leftIsHigh] += leftTimeStamp[leftIsHigh] - leftTimeStamp[!leftIsHigh];
}

/*
 * Prints current step counts for both motors.
 */
void printStepCounts() {
  Serial.print(rightStepCount);
  Serial.print(",");
  Serial.println(leftStepCount);
}

/*
 * Prints the state of both encoders (HIGH/LOW) 
 */
void printEnc() {
  Serial.print("ENC: ");
  Serial.print(digitalRead(2));
  Serial.print(",");
  Serial.println(digitalRead(3));
}

/*
 * Brakes both motors by setting full hold power on for 200 ms and then releses the wheels.
 */
void holdAndReleaseBrake() {
  changeMotorState(LEFT_MOTOR, 0, -1);
  changeMotorState(RIGHT_MOTOR, 0, -1);
  // brake for 200 ms
  delay(200);
  // neutral gear
  changeMotorState(LEFT_MOTOR, 0, 0);
  changeMotorState(RIGHT_MOTOR, 0, 0);
}

/*
 * Blocks until the motors have completed the given amount of motion.
 * Brakes briefly.
 */
void waitForMotors(int timeInMs) {
  if (timeInMs > 0)
    delay(timeInMs);
  else {
    while(rightStepCount < rightStepLimit && leftStepCount < leftStepLimit){
      // wait while one of the motors reaches its step limit
    }
  }
  holdAndReleaseBrake();
  //printStepCounts();
}

/*
 * Moves the robot in straight line for a given number of steps (1 step ~ 0,5 cm).
 * Time is in ms, always positive.
 */
void moveStraightBySteps(int motorGear, int steps) {
  if (!isEncodersEnabled)
    enableEncoders(1);
  leftStepLimit = steps;
  rightStepLimit = steps;
  changeMotorState(LEFT_MOTOR, motorGear, leftMotorSpeed);
  changeMotorState(RIGHT_MOTOR, motorGear, rightMotorSpeed);  
  waitForMotors(0);
}

/*
 * Moves the robot in straight line for a given time. Time is in ms, always positive.
 */
void moveStraightByTime(int motorGear, int timeInMs) {
  if (isEncodersEnabled)
    enableEncoders(0);
  changeMotorState(LEFT_MOTOR, motorGear, leftMotorSpeed);
  changeMotorState(RIGHT_MOTOR, motorGear, rightMotorSpeed);  
  waitForMotors(timeInMs);
}

/*
 * Rotates the robot by steps (1 step ~ 5 deg). Positive is to the right, negative to the left.
 */
void rotateBySteps(int steps) {
  if (!isEncodersEnabled)
    enableEncoders(1);
  bool doRotateRight = steps > 0;
  leftStepLimit = abs(steps);
  rightStepLimit = abs(steps);
  changeMotorState(LEFT_MOTOR, !doRotateRight, leftMotorSpeed);
  changeMotorState(RIGHT_MOTOR, doRotateRight, rightMotorSpeed);
  waitForMotors(0);
}

/*
 * Rotates the robot for a given time. Time in ms, positive is to the right, negative to the left.
 */
void rotateByTime(int timeInMs) {
  if (isEncodersEnabled)
    enableEncoders(0);
  bool doRotateRight = timeInMs > 0;
  changeMotorState(LEFT_MOTOR, !doRotateRight, leftMotorSpeed);
  changeMotorState(RIGHT_MOTOR, doRotateRight, rightMotorSpeed);
  waitForMotors(abs(timeInMs));
}

/*
 * The lowest level method to contorl the motors.
 * Side effect: resets the motor's step counter when the motor is started.
 * Parameters:
 * motorSide: 
 *   LEFT / RIGHT (use defined variables)
 * motorGear: 
 *   FORWARD / BACKWARDS (use defined variables)
 * motorSpeed: 
 *   Individual motor speed in PWM (0-255). 
 *   If set to 0, the wheel is free rolling. 
 *   If set to -1, the hold power is on (brake)
 */
void changeMotorState(int motorSide, int motorGear, int motorSpeed) {
  if (motorSide == LEFT_MOTOR) {
    leftStepCount = motorSpeed > 0 ? 0 : leftStepCount;
  }
  if (motorSide == RIGHT_MOTOR) {
    rightStepCount = motorSpeed > 0 ? 0 : rightStepCount;
  }
  if (motorSpeed < 0) {
    // brakes on
    analogWrite(motorSide, 255);
    analogWrite(motorSide + 1, 255);
    return;
  }
  // motorGear = FORWARD   => g1 = 0, g2 = 1
  // motorGear = BACKWARDS => g1 = 1, g2 = 0
  int g1 =  motorGear % 2;
  int g2 = (motorGear + 1) % 2;
  analogWrite(motorSide + g1, 0);
  analogWrite(motorSide + g2, motorSpeed);
}

