# ADS-TGT-001 🚀
Telescope Guidance System powered by Python, Arduino, and Skyfield.

ADS-TGT-001 is a Windows application that streams telescope guidance coordinates (Alt-Az) from Python to Arduino, integrates with MPU6050 orientation sensors, and uses Skyfield ephemeris data for planetary tracking. The installer is built with Inno Setup and deploys into the user’s AppData folder for a clean, per-user installation.

---

## ✨ Features
- **Planetary Guidance**: Uses Skyfield and the `de421.bsp` ephemeris for accurate planetary positions.
- **Arduino Integration**: Streams coordinates to Arduino Nano (ATmega328P, Old Bootloader).
- **Sensor Fusion**: MPU6050 orientation + OLED + buzzer feedback.
- **Self-contained Installer**:
  - Deploys into `C:\Users\<username>\AppData\Local\dist\ADS-TGT-001`
  - Bundles `de421.bsp` locally (no runtime downloads)
  - Installs VC++ runtime automatically
- **No Admin Rights Needed**: Runs entirely in user space.
---
## 📥 Download
Grab the latest installer from [Releases](../../releases).

Once downloaded:
- Run `ADS-TGT-001.exe`.
- The app installs into `C:\Users\<username>\AppData\Local\dist\ADS-TGT-001`.
- Start Menu and Desktop shortcuts are created automatically.
---

## 🛰 Usage
Connect Arduino Nano (ATmega328P, Old Bootloader).

Launch ADS-TGT-001 from Start Menu or Desktop.

The app streams telescope guidance coordinates to Arduino.

Skyfield loads de421.bsp locally from AppData.

---

##   🛠 Troubleshooting
Permission errors: Ensure the app is installed under AppData, not Program Files.

Skyfield download errors: Confirm de421.bsp exists in the AppData install folder.

Arduino not detected: Install the correct USB driver (FTDI or CH340 depending on your Nano).

Upload fails in Arduino IDE: Try switching between ATmega328P and ATmega328P (Old Bootloader).
---

