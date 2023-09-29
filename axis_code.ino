// Define stepper motor connections and steps per revolution:
#define dirPinX 5
#define stepPinX 2
#define dirPinY 6
#define stepPinY 3
#define dirPinZ 7
#define stepPinZ 4
#define dirPinA 13
#define stepPinA 12
#define enablePin 8
#define stepsPerRevolution 500

void setup() {
  // Declare pins as output:
  pinMode(stepPinX, OUTPUT);
  pinMode(dirPinX, OUTPUT);
  pinMode(stepPinY, OUTPUT);
  pinMode(dirPinY, OUTPUT);
  pinMode(stepPinZ, OUTPUT);
  pinMode(dirPinZ, OUTPUT);
  pinMode(stepPinA, OUTPUT);
  pinMode(dirPinA, OUTPUT);
  pinMode(enablePin, OUTPUT);
}

void loop() {
  // Set the spinning direction clockwise:
  digitalWrite(dirPinX, LOW);
  digitalWrite(dirPinY, LOW);
  digitalWrite(dirPinZ, LOW);
  digitalWrite(dirPinA, LOW);

  // Spin the stepper motor 5 revolutions fast:
  for (int i = 0; i < 50 * stepsPerRevolution; i++) {
    // These four lines result in 1 step:
    digitalWrite(stepPinX, HIGH);
    digitalWrite(stepPinY, HIGH);
    digitalWrite(stepPinZ, HIGH);
    digitalWrite(stepPinA, HIGH);
    delayMicroseconds(500);
    
    digitalWrite(stepPinX, LOW);
    digitalWrite(stepPinY, LOW);
    digitalWrite(stepPinZ, LOW);
    digitalWrite(stepPinA, LOW);
    delayMicroseconds(500);
  }

  delay(1000);
}
