import cv2
import face_recognition
import os
from picamera2 import Picamera2
from datetime import datetime

# Estensioni immagine supportate
estensioni_immagini = {".jpg", ".jpeg", ".png", ".gif", ".bmp", ".tiff", ".webp"}
img_encoding2 = []
name_encoding2 = []

def crea_directory():
  prefix = "data/"
  nome_directory = input("Inserisci il nome della persona da acquisire: ").strip()

  if not nome_directory:
    print("Nome non valido.")
    return "error"

  if not os.path.exists(os.path.join(prefix, nome_directory)):
    os.makedirs(os.path.join(prefix, nome_directory))
    print(f"Ora puoi acquisire le foto di '{nome_directory}'.")
    return nome_directory
  else:
    print("Utente essite.")
    return "error"

def enroll_immagini(percorso):
    img_encoding2.clear()
    name_encoding2.clear()
    for radice, cartelle, file in os.walk(percorso):
        for nome_file in file:
            if os.path.splitext(nome_file)[1].lower() in estensioni_immagini:
                percorso_completo = os.path.abspath(os.path.join(radice, nome_file))
                print(percorso_completo)
                img2 = cv2.imread(os.path.abspath(percorso_completo))
                rgb_img2 = cv2.cvtColor(img2, cv2.COLOR_BGR2RGB)
                if len(face_recognition.face_encodings(rgb_img2))>0:
                  img_encoding2.append(face_recognition.face_encodings(rgb_img2)[0])
                  name_encoding2.append(os.path.basename(os.path.dirname(percorso_completo)))

def face_detecting(foto, encode2):
  foto_resize = cv2.resize(foto, (0, 0), fx=(0.25), fy=(0.25))
  rgb_img = cv2.cvtColor(foto_resize, cv2.COLOR_BGR2RGB)
  recognized = face_recognition.face_encodings(rgb_img)
  localized = face_recognition.face_locations(rgb_img)
  if len(recognized)>0 and len(encode2)>0:
    for (y1,x2,y2,x1),recon in zip(localized,recognized):
      x1 *=4
      y1 *=4
      x2 *=4
      y2 *=4
      result = face_recognition.compare_faces(recon, encode2)
      try:
        _nome = name_encoding2[result.index(True)]
        print(_nome)
      except ValueError:
        print("Sconosciuto")
        _nome = "Sconosciuto"
      cv2.rectangle(foto, (x1, y1), (x2, y2), (2, 42, 3), 3)
      cv2.rectangle(foto, (x1 -3, y1 - 35), (x2+3, y1), (2, 42, 3), cv2.FILLED)
      font = cv2.FONT_HERSHEY_DUPLEX
      cv2.putText(foto, _nome, (x1 + 6, y1 - 6), font, 1.0, (255, 255, 255), 1)
  return foto


# Esempio di utilizzo
if __name__ == "__main__":
  enroll_immagini("data/")
  #inizializzazione PiCamera
  picam2 = Picamera2()
  picam2.configure(picam2.create_preview_configuration(main={"format": 'XRGB8888', "size": (480, 640)}))
  picam2.start()
  photo_count = 0
  while True:
    frame = picam2.capture_array()
    frame = cv2.rotate(frame, cv2.ROTATE_90_COUNTERCLOCKWISE)
    frame = face_detecting(frame,img_encoding2)
    cv2.imshow('Capture', frame)
    key = cv2.waitKey(1) & 0xFF
    if key == ord('a'):  # a key
      folder = crea_directory()
      print(folder)
      if folder != "error":
        while True:
          frame = picam2.capture_array()
          frame = cv2.rotate(frame, cv2.ROTATE_90_COUNTERCLOCKWISE)
          cv2.imshow('Capture', frame)
          key = cv2.waitKey(1) & 0xFF
          if key == ord(' '):  # Space key
            photo_count += 1
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"{folder}_{timestamp}.jpg"
            filepath = os.path.join("data/",os.path.join(folder, filename))
            cv2.imwrite(filepath, frame)
            print(f"Photo {photo_count} saved: {filepath}")
          elif key == ord('q'):  # Q ke
            break
    elif key == ord('e'):
      enroll_immagini("data/")

    elif key == ord('q'):  # Q ke
      break

  cv2.destroyAllWindows()
  picam2.stop()

