# k230_link_test.py — G3519 <-> K230 UART link test (no Yahboom libs needed)
#
# Purpose : Verify the J11(UART6) <-> K230(UART1) wiring and the G3519-side
#           YbProtocol parser WITHOUT restoring the Yahboom GUI firmware.
#           Emits simulated color-recognition frames with x sweeping 0..315.
#
# Usage   : 1. Connect K230 to PC via USB, open CanMV IDE, click "Connect"
#           2. Open this file and click "Run" (temporary run, SD card untouched)
#           3. On G3519: enter menu "K230 Test"
#           Expect : ID:1, X scrolling 0..315, Frm increasing, Err ~ 0
#
# Frame   : $LL,01,xxx,120,050,040#\n   (YbProtocol color frame, see
#           docs/K230_Vision_Module_Use.md §3.1)

from machine import UART, FPIOA
import time

fpioa = FPIOA()
fpioa.set_function(9,  FPIOA.UART1_TXD)   # carrier-board serial TX (from Yahboom firmware)
fpioa.set_function(10, FPIOA.UART1_RXD)   # carrier-board serial RX
uart = UART(UART.UART1, 115200)

x = 0
while True:
    x = (x + 5) % 320
    # LL = frame length including '$' and '#', excluding '\n' (YbProtocol rule)
    temp = "$00,01,%03d,120,050,040#" % x
    frame = "$%02d,01,%03d,120,050,040#\n" % (len(temp), x)
    uart.write(frame)
    print(frame, end="")       # mirrored in IDE serial terminal
    time.sleep_ms(50)          # 20 Hz report rate
