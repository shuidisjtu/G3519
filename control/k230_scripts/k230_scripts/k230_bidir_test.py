# k230_bidir_test.py — G3519 <-> K230 bidirectional link test
#
# TX (K230→G3519): sends simulated color-recognition frames at 20 Hz,
#   x sweeps 0..315 in sweep mode, stays at 160 in fixed mode.
# RX (G3519→K230): listens for "$SWITCH#" command — toggles sweep/fixed.
#
# Usage:
#   1. Connect K230 to PC via USB, open CanMV IDE, click "Connect"
#   2. Open this file → "Save as main.py" → restart K230
#   3. On G3519: enter "K230 Test", press S0 to toggle sweep/fixed
#   Expect: S0 toggles between moving rectangle (sweep) and center-fixed.

from machine import UART, FPIOA
import time

fpioa = FPIOA()
fpioa.set_function(9,  FPIOA.UART1_TXD)   # carrier-board serial TX
fpioa.set_function(10, FPIOA.UART1_RXD)   # carrier-board serial RX
uart = UART(UART.UART1, 115200)

x = 0
mode = 0                # 0 = sweep (x scrolls), 1 = fixed center
rx_buf = ""

print("K230 bidir test: sweep mode. Press S0 on G3519 to toggle.")
while True:
    # ─── RX: drain UART, look for "$SWITCH#" command ───
    while uart.any():
        ch = uart.read(1)
        if ch:
            rx_buf += ch.decode()
            if '#' in rx_buf:
                # Command delimiter found — check for known commands
                if '$SWITCH#' in rx_buf:
                    mode = 1 - mode
                    print("Mode:", "FIXED" if mode else "SWEEP")
                # else unknown command, ignore
                rx_buf = ""
            # Safety: keep buffer bounded
            if len(rx_buf) > 64:
                rx_buf = rx_buf[-32:]

    # ─── TX: generate YbProtocol color frame ───
    if mode == 0:
        x = (x + 5) % 320       # sweep right, wrap
    else:
        x = 160                  # fixed center

    temp = "$00,01,%03d,120,050,040#" % x
    frame = "$%02d,01,%03d,120,050,040#\n" % (len(temp), x)
    uart.write(frame)
    time.sleep_ms(50)           # 20 Hz
