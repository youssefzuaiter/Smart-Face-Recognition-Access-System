# Python Script To Capture FacesHOCAM

import cv2   # OpenCV - camera + face detection
import os    # to create folders

# How many face pictures we want to save for each person.
HOW_MANY_PICTURES = 150

# Load the Haar cascade face detector that comes with OpenCV.
FACE_DETECTOR = cv2.CascadeClassifier(
    cv2.data.haarcascades + "haarcascade_frontalface_default.xml"
)


def capture_faces(name):
    """Take HOW_MANY_PICTURES face photos of `name` and save them."""

    # Make a clean folder name (no spaces, all lowercase).
    name = name.strip().lower().replace(" ", "_")
    if name == "":
        print("No name was given. Stopping.")
        return

    # Create the folder dataset/<name>/ if it does not exist yet.
    save_folder = os.path.join("dataset", name)
    os.makedirs(save_folder, exist_ok=True)
    print("Pictures will be saved in:", save_folder)

    # Open the built-in webcam (0 = the first/default camera).
    camera = cv2.VideoCapture(0)
    if not camera.isOpened():
        print("ERROR: Could not open the webcam.")
        return

    print("Look at the camera. Move your head a little so we get")
    print("different angles. Press  q  to stop early.")

    count = 0  # how many pictures we have saved so far

    while count < HOW_MANY_PICTURES:
        # Read one frame (picture) from the camera.
        ok, frame = camera.read()
        if not ok:
            print("Could not read from camera, trying again...")
            continue

        # Convert the colour frame to gray (the detector needs gray).
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

        # Find faces. Returns a list of boxes (x, y, width, height).
        faces = FACE_DETECTOR.detectMultiScale(
            gray, scaleFactor=1.2, minNeighbors=5, minSize=(80, 80)
        )

        # Save one face per frame (the first one found).
        for (x, y, w, h) in faces:
            count += 1

            # Cut out just the face area from the gray picture.
            face_only = gray[y:y + h, x:x + w]

            # Save it as a .jpg inside the person's folder.
            file_path = os.path.join(save_folder, str(count) + ".jpg")
            cv2.imwrite(file_path, face_only)

            # Draw a green rectangle around the face on the preview.
            cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 2)

            # Write a counter like "Captured 15/40" on the preview.
            cv2.putText(
                frame,
                "Captured " + str(count) + "/" + str(HOW_MANY_PICTURES),
                (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2
            )

            print("Saved picture", count, "of", HOW_MANY_PICTURES)
            break  # only save one face per frame

        # Show the live preview window.
        cv2.imshow("Capturing faces - press q to quit", frame)

        # Wait 1 ms for a key press. If it is q, stop early.
        if cv2.waitKey(1) & 0xFF == ord("q"):
            print("You pressed q - stopping early.")
            break

    # Always release the camera and close the windows when done.
    camera.release()
    cv2.destroyAllWindows()
    print("Done! Saved", count, "pictures for", name)


# This part only runs when you start the file directly
# (python capture_faces.py). It does NOT run when main_access.py
# imports the capture_faces function.
if __name__ == "__main__":
    person_name = input("Type the person's name and press Enter: ")
    capture_faces(person_name)
