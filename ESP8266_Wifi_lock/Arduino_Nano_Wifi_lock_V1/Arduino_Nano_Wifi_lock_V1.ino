
#include <Servo.h>

Servo myservo;  // create servo object to control a servo

//////////////////////////////////// PIN VALUES //////////////////////////////////////////////////////
const int servoAnalogOut=A1; //The new Servo Pin
const int OUT_PIN = 5;
const int IN_PIN = 3;
const int IN_PIN1 = 2;
const int SERVO_PIN = 7;

unsigned int servoValue0Deg, servoValue180Deg; // Vaiables to store min and max values of servo's pot
int pos = 0;    // variable to store the servo position

int servoRead;

void setup() {
  Serial.begin(9600);
  myservo.attach(SERVO_PIN);  // attaches the servo on pin 9 to the servo object
  pinMode(IN_PIN, INPUT);
  pinMode(IN_PIN1, INPUT);
  pinMode(OUT_PIN, OUTPUT);
  calibration();
}

void loop() {
 servoRead = map(analogRead(servoAnalogOut),servoValue0Deg,servoValue180Deg, 0, 180);
 Serial.println(servoRead);
 if (digitalRead(IN_PIN) == LOW && digitalRead(IN_PIN1) == HIGH) {
    door_open();
    digitalWrite(OUT_PIN, LOW);
  }
  if (digitalRead(IN_PIN) == HIGH && digitalRead(IN_PIN1) == LOW) {
    door_lock();
    digitalWrite(OUT_PIN, HIGH);
  }
  if (servoRead < 90) {
    digitalWrite(OUT_PIN, LOW);
  }
  if (servoRead >= 90) {
    digitalWrite(OUT_PIN, HIGH);
  }
}

void calibration(){
  myservo.write(0); //set the servo to 0 position
  delay(2000); //wait for the servo to reach there
  servoValue0Deg= analogRead(servoAnalogOut); // Pot value at 0 degrees
  delay(500); //fancy delay
  myservo.write(180); //go to 180 degrees
  delay(2000); //wait for the servo to reach there
  servoValue180Deg= analogRead(servoAnalogOut); //pot value at 180 deg
  delay(500); //fancy delay
  myservo.detach(); // stop the PWM so that we can freely move the servo
  delay(1000);
}

void door_lock() {
  if (servoRead < 90) {
      pos = 0;
  }
  myservo.attach(SERVO_PIN);              // attaches the servo on pin 9 to the servo object 
   while(pos != 180)
 {
   myservo.write(pos);
   delay(10);                            // waits 12 ms for the servo to reach the position 
   pos += 1;
 }
   myservo.detach();                      // detach the servo on pin 9 to the servo object
}


void door_open() {
  if (servoRead >= 90) {
      pos = 180;
  }
   myservo.attach(SERVO_PIN);              // attaches the servo on pin 9 to the servo object
   while(pos != 0)
 {
   myservo.write(pos);                   // waits 12 ms for the servo to reach the position    
   delay(10);
   pos -= 1;    
  }
   myservo.detach();                      // detach the servo on pin 9 to the servo object   
}
