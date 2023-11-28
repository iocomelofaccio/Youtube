import board
import busio
import time
import rotaryio
import pwmio
import os
import socketpool
import wifi
import simpleio

from adafruit_httpserver import Server, Request, Response, SSEResponse, GET

from lcd.lcd import LCD
from lcd.i2c_pcf8574_interface import I2CPCF8574Interface
from lcd.lcd import CursorMode
from digitalio import DigitalInOut, Direction, Pull
from adafruit_simplemath import map_range

i2c = busio.I2C(board.GP21, board.GP20)
lcd = LCD(I2CPCF8574Interface(i2c, 0x27), num_rows=2, num_cols=16)

encoder = rotaryio.IncrementalEncoder(board.GP19, board.GP18)

btn = [DigitalInOut(board.GP22),DigitalInOut(board.GP15)]

i=0
while i < len(btn):
    btn[i].direction = Direction.INPUT
    btn[i].pull = Pull.UP
    i += 1

lcd.set_cursor_pos(0,0)
lcd.print(" IOCOMELOFACCIO")
lcd.set_cursor_pos(1,0)
lcd.print(" PWM CONTROLLER")
time.sleep(2)
lcd.clear()
lcd.set_cursor_mode(CursorMode.LINE)

ssid = os.getenv("CIRCUITPY_WIFI_SSID")
password = os.getenv("CIRCUITPY_WIFI_PASSWORD")

print("Connecting to", ssid)
wifi.radio.connect(ssid, password)
print("Connected to", ssid)

pool = socketpool.SocketPool(wifi.radio)
server = Server(pool, debug=True)

@server.route("/", GET)

def base(request: Request):
    return Response(request, HTML_TEMPLATE, content_type="text/html")

server.start(str(wifi.radio.ipv4_address))

def get_rotary_value(old):
    position = encoder.position
    if old < position:
        simpleio.tone(board.GP14, 1440, duration=0.05)
        lcd.clear()
        return(-1)
    elif old > position:
        simpleio.tone(board.GP14, 1440, duration=0.05)
        lcd.clear()
        return(1)
    return(0)

def get_frequency(_out):
    if _out.frequency <= 999:
        return "{}{}".format(_out.frequency,"Hz")
    if _out.frequency <= 999999:
        return "{}{}".format(round(_out.frequency / 1000,1),"Khz")
    if _out.frequency <= 999999999:
        return "{}{}".format(round(_out.frequency / 1000000,2),"Mhz")

def get_duty(_out):
    return int(map_range(_out.duty_cycle,0,65535,1,100))

def set_duty(_out,_duty):
    _out.duty_cycle = int(map_range(_duty,0,100,0,65535))
    return int(map_range(_duty,0,100,0,65535))

out = [
        pwmio.PWMOut(board.GP0, frequency=50, duty_cycle=32012, variable_frequency=True),
        pwmio.PWMOut(board.GP2, frequency=900, duty_cycle=20321, variable_frequency=True),
        pwmio.PWMOut(board.GP4, frequency=500000, duty_cycle=1032, variable_frequency=True),
        pwmio.PWMOut(board.GP6, frequency=62400000, duty_cycle=61230, variable_frequency=True)
    ]

old_rotary_value = 0

out_point = 0
menu = 0
moltiplicatore = 1

HTML_TEMPLATE = """
<html>
    <head>
        <title> IOCOMELOFACCIO PWM </title>
    </head>
    <body>
        IOCOMELOFACCIO <br>
        Moltiplicatore: {}
    </body>
</html>

""".format(moltiplicatore)

while True:
    lcd.set_cursor_pos(0,1)
    lcd.print("OUT:")
    lcd.set_cursor_pos(0,4)
    lcd.print(str(out_point + 1))
    lcd.set_cursor_pos(0,6)
    lcd.print("X")
    lcd.set_cursor_pos(0,7)
    lcd.print(str(moltiplicatore))
    lcd.set_cursor_pos(1,0)
    lcd.print("F:")
    lcd.set_cursor_pos(1,2)
    lcd.print(str(get_frequency(out[out_point])))
    lcd.set_cursor_pos(1,11)
    lcd.print("D:")
    lcd.set_cursor_pos(1,13)
    lcd.print(str(get_duty(out[out_point]))+"%")
    
    
    pool_result = server.poll()
     
        
    if menu == 0:
        lcd.set_cursor_pos(0,6)
        out_point += get_rotary_value(old_rotary_value)
        if out_point >= len(out):
            out_point = 0
        elif out_point < 0:
            out_point = len(out)-1
    
    if menu == 1:
        lcd.set_cursor_pos(1,2)
        _fr = out[out_point].frequency
        _rt = get_rotary_value(old_rotary_value)
        _rt *= moltiplicatore
        _res= _fr + _rt
        if _res > 62400000:
            _res = 62400000
        if _res < 8:
            _res = 8
        #print("{} + {} = {}".format(_fr,_rt,_res))
        out[out_point].frequency = _res
        #out[out_point].frequency += get_rotary_value(old_rotary_value)
    
    if menu == 2:
        lcd.set_cursor_pos(1,13)
        set_duty(out[out_point],get_duty(out[out_point]) + get_rotary_value(old_rotary_value))
        
    
    if not btn[0].value:
        menu += 1
        if menu > 2:
            menu = 0
    
    if not btn[1].value:
        lcd.clear()
        moltiplicatore *= 10
        if moltiplicatore > 100000000:
            moltiplicatore = 1
        
    old_rotary_value = encoder.position
    time.sleep(0.1)

