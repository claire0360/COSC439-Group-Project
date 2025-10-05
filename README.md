# 🧠 EchoMe — Tangible Assistive Device for Dementia Support
**University of Canterbury · COSC439 Group 5**

EchoMe is an **interactive tangible device** that supports people living with dementia through **photo recall**, **guided breathing**, and a **memory game**.  
It combines an **Arduino-based firmware** with a **C# WinForms desktop app**, providing calming and intuitive interactions using light, sound, and touch.

---

## 🎯 Features
- 🟢 **Photo Recall:** Place a cube to trigger a photo slideshow.
- 💨 **Breathing Guide:** Soft LED bloom for guided breathing rhythm.
- 🎵 **Memory Game:** Six LED-guided buttons produce piano tones and light feedback.
- 🔊 **Multisensory Feedback:** Combines light, sound, and tactile cues.
- 🔒 **Offline and Privacy-First:** No internet or cloud connection.

---

## ⚙️ Hardware Overview
| Component | Pin(s) | Description |
|------------|---------|-------------|
| Ultrasonic A | D3 | Breathing LED effect |
| Ultrasonic B | D6 (TRIG), D7 (ECHO) | Cube detection for slideshow |
| LED Strip A | D13 | Breathing light |
| LED Strip B | D2 | Cube presence indicator |
| Buttons (1–6) | D12, D10, D8, D4, A1, A3 | Piano-style inputs |
| Button LEDs | D11, D9, D5, A0, A2, A4 | LED feedback |
| Buzzer | A5 | Sound output |

**Board:** Arduino Nano  
**LED Type:** 5-pixel NeoPixel strip (x2)

---

## 🧩 Repository Structure

---

## 💡 Firmware (Arduino)
**File:** `firmware/firmware.c` (works as `.ino` in Arduino IDE)  
**Library:** [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel)

### Key Functions
- Ultrasonic A → LED “breathing” pulse  
- Ultrasonic B → Cube detection + serial output  
- Six buttons → piano notes (C–A) + light feedback  
- Buzzer → melodies and game tones  
- Startup melody and rainbow LED test  

### Upload Steps
1. Open file in **Arduino IDE** or **VS Code + PlatformIO**  
2. Install **Adafruit NeoPixel** library  
3. Select the correct **board** and **COM port**  
4. Set **baud rate = 9600** and **Upload**

---

## 💻 Desktop App (C# WinForms)
**Path:** `/app/PhotoApp.sln`  
Handles serial input, smoothing, hysteresis, and slideshow control.

### Highlights
- Parses distance `B:` from serial lines  
- 5-sample moving average smoothing  
- Debounce logic (250 ms on / 700 ms off)  
- Automatic slideshow start/stop  
- COM-port auto-refresh  



