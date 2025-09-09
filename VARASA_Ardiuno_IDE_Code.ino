#include <SoftwareSerial.h>
#include <Servo.h>

// Serial Definitions
SoftwareSerial gsm(7, 8); // SIM900A TX -> D7, RX -> D8

// Voice module assumed to be on Serial (0,1), adjust if needed
#define voice Serial 

// Motor Control Pins
const int enA = 5;
const int enB = 6;
const int in1 = 2;
const int in2 = 3;
const int in3 = 4;
const int in4 = 9;

// Ultrasonic Sensor Pins
const int trigPin = 12;
const int echoPin = 13;

// Servo Motor
Servo myservo;
const int servoPin = 10;



const String authorizedNumbers[] = {"+91XXXXXXXXX","+91XXXXXXXXXXX"};
const int numAuthorized = 2;

bool roverBusy = false;
String lastSender = "";

void setup() {
  Serial.begin(9600);
  gsm.begin(9600); 
  voice.begin(9600);

  pinMode(enA, OUTPUT);
  pinMode(enB, OUTPUT);
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  pinMode(in3, OUTPUT);
  pinMode(in4, OUTPUT);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  myservo.attach(servoPin);

  sendATCommand("AT+CMGF=1");         // Text mode
  sendATCommand("AT+CNMI=1,2,0,0,0"); // Immediate message notification
  sendATCommand("AT+CSCS=\"GSM\"");   // Character set

  notifyUsers("Rover ready. Waiting for command.");
}

void loop() {
  if (!roverBusy) {
    if (voice.available()) {
      handleVoiceCommand();
    }

    String smsData = receiveSMS();
    if (smsData.length() > 0 && smsData.indexOf("+CMT:") != -1) {
      processSMS(smsData);
    }
  }
}

// ================== VOICE COMMAND HANDLER ==================
void handleVoiceCommand() {
  byte voiceCode = voice.read();
  Serial.print("Voice command code: ");
  Serial.println(voiceCode, HEX);

  roverBusy = true;

  switch (voiceCode) {
    case 0xAC: AB(); notifyUsers("Rover is currently free at the Analog Lab."); break;
    case 0xAE: AC(); notifyUsers("Rover is currently free at the HODCabin."); break;
    case 0xAB: BA(); notifyUsers("Rover is currently free at the DigitalLab."); break;
    case 0xAD: BC(); notifyUsers("Rover is currently free at the HODCabin."); break;
    case 0xAF: CA(); notifyUsers("Rover is currently free at the DigitalLab."); break;
    case 0xBA: CB(); notifyUsers("Rover is currently free at the Analog Lab."); break;
    case 0xFA: moveForward(10000); notifyUsers("Rover is currently free at the Analog Lab.");  break;
    case 0xFB: moveBackward(10000); notifyUsers("Rover is currently free at the Analog Lab.");  break;
    case 0xFC: turnRight(3000); notifyUsers("Rover is currently free at the Analog Lab.");  break;
    case 0xFD: turnLeft(3000); notifyUsers("Rover is currently free at the Analog Lab.");  break;
    default:
      Serial.println("Unknown voice command!");
      roverBusy = false;
      break;
  }
}

// ================== SMS HANDLING ==================
String receiveSMS() {
  String sms = "";
  long timeout = millis() + 5000;
  while (millis() < timeout) {
    while (gsm.available()) {
      sms += (char)gsm.read();
    }
  }
  sms.trim();
  Serial.println("Received SMS:\n" + sms);
  return sms;
}

void processSMS(String sms) {
  String senderNumber = extractSender(sms);
  String message = extractMessage(sms);

  Serial.println("Sender: " + senderNumber);
  Serial.println("Message: " + message);

  if (!isAuthorized(senderNumber)) {
    sendSMS(senderNumber, "Unauthorized access detected!");
    return;
  }

  if (roverBusy) {
    sendSMS(senderNumber, "Rover is currently BUSY.");
    return;
  }

  int delimiter = message.indexOf(":");
  if (delimiter != -1) {
    String currentPosition = message.substring(0, delimiter);
    String destination = message.substring(delimiter + 1);

    if (currentPosition.equals("DigitalLab") && destination.equals("AnalogLab"))
    { 
      roverBusy = false;
      lastSender = senderNumber;
      AB();
      sendSMS(senderNumber, "Reached Destination");
      notifyUsers("Rover is currently free at the Analog Lab.");
    }
    else if (currentPosition.equals("DigitalLab") && destination.equals("HODCabin")) 
    {
      roverBusy = false;
      lastSender = senderNumber;
      AC();
      sendSMS(senderNumber, "Reached Destination");
      notifyUsers("Rover is currently free at the HODCabin.");
    }
    else if (currentPosition.equals("AnalogLab") && destination.equals("DigitalLab")) 
    {
      roverBusy = false;
      lastSender = senderNumber;
      BA();
      sendSMS(senderNumber, "Reached Destination");
      notifyUsers("Rover is currently free at the DigitalLab.");
    }
    else if (currentPosition.equals("AnalogLab") && destination.equals("HODCabin")) 
    {
      roverBusy = false;
      lastSender = senderNumber;
      BC();
      sendSMS(senderNumber, "Reached Destination");
      notifyUsers("Rover is currently free at the HODCabin.");
    }
    else if (currentPosition.equals("HODCabin") && destination.equals("DigitalLab")) 
    {
      roverBusy = false;
      lastSender = senderNumber;
      CA();
      sendSMS(senderNumber, "Reached Destination");
      notifyUsers("Rover is currently free at the DigitalLab.");
    }
    else if (currentPosition.equals("HODCabin") && destination.equals("AnalogLab"))
    {
      roverBusy = false;
      lastSender = senderNumber;
       CB();
       sendSMS(senderNumber, "Reached Destination");
       notifyUsers("Rover is currently free at the Analog Lab.");
    }
    else sendSMS(senderNumber, "Invalid destination format.");
  } else {
    sendSMS(senderNumber, "Invalid message format. Use: CurrentPosition:Destination");
  }
}



// ================== OBSTACLE AVOIDANCE ==================
void checkObstacleWhileMoving() {
  int distance = readUltrasonicDistance();
  if (distance < 35) {
    stopMotors();
    Serial.println("Obstacle detected. Scanning surroundings...");
    int freeSpace = 0;
    moveServo(30);
    for (int angle = 30; angle <= 150; angle += 20) {
      moveServo(angle);
      delay(220);
      int d = readUltrasonicDistance();

      Serial.print("Angle: ");
      Serial.print(angle);
      Serial.print(" degrees, Distance: ");
      Serial.print(d);
      Serial.println(" cm");

      if (d > 35) freeSpace += 20;
    }

    if (freeSpace <= 90)
    { Serial.println("Free Space at Left");
      turnRight(3000);
      moveForward(500);
      turnLeft(2000);
    }
    else if(freeSpace>90 && freeSpace<=180)
    { Serial.println("Free Space at Right");
      turnLeft(3000);
      moveForward(500);
      turnRight(2000);
    }
    else{
     stopMotors();
    }

    moveForward(500);

  }
  moveServo(90);
}

// ================== MOVEMENT ==================
void moveServo(int angle) {
  myservo.write(angle);
  delay(500);
}

int readUltrasonicDistance() {
  digitalWrite(trigPin, LOW); delayMicroseconds(2);
  digitalWrite(trigPin, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  return duration * 0.034 / 2;
}

void moveForward(int duration) {
  analogWrite(enA, 255); 
  analogWrite(enB, 255);
  digitalWrite(in1, HIGH); 
  digitalWrite(in2, LOW);
  digitalWrite(in3, HIGH); 
  digitalWrite(in4, LOW);

  unsigned long start = millis();
  while (millis() - start < duration) {
    checkObstacleWhileMoving();
    delay(50);  // small delay to prevent tight looping, adjust as needed
  }
  stopMotors();
}


void turnLeft(int duration) {
  analogWrite(enA, 255); analogWrite(enB, 255);
  digitalWrite(in1, LOW); digitalWrite(in2, HIGH);
  digitalWrite(in3, HIGH); digitalWrite(in4, LOW);
  delay(duration);
  stopMotors();
}

void turnRight(int duration) {
  analogWrite(enA, 255); analogWrite(enB, 255);
  digitalWrite(in1, HIGH); digitalWrite(in2, LOW);
  digitalWrite(in3, LOW); digitalWrite(in4, HIGH);
  delay(duration);
  stopMotors();
}

void moveBackward(int duration) {
  analogWrite(enA, 255); analogWrite(enB, 255);
  digitalWrite(in1, LOW); digitalWrite(in2, HIGH);
  digitalWrite(in3, LOW); digitalWrite(in4, HIGH);
  delay(duration);
  stopMotors();
}

void stopMotors() {
  analogWrite(enA, 0); analogWrite(enB, 0);
  digitalWrite(in1, LOW); digitalWrite(in2, LOW);
  digitalWrite(in3, LOW); digitalWrite(in4, LOW);
}

// ================== PATHS ==================
void AB() {
    Serial.println("Moving from the Digital Lab---->Analog Lab");
    moveForward(7000);
    Serial.println("Moving from the Digital Lab---->Analog Lab");
    turnRight(3000);
    Serial.println("Moving from the Digital Lab---->Analog Lab");
    moveForward(5000);
    Serial.println("Moving from the Digital Lab---->Analog Lab");
    turnRight(3000);
    Serial.println("Moving from the Digital Lab---->Analog Lab");
    moveForward(6000);
    Serial.println("Moving from the Digital Lab---->Analog Lab");
    Serial.println("Arrived at Analog Lab.");
}

void AC() {
    Serial.println("Moving from the Digital Lab---->HOD Cabin");
    moveForward(7000);
    turnRight(3000);
    moveForward(35000);
    turnLeft(3000);
    moveForward(6000);
    turnRight(3000);
    moveForward(6000);
    turnRight(8000);
    Serial.println("Arrived at HOD Cabin.");
}

void BA() {
    Serial.println("Moving from the Analog Lab---->Digital Lab");
    moveForward(15000);
    turnLeft(4500);
    moveForward(6000);
    //turnRight(8000);
    Serial.println("Arrived at Digital Lab.");
}

void BC() {
    Serial.println("Moving from the Analog Lab---->HOD Cabin");
    moveForward(7000);
    moveForward(5000);
    turnLeft(4500);
    Serial.println("Arrived at HOD Cabin.");
}

void CA() {
    Serial.println("Moving from the HOD Cabin---->Digital Lab");
    moveForward(7000);
    Serial.println("Moving from the HOD Cabin---->Digital Lab");
    turnLeft(3000);
    Serial.println("Moving from the HOD Cabin---->Digital Lab");
    moveForward(6000);
    Serial.println("Moving from the HOD Cabin---->Digital Lab");
    turnRight(3000);
    Serial.println("Moving from the HOD Cabin---->Digital Lab");
    moveForward(5000);
    Serial.println("Moving from the HOD Cabin---->Digital Lab");
    turnLeft(3000);
    Serial.println("Moving from the HOD Cabin---->Digital Lab");
    moveForward(6000);
    Serial.println("Moving from the HOD Cabin---->Digital Lab");
    turnRight(8000);
    Serial.println("Moving from the HOD Cabin---->Digital Lab");
    Serial.println("Arrived at Digital Lab.");
}

void CB() {
    Serial.println("Moving from the HOD Cabin---->Analog Lab");
    moveForward(7000);
    turnLeft(3000);
    moveForward(25000);
    turnRight(8000);
    Serial.println("Arrived at Analog Lab.");
}

// ================== GSM UTILS ==================
void sendSMS(String phoneNumber, String message) {
  sendATCommand("AT+CMGF=1");
  gsm.print("AT+CMGS=\""); gsm.print(phoneNumber); gsm.println("\"");
  delay(500);
  gsm.print(message);
  gsm.write(26); // Ctrl+Z
  delay(3000);
}

void notifyUsers(String message) {
  for (int i = 0; i < numAuthorized; i++) {
    sendSMS(authorizedNumbers[i], message);
    delay(5000);
  }
}

void sendATCommand(String command) {
  gsm.println(command);
  delay(1000);
}

// ================== UTILS ==================
String extractSender(String sms) {
  int idx = sms.lastIndexOf("+CMT: \"");
  if (idx == -1) return "";
  int start = idx + 7;
  int end = sms.indexOf("\"", start);
  return (end == -1) ? "" : sms.substring(start, end);
}

String extractMessage(String sms) {
  int idx = sms.lastIndexOf("+CMT: \"");
  if (idx == -1) return "";

  int start = sms.indexOf("\n", idx);
  if (start == -1) return "";

  String msg = sms.substring(start + 1);
  msg.trim();  // now valid
  return msg;
}


bool isAuthorized(String number) {
  for (int i = 0; i < numAuthorized; i++) {
    if (number == authorizedNumbers[i]) return true;
  }
  return false;
}