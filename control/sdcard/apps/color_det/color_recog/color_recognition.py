# 导入必要的模块 Import required modules
import time, os, sys, gc
from media.sensor import *
from media.display import *
from media.media import *

DISPLAY_WIDTH = 640
DISPLAY_HEIGHT = 480
from libs.YbProtocol import YbProtocol
from ybUtils.YbUart import YbUart
uart = None
pto = YbProtocol()
# Import touch sensor module
# 导入触摸传感器模块
from machine import TOUCH
tp = TOUCH(0)

THRESHOLDS = [
    (32, 55, 26, 92, -3, 41),         # 红色阈值 / Red threshold
    (42, 100, -128, -17, 6, 66),       # 绿色阈值 / Green threshold
    (43, 99, -43, -4, -56, -7),        # 蓝色阈值 / Blue threshold
    (37, 100, -128, 127, -128, -27)    # 亚博智能Logo的颜色 color of YAHBOOM
]

# Display colors matching each threshold index
DRAW_COLORS = [
    (255, 0, 0),     # 红色
    (0, 255, 0),     # 绿色
    (0, 0, 255),     # 蓝色
    (255, 255, 255), # 白色 (YAHBOOM logo)
]


def process_blobs(img, blobs, color):
    """处理检测到的色块 / Process detected color blobs"""
    for blob in blobs:
        img.draw_rectangle(blob[0:4], color=color, thickness=4)
        img.draw_cross(blob[5], blob[6], color=color, thickness=2)

class YAHBOOM_DEMO:
    def __init__(self, pl, _uart=None):
        global uart
        self.pl = pl
        uart = _uart
        self.color_index = 0        # current threshold index (0=red, 1=green, …)
        self.rx_buf = ""            # RX buffer for G3519→K230 commands

    def _rx_poll(self):
        """Non-blocking UART RX poll: listen for $SWITCH# from G3519.
           $SWITCH# toggles to the next color threshold."""
        global uart
        if uart is None:
            return
        while uart.any():
            ch = uart.read(1)
            if ch:
                self.rx_buf += ch.decode()
                if '#' in self.rx_buf:
                    if '$SWITCH#' in self.rx_buf:
                        self.color_index = (self.color_index + 1) % len(THRESHOLDS)
                    self.rx_buf = ""
                # Safety: keep buffer bounded
                if len(self.rx_buf) > 64:
                    self.rx_buf = self.rx_buf[-32:]

    def exce_demo(self, loading_text="Loading ..."):
        global uart
        while True:
            # ── Touch: return arrow (top-left 100×100) → exit ──
            point = tp.read(1)
            if len(point):
                pt = point[0]
                if pt.event == TOUCH.EVENT_DOWN:
                    if pt.x < 100 and pt.y < 100:
                        pass
                        self.exit_demo()
                        time.sleep_ms(10)
                        break

            # ── RX: listen for $SWITCH# from G3519 (non-blocking) ──
            self._rx_poll()

            # ── Camera capture + blob detection ──
            img = self.pl.sensor.snapshot(chn=CAM_CHN_ID_1)
            blobs = img.find_blobs([THRESHOLDS[self.color_index]])
            img.clear()
            if blobs:
                draw_color = DRAW_COLORS[self.color_index]
                process_blobs(img, blobs, draw_color)

                # ── TX: send largest blob (by area) via YbProtocol ──
                if uart is not None:
                    largest = max(blobs, key=lambda b: b[2] * b[3])
                    frame = pto.get_color_data(largest[0], largest[1],
                                               largest[2], largest[3])
                    uart.send(frame)

            Display.show_image(img)
            time.sleep_us(1)

    def exit_demo(self):
        pass
