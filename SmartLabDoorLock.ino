#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>
#include <Servo.h>
#include <DHT.h>

#define OLED_ADDRESS 0x3C
#define GREEN_LED 12
#define BUZZER_PIN 13
#define MQ2_PIN A0
#define SERVO_PIN A1
#define RED_LED A2
#define DHT_PIN A3
#define DOOR_LOCK 0
#define DOOR_OPEN 90
#define MQ2_THRESHOLD 450
#define AUTO_LOCK_TIME 5000
#define DHT_INTERVAL 2000
#define OLED_INTERVAL 250
#define ENTRY_TIMEOUT 8000
#define DENIED_TIME 1500
#define WHT SSD1306_WHITE
#define BLK SSD1306_BLACK

Adafruit_SSD1306 display(128, 64, &Wire, -1);
DHT dht(DHT_PIN, DHT22);
Servo doorServo;
const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'}, {'4','5','6','B'},
  {'7','8','9','C'}, {'*','0','#','D'}
};
byte rowPins[ROWS] = {4, 5, 6, 7};
byte colPins[COLS] = {8, 9, 10, 11};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
const char password[] = "1234";
char inputPassword[5];
byte inputLength = 0, wrongAttempts = 0;
bool doorOpen = false, securityAlarm = false, gasAlarm = false;
int gasValue = 0;
float temperature = 0, humidity = 0;
unsigned long doorTimer = 0, lastDHT = 0, lastOLED = 0;
unsigned long lastKey = 0, deniedUntil = 0;

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  digitalWrite(RED_LED, HIGH);
  dht.begin();
  doorServo.attach(SERVO_PIN);
  doorServo.write(DOOR_LOCK);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    while (1) {
      digitalWrite(RED_LED, !digitalRead(RED_LED));
      delay(300);
    }
  }
  showSplash();
}
void loop() {
  readSensors();
  readKeypad();
  if (doorOpen && millis() - doorTimer >= AUTO_LOCK_TIME) lockDoor();
  if (inputLength > 0 && millis() - lastKey >= ENTRY_TIMEOUT) clearPassword();
  updateAlarm();
  if (millis() - lastOLED >= OLED_INTERVAL) {
    updateOLED();
    lastOLED = millis();
  }
}
void readSensors() {
  gasValue = analogRead(MQ2_PIN);
  gasAlarm = gasValue >= MQ2_THRESHOLD;
  if (millis() - lastDHT >= DHT_INTERVAL) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t)) temperature = t;
    if (!isnan(h)) humidity = h;
    lastDHT = millis();
  }
}
void readKeypad() {
  char key = keypad.getKey();
  if (!key) return;
  if (securityAlarm) {
    if (key == 'D') {
      securityAlarm = false;
      wrongAttempts = 0;
    }
    return;
  }
  if (gasAlarm) return;
  if (doorOpen) {
    if (key == 'B') lockDoor();
    return;
  }
  lastKey = millis();
  lastOLED = 0;
  if (key == '*') {
    clearPassword();
  } else if (key >= '0' && key <= '9' && inputLength < 4) {
    inputPassword[inputLength++] = key;
    inputPassword[inputLength] = '\0';
  } else if (key == '#' && inputLength > 0) {
    if (strcmp(inputPassword, password) == 0) openDoor();
    else wrongPassword();
  }
}
void clearPassword() {
  inputLength = 0;
  inputPassword[0] = '\0';
}
void openDoor() {
  wrongAttempts = 0;
  doorServo.write(DOOR_OPEN);
  doorOpen = true;
  doorTimer = millis();
  clearPassword();
}
void lockDoor() {
  doorServo.write(DOOR_LOCK);
  doorOpen = false;
}
void wrongPassword() {
  wrongAttempts++;
  clearPassword();
  deniedUntil = millis() + DENIED_TIME;
  updateOLED();
  for (byte i = 0; i < 3; i++) {
    tone(BUZZER_PIN, 2000);
    delay(100);
    noTone(BUZZER_PIN);
    delay(80);
  }
  if (wrongAttempts >= 3) securityAlarm = true;
}
void updateAlarm() {
  bool alarm = securityAlarm || gasAlarm;
  digitalWrite(RED_LED, alarm || !doorOpen);
  digitalWrite(GREEN_LED, doorOpen && !alarm);
  if (alarm) tone(BUZZER_PIN, 2000);
  else noTone(BUZZER_PIN);
}
void drawLock(int x, int y, byte s, uint16_t c) {
  uint16_t k = (c == WHT) ? BLK : WHT;
  for (byte t = 0; t < s; t++) {
    display.drawRoundRect(x+s+t, y+t, 6*s-2*t, 8*s-2*t, 2*s-t, c);
  }
  display.fillRect(x, y+5*s, 8*s, 6*s, c);
  display.fillRect(x+4*s-s/2, y+7*s, s, 2*s, k);
}
void drawCheck(int cx, int cy, int r, uint16_t c) {
  display.drawCircle(cx, cy, r, c);
  display.drawCircle(cx, cy, r-1, c);
  int a=r/2, b=r/3, m=r/6;
  for (int d=0; d<2; d++) {
    display.drawLine(cx-a, cy+d, cx-m, cy+b+d, c);
    display.drawLine(cx-m, cy+b+d, cx+a, cy-b+d, c);
  }
}
void drawCross(int cx, int cy, int r, uint16_t c) {
  display.drawCircle(cx, cy, r, c);
  display.drawCircle(cx, cy, r-1, c);
  int a=r/2;
  for (int d=-1; d<=0; d++) {
    display.drawLine(cx-a+d, cy-a, cx+a+d, cy+a, c);
    display.drawLine(cx+a+d, cy-a, cx-a+d, cy+a, c);
  }
}
void drawWarning(int x, int y, uint16_t c) {
  uint16_t k=(c==WHT)?BLK:WHT;
  display.fillTriangle(x+12,y,x,y+21,x+24,y+21,c);
  display.fillRect(x+11,y+7,2,7,k);
  display.fillRect(x+11,y+16,2,2,k);
}
void drawThermo(int x, int y) {
  display.drawRoundRect(x+1,y,5,11,2,WHT);
  display.fillCircle(x+3,y+12,3,WHT);
  display.fillRect(x+3,y+4,1,8,WHT);
}
void drawDrop(int x, int y) {
  display.fillTriangle(x+4,y,x,y+8,x+8,y+8,WHT);
  display.fillCircle(x+4,y+9,4,WHT);
}
void header(const __FlashStringHelper *title, const __FlashStringHelper *right) {
  display.fillRect(0,0,128,14,WHT);
  drawLock(4,1,1,BLK);
  display.setTextColor(BLK);
  display.setTextSize(1);
  display.setCursor(18,3);
  display.print(title);
  if (right) {
    display.setCursor(88,3);
    display.print(right);
  }
  display.setTextColor(WHT);
}
void drawReading(int cx, int cy, int value, bool isTemp) {
  display.drawRoundRect(cx,cy,60,28,4,WHT);
  if (isTemp) drawThermo(cx+5,cy+6);
  else drawDrop(cx+5,cy+7);
  display.setTextSize(2);
  display.setCursor(cx+18,cy+4);
  display.print(value);
  int ux=display.getCursorX()+3;
  display.setTextSize(1);
  if (isTemp) {
    display.drawCircle(ux+1,cy+6,1,WHT);
    display.setCursor(ux+4,cy+4);
    display.print(F("C"));
  } else {
    display.setCursor(ux,cy+4);
    display.print(F("%"));
  }
  display.setCursor(cx+18,cy+20);
  display.print(isTemp?F("TEMP"):F("HUMID"));
}
void drawGasBar(int y, uint16_t c) {
  display.setTextColor(c);
  display.setTextSize(1);
  display.setCursor(2,y+2);
  display.print(F("GAS"));
  display.drawRect(24,y,74,10,c);
  int w=map(constrain(gasValue,0,1023),0,1023,0,70);
  display.fillRect(26,y+2,w,6,c);
  int t=26+(long)MQ2_THRESHOLD*70/1023;
  display.drawFastVLine(t,y-3,16,c);
  display.setCursor(102,y+2);
  display.print(gasValue);
}
void showSplash() {
  display.clearDisplay();
  display.setTextColor(WHT);
  drawLock(56,4,2,WHT);
  display.setTextSize(2);
  display.setCursor(10,30);
  display.print(F("SMART LAB"));
  display.setTextSize(1);
  display.setCursor(16,48);
  display.print(F("DOOR LOCK SYSTEM"));
  display.drawRect(16,59,96,4,WHT);
  for (byte i=0; i<=92; i+=4) {
    display.fillRect(18,60,i,2,WHT);
    display.display();
    delay(40);
  }
}
void screenDashboard() {
  header(F("SMART LAB"),F("LOCKED"));
  drawReading(2,17,(int)round(temperature),true);
  drawReading(66,17,(int)round(humidity),false);
  drawGasBar(49,WHT);
}
void screenEntry() {
  header(F("ENTER PASSWORD"),nullptr);
  for (byte i=0; i<4; i++) {
    int bx=15+i*26;
    display.drawRoundRect(bx,20,20,24,4,WHT);
    if (i<inputLength) display.fillCircle(bx+10,32,4,WHT);
    else display.drawFastHLine(bx+6,38,8,WHT);
  }
  display.setTextSize(1);
  display.setCursor(10,52);
  display.print(F("# = OK   * = CLEAR"));
}
void screenGranted() {
  drawCheck(21,24,15,WHT);
  display.setTextSize(2);
  display.setCursor(42,8);
  display.print(F("ACCESS"));
  display.setCursor(42,26);
  display.print(F("GRANTED"));
  long left=AUTO_LOCK_TIME-(long)(millis()-doorTimer);
  if (left<0) left=0;
  display.setTextSize(1);
  display.setCursor(6,46);
  display.print(F("LOCK IN "));
  display.print((left+999)/1000);
  display.print('s');
  display.setCursor(78,46);
  display.print(F("[B] NOW"));
  display.drawRoundRect(6,55,116,8,3,WHT);
  display.fillRect(8,57,112L*left/AUTO_LOCK_TIME,4,WHT);
}
void screenDenied() {
  drawCross(21,24,15,WHT);
  display.setTextSize(2);
  display.setCursor(42,8);
  display.print(F("ACCESS"));
  display.setCursor(42,26);
  display.print(F("DENIED"));
  display.setTextSize(1);
  display.setCursor(6,46);
  display.print(F("ATTEMPTS"));
  for (byte i=0; i<3; i++) {
    if (i<wrongAttempts) display.fillCircle(80+i*16,49,4,WHT);
    else display.drawCircle(80+i*16,49,4,WHT);
  }
}
void screenSecurity(bool inv) {
  uint16_t fg=inv?BLK:WHT;
  if (inv) display.fillRect(0,0,128,64,WHT);
  drawWarning(6,8,fg);
  display.setTextColor(fg);
  display.setTextSize(2);
  display.setCursor(38,6);
  display.print(F("ALARM!"));
  display.setTextSize(1);
  display.setCursor(38,26);
  display.print(F("3 WRONG CODES"));
  display.drawRoundRect(4,44,120,16,4,fg);
  display.setCursor(10,48);
  display.print(F("PRESS [D] TO RESET"));
}
void screenGas(bool inv) {
  uint16_t fg=inv?BLK:WHT;
  if (inv) display.fillRect(0,0,128,64,WHT);
  drawWarning(6,9,fg);
  display.setTextColor(fg);
  display.setTextSize(2);
  display.setCursor(38,6);
  display.print(F("GAS"));
  display.setCursor(38,24);
  display.print(F("ALERT"));
  drawGasBar(48,fg);
}
void updateOLED() {
  bool blink=(millis()/500)%2;
  bool denied=(long)(deniedUntil-millis())>0;
  display.clearDisplay();
  display.setTextColor(WHT);
  display.setTextSize(1);
  if (denied) screenDenied();
  else if (securityAlarm) screenSecurity(blink);
  else if (gasAlarm) screenGas(blink);
  else if (doorOpen) screenGranted();
  else if (inputLength>0) screenEntry();
  else screenDashboard();
  display.display();
}
