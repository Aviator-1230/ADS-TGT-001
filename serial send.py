from skyfield.api import load, Topos
import serial, time
import geocoder   # pip install geocoder

# --- CONFIG ---
PORT = 'COM3'        # Change to your Arduino port
BAUD = 9600

# --- Auto-detect location ---
g = geocoder.ip('me')   # gets approximate location from your IP
lat = g.latlng[0]
lon = g.latlng[1]
print(f"Detected location: {lat}, {lon}")

# --- Setup ---
ser = serial.Serial(PORT, BAUD, timeout=1)
planets = load('de421.bsp')
earth = planets['earth']
moon = planets['moon']
ts = load.timescale()

# --- Functions ---
def get_moon_altaz(lat, lon):
    t = ts.now()
    observer = earth + Topos(latitude_degrees=lat, longitude_degrees=lon)
    astrometric = observer.at(t).observe(moon).apparent()
    alt, az, distance = astrometric.altaz()
    return alt.degrees, az.degrees

def get_target_altaz(lat, lon, target_name='mars'):
    t = ts.now()
    observer = earth + Topos(latitude_degrees=lat, longitude_degrees=lon)
    target = planets[target_name]
    astrometric = observer.at(t).observe(target).apparent()
    alt, az, distance = astrometric.altaz()
    return alt.degrees, az.degrees

# --- Initial Calibration ---
moonAlt, moonAz = get_moon_altaz(lat, lon)
calib_cmd = f"CALIBRATE_MOON,{moonAlt:.2f},{moonAz:.2f}\n"
ser.write(calib_cmd.encode())
print("Sent:", calib_cmd.strip())

# --- Guidance Loop ---
last_calibration = time.time()
current_target = 'mars'   # default target

while True:
    # --- Dynamic target selection ---
    # You can change target by typing into console
    user_input = input("Enter planet name (mars/jupiter/saturn) or press Enter to keep current: ")
    if user_input.strip():
        if user_input.lower() in planets:
            current_target = user_input.lower()
            print(f"Switched target to {current_target}")
        else:
            print("Invalid target, keeping current.")

    # --- Send target coordinates ---
    targetAlt, targetAz = get_target_altaz(lat, lon, target_name=current_target)
    cmd = f"{targetAlt:.2f},{targetAz:.2f}\n"
    ser.write(cmd.encode())
    print("Sent:", cmd.strip())

    # --- Re-anchor every 5 minutes ---
    if time.time() - last_calibration >= 300:
        moonAlt, moonAz = get_moon_altaz(lat, lon)
        calib_cmd = f"CALIBRATE_MOON,{moonAlt:.2f},{moonAz:.2f}\n"
        ser.write(calib_cmd.encode())
        print("Recalibrated:", calib_cmd.strip())
        last_calibration = time.time()

    time.sleep(5)  # update every 5 seconds
