import sensor
import time
import struct
from machine import UART

# UART1,TX:P1 RX:P0
uart = UART(1, 115200)
uart.init(115200, bits=8, parity=None, stop=1)

# Debug switch
DEBUG = True

THRESHOLD = (0, 64)
BINARY_VISIBLE = True

sensor.reset()
sensor.set_pixformat(sensor.GRAYSCALE)
sensor.set_framesize(sensor.QQVGA)
sensor.set_auto_gain(False)
sensor.set_auto_whitebal(False)
sensor.set_auto_exposure(False, exposure_us=25000)
sensor.skip_frames(time=2000)

clock = time.clock()

while True:
    clock.tick()
    img = sensor.snapshot().binary([THRESHOLD]) if BINARY_VISIBLE else sensor.snapshot()

    line = img.get_regression([(255, 255) if BINARY_VISIBLE else THRESHOLD])
    if line and DEBUG:
        img.draw_line(line.line(), color=127)

    offset_val = line.rho() - img.width() // 2

    data = struct.pack("<bbb", 0xAA, offset_val, 0xBB)
    if DEBUG:
        byte_sent = uart.write(data)

        print(f"Offset Valuse: {offset_val}")
        print(f"Mag: {line.magnitude()}")
        print(f"FPS: {clock.fps()}")
        print(f"Sent Num:{byte_sent}")
    else:
        uart.write(data)
