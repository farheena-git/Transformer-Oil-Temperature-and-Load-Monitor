const int tempPin = 34;
const int currentPin = 35;
const int greenLED = 2;
const int redLED = 4;
const int buzzerPin = 15;
const int wifiButton = 32;

const unsigned long WARNING_BLINK_MS = 500;
const unsigned long FAULT_BLINK_MS   = 150;
const unsigned long BUZZER_BEEP_MS   = 300;

const unsigned long SAMPLE_INTERVAL_MS = 2000;

const float TEMP_PLAUSIBLE_MIN = -20.0;
const float TEMP_PLAUSIBLE_MAX = 250.0;
const float CURRENT_PLAUSIBLE_MIN = 0.0;
const float CURRENT_PLAUSIBLE_MAX = 500.0;

const float TEMP_WARNING = 75.0;
const float TEMP_CRITICAL = 88.0;
const float CURRENT_WARNING = 60.0;
const float CURRENT_CRITICAL = 80.0;

const int CONSECUTIVE_READINGS_TO_ACT = 5;
const int MOVING_AVG_WINDOW = 3;
const int STUCK_READING_COUNT = 6;
const int BUFFER_SIZE = 30;

enum Status { SAFE, WARNING, CRITICAL, SENSOR_ERROR };
Status confirmedStatus = SAFE;

float tempHistory[MOVING_AVG_WINDOW];
float currHistory[MOVING_AVG_WINDOW];
int histIndex = 0;
int histFilled = 0;

Status pendingStatus = SAFE;
int pendingCount = 0;

float lastRawTemp = NAN;
int stuckCount = 0;

struct Reading {
  unsigned long timestamp;
  float temperature;
  float current;
  Status status;
};
Reading buffer[BUFFER_SIZE];
int bufHead = 0, bufTail = 0, bufCount = 0;

unsigned long lastSampleTime = 0;

void setup() {
  Serial.begin(115200);
  pinMode(greenLED, OUTPUT);
  pinMode(redLED, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(wifiButton, INPUT_PULLUP);

  for (int i = 0; i < MOVING_AVG_WINDOW; i++) {
    tempHistory[i] = 0;
    currHistory[i] = 0;
  }

  Serial.println("Transformer monitor starting...");
}

void loop() {
  unsigned long now = millis();

  if (now - lastSampleTime >= SAMPLE_INTERVAL_MS) {
    lastSampleTime = now;
    runOneCycle(now);
  }

  drainBufferIfWifiAvailable();
  updateOutputs(now);
}

void runOneCycle(unsigned long now) {

  int tempRaw = analogRead(tempPin);
  int currentRaw = analogRead(currentPin);
  float temperature = map(tempRaw, 0, 4095, 20, 120);
  float current = map(currentRaw, 0, 4095, 0, 100);

  bool tempPlausible = (temperature >= TEMP_PLAUSIBLE_MIN && temperature <= TEMP_PLAUSIBLE_MAX);
  bool currPlausible = (current >= CURRENT_PLAUSIBLE_MIN && current <= CURRENT_PLAUSIBLE_MAX);

  // Change 1: internal value the code already works out (reading-to-reading
  // temperature change) but did not previously print anywhere.
  float tempDelta = isnan(lastRawTemp) ? 0.0 : (temperature - lastRawTemp);

  bool stuck = (!isnan(lastRawTemp) && fabs(tempDelta) < 0.01);
  if (stuck) stuckCount++; else stuckCount = 0;
  lastRawTemp = temperature;
  stuck = (stuckCount >= STUCK_READING_COUNT);

  if (!tempPlausible || !currPlausible || stuck) {
    handleSensorError(tempPlausible, currPlausible, stuck);
    return;
  }

  tempHistory[histIndex] = temperature;
  currHistory[histIndex] = current;
  histIndex = (histIndex + 1) % MOVING_AVG_WINDOW;
  if (histFilled < MOVING_AVG_WINDOW) histFilled++;

  float smoothedTemp = averageOf(tempHistory, histFilled);
  float smoothedCurrent = averageOf(currHistory, histFilled);

  Status rawStatus = decideStatus(smoothedTemp, smoothedCurrent);

  // Change 2: alarm only fires once the same status has held for
  // CONSECUTIVE_READINGS_TO_ACT readings in a row, so one odd/spiked
  // reading cannot trigger it on its own — only a sustained change can.
  if (rawStatus == pendingStatus) {
    pendingCount++;
  } else {
    pendingStatus = rawStatus;
    pendingCount = 1;
  }

  if (pendingCount >= CONSECUTIVE_READINGS_TO_ACT) {
    confirmedStatus = pendingStatus;
  }

  pushReading(now, smoothedTemp, smoothedCurrent, confirmedStatus);

  Serial.print("raw T=");
  Serial.print(temperature);
  Serial.print(" smoothT=");
  Serial.print(smoothedTemp);
  Serial.print(" tempDelta="); // Change 1: printed here
  Serial.print(tempDelta);
  Serial.print(" raw C=");
  Serial.print(current);
  Serial.print(" smoothC=");
  Serial.print(smoothedCurrent);
  Serial.print(" pending=");
  Serial.print(statusName(pendingStatus));
  Serial.print("(");
  Serial.print(pendingCount);
  Serial.print(") confirmed="); // Change 2: confirmed status only updates after the debounce above
  Serial.print(statusName(confirmedStatus));
  Serial.print(" buffered=");
  Serial.println(bufCount);
}

Status decideStatus(float temperature, float current) {
  Status tempStatus, currStatus;

  if (temperature >= TEMP_CRITICAL) tempStatus = CRITICAL;
  else if (temperature >= TEMP_WARNING) tempStatus = WARNING;
  else tempStatus = SAFE;

  if (current >= CURRENT_CRITICAL) currStatus = CRITICAL;
  else if (current >= CURRENT_WARNING) currStatus = WARNING;
  else currStatus = SAFE;

  if (tempStatus == CRITICAL || currStatus == CRITICAL) return CRITICAL;
  if (tempStatus == WARNING || currStatus == WARNING) return WARNING;
  return SAFE;
}

void updateOutputs(unsigned long now) {
  switch (confirmedStatus) {
    case SAFE:
      digitalWrite(greenLED, HIGH);
      digitalWrite(redLED, LOW);
      digitalWrite(buzzerPin, LOW);
      break;
    case WARNING:
      digitalWrite(greenLED, LOW);
      digitalWrite(redLED, blinkState(now, WARNING_BLINK_MS));
      digitalWrite(buzzerPin, LOW);
      break;
    case CRITICAL:
      digitalWrite(greenLED, LOW);
      digitalWrite(redLED, HIGH);
      digitalWrite(buzzerPin, blinkState(now, BUZZER_BEEP_MS));
      break;
    case SENSOR_ERROR:
      digitalWrite(greenLED, LOW);
      digitalWrite(redLED, blinkState(now, FAULT_BLINK_MS));
      digitalWrite(buzzerPin, LOW);
      break;
  }
}

int blinkState(unsigned long now, unsigned long periodMs) {
  return ((now / periodMs) % 2 == 0) ? HIGH : LOW;
}

void handleSensorError(bool tempOk, bool currOk, bool stuck) {
  confirmedStatus = SENSOR_ERROR;
  Serial.print("SENSOR ERROR ->");
  if (!tempOk) Serial.print(" temp implausible;");
  if (!currOk) Serial.print(" current implausible;");
  if (stuck) Serial.print(" stuck value;");
  Serial.println();
}

float averageOf(float arr[], int count) {
  float sum = 0;
  for (int i = 0; i < count; i++) sum += arr[i];
  return sum / count;
}

const char* statusName(Status s) {
  switch (s) {
    case SAFE: return "SAFE";
    case WARNING: return "WARNING";
    case CRITICAL: return "CRITICAL";
    default: return "SENSOR_ERROR";
  }
}

void pushReading(unsigned long ts, float t, float c, Status s) {
  if (bufCount >= BUFFER_SIZE) {
    bufTail = (bufTail + 1) % BUFFER_SIZE;
    bufCount--;
  }
  buffer[bufHead] = { ts, t, c, s };
  bufHead = (bufHead + 1) % BUFFER_SIZE;
  bufCount++;
}

bool wifiAvailable() {
  return digitalRead(wifiButton) == LOW;
}

void drainBufferIfWifiAvailable() {
  static unsigned long lastSend = 0;
  unsigned long now = millis();
  if (now - lastSend < 500) return;
  lastSend = now;

  if (!wifiAvailable() || bufCount == 0) return;

  Reading r = buffer[bufTail];
  bufTail = (bufTail + 1) % BUFFER_SIZE;
  bufCount--;

  Serial.print("SENT -> ts=");
  Serial.print(r.timestamp);
  Serial.print(" temp=");
  Serial.print(r.temperature);
  Serial.print(" current=");
  Serial.print(r.current);
  Serial.print(" status=");
  Serial.println(statusName(r.status));
}

