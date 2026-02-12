# Attendance Management System (RFID + ESP8266)
A simple IoT-based attendance system built using **ESP8266**, **RFID (MFRC522)**, **RTC (DS3231)** and a **Google Sheets backend**.
The system is designed to work **offline-first** with local storage and later synchronization to the cloud when internet is available.

This project was developed as part of an academic project.

---

## Project Overview

The system separates **data collection** and **data synchronization**:

* **Local (Device Side)**

  * RFID card scanning
  * Local database storage (flash memory)
  * Offline attendance marking

* **Server (Cloud Side)**

  * Google Sheets + Apps Script
  * Data upload
  * Logs, reports, and summaries

This allows attendance to work even without internet and sync later when WiFi is available.

---

## Objectives

* Replace manual attendance with RFID-based logging
* Reduce class time wasted in roll calls
* Prevent proxy attendance (physical card required)
* Maintain accurate digital records
* Enable offline operation with later synchronization

---

## Hardware Used

* **ESP8266** (Microcontroller + WiFi + Flash storage)
* **RFID Reader (MFRC522)**
* **RTC Module (DS3231)**
* **LCD Display (I2C)**
* **2 Switches (Mode selection)**

---

## System Architecture

### Local Storage

* Student database: `lookup.csv`
* Attendance logs: `attendance.csv`
* Stored in ESP8266 flash (SPIFFS/LittleFS)

### Cloud Backend

* Google Sheets
* Google Apps Script API

---

## Operating Modes

The system uses 2 switches to control 4 modes (Finite State Machine):

### Mode 0 – Configuration Mode

* ESP8266 creates a local WiFi network (`Attendance_Setup`)
* Admin connects via phone/laptop
* Local web page opens (`http://configAttendance`)
* Register students by scanning RFID and entering:

  * Name
  * Roll number
* Data stored in `lookup.csv`

---

### Mode 1 – Attendance Mode

* Fully offline mode
* Students tap RFID card
* System:

  * Reads UID
  * Matches with `lookup.csv`
  * Logs entry in `attendance.csv`

### Attendance Log Format

**Structure of each entry (`attendance.csv`):**

| Field     | Description                   |
| --------- | ----------------------------- |
| UID       | Unique RFID card identifier   |
| Name      | Student name                  |
| Roll No.  | Student roll number           |
| Timestamp | Date & time from RTC (DS3231) |
| Type      | Entry type: `IN` or `OUT`     |

**Example Record:**

```
A1B2C3D4,23EC01032,Nikhil,2025-12-04 09:12,IN
```

---

### Validation & Logic Rules

| Rule Type            | Purpose                                               |
| -------------------- | ----------------------------------------------------- |
| Duplicate Prevention | Prevents repeated taps from creating multiple entries |
| Time Constraint Rule | Enforces minimum class duration before `OUT` entry    |
| IN/OUT Validation    | Ensures correct sequence of `IN → OUT` logs           |

This ensures clean, consistent, and tamper-resistant attendance records.

---

### Mode 2 – Daily Log Upload

* Device connects to WiFi
* Reads `attendance.csv`
* Uploads logs to Google Sheets via Apps Script
* Data integrity ensured:

  * Failed uploads are retried
  * No duplicate entries
  * Logs are only cleared after confirmation

---

### Mode 3 – Database Upload

* Uploads `lookup.csv` to server
* Initializes/updates student database sheet
* Server uses this as master student record

---

## Server Structure (Google Sheets)

### 1. Database Sheet (`Student_Attendance_Record`)

| Field Name | Description |
|-----------|-------------|
| UID | RFID unique identifier |
| Roll | Student roll number |
| Name | Student name |
| Total Attendance | Cumulative attendance count |

---

### 2. Log Sheet (`Attendance_Log_Records`)

| Field Name | Description |
|-----------|-------------|
| Date | Attendance date |
| Time | Attendance time |
| UID | RFID unique identifier |
| Roll | Student roll number |
| Name | Student name |
| Type | IN / OUT |
### 3. Report Sheet

* Auto-generated daily summary
* Processed from log sheet
* Structured attendance matrix

---

## Storage Design

### `lookup.csv` (per student)

```
UID(8B),Roll(9B),Name(20B)  ≈ 41 bytes per student
```

### `attendance.csv` (per entry)

```
UID,Roll,Name,Timestamp,Type ≈ 60 bytes per log
```

Minimum file system size: **1MB**, which supports:

* Student database
* Several months of attendance logs

---

## Features

* Offline attendance
* Local storage
* RFID-based identity verification
* RTC-based accurate timestamps
* WiFi sync
* Cloud backup
* Web-based configuration
* Google Sheets integration

---

## Limitations

* Requires RFID cards
* Depends on WiFi for cloud sync
* Not encrypted communication
* Designed for small/medium-scale deployment

---

## Demo
Video demonstration:
[https://go.screenpal.com/watch/cTl1bBnYeZ1](https://go.screenpal.com/watch/cTl1bBnYeZ1)

---

## Team Members

- [Prince Kumar](https://github.com/pkgit09)
- [Raj Vardhan](https://github.com/RajVardhan06)  
- [Nikhil Grandhi](https://github.com/Nikhil-Grandhi)  

---

## Note

This project focuses on **reliability, simplicity, and offline-first design** rather than commercial-scale deployment. It is built as a functional academic prototype to demonstrate embedded systems + IoT integration.
