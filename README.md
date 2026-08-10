# Smart Face Recognition Access System

A 3-layer, event-driven access control system: ultrasonic/NFC/vibration sensors on an Arduino UNO hand off to a Python/OpenCV program that recognizes faces via a laptop webcam, and the Arduino acts on the verdict — servo-driven door, relay lock, LCD, buzzer, LEDs. NFC cards and an IR remote work as backups to the camera.

Built for CMP3010 (Embedded Systems) at Bahçeşehir University, June 2026, by a team of four:

- Majd Javadah
- Yousef Zuaiter
- Yousef Salama
- Walid M. W. Kobahalabi

My part was the Python recognition pipeline and the serial integration between the Arduino and the laptop.

## How it works

```
[Sensing layer]                 [Arduino UNO]                [Recognition layer]
HC-SR04 ultrasonic  ──┐                                      Python + OpenCV (laptop)
LDR (day/night)       ├──►  USB serial  ◄────────────────►   LBPH face recognizer
Vibration (tamper)    │      "D" = check face                Haar cascade detector
RC522 NFC reader    ──┘      "F"/"U" = grant/deny             pyserial + pyttsx3
IR remote (arm/override)

                              [Feedback layer]
                    SG90 servo (door) · relay (lock) · 16x2 I2C LCD
                    buzzer · red/green LEDs
```

1. The ultrasonic sensor detects someone within **15 cm** and the Arduino sends `D` over serial.
2. The laptop's Python script wakes the webcam, runs Haar-cascade face detection, then LBPH recognition against a trained model. It has up to **6 seconds** to answer.
3. It replies `F` (known face → grant) or `U` (unknown → deny). The Arduino swings the servo, fires the relay, updates the LCD, and logs the event.
4. An NFC card/keychain and an IR remote work in parallel as backups — card access, emergency override, arm/disarm, and a visitor counter reset.

## Hardware

| Component | Role |
|---|---|
| Arduino UNO | Central controller |
| HC-SR04 ultrasonic | Presence detection (trigger distance) |
| RC522 NFC reader + card/keychain | Backup entry method |
| SG90 servo | Physical door/gate |
| Relay module | Lock / lamp switching |
| 16x2 I2C LCD | Status display |
| IR receiver + remote | Arm/disarm, emergency open, counter reset |
| Vibration sensor | Tamper detection |
| LDR | Day/night mode |
| Buzzer, red/green LEDs | Audible + visual feedback |
| 9V battery (barrel jack) | Stable power for servo/relay under load |

Full schematic: [`docs/circuit_schematic.jpeg`](docs/circuit_schematic.jpeg) (Proteus).

## Software

- **`arduino/SmartAccess_Project_Code.ino`** — sensor polling, servo/relay/LCD control, NFC + IR handling, serial protocol with the laptop.
- **`python/capture_faces.py`** — captures 150 face images per enrolled person.
- **`python/train_faces.py`** — trains an OpenCV LBPH recognizer on the captured dataset, saves `trainer.yml` + `labels.json`.
- **`python/main_access.py`** — the main loop: listens for `D` on serial, runs recognition, replies `F`/`U`, logs to `access_log.csv`, speaks the result out loud.
- **`python/test_faces.py`** — standalone recognition test against the webcam, no Arduino required.

### Setup

```bash
cd python
pip install -r requirements.txt
python capture_faces.py     # enroll a person (150 photos)
python train_faces.py       # train the LBPH model
python main_access.py       # run the full system (Arduino must be connected)
```

Set `SERIAL_PORT` in `main_access.py` to match your Arduino's port (`COM6` on Windows, `/dev/ttyUSB0`/`/dev/ttyACM0` on Linux/Mac).

## Engineering problems solved

The build worked; getting there meant debugging four real conflicts:

1. **Timer collision** — the IR remote library and Arduino's `tone()` both claimed Timer 2, corrupting remote signals whenever the buzzer sounded. Fixed by toggling the buzzer pin manually (`beep()` in the sketch) instead of using `tone()`.
2. **SPI/IR interference** — the NFC reader's constant polling interfered with the IR receiver. Fixed by rate-limiting NFC checks to every 600ms and pausing/resuming the IR receiver around each read.
3. **LBPH accuracy with glasses/beards** — recognition degraded on some faces. Fixed by increasing training data to 150 images per person and retuning the confidence threshold (65).
4. **Power brownout** — the servo and relay drew enough current on USB power alone to reset the Arduino. Fixed with a dedicated 9V supply via the barrel jack.

## Results

From `data/access_log.csv`, logged during testing:

- **140** total access attempts
- **107** granted, **33** denied
- **5** enrolled people
- **~15 cm** detection range, **≤6s** recognition decision time

## What's not in this repo

The face training dataset, the "stranger" photos captured on denied attempts, and the trained model file (`trainer.yml`, ~111MB) are excluded — the first two are real people's photos who didn't consent to being published, and the model file exceeds GitHub's file size limits anyway. See `.gitignore`. To reproduce results, run `capture_faces.py` and `train_faces.py` with your own enrolled users.

## License

MIT — see [LICENSE](LICENSE).
