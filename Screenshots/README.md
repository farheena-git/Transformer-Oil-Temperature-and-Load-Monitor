1.Normal Operating Condition (SAFE Mode)

Description:
-The transformer operates within the safe temperature and load limits. The measured oil temperature and load current are continuously monitored by the ESP32. Since both parameters remain below the warning thresholds, the system indicates SAFE status by turning ON the Green LED while the buzzer remains OFF. The readings are stored for continuous monitoring.

2.Critical Transformer Condition

Description:
-The transformer experiences excessive oil temperature and/or overload current beyond the critical threshold. The ESP32 immediately classifies the condition as CRITICAL, activates the Red LED, and triggers the buzzer to alert the operator. The event is stored in memory and is ready for transmission to the cloud when Wi-Fi is available.

3.Sensor Failure – Stuck Sensor Detection

Description:
-The system continuously verifies sensor health by checking for repeated identical readings. When the same value is detected continuously for several sampling cycles, the ESP32 identifies the sensor as stuck and classifies it as a Sensor Error. The Red LED is activated to indicate maintenance is required, preventing unreliable data from being processed.
