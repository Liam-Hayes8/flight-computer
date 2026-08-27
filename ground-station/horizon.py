#!/usr/bin/env python3
"""Flight computer ground station: live artificial horizon.

Usage:
    python3 horizon.py [serial_port]

Reads $TLM,<roll>,<pitch>,<alt> sentences at 115200 baud from the flight
computer and draws a real-time artificial horizon.

IMPORTANT: quit tio first. Only one program can own the serial port.

If the horizon banks or pitches the wrong way for your sensor mounting,
flip ROLL_SIGN / PITCH_SIGN below.
"""

import glob
import math
import sys
import time

import pygame
import serial

BAUD = 115200
W, H = 700, 700
PIX_PER_DEG = 6          # vertical pixels per degree of pitch
ROLL_SIGN = 1.0          # set to -1.0 if the horizon banks backwards
PITCH_SIGN = 1.0         # set to -1.0 if pitch moves the wrong way

SKY = (72, 138, 244)
GROUND = (128, 88, 60)
LINE = (255, 255, 255)
SYMBOL = (255, 210, 60)
ALERT = (235, 80, 80)


def find_port():
    if len(sys.argv) > 1:
        return sys.argv[1]
    matches = sorted(glob.glob("/dev/tty.usbmodem*"))
    if not matches:
        sys.exit("No /dev/tty.usbmodem* found. Pass the port explicitly:\n"
                 "  python3 horizon.py /dev/tty.usbmodemXXXXX")
    return matches[0]


def draw_horizon(screen, font, roll_deg, pitch_deg):
    r = math.radians(roll_deg)
    # Unit vector along the horizon line, and its normal.
    dx, dy = math.cos(r), math.sin(r)
    nx, ny = -math.sin(r), math.cos(r)

    # Pitch up -> horizon slides down the screen (more sky visible).
    cx = W / 2 + nx * pitch_deg * PIX_PER_DEG
    cy = H / 2 + ny * pitch_deg * PIX_PER_DEG

    big = 2000  # larger than any screen dimension

    # Ground fills everything; sky polygon covers the upper side of the line.
    screen.fill(GROUND)
    sky_poly = [
        (cx - dx * big, cy - dy * big),
        (cx + dx * big, cy + dy * big),
        (cx + dx * big - nx * big, cy + dy * big - ny * big),
        (cx - dx * big - nx * big, cy - dy * big - ny * big),
    ]
    pygame.draw.polygon(screen, SKY, sky_poly)
    pygame.draw.line(screen, LINE,
                     (cx - dx * big, cy - dy * big),
                     (cx + dx * big, cy + dy * big), 3)

    # Pitch ladder every 10 degrees.
    for k in range(-60, 61, 10):
        if k == 0:
            continue
        off = (pitch_deg - k) * PIX_PER_DEG
        lx = W / 2 + nx * off
        ly = H / 2 + ny * off
        half = 70 if k % 20 == 0 else 40
        a = (lx - dx * half, ly - dy * half)
        b = (lx + dx * half, ly + dy * half)
        pygame.draw.line(screen, LINE, a, b, 2)
        if k % 20 == 0:
            label = font.render(f"{abs(k)}", True, LINE)
            screen.blit(label, (b[0] + 8, b[1] - 10))


def main():
    port = find_port()
    ser = serial.Serial(port, BAUD, timeout=0)
    print(f"ground station listening on {port}")

    pygame.init()
    screen = pygame.display.set_mode((W, H))
    pygame.display.set_caption("flight-computer ground station")
    font = pygame.font.SysFont("menlo", 20)
    hud = pygame.font.SysFont("menlo", 26, bold=True)
    clock = pygame.time.Clock()

    roll = pitch = alt = 0.0
    last_rx = 0.0
    buf = b""

    running = True
    while running:
        for ev in pygame.event.get():
            if ev.type == pygame.QUIT:
                running = False
            elif ev.type == pygame.KEYDOWN and ev.key == pygame.K_ESCAPE:
                running = False

        # Drain the serial port; keep the newest complete $TLM sentence.
        data = ser.read(4096)
        if data:
            buf += data
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                line = raw.strip().decode(errors="ignore")
                if line.startswith("$TLM,"):
                    parts = line.split(",")
                    if len(parts) == 4:
                        try:
                            roll = ROLL_SIGN * float(parts[1])
                            pitch = PITCH_SIGN * float(parts[2])
                            alt = float(parts[3])
                            last_rx = time.time()
                        except ValueError:
                            pass

        draw_horizon(screen, font, roll, pitch)

        # Fixed aircraft symbol.
        cy = H // 2
        pygame.draw.line(screen, SYMBOL, (W // 2 - 120, cy), (W // 2 - 35, cy), 6)
        pygame.draw.line(screen, SYMBOL, (W // 2 + 35, cy), (W // 2 + 120, cy), 6)
        pygame.draw.circle(screen, SYMBOL, (W // 2, cy), 7)

        # HUD readouts.
        screen.blit(hud.render(f"ROLL  {roll:+7.1f}", True, LINE), (16, 12))
        screen.blit(hud.render(f"PITCH {pitch:+7.1f}", True, LINE), (16, 44))
        screen.blit(hud.render(f"ALT   {alt:7.1f} m", True, LINE), (16, 76))

        if time.time() - last_rx > 0.7:
            warn = hud.render("NO DATA", True, ALERT)
            screen.blit(warn, (W // 2 - warn.get_width() // 2, 16))

        pygame.display.flip()
        clock.tick(60)

    ser.close()
    pygame.quit()


if __name__ == "__main__":
    main()
