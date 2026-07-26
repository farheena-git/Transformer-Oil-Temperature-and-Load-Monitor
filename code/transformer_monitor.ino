// ---------------- Pin Definitions (yours, plus new ones) ----------------
const int tempPin = 34;
const int currentPin = 35;
const int greenLED = 2;
const int redLED = 4;
const int buzzerPin = 15;
const int wifiButton = 32;

// ---- Blink timing (2-LED setup: red does double duty for WARNING/CRITICAL/FAULT) ----
const unsigned long WARNING_BLINK_MS = 500;  // slow blink = warning
const unsigned long FAULT_BLINK_MS   = 150;  // fast blink = sensor fault
const unsigned long BUZZER_BEEP_MS   = 300;  // beep on/off period during CRITICAL

// ---------------- Requirement constants (Task 1 numbers — match your table) ------------
const unsigned long SAMPLE_INTERVAL_MS = 2000;   // how often we take a reading

// V2: plausibility limits — a reading outside this is physically impossible
const float TEMP_PLAUSIBLE_MIN = -20.0;
const float TEMP_PLAUSIBLE_MAX = 250.0;
const float CURRENT_PLAUSIBLE_MIN = 0.0;
const float CURRENT_PLAUSIBLE_MAX = 500.0;

// V3: status thresholds (matches your worked examples: 65->SAFE, 78->WARNING, 90->CRITICAL)
const float TEMP_WARNING = 75.0;
const float TEMP_CRITICAL = 88.0;
const float CURRENT_WARNING = 60.0;
const float CURRENT_CRITICAL = 80.0;

// V5: don't act until the SAME status has shown up this many readings in a row
const int CONSECUTIVE_READINGS_TO_ACT = 5;

// V6: moving average window
const int MOVING_AVG_WINDOW = 3;

// V7: identical raw readings this many times in a row = sensor stuck
const int STUCK_READING_COUNT = 6;

// V8: store-and-forward buffer depth
const int BUFFER_SIZE = 30;

// ---------------- Status enum ----------------
enum Status { SAFE, WARNING, CRITICAL, SENSOR_ERROR };
Status confirmedStatus = SAFE;   // the status actually acted on (after debounce)

// ---------------- V6: moving average history ----------------
float tempHistory[MOVING_AVG_WINDOW];
float currHistory[MOVING_AVG_WINDOW];
int histIndex = 0;
int histFilled = 0; // counts up to MOVING_AVG_WINDOW so we don't average zeros at boot

// ---------------- V5: consecutive-reading tracking ----------------
Status pendingStatus = SAFE;
int pendingCount = 0;

// ---------------- V7: stuck-sensor tracking ----------------
float lastRawTemp = NAN;
int stuckCount = 0;

// ---------------- V8: store-and-forward ring buffer ----------------
struct Reading {
  unsigned long timestamp;
  float temperature;
  float current;
  Status status;
};
Reading buffer[BUFFER_SIZE];
int bufHead = 0, bufTail = 0, bufCount = 0;

unsigned long lastSampleTime = 0;

// =========================================================================
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

// =========================================================================
void loop() {
  unsigned long now = millis();

  // Non-blocking scheduling — replaces your delay(1000). The rest of the
  // loop (buffer draining, serial status) still runs every pass.
  if (now - lastSampleTime >= SAMPLE_INTERVAL_MS) {
    lastSampleTime = now;
    runOneCycle(now);
  }

  drainBufferIfWifiAvailable();
  updateOutputs(now);   // LEDs/buzzer refreshed every pass, independent of sampling
}

// =========================================================================
void runOneCycle(unsigned long now) {

  // ---- Read raw values (your original code) ----
  int tempRaw = analogRead(tempPin);
  int currentRaw = analogRead(currentPin);
  float temperature = map(tempRaw, 0, 4095, 20, 120);
  float current = map(currentRaw, 0, 4095, 0, 100);

  // ============ VERSION 2: Plausibility check ============
  bool tempPlausible = (temperature >= TEMP_PLAUSIBLE_MIN && temperature <= TEMP_PLAUSIBLE_MAX);
  bool currPlausible = (current >= CURRENT_PLAUSIBLE_MIN && current <= CURRENT_PLAUSIBLE_MAX);
  // NOTE: because map() above always outputs within [20,120] and [0,100], this check
  // can never actually fire with your current ranges — that's expected, it's a safety
  // net for real hardware glitches. To DEMONSTRATE it for your video/screenshots,
  // temporarily change the two map() lines above to a wider range (e.g. map(tempRaw,
  // 0, 4095, -30, 300)) so an extreme pot position produces an "impossible" value.

  // ============ VERSION 7: Sensor failure (stuck sensor) ============
  if (!isnan(lastRawTemp) && fabs(temperature - lastRawTemp) < 0.01) {
    stuckCount++;
  } else {
    stuckCount = 0;
  }
  lastRawTemp = temperature;
  bool stuck = (stuckCount >= STUCK_READING_COUNT);

  if (!tempPlausible || !currPlausible || stuck) {
    handleSensorError(tempPlausible, currPlausible, stuck);
    return; // don't push a bad reading into filtering/decision/storage
  }

  // ============ VERSION 6: Moving average filter ============
  tempHistory[histIndex] = temperature;
  currHistory[histIndex] = current;
  histIndex = (histIndex + 1) % MOVING_AVG_WINDOW;
  if (histFilled < MOVING_AVG_WINDOW) histFilled++;

  float smoothedTemp = averageOf(tempHistory, histFilled);
  float smoothedCurrent = averageOf(currHistory, histFilled);

  // ============ VERSION 3: Safe / Warning / Critical decision ============
  Status rawStatus = decideStatus(smoothedTemp, smoothedCurrent);

  // ============ VERSION 5: Consecutive readings before acting ============
  if (rawStatus == pendingStatus) {
    pendingCount++;
  } else {
    pendingStatus = rawStatus;
    pendingCount = 1;
  }

  if (pendingCount >= CONSECUTIVE_READINGS_TO_ACT) {
    confirmedStatus = pendingStatus; // this is the status we actually act on
  }

  // ============ VERSION 4: LEDs + buzzer ============
  // (actual pin writes now happen in updateOutputs(), called every loop pass —
  //  this keeps blinking smooth instead of jumping only once per sample)

  // ============ VERSION 8: store the reading (timestamped at capture time) ============
  pushReading(now, smoothedTemp, smoothedCurrent, confirmedStatus);

  // ---- Status line so you always have something on the Serial Monitor ----
  Serial.print("raw T=");
  Serial.print(temperature);
  Serial.print(" smoothT=");
  Serial.print(smoothedTemp);
  Serial.print(" raw C=");
  Serial.print(current);
  Serial.print(" smoothC=");
  Serial.print(smoothedCurrent);
  Serial.print(" pending=");
  Serial.print(statusName(pendingStatus));
  Serial.print("(");
  Serial.print(pendingCount);
  Serial.print(") confirmed=");
  Serial.print(statusName(confirmedStatus));
  Serial.print(" buffered=");
  Serial.println(bufCount);
}

// =========================================================================
// ---------------- VERSION 3 helper ----------------
Status decideStatus(float temperature, float current) {
  Status tempStatus, currStatus;

  if (temperature >= TEMP_CRITICAL) tempStatus = CRITICAL;
  else if (temperature >= TEMP_WARNING) tempStatus = WARNING;
  else tempStatus = SAFE;

  if (current >= CURRENT_CRITICAL) currStatus = CRITICAL;
  else if (current >= CURRENT_WARNING) currStatus = WARNING;
  else currStatus = SAFE;

  // overall status = the worse of the two
  if (tempStatus == CRITICAL || currStatus == CRITICAL) return CRITICAL;
  if (tempStatus == WARNING || currStatus == WARNING) return WARNING;
  return SAFE;
}

// ---------------- VERSION 4: LED + buzzer driver (2-LED setup) ----------------
// Called every loop() pass with the current millis(), NOT gated behind the
// sample interval, so blinking looks smooth instead of stepping once per sample.
//
//   SAFE          -> green solid ON,  red OFF,            buzzer OFF
//   WARNING       -> green OFF,       red blinks SLOW,    buzzer OFF
//   CRITICAL      -> green OFF,       red solid ON,       buzzer beeps
//   SENSOR_ERROR  -> green OFF,       red blinks FAST,     buzzer OFF
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
      digitalWrite(buzzerPin, LOW); // fault is not the same as a real alarm — buzzer stays silent
      break;
  }
}

// Returns HIGH/LOW, toggling every `periodMs`, without ever using delay().
int blinkState(unsigned long now, unsigned long periodMs) {
  return ((now / periodMs) % 2 == 0) ? HIGH : LOW;
}

void handleSensorError(bool tempOk, bool currOk, bool stuck) {
  confirmedStatus = SENSOR_ERROR; // updateOutputs() picks this up on the next loop pass
  Serial.print("SENSOR ERROR ->");
  if (!tempOk) Serial.print(" temp implausible;");
  if (!currOk) Serial.print(" current implausible;");
  if (stuck) Serial.print(" stuck value;");
  Serial.println();
}

// ---------------- VERSION 6 helper ----------------
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

// ---------------- VERSION 8: store-and-forward ----------------
void pushReading(unsigned long ts, float t, float c, Status s) {
  if (bufCount >= BUFFER_SIZE) {
    bufTail = (bufTail + 1) % BUFFER_SIZE; // drop oldest to make room
    bufCount--;
  }
  buffer[bufHead] = { ts, t, c, s };
  bufHead = (bufHead + 1) % BUFFER_SIZE;
  bufCount++;
}

bool wifiAvailable() {
  return digitalRead(wifiButton) == LOW; // pressed = available
}

void drainBufferIfWifiAvailable() {
  static unsigned long lastSend = 0;
  unsigned long now = millis();
  if (now - lastSend < 500) return; // don't hammer the "network" every loop pass
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
