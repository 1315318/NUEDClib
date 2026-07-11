import time
import sensor
import image
from image import SEARCH_EX

# Debug switch
DEBUG = True

sensor.set_contrast(1)
sensor.set_gainceiling(16)
sensor.reset()
sensor.set_pixformat(sensor.GRAYSCALE)
sensor.set_framesize(sensor.QQVGA)
sensor.set_auto_gain(False)
sensor.set_auto_whitebal(False)
sensor.set_auto_exposure(False, exposure_us=25000)

clock = time.clock()

template = image.Image("template.pgm")
relevance = 0.6

while True:
    clock.tick()
    img = sensor.snapshot()

    match_result = img.find_template(template, relevance, step=4, search=SEARCH_EX)
    if match_result and DEBUG:
        img.draw_rectangle(match_result)

    if DEBUG:
        print(clock.fps())
