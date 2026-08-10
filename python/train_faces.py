# train_faces.py
# ------------------------------------------------------------


import cv2                       # OpenCV: image reading and the face recognizer
import os                        # to walk through folders and files
import json                      # to save the id-to-name map as a .json file
import numpy as np               # OpenCV needs the labels as a numpy array

DATASET_FOLDER = "dataset"       # the folder that holds one sub-folder per person


def train():                     # does all the training work
    # Make the recognizer object.
    recognizer = cv2.face.LBPHFaceRecognizer_create()  # create an empty LBPH model

    faces = []                   # will hold every face image
    ids = []                     # will hold the matching number id for each image
    id_to_name = {}              # remembers which number means which name

    if not os.path.isdir(DATASET_FOLDER):              # is the dataset folder missing?
        print("ERROR: No 'dataset' folder found.")     # tell the user
        print("Run capture_faces.py first to take some face pictures.")  # how to fix
        return                   # stop here, nothing to train

    # Each sub-folder inside dataset/ is one person.
    person_names = sorted(os.listdir(DATASET_FOLDER))  # list people, in a fixed order

    current_id = 0               # the number id we give to the current person
    for name in person_names:    # go through each person's folder
        person_folder = os.path.join(DATASET_FOLDER, name)  # full path to that folder
        if not os.path.isdir(person_folder):           # skip anything that isn't a folder
            continue             # skip any stray files

        # Give this person a number id and remember the name.
        id_to_name[current_id] = name                  # link this id to this name
        print("Reading pictures for:", name, "(id", current_id, ")")  # progress message

        # Read every picture in this person's folder.
        for file_name in os.listdir(person_folder):    # loop over the person's photos
            file_path = os.path.join(person_folder, file_name)  # full path to the photo

            # Read the picture in grayscale.
            image = cv2.imread(file_path, cv2.IMREAD_GRAYSCALE)  # load it as gray
            if image is None:                          # not a valid image file?
                continue         # skip files that are not images

            faces.append(image) # add this face image to the list
            ids.append(current_id)                     # add its owner's id to the list

        current_id += 1          # move to the next id for the next person

    # Make sure we actually found some faces.
    if len(faces) == 0:                                # no images were loaded at all?
        print("ERROR: No face pictures were found inside 'dataset'.")  # warn
        print("Run capture_faces.py first.")           # how to fix
        return                   # stop here

    # Train the recognizer.
    print("Training the model, please wait...")        # status message
    recognizer.train(faces, np.array(ids))             # learn: each face -> its id

    # Save the trained model.
    recognizer.save("trainer.yml")                     # write the model to disk

    # Save the id<->name map as labels.json.
    with open("labels.json", "w") as f:                # open labels.json for writing
        json.dump(id_to_name, f)                       # save the id-to-name map

    print("-----")                                     # divider line
    print("Training finished!")                        # done message
    print("People trained:", len(id_to_name))          # how many people
    print("Total pictures used:", len(faces))          # how many images in total
    print("Saved: trainer.yml and labels.json")        # which files were created


if __name__ == "__main__":       # only run when started directly (not when imported)
    train()                      # start the training