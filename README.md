# An IoT-Based Multi-Sensor Monitoring System with LoRaWAN for Landslide Early Detection in Risk-Prone Areas

This repository contains the source code, firmware, and dataset supporting the research article **"An IoT-Based Multi-Sensor Monitoring System with LoRaWAN for Landslide Early Detection in Risk-Prone Areas"**.

The project presents a custom-developed, low-cost LoRaWAN sensor node designed to monitor critical parameters affecting canal bank settlements and landslides in Thailand.

## 👥 Authors
* **First Author: Nisit Pukrongta** (Rajamangala University of Technology Thanyaburi)
* **Corresponding Author: Boonyarit Kumkhet** (Rajamangala University of Technology Thanyaburi)
* **Co-Corresponding Author: Srikulnath Nilnoree** (Suranaree University of Technology)
* **Author: Taweephong Suksawat** (Department of Rural Roads, Thailand)
* **Author: Tanaporn Pechrkool** (Rajamangala University of Technology Thanyaburi)
* **Author: Virote Pirajnanchai** (Rajamangala University of Technology Thanyaburi)

---

## 📂 Repository Contents

This repository provides the firmware used in the developed system architecture (as illustrated in **Figure 2** of the manuscript).

### 1. Transmission Node (Tx) Firmware
* **File:** `ClientNodeSender.ino`
* **Description:** This code is designed for the **Transmission Node (Tx)** or the Sensor Node. It manages data acquisition from multiple sensors and transmits data packets via LoRa.
* **Hardware Implementation:** The code corresponds to the hardware design shown in **Figure 4** and the prototype shown in **Figure 5** of the manuscript.
* **Key Components:**
    * Microcontroller: ESP32 MH-ET
    * Communication: LoRa Module RFM95 (925MHz)
    * Sensors: Soil Moisture (20/40/60 cm), Gyroscope (MPU6050), Vibration (801S), DHT22, Rain Sensor.

### 2. Receiver/Gateway (Rx) Firmware
* **File:** `LoRaGateway.ino`
* **Description:** This code is for the **Receiver (Rx)** unit, which functions as the Gateway. It receives LoRa packets from the Tx node and forwards the data to the cloud (Google Sheets) and notification services (LINE Notify).
* **Hardware Implementation:** This corresponds to the Receiver and Gateway functionalities illustrated in **Figure 6** of the manuscript.
* **Key Components:**
    * Microcontroller: ESP32
    * Connectivity: Wi-Fi (2.4 GHz)

---

## 📊 Data Availability

To support reproducibility and transparency, the raw experimental data used for the analysis in this study is publicly available.

* **Repository:** Mendeley Data
* **Dataset Title:** Data for: An IoT-Based Multi-Sensor Monitoring System with LoRaWAN for Landslide Early Detection in Risk-Prone Areas
* **DOI:** 10.17632/9w43sg73bt.1
* **Direct URL:** https://data.mendeley.com/datasets/9w43sg73bt/1

The dataset includes time-series recordings of soil moisture, inclination (gyroscope), vibration, and environmental parameters collected during the field testing phase.

---

## 🚀 Usage

To use the source code provided in this repository:

1.  **IDE:** Install the [Arduino IDE](https://www.arduino.cc/en/software).
2.  **Boards Manager:** Install the **ESP32** board package in the Arduino IDE.
3.  **Libraries:** Ensure the following libraries are installed:
    * `LoRa` by Sandeep Mistry
    * `DHT sensor library`
    * `Adafruit MPU6050`
    * Other dependencies as specified in the `.ino` files.
4.  **Configuration:** Update the LoRa frequency and Wi-Fi credentials in `LoRaGateway.ino` before uploading.

---

## 📜 License

This project is open-source and available for research and educational purposes.
