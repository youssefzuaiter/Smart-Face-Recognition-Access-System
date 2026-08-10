# test_faces.py
# Quick test: does the camera recognise your trained faces?
# No Arduino needed. Press q to quit.
import cv2, json, os

CONFIDENCE_THRESHOLD = 55   # lower = stricter (same as main program)

FACE_DETECTOR = cv2.CascadeClassifier(
    cv2.data.haarcascades + "haarcascade_frontalface_default.xml")

if not os.path.exists("trainer.yml") or not os.path.exists("labels.json"):
    print("Run capture_faces.py and train_faces.py first.")
    raise SystemExit

recognizer = cv2.face.LBPHFaceRecognizer_create()
recognizer.read("trainer.yml")
with open("labels.json") as f:
    raw = json.load(f)
id_to_name = {int(k): v for k, v in raw.items()}
print("Known people:", list(id_to_name.values()))
print("Look at the camera. Press q to quit.")

cam = cv2.VideoCapture(0)
while True:
    ok, frame = cam.read()
    if not ok:
        continue
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    faces = FACE_DETECTOR.detectMultiScale(gray, 1.2, 5, minSize=(80, 80))
    for (x, y, w, h) in faces:
        pid, conf = recognizer.predict(gray[y:y+h, x:x+w])
        if conf < CONFIDENCE_THRESHOLD:
            label = "{} ({:.0f})".format(id_to_name.get(pid, "?"), conf)
            colour = (0, 255, 0)
        else:
            label = "Unknown ({:.0f})".format(conf)
            colour = (0, 0, 255)
        cv2.rectangle(frame, (x, y), (x+w, y+h), colour, 2)
        cv2.putText(frame, label, (x, y-10),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, colour, 2)
    cv2.imshow("Face test - press q to quit", frame)
    if cv2.waitKey(1) & 0xFF == ord("q"):
        break
cam.release()
cv2.destroyAllWindows()