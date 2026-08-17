# FoController 🎮

An advanced, feature-rich custom Bluetooth game controller powered by an **ESP32**, complete with an OLED status display, a web-based configuration dashboard, RGB LED indicator, and **GitHub Releases OTA (Over-The-Air) updates**.

---

## 🚀 Key Features

* **Bluetooth LE Gamepad:** Connects wirelessly to PCs, phones, and tablets as a standard gamepad.
* **Web Configuration Dashboard:** Connect to the controller's Wi-Fi Access Point to customize button mappings, key record inputs, analog joystick axes, and presets right from your browser.
* **Dual Preset System (`PC` & `Phone`):** Switch seamlessly between PC and Phone configurations on the fly using key combinations (`Fn + B1`).
* **Test Mode:** Quick-press the Fn button to view live digital and analog input diagnostics directly on the OLED screen.
* **Automated GitHub OTA Updates:** Pull and install new firmware versions directly over Wi-Fi using GitHub Releases.

---

## 📌 Pinout & Hardware Configuration

| Component | ESP32 Pin | Description |
| :--- | :--- | :--- |
| **Button 1 (B1)** | Pin 27 | Primary Action / Chord Modifier |
| **Button 2 (B2)** | Pin 12 | Secondary Action |
| **Button 3 (B3)** | Pin 14 | Tertiary Action |
| **Button 4 (B4)** | Pin 4 | Quaternary Action |
| **Fn Button** | Pin 18 | Function Key / Test Mode Toggle |
| **Joystick VRX** | Pin 34 | Analog X-Axis |
| **Joystick VRY** | Pin 35 | Analog Y-Axis |
| **Joystick SW** | Pin 32 | Joystick Click (L3) |
| **RGB Red** | Pin 26 | LED Red Channel |
| **RGB Green** | Pin 33 | LED Green Channel |
| **RGB Blue** | Pin 25 | LED Blue Channel |
| **OLED SDA** | Pin 21 | I2C Data |
| **OLED SCL** | Pin 22 | I2C Clock |

---

## 🌐 Web Dashboard Setup & Key Recording

1. Power on your controller. Connect your phone or computer to the Wi-Fi network named **`FoController`** (Password: `FoController V1`).
2. Open a web browser and go to `http://192.168.4.1`.
3. **Key Recording:** Click the **"Record"** button next to any mapping field, then press any key on your keyboard (e.g., Up Arrow) to automatically capture its key code.
4. **Joystick Configuration:** Toggle **Swap X & Y Axes**, **Invert X Axis**, or **Invert Y Axis** directly from the dashboard depending on whether you are using the `PC` or `Phone` preset.
5. Click **Save Changes**.

---

## 🔄 OTA Firmware Updates

To check for updates from GitHub:
1. Ensure your hotspot is active (`your_hotspot_name` / `your_password_name`).
2. Press and **hold all 4 main buttons (`Button 1 + Button 2 + Button 3 + Button 4`) together for 2 seconds**.
3. Once an update is found, the OLED screen will display **"Do you want to update?"**:
   * Press **`Fn + Button 2`** to confirm and download the update.
   * Press **`Fn + Button 1`** to cancel.
4. If `version.txt` is missing from GitHub, the firmware safely defaults to version **`1.0`**.
