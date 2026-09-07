import sys, json, serial, serial.tools.list_ports, geocoder, struct
from PyQt5.QtWidgets import (
    QApplication, QWidget, QPushButton, QLabel, QVBoxLayout,
    QComboBox, QMessageBox
)
from PyQt5.QtCore import QTimer
from skyfield.api import load, Topos

# --- Globals ---
ser = None
nano_port = None
baseline_ports = []

BAUD = 9600
OFFSET_FILE = "offsets.json"

# --- Location ---
g = geocoder.ip('me')
lat, lon = g.latlng if g.latlng else (0.0, 0.0)

# --- Skyfield Setup ---
planets = load('de421.bsp')
earth = planets['earth']
moon = planets['moon']
ts = load.timescale()

planet_map = {
    "Mercury": planets["mercury barycenter"],
    "Venus": planets["venus barycenter"],
    "Mars": planets["mars barycenter"],
    "Jupiter": planets["jupiter barycenter"],
    "Saturn": planets["saturn barycenter"],
    "Uranus": planets["uranus barycenter"],
    "Neptune": planets["neptune barycenter"],
    "Sun": planets["sun"],
    "Moon": moon
}

# --- Offset Handling ---
def load_offsets():
    try:
        with open(OFFSET_FILE) as f:
            return json.load(f)
    except:
        return {"alt":0.0,"az":0.0}

def save_offsets(alt, az):
    with open(OFFSET_FILE,"w") as f:
        json.dump({"alt":alt,"az":az}, f)

def get_altaz(target, lat, lon):
    t = ts.now()
    observer = earth + Topos(latitude_degrees=lat, longitude_degrees=lon)
    astrometric = observer.at(t).observe(target).apparent()
    alt, az, _ = astrometric.altaz()
    return alt.degrees, az.degrees

def list_ports():
    return [p.device for p in serial.tools.list_ports.comports()]

class TelescopeGUI(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Telescope Guidance System")
        self.setGeometry(200, 200, 420, 400)

        layout = QVBoxLayout()

        self.location_label = QLabel(f"Location: {lat:.2f}, {lon:.2f}")
        layout.addWidget(self.location_label)

        self.disconnect_button = QPushButton("Step 1: Disconnect Nano, then Click")
        self.disconnect_button.clicked.connect(self.record_baseline)
        layout.addWidget(self.disconnect_button)

        self.connect_button = QPushButton("Step 2: Reconnect Nano, then Click")
        self.connect_button.clicked.connect(self.detect_nano)
        layout.addWidget(self.connect_button)

        self.serial_status = QLabel("Serial: Not Connected ❌")
        layout.addWidget(self.serial_status)

        self.moon_button = QPushButton("Calibrate to Moon 🌙")
        self.moon_button.clicked.connect(self.calibrate_moon)
        layout.addWidget(self.moon_button)

        self.planet_dropdown = QComboBox()
        self.planet_dropdown.addItems(list(planet_map.keys()))
        layout.addWidget(self.planet_dropdown)

        self.preview_label = QLabel("Alt: --°, Az: --°")
        layout.addWidget(self.preview_label)

        self.track_button = QPushButton("Track Selected Planet")
        self.track_button.clicked.connect(self.track_planet)
        layout.addWidget(self.track_button)

        self.status_label = QLabel("Ready")
        layout.addWidget(self.status_label)

        self.setLayout(layout)

        self.timer = QTimer()
        self.timer.timeout.connect(self.update_preview)
        self.timer.start(5000)

        self.update_preview()
        self.offsets = load_offsets()

    def record_baseline(self):
        global baseline_ports
        baseline_ports = list_ports()
        QMessageBox.information(self, "Baseline Recorded",
                                f"Baseline ports: {baseline_ports}\nNow reconnect Nano and click Step 2.")

    def detect_nano(self):
        global nano_port, ser
        new_ports = list_ports()
        diff = [p for p in new_ports if p not in baseline_ports]
        if diff:
            nano_port = diff[0]
            try:
                ser = serial.Serial(nano_port, BAUD, timeout=1)
                self.serial_status.setText(f"Serial: Connected ✅ ({nano_port})")
                self.serial_status.setStyleSheet("color: green;")
                cmd = f"CALIBRATE_MOON,{self.offsets['alt']:.2f},{self.offsets['az']:.2f}\n"
                ser.write(cmd.encode())
                self.status_label.setText(f"Resent offsets: {cmd.strip()}")
            except Exception as e:
                QMessageBox.critical(self, "Error", f"Failed to open {nano_port}: {e}")
        else:
            QMessageBox.warning(self, "Not Found", "No new port detected. Try again.")

    def calibrate_moon(self):
        try:
            alt, az = get_altaz(moon, lat, lon)
            if alt < 0:
                QMessageBox.warning(self, "Moon Calibration", "Moon is below horizon!")
                return
            cmd = f"CALIBRATE_MOON,{alt:.2f},{az:.2f}\n"
            if ser:
                ser.write(cmd.encode())
            self.status_label.setText(f"Sent: {cmd.strip()}")
            save_offsets(alt, az)
            self.offsets = {"alt":alt,"az":az}
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Calibration failed: {e}")

    def update_preview(self):
        try:
            planet_name = self.planet_dropdown.currentText()
            target = planet_map[planet_name]
            alt, az = get_altaz(target, lat, lon)
            visibility = "Above Horizon ✅" if alt > 0 else "Below Horizon ❌"
            self.preview_label.setText(f"{planet_name} → Alt: {alt:.2f}°, Az: {az:.2f}° ({visibility})")
        except Exception as e:
            self.preview_label.setText(f"Error: {e}")

    def track_planet(self):
        try:
            planet_name = self.planet_dropdown.currentText()
            target = planet_map[planet_name]
            alt, az = get_altaz(target, lat, lon)
            if ser:
                data = struct.pack('<ff', alt, az)
                ser.write(data)
                ser.flush()
            self.status_label.setText(f"Sent floats: Alt={alt:.2f}, Az={az:.2f}")
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Tracking failed: {e}")

if __name__ == "__main__":
    app = QApplication(sys.argv)
    gui = TelescopeGUI()
    gui.show()
    sys.exit(app.exec_())
