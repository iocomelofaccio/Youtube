from picamera2 import Picamera2
import cv2

picam2 = Picamera2()
picam2.configure(picam2.create_preview_configuration(main={"format": 'XRGB8888', "size": (640, 480)}))
picam2.start()
while True:
    frame = picam2.capture_array()
    frame = cv2.rotate(frame, cv2.ROTATE_180)
    cv2.imshow('Capture', frame)
    key = cv2.waitKey(1) & 0xFF
    if key == ord('q'):
        break
cv2.destroyAllWindows()
picam2.stop()
