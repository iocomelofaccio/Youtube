import board
import busio
import time

i2c = busio.I2C(board.GP21, board.GP20)

while not i2c.try_lock():
    pass

while True:
    print("I2C addresses found:", [hex(device_address)
                                   for device_address in i2c.scan()])
    time.sleep(2)