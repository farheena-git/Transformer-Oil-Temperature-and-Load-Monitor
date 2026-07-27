**DEMO VIDEO DRIVE LINK: https://drive.google.com/file/d/1UZVhaGOBj1DRFU9TpZswwveCN18Wawgf/view?usp=sharing**


# 🟢 1. NORMAL OPERATION (SAFE STATE)

### What happens?

The system continuously monitors the transformer's oil temperature and load current. In this demonstration, both values remain well within the predefined safe operating limits.

Since the transformer is operating normally, the ESP32 classifies the condition as **SAFE**.

The **Green LED turns ON**, indicating that the transformer is healthy and no operator intervention is required. The **Red LED and buzzer remain OFF**, as there is no abnormal condition.

Even though everything is normal, the system continues to record and monitor the readings at regular intervals. This ensures that any future changes are detected immediately.

### Why is this important?

A transformer spends most of its operating life in this condition. Continuous monitoring during normal operation helps detect gradual changes before they become serious faults.

### System Response

* Temperature is within safe limit
* Load current is within safe limit
* Green LED ON
* Red LED OFF
* Buzzer OFF
* Status displayed as **SAFE**
* Data stored for continuous monitoring

---

# 🟡 2. WARNING CONDITION

### What happens?

As the potentiometer is adjusted, the simulated temperature or current gradually increases and approaches the warning threshold.

The transformer is still operating, but its condition is no longer considered completely healthy.

The ESP32 identifies this as a **WARNING** condition.

Instead of generating an emergency alarm immediately, the system first verifies that the warning persists for several consecutive readings. This prevents temporary fluctuations or electrical noise from creating unnecessary alarms.

Once confirmed, the **Yellow LED turns ON**, informing the operator that preventive action should be considered before the condition worsens.

### Why is this important?

Many transformer failures do not occur suddenly.

They begin as small abnormalities such as increasing temperature or excessive loading.

Providing an early warning allows maintenance personnel to reduce the load or inspect the transformer before a critical failure occurs.

### System Response

* Temperature approaching unsafe range
* Current approaching overload
* Yellow LED ON
* Green LED OFF
* Red LED OFF
* Buzzer OFF
* Status displayed as **WARNING**

---

# 🔴 3. CRITICAL CONDITION

### What happens?

In this stage, the simulated temperature or current exceeds the critical safety threshold.

The ESP32 immediately recognizes that the transformer is operating under dangerous conditions.

The system classifies the transformer as **CRITICAL**.

The **Red LED turns ON**, and the **buzzer is activated** to provide both visual and audible alerts.

This immediate indication allows operators to react quickly before severe transformer damage occurs.

At the same time, the event is recorded in memory and can later be transmitted to the cloud once network connectivity is available.

### Why is this important?

Operating a transformer at excessive temperature or current for a prolonged period can lead to:

* Insulation breakdown
* Oil degradation
* Equipment damage
* Unexpected power outages
* High repair costs

The alert system helps reduce these risks by notifying operators immediately.

### System Response

* Temperature exceeds critical threshold
* Current exceeds overload limit
* Red LED ON
* Green LED OFF
* Buzzer ON
* Status displayed as **CRITICAL**
* Reading stored for future transmission

---

# ⚠️ 4. SENSOR STUCK DETECTION

### What happens?

A monitoring system is only as reliable as its sensors.

Sometimes a sensor may stop responding and repeatedly produce the exact same reading, even though the actual transformer condition is changing.

The ESP32 continuously checks whether identical readings are received for multiple consecutive measurement cycles.

When the same value repeats continuously beyond the allowed limit, the system assumes that the sensor has become **stuck** or has failed.

Instead of trusting this incorrect information, the ESP32 marks the sensor as faulty and reports a **Sensor Error**.

The **Red LED is activated** to indicate that maintenance is required.

Unlike a critical transformer fault, the **buzzer remains OFF**, because the issue is with the monitoring device rather than the transformer itself.

### Why is this important?

Without sensor validation, operators might believe that the transformer is operating normally while the sensor is actually providing incorrect information.

Detecting sensor failures improves the overall reliability and safety of the monitoring system.

### System Response

* Repeated identical sensor readings detected
* Sensor classified as faulty
* Red LED ON
* Buzzer OFF
* Status displayed as **SENSOR ERROR**
* Invalid readings ignored

---

> This prototype demonstrates how an ESP32-based monitoring system can continuously supervise transformer operating conditions using temperature and 
load measurements. The system intelligently distinguishes between normal operation, warning conditions, critical faults, and sensor failures while 
providing appropriate visual and audible alerts. Additionally, the store-and-forward mechanism ensures that no monitoring data is lost during network 
interruptions, making the solution reliable and suitable for future IoT-based transformer health monitoring applications.
