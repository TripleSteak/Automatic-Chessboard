#define motorXStep 2
#define motorYStep 3
#define motorZStep 4
#define motorAStep 12

#define motorXDir 5
#define motorYDir 6
#define motorZDir 7
#define motorADir 13

#define enableMotors 8

void setup()
{
  pinMode(motorXStep, OUTPUT);
  pinMode(motorYStep, OUTPUT);
  pinMode(motorZStep, OUTPUT);
  pinMode(motorAStep, OUTPUT);

  pinMode(motorXDir, OUTPUT);
  pinMode(motorYDir, OUTPUT);
  pinMode(motorZDir, OUTPUT);
  pinMode(motorADir, OUTPUT);

  digitalWrite(8, LOW);
}

void loop()
{
  
  digitalWrite(motorXDir, HIGH); // downwards -> LOW
  digitalWrite(motorYDir, LOW); // left -> HIGH
  digitalWrite(motorZDir, LOW); // downwards -> HIGH
  digitalWrite(motorADir, HIGH);

  for (int i = 0; i < 10000; i++)
  {
    digitalWrite(motorXStep, HIGH);
    digitalWrite(motorYStep, HIGH);
    digitalWrite(motorZStep, HIGH);
    digitalWrite(motorAStep, HIGH);

    digitalWrite(motorXStep, LOW);
    digitalWrite(motorYStep, LOW);
    digitalWrite(motorZStep, LOW);
    digitalWrite(motorAStep, LOW);

    delay(1);
  }
  
  delay(10000);
}
