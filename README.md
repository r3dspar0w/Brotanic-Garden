# 🌿🐢 Brotanic Gardens — MAPP Project

![Instruction Manual](instructionmanual/instruction_manual.jpg)

An interactive **Smart Greenhouse / Smart Garden System** developed for the MAPP project.  
The system integrates **STM32 embedded hardware**, a **local web dashboard**, and a **Telegram bot** to monitor and control plant conditions in real time.

---

## 📽 Demo Video
Watch the full demonstration here:  
https://youtu.be/MdiBvYiozMI?si=ZQaVtsXMWabsImn7

---

## 📑 Project Slides
View the full presentation slides here:  
https://www.canva.com/design/DAG4xBIvEpg/LWL0BFeIUPzBWzoZ4xI4-A/view?utm_content=DAG4xBIvEpg&utm_campaign=designshare&utm_medium=link2&utm_source=uniquelinks&utlId=h2ac780ee52

---

## 🌐 Website (Local Dashboard)

This project includes a **local web dashboard** located inside:

full software code/website/

To run the website locally:

cd "full software code/website"  
python -m http.server 8000  

Then open your browser and go to:

http://localhost:8000


---

## 🤖 Telegram Bot Server

The Telegram bot server is located inside:

full software code/software/telegram/

To run:

cd "full software code/software/telegram"  
python telegram_server.py  

Make sure Python 3 is installed and your Telegram bot token is configured correctly.

---

## 🧭 System Usage Guide

### 🌱 Plant Selection
Press **C** to select your plant on ESP:
1. Peace Lily  
2. Water Lily  
3. Spider Lily  

---

### 🔐 User Door Access
Press **A** to enter your 4-digit PIN.  
Correct → Door opens  
Incorrect → Try again  

---

### 📊 View Sensor Data
Press **B**, then:
5 → Temperature  
6 → Humidity  
7 → Moisture  

---

### 🌀 Manual Fan Control
Press **PC12** → Toggle manual fan  
Press **PA15** → Resume automatic mode  

---

### 💧 Watering & Plant Growth
Swipe hand once → Water plant  
Swipe hand twice → Update plant day  

---

### 🛠 Admin RFID Access
Tap RFID tag → Unlock door  
Tap again → Lock door  

---

### ⏱ Auto Safety Feature
Door closes automatically after 20 seconds of inactivity.

---


## 🧪 Requirements

Python 3 installed  
Web browser (Chrome recommended)  
STM32 hardware setup  
Specific Internet connection (for Telegram and API features)  
Configure a hotspot at 2.4G as "MAPP" and password as "Mapp1234"

---

## 🧯 Troubleshooting

If Python is not recognized → Install Python and enable "Add Python to PATH".

If port 8000 is busy, run:

python -m http.server 8080  

Then open:

http://localhost:8080  

---

