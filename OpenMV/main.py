import sensor
import time
import struct
from machine import UART
from machine import LED

# UART1,TX:P1 RX:P0
uart = UART(1, 115200)
uart.init(115200, bits=8, parity=None, stop=1)

# Debug switch
DEBUG = False

thresholds = [(0, 64)]
# dete_area: [x, y, w, h, weight]
dete_area = [(0, 80, 160, 30, 0.6), (0, 50, 160, 25, 0.3), (0, 20, 160, 25, 0.1)]
min_pixel = 100
min_area = 100

weight_sum = 0
for area in dete_area:
    weight_sum += area[4]

sensor.reset()
sensor.set_pixformat(sensor.GRAYSCALE)
sensor.set_framesize(sensor.QQVGA)
sensor.set_auto_gain(False)
sensor.set_auto_whitebal(False)
sensor.set_auto_exposure(False, exposure_us=25000)
sensor.skip_frames(time=2000)

clock = time.clock()

red = LED("LED_RED")
red.on()

while True:
    clock.tick()
    img = sensor.snapshot()

    centroid_sum = 0

    for area in dete_area:
        blobs = img.find_blobs(thresholds, roi=area[0:4], pixels_threshold=min_pixel, area_threshold=min_area, merge=False)

        if blobs:
            # Find the blob with the most pixels
            largest_blob = max(blobs, key=lambda b: b.pixels())

            if DEBUG:
                # Draw a rect around the blob
                img.draw_rectangle(largest_blob.rect())
                img.draw_cross(largest_blob.cx(), largest_blob.cy())

            centroid_sum += largest_blob.cx() * area[4]

    # Determine center of line
    center_pos = int(centroid_sum / weight_sum)
    # compute the offset value
    offset_val = center_pos - img.width() // 2

    data = struct.pack("<bbb", 0xAA, offset_val, 0xBB)
    if DEBUG:
        byte_sent = uart.write(data)

        print(f"Offset Valuse: {offset_val}")
        print(f"FPS: {clock.fps()}")
        print(f"Sent Num:{byte_sent}")
    else:
        uart.write(data)
