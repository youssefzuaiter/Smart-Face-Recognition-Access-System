# main_access.py
# ------------------------------------------------------------

import cv2                       # OpenCV: camera, face detection, recognizer
import os                        # to create folders and build file paths
import csv                       # to write the access log as a CSV file
import json                      # to read the id-to-name map (labels.json)
import time                      # for timers and timestamps
import serial      # pyserial - talks to the Arduino
import pyttsx3     # makes the computer speak

# We reuse the capture function from capture_faces.py
from capture_faces import capture_faces   # used for the "ADD" command

# ----------------------- SETTINGS ---------------------------
SERIAL_PORT = "COM6"             # the USB port the Arduino is on
BAUD_RATE = 9600                 # serial speed (must match the Arduino)

# How long (in seconds) to look for a face after "D" arrives.
LOOK_TIME = 10                   # camera stays on up to 10 seconds per check

# LBPH confidence: LOWER number = BETTER (closer) match.
# If the best face match is BELOW this number we accept it.
#   - RAISE this number  -> accept faces more easily (LESS strict)
#   - LOWER this number  -> accept faces harder       (MORE strict)
# Start at 70 and adjust after testing with real faces.
CONFIDENCE_THRESHOLD = 65        # our tuned cut-off between known and stranger

# Files made by train_faces.py
MODEL_FILE = "trainer.yml"       # the trained face model
LABELS_FILE = "labels.json"      # the id-to-name map
LOG_FILE = "access_log.csv"      # where entries are recorded
# ------------------------------------------------------------

# Load the Haar cascade face detector.
FACE_DETECTOR = cv2.CascadeClassifier(                       # the face-finder
    cv2.data.haarcascades + "haarcascade_frontalface_default.xml"  # built-in model file
)


def make_sure_folders_exist():   # create needed folders if missing
    """Create the folders we need if they are missing."""
    os.makedirs("dataset", exist_ok=True)     # where face photos live
    os.makedirs("strangers", exist_ok=True)   # where stranger photos are saved


def speak(text):                 # say a sentence out loud
    """Say a sentence out loud using pyttsx3."""
    try:                         # voice can fail on some PCs, so guard it
        engine = pyttsx3.init()  # start the text-to-speech engine
        engine.say(text)         # queue the sentence
        engine.runAndWait()      # actually speak it
        engine.stop()            # close the engine
    except Exception as e:       # if speaking failed
        # If the voice fails for any reason, just print instead.
        print("(Could not speak:", e, ")")   # show the text instead of crashing


def write_log(name, result):     # add one row to the access log
    """Add one row to access_log.csv (makes the file + header if needed)."""
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")   # current date and time
    file_is_new = not os.path.exists(LOG_FILE)       # is this the first write?
    with open(LOG_FILE, "a", newline="") as f:       # open the CSV to append
        writer = csv.writer(f)                       # CSV writer object
        if file_is_new:                              # only on a brand-new file
            writer.writerow(["timestamp", "name", "result"])  # column titles
        writer.writerow([timestamp, name, result])   # write this event's row


def load_model_and_labels():     # load the trained model + names
    """Load trainer.yml and labels.json. Returns (recognizer, id_to_name)."""
    # Check the files exist first.
    if not os.path.exists(MODEL_FILE) or not os.path.exists(LABELS_FILE):  # missing?
        print("ERROR: Could not find", MODEL_FILE, "and/or", LABELS_FILE)  # warn
        print("Please run these two files first:")        # how to fix
        print("   1) python capture_faces.py")            # step 1
        print("   2) python train_faces.py")              # step 2
        return None, None        # signal failure to the caller

    recognizer = cv2.face.LBPHFaceRecognizer_create()     # create an LBPH model
    recognizer.read(MODEL_FILE)  # load the trained data into it

    with open(LABELS_FILE, "r") as f:    # open the id-to-name map
        raw = json.load(f)               # read it as a dictionary

    # The json keys are text ("0"), so turn them into numbers (0).
    id_to_name = {}              # will hold {number: name}
    for key in raw:              # for each saved entry
        id_to_name[int(key)] = raw[key]  # convert the text key into a number

    return recognizer, id_to_name        # hand both back to the caller


def find_largest_face(faces):    # pick the closest (biggest) face
    """From a list of face boxes, return the biggest one (or None)."""
    biggest = None               # best face box so far
    biggest_area = 0             # its area
    for (x, y, w, h) in faces:   # check every detected face
        area = w * h             # area = width times height
        if area > biggest_area:  # is this one bigger?
            biggest_area = area  # remember the new biggest area
            biggest = (x, y, w, h)           # and the box itself
    return biggest               # return the biggest face (or None if none)


def recognise_person(recognizer, id_to_name):   # camera loop that identifies a face
    """
    Turn on the webcam and look for a known face for LOOK_TIME seconds.
    Returns (name, last_frame):
        name       = the known person's name, or None if unknown / no face.
        last_frame = the most recent camera picture (used to save strangers).
    """
    camera = cv2.VideoCapture(0)             # open the built-in webcam (index 0)
    if not camera.isOpened():                # did the camera fail to open?
        print("ERROR: Could not open the webcam.")   # warn
        return None, None        # give up

    start_time = time.time()     # record when we started looking
    found_name = None            # the recognised name (None until found)
    last_frame = None            # the latest camera picture (for stranger photo)

    while time.time() - start_time < LOOK_TIME:    # keep looking until time runs out
        ok, frame = camera.read()                  # grab one frame
        if not ok:               # frame grab failed?
            continue             # try again
        last_frame = frame       # remember this frame

        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)   # convert to grayscale
        faces = FACE_DETECTOR.detectMultiScale(          # find faces in the frame
            gray, scaleFactor=1.2, minNeighbors=5, minSize=(80, 80)  # detection settings
        )

        # Default banner = red "ACCESS DENIED" (OpenCV colours are B,G,R).
        banner_text = "ACCESS DENIED"        # default message
        banner_colour = (0, 0, 255)  # red   # default colour (blue,green,red)

        # Only look at the BIGGEST face in view.
        biggest = find_largest_face(faces)   # pick the closest face
        if biggest is not None:              # if a face was found
            (x, y, w, h) = biggest           # unpack its position and size
            face_only = gray[y:y + h, x:x + w]   # crop just the face region

            # Ask the recognizer who this is.
            # It returns an id number and a confidence (lower = better).
            person_id, confidence = recognizer.predict(face_only)  # guess who + score

            if confidence < CONFIDENCE_THRESHOLD:    # close enough match?
                # Good enough match -> known person.
                found_name = id_to_name.get(person_id, "Unknown")  # look up the name
                banner_text = "WELCOME " + found_name              # green banner text
                banner_colour = (0, 255, 0)  # green               # banner turns green
                box_colour = (0, 255, 0)     # green box around the face
            else:                            # match too weak
                # Too far off -> treat as a stranger.
                found_name = None            # no known name
                box_colour = (0, 0, 255)     # red box around the face

            # Draw the rectangle and the name + confidence number.
            shown_name = found_name if found_name else "Unknown"   # label to show
            cv2.rectangle(frame, (x, y), (x + w, y + h), box_colour, 2)  # draw the box
            cv2.putText(                     # draw the label text
                frame, "{} ({:.0f})".format(shown_name, confidence),  # name + score
                (x, y - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.7, box_colour, 2  # style
            )

        # Draw the big banner across the top of the window.
        cv2.rectangle(frame, (0, 0), (frame.shape[1], 40), banner_colour, -1)  # filled bar
        cv2.putText(                         # write the banner text on the bar
            frame, banner_text, (10, 28),    # text and position
            cv2.FONT_HERSHEY_SIMPLEX, 0.9, (255, 255, 255), 2   # white, bold-ish
        )

        cv2.imshow("Access Control - looking for a face", frame)  # show the live window
        cv2.waitKey(1)           # let the window refresh (1 ms)

        # If we found a known person we can stop early.
        if found_name is not None:           # a known person was matched
            time.sleep(1.0)  # hold the welcome banner so it can be seen
            break            # stop looking, we have our answer

    # Close the camera and window before we reply to the Arduino.
    camera.release()         # free the webcam
    cv2.destroyAllWindows()  # close the preview window
    return found_name, last_frame        # return the result and last picture


def main():                  # the program's starting point
    print("===== Smart Face-Recognition Access System =====")   # banner
    make_sure_folders_exist()            # create dataset/ and strangers/ if needed

    # Load the trained model. Stop here if it is missing.
    recognizer, id_to_name = load_model_and_labels()   # load model + names
    if recognizer is None:   # loading failed?
        return               # stop the program

    print("Model loaded. People I know:", list(id_to_name.values()))  # list the names

    # Open the serial connection to the Arduino.
    try:                     # opening the port can fail (busy/wrong port)
        arduino = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)  # connect to COM6
    except Exception:        # if it failed
        print("Could not open COM6 - is the Arduino IDE Serial Monitor "  # explain
              "still open? Close it and try again.")                      # the fix
        return               # stop the program

    # Give the Arduino a moment to reset after the connection opens.
    time.sleep(2)            # wait 2 seconds for the Arduino to be ready
    print("Connected to Arduino on", SERIAL_PORT)      # confirm connection
    print("Waiting for the Arduino to send 'D' (someone detected)...")  # status

    while True:              # loop forever, listening to the Arduino
        # Read one line from the Arduino.
        raw_line = arduino.readline()    # read until a newline (or timeout)
        if not raw_line:     # nothing came this second?
            continue  # nothing arrived this second, keep waiting

        # Turn the bytes into clean text.
        line = raw_line.decode("utf-8", errors="ignore").strip()  # bytes -> clean text
        if line == "":       # empty line?
            continue          # ignore it

        # IGNORE every line that is not exactly "D" or "ADD".
        if line != "D" and line != "ADD":    # any other debug text?
            # (Remove the # below if you want to see the Arduino debug text.)
            # print("Ignoring Arduino message:", line)
            continue          # skip it

        if line == "D":       # the Arduino detected a person
            print("\n--- Arduino says someone is at the door (D) ---")  # log
            print("Turning on the camera to look for a face...")        # log

            name, last_frame = recognise_person(recognizer, id_to_name)  # run the camera

            if name is not None:             # a known person was found
                # KNOWN person.
                print("KNOWN face:", name, "-> ACCESS GRANTED")  # log
                speak("Welcome " + name)     # say the greeting out loud
                arduino.write(b"F")            # tell Arduino: open the door
                write_log(name, "GRANTED")   # record the entry
            else:                            # unknown or no face seen
                # UNKNOWN or no face seen in time.
                print("UNKNOWN face / no face -> ACCESS DENIED")  # log
                speak("Access denied")       # say the denial out loud
                arduino.write(b"U")            # tell Arduino: keep it locked

                # Save a photo of the stranger (if we have one).
                if last_frame is not None:   # if we captured a frame
                    stamp = time.strftime("%Y-%m-%d_%H-%M-%S")    # timestamp for the name
                    photo_path = os.path.join("strangers", stamp + ".jpg")  # file path
                    cv2.imwrite(photo_path, last_frame)           # save the photo
                    print("Saved stranger photo:", photo_path)    # log

                write_log("UNKNOWN", "DENIED")   # record the denial

            print("Done. Waiting for the next 'D'...")   # ready for the next person

        elif line == "ADD":  # the Arduino asked to enrol a new person
            print("\n--- Arduino asked to ADD a new person ---")  # log
            new_name = input("Type the new person's name and press Enter: ")  # ask name
            capture_faces(new_name)          # take photos of the new person
            print("Now run train_faces.py to update.")   # remind to retrain
            print("Waiting for the next message...")     # back to listening


if __name__ == "__main__":   # only run when started directly
    main()                   # start the program