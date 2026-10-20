#!/usr/bin/env python3
"""
SWIS live simulation - a forest/farm scene driven by the real hardware.

Reads the telemetry that simulation/firmware/sim_main.c sends over the HC-05 Bluetooth
link and animates it: break the IR beam with your hand and an animal walks out of the
forest into the farm; when the firmware confirms a breach, the siren and strobe light up
on screen at the same moment the real buzzer and siren sound on the bench, and a
Bluetooth alert card appears on the farmer's phone.

    python swis_gui.py --list                 show the serial ports Windows has
    python swis_gui.py --port COM5            run against the hardware
    python swis_gui.py --demo                 run with no hardware at all
    python swis_gui.py --port COM5 --sound    also beep the PC speaker on a breach

Needs pyserial for --port:   pip install pyserial
--demo needs nothing but Python. tkinter ships with Python on Windows.

Protocol (see sim_main.c):
    EV,SRC=IR|PIR|VIB
    ST,<STATE>,DETER=0|1
    ALERT,ZONE1,ANIMAL BREACH
    SWIS,STATE=..,BREACH=..,FALSE=..,LDR=..,DAY|NIGHT,DEMO=0|1
"""

import argparse
import math
import queue
import sys
import threading
import time
import tkinter as tk
from tkinter import font as tkfont

# ----------------------------------------------------------------------------
# Palette. Two sets, swapped when the firmware reports NIGHT.
# ----------------------------------------------------------------------------
DAY = {
    "sky": "#bfe3f5", "ground": "#7fae54", "farm": "#c9b870", "forest": "#2f5d3a",
    "tree": "#1f4429", "tree2": "#2a5a35", "trunk": "#4a3524", "haze": "#dff1fa",
}
NIGHT = {
    "sky": "#101d33", "ground": "#1e3a22", "farm": "#3b3a24", "forest": "#101f16",
    "tree": "#0c2415", "tree2": "#12301d", "trunk": "#241a12", "haze": "#16263f",
}

INK = "#0f1115"
PANEL = "#171a21"
PANEL_2 = "#21252e"
MUTED = "#8b93a3"
TEXT = "#e8ecf3"

STATE_COLOR = {
    "WARMUP":   ("#6b7280", "WARMING UP"),
    "SECURE":   ("#22c55e", "SECURE"),
    "VALIDATE": ("#f59e0b", "CHECKING"),
    "BREACH":   ("#ef4444", "BREACH"),
    "COOLDOWN": ("#3b82f6", "COOLDOWN"),
}

SCENE_W, SCENE_H = 840, 460
GROUND_Y = 360          # where the animal's feet sit
BEAM_X = 330            # the IR beam, and the forest/farm boundary


# ----------------------------------------------------------------------------
# Sources of telemetry lines. Both push plain strings into the same queue, so the
# GUI cannot tell the difference between real hardware and the demo.
# ----------------------------------------------------------------------------
class SerialSource(threading.Thread):
    """Reads CRLF lines from the HC-05 over a Windows COM port."""

    def __init__(self, port, baud, out_q):
        super().__init__(daemon=True)
        self.port, self.baud, self.q = port, baud, out_q
        self.ser = None
        self.stop_flag = threading.Event()

    def run(self):
        try:
            import serial
        except ImportError:
            self.q.put("#ERR,pyserial is not installed. Run: pip install pyserial")
            return
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=0.3)
        except Exception as exc:                       # noqa: BLE001 - surfaced in the UI
            self.q.put("#ERR,could not open %s: %s" % (self.port, exc))
            return

        self.q.put("#OK,connected to %s at %d baud" % (self.port, self.baud))
        buf = b""
        while not self.stop_flag.is_set():
            try:
                chunk = self.ser.read(256)
            except Exception as exc:                   # noqa: BLE001
                self.q.put("#ERR,serial read failed: %s" % exc)
                return
            if chunk:
                buf += chunk
                while b"\n" in buf:
                    raw, buf = buf.split(b"\n", 1)
                    line = raw.decode("ascii", "replace").strip()
                    if line:
                        self.q.put(line)

    def send(self, text):
        """Send a command character back to the firmware (?, S, R, T, D)."""
        if self.ser:
            try:
                self.ser.write(text.encode("ascii"))
            except Exception:                          # noqa: BLE001
                pass

    def close(self):
        self.stop_flag.set()
        if self.ser:
            try:
                self.ser.close()
            except Exception:                          # noqa: BLE001
                pass


class DemoSource(threading.Thread):
    """Replays a realistic script so the GUI can be shown with no hardware present."""

    def __init__(self, out_q):
        super().__init__(daemon=True)
        self.q = out_q
        self.stop_flag = threading.Event()
        self.manual = threading.Event()        # set by pressing SPACE

    def _emit(self, line):
        self.q.put(line)

    def _sleep(self, secs):
        """Interruptible sleep; returns False if we are shutting down."""
        end = time.time() + secs
        while time.time() < end:
            if self.stop_flag.is_set():
                return False
            time.sleep(0.02)
        return True

    def _breach_sequence(self):
        self._emit("EV,SRC=IR")
        self._emit("ST,VALIDATE,DETER=0")
        if not self._sleep(1.1):
            return
        self._emit("EV,SRC=PIR")
        if not self._sleep(0.35):
            return
        self._emit("ST,BREACH,DETER=1")
        self._emit("ALERT,ZONE1,ANIMAL BREACH")
        if not self._sleep(8.0):
            return
        self._emit("ST,COOLDOWN,DETER=0")
        if not self._sleep(3.0):
            return
        self._emit("ST,SECURE,DETER=0")

    def run(self):
        self.q.put("#OK,DEMO MODE - no hardware. Press SPACE to trigger an intrusion.")
        self._emit("ST,WARMUP,DETER=0")
        breaches, false_alarms, t = 0, 0, 0.0
        self._sleep(3.0)
        self._emit("ST,SECURE,DETER=0")

        next_auto = time.time() + 14.0
        while not self.stop_flag.is_set():
            # 1 Hz heartbeat, exactly like the firmware
            ldr = 1800 + int(700 * math.sin(t / 9.0))
            self._emit("SWIS,STATE=SECURE,BREACH=%d,FALSE=%d,LDR=%d,DAY,DEMO=1"
                       % (breaches, false_alarms, ldr))
            t += 1.0

            if self.manual.is_set() or time.time() >= next_auto:
                self.manual.clear()
                breaches += 1
                self._breach_sequence()
                next_auto = time.time() + 16.0
            elif int(t) % 11 == 0:
                # an unconfirmed trigger: one sensor only, no breach
                false_alarms += 1
                self._emit("EV,SRC=VIB")
                self._emit("ST,VALIDATE,DETER=0")
                self._sleep(2.0)
                self._emit("ST,SECURE,DETER=0")
            else:
                self._sleep(1.0)

    def send(self, _text):
        pass

    def close(self):
        self.stop_flag.set()


# ----------------------------------------------------------------------------
# Optional PC speaker, so the demo makes a noise even without the hardware.
# ----------------------------------------------------------------------------
class Siren(threading.Thread):
    def __init__(self):
        super().__init__(daemon=True)
        self.on = threading.Event()
        self.stop_flag = threading.Event()

    def run(self):
        try:
            import winsound
        except ImportError:
            return
        step = 0
        while not self.stop_flag.is_set():
            if self.on.wait(0.2):
                freq = 700 + 220 * (step % 3)
                try:
                    winsound.Beep(freq, 140)
                except Exception:                      # noqa: BLE001
                    return
                step += 1
            time.sleep(0.03)

    def close(self):
        self.on.clear()
        self.stop_flag.set()


# ----------------------------------------------------------------------------
# The GUI
# ----------------------------------------------------------------------------
class App(tk.Tk):
    def __init__(self, source, use_sound):
        super().__init__()
        self.title("SWIS - Wildlife Intrusion Sentinel - live simulation")
        self.configure(bg=INK)
        self.resizable(False, False)

        self.source = source
        self.q = source.q
        self.siren = Siren() if use_sound else None
        if self.siren:
            self.siren.start()

        # --- model state, all driven by telemetry ---
        self.state = "WARMUP"
        self.deter = False
        self.night = False
        self.breaches = 0
        self.false_alarms = 0
        self.ldr = 0
        self.demo_flag = False
        self.connected = False

        self.animal_phase = "hidden"     # hidden | entering | grazing | fleeing
        self.animal_x = -90.0
        self.phase_t = 0.0
        self.beam_broken_until = 0.0
        self.lamp_flash = {"IR": 0.0, "PIR": 0.0, "VIB": 0.0}
        self.frame = 0

        self._build_fonts()
        self._build_layout()

        self.bind("<space>", self._on_space)
        self.bind("<Key>", self._on_key)
        self.protocol("WM_DELETE_WINDOW", self._on_close)

        self.after(30, self._tick)

    # -------------------------------------------------- layout
    def _build_fonts(self):
        self.f_title = tkfont.Font(family="Segoe UI", size=15, weight="bold")
        self.f_pill = tkfont.Font(family="Segoe UI", size=13, weight="bold")
        self.f_body = tkfont.Font(family="Segoe UI", size=10)
        self.f_small = tkfont.Font(family="Segoe UI", size=9)
        self.f_mono = tkfont.Font(family="Consolas", size=9)
        self.f_num = tkfont.Font(family="Segoe UI", size=17, weight="bold")

    def _build_layout(self):
        # ---------- header ----------
        head = tk.Frame(self, bg=INK)
        head.grid(row=0, column=0, columnspan=2, sticky="ew", padx=14, pady=(12, 6))
        tk.Label(head, text="WILDLIFE INTRUSION SENTINEL",
                 font=self.f_title, fg=TEXT, bg=INK).pack(side="left")
        self.lbl_conn = tk.Label(head, text="connecting...", font=self.f_small,
                                 fg=MUTED, bg=INK)
        self.lbl_conn.pack(side="right")

        # ---------- scene ----------
        self.canvas = tk.Canvas(self, width=SCENE_W, height=SCENE_H,
                                bg=DAY["sky"], highlightthickness=0)
        self.canvas.grid(row=1, column=0, padx=(14, 7), pady=4)

        # ---------- status strip ----------
        strip = tk.Frame(self, bg=PANEL)
        strip.grid(row=2, column=0, sticky="ew", padx=(14, 7), pady=(6, 14))
        self.pill = tk.Label(strip, text="WARMING UP", font=self.f_pill, fg="white",
                             bg="#6b7280", padx=18, pady=8)
        self.pill.pack(side="left", padx=10, pady=10)

        self.stat_widgets = {}
        for key, caption in (("breaches", "BREACHES"), ("false", "FILTERED"),
                             ("ldr", "LIGHT"), ("mode", "TIME")):
            cell = tk.Frame(strip, bg=PANEL)
            cell.pack(side="left", padx=16)
            val = tk.Label(cell, text="0", font=self.f_num, fg=TEXT, bg=PANEL)
            val.pack()
            tk.Label(cell, text=caption, font=self.f_small, fg=MUTED, bg=PANEL).pack()
            self.stat_widgets[key] = val

        self.lbl_hint = tk.Label(strip, font=self.f_small, fg=MUTED, bg=PANEL,
                                 text="keys:  S status   R reset   T test   D demo mode")
        self.lbl_hint.pack(side="right", padx=14)

        # ---------- right column: phone + log ----------
        side = tk.Frame(self, bg=INK)
        side.grid(row=1, column=1, rowspan=2, sticky="n", padx=(7, 14), pady=4)

        tk.Label(side, text="FARMER'S PHONE", font=self.f_small,
                 fg=MUTED, bg=INK).pack(anchor="w", pady=(0, 4))
        self.phone = tk.Canvas(side, width=300, height=330, bg=INK, highlightthickness=0)
        self.phone.pack()
        self.alerts = []          # newest first: (text, clock string)

        tk.Label(side, text="BLUETOOTH LINK", font=self.f_small,
                 fg=MUTED, bg=INK).pack(anchor="w", pady=(10, 4))
        self.log = tk.Text(side, width=40, height=9, bg=PANEL, fg="#9fe8b5",
                           font=self.f_mono, bd=0, highlightthickness=0,
                           padx=8, pady=6, state="disabled", wrap="none")
        self.log.pack()

    # -------------------------------------------------- telemetry
    def _pump_queue(self):
        while True:
            try:
                line = self.q.get_nowait()
            except queue.Empty:
                return
            self._log(line)
            if line.startswith("#OK,"):
                self.connected = True
                self.lbl_conn.config(text=line[4:], fg="#22c55e")
            elif line.startswith("#ERR,"):
                self.connected = False
                self.lbl_conn.config(text=line[5:], fg="#ef4444")
            else:
                self._handle(line)

    def _handle(self, line):
        parts = [p.strip() for p in line.split(",")]
        kind = parts[0]

        if kind == "EV":
            src = self._field(parts, "SRC") or ""
            if src in self.lamp_flash:
                self.lamp_flash[src] = time.time() + 0.9
            if src == "IR":
                self.beam_broken_until = time.time() + 1.0
                if self.animal_phase in ("hidden",):
                    self.animal_phase, self.phase_t = "entering", 0.0

        elif kind == "ST":
            new_state = parts[1] if len(parts) > 1 else self.state
            self.state = new_state
            self.deter = self._field(parts, "DETER") == "1"
            if self.siren:
                self.siren.on.set() if self.deter else self.siren.on.clear()
            if new_state == "BREACH":
                # the deterrent fires: the animal turns and runs for the trees
                self.animal_phase, self.phase_t = "fleeing", 0.0
            elif new_state == "SECURE":
                self.animal_phase, self.animal_x = "hidden", -90.0

        elif kind == "ALERT":
            text = parts[2] if len(parts) > 2 else "ANIMAL BREACH"
            zone = parts[1] if len(parts) > 1 else "ZONE1"
            self.alerts.insert(0, ("%s: %s" % (zone, text), time.strftime("%H:%M:%S")))
            del self.alerts[6:]

        elif kind == "SWIS":
            self.state = self._field(parts, "STATE") or self.state
            self.breaches = int(self._field(parts, "BREACH") or self.breaches)
            self.false_alarms = int(self._field(parts, "FALSE") or self.false_alarms)
            self.ldr = int(self._field(parts, "LDR") or self.ldr)
            self.night = "NIGHT" in parts
            self.demo_flag = self._field(parts, "DEMO") == "1"

    @staticmethod
    def _field(parts, key):
        for p in parts:
            if p.startswith(key + "="):
                return p.split("=", 1)[1]
        return None

    def _log(self, line):
        self.log.config(state="normal")
        self.log.insert("end", line + "\n")
        self.log.see("end")
        if int(self.log.index("end-1c").split(".")[0]) > 300:
            self.log.delete("1.0", "100.0")
        self.log.config(state="disabled")

    # -------------------------------------------------- animation
    def _tick(self):
        self.frame += 1
        self._pump_queue()
        self._advance_animal()
        self._draw_scene()
        self._draw_phone()
        self._refresh_stats()
        self.after(30, self._tick)

    def _advance_animal(self):
        dt = 0.03
        self.phase_t += dt
        if self.animal_phase == "entering":
            self.animal_x += 150 * dt                 # walking in from the forest
            if self.animal_x >= 545:
                self.animal_x, self.animal_phase = 545.0, "grazing"
        elif self.animal_phase == "fleeing":
            self.animal_x -= 340 * dt                 # bolting back to the trees
            if self.animal_x < -90:
                self.animal_x, self.animal_phase = -90.0, "hidden"
        elif self.animal_phase == "grazing":
            self.animal_x += 11 * dt * math.sin(self.phase_t * 1.4)

    # ---- scene primitives ----
    def _tree(self, c, x, base_y, scale, pal):
        w = 34 * scale
        h = 78 * scale
        c.create_rectangle(x - 4 * scale, base_y - 18 * scale, x + 4 * scale, base_y,
                           fill=pal["trunk"], width=0)
        for i in range(3):
            top = base_y - 18 * scale - h * (0.42 + 0.26 * i)
            half = w * (1.0 - 0.22 * i)
            c.create_polygon(x - half, top + h * 0.34, x + half, top + h * 0.34, x, top,
                             fill=pal["tree"] if i % 2 == 0 else pal["tree2"], width=0)

    def _animal(self, c, x, y, fleeing):
        """A boar, facing right when entering and left when running away."""
        d = -1 if fleeing else 1
        body = "#4b3a2a"
        dark = "#33281d"
        # legs - a simple two-phase trot
        swing = 7 * math.sin(self.frame * 0.45)
        for off in (-17, -6, 8, 18):
            sw = swing if off < 0 else -swing
            c.create_line(x + off * d, y - 16, x + off * d + sw, y,
                          fill=dark, width=5, capstyle="round")
        c.create_oval(x - 30, y - 44, x + 30, y - 12, fill=body, width=0)
        hx = x + 32 * d
        c.create_oval(hx - 17 * d, y - 48, hx + 17 * d, y - 20, fill=body, width=0)
        c.create_polygon(hx + 15 * d, y - 36, hx + 15 * d, y - 26, hx + 30 * d, y - 31,
                         fill=dark, width=0)                              # snout
        c.create_polygon(hx + 24 * d, y - 32, hx + 33 * d, y - 38, hx + 26 * d, y - 30,
                         fill="#efe6d4", width=0)                         # tusk
        c.create_polygon(hx - 8 * d, y - 46, hx + 2 * d, y - 60, hx + 8 * d, y - 44,
                         fill=dark, width=0)                              # ear
        c.create_oval(hx + 4 * d, y - 40, hx + 9 * d, y - 35, fill="#f3d14a", width=0)
        c.create_line(x - 30, y - 34, x - 42, y - 44, fill=dark, width=4, capstyle="round")

    def _draw_scene(self):
        c = self.canvas
        c.delete("all")
        pal = NIGHT if self.night else DAY
        now = time.time()
        breach = self.state == "BREACH"

        c.configure(bg=pal["sky"])
        c.create_rectangle(0, 0, SCENE_W, GROUND_Y - 120, fill=pal["sky"], width=0)
        c.create_rectangle(0, GROUND_Y - 120, SCENE_W, GROUND_Y + 10,
                           fill=pal["haze"], width=0)
        c.create_rectangle(0, GROUND_Y, SCENE_W, SCENE_H, fill=pal["ground"], width=0)
        # farm side soil + crop rows
        c.create_rectangle(BEAM_X, GROUND_Y, SCENE_W, SCENE_H, fill=pal["farm"], width=0)
        for row in range(5):
            yy = GROUND_Y + 18 + row * 19
            for cx in range(BEAM_X + 24, SCENE_W - 10, 34):
                c.create_line(cx, yy, cx, yy - 11, fill="#4e7a33", width=3)

        # sun or moon
        if self.night:
            c.create_oval(726, 44, 774, 92, fill="#e8eefc", width=0)
            c.create_oval(738, 40, 786, 88, fill=pal["sky"], width=0)
        else:
            c.create_oval(720, 38, 782, 100, fill="#ffdf6e", width=0)

        # forest
        for i, (tx, sc) in enumerate(((34, 1.15), (96, 0.92), (150, 1.25),
                                      (210, 0.98), (262, 1.1))):
            self._tree(c, tx, GROUND_Y + 6 + (i % 2) * 6, sc, pal)
        c.create_text(150, 36, text="FOREST", fill="#ffffff" if self.night else "#1d3b25",
                      font=self.f_pill)
        c.create_text(600, 36, text="FARM", fill="#ffffff" if self.night else "#4a3c12",
                      font=self.f_pill)

        # ---- the IR break-beam at the boundary ----
        broken = now < self.beam_broken_until
        beam_col = "#ff4d4d" if broken else "#56e08a"
        c.create_rectangle(BEAM_X - 9, 150, BEAM_X + 9, 176, fill="#2b2f38", width=0)
        c.create_rectangle(BEAM_X - 9, GROUND_Y - 24, BEAM_X + 9, GROUND_Y + 2,
                           fill="#2b2f38", width=0)
        c.create_text(BEAM_X, 140, text="IR TX", fill="#2b2f38", font=self.f_small)
        if broken:
            # beam interrupted: draw it only above and below the animal
            c.create_line(BEAM_X, 176, BEAM_X, GROUND_Y - 96, fill=beam_col,
                          width=3, dash=(5, 5))
            c.create_text(BEAM_X, GROUND_Y - 112, text="BEAM BROKEN",
                          fill="#ff4d4d", font=self.f_small)
        else:
            c.create_line(BEAM_X, 176, BEAM_X, GROUND_Y - 24, fill=beam_col,
                          width=3, dash=(6, 6))

        # ---- the animal ----
        if self.animal_phase != "hidden":
            self._animal(c, self.animal_x, GROUND_Y + 8,
                         fleeing=(self.animal_phase == "fleeing"))

        # ---- deterrent: strobe lamp + buzzer, on the farm side ----
        self._deterrent(c, breach or self.deter)

        # ---- sensor lamps ----
        for i, name in enumerate(("IR", "PIR", "VIB")):
            lx = 24 + i * 78
            lit = now < self.lamp_flash[name]
            col = "#ff5a5a" if lit else "#39414f"
            c.create_oval(lx, 64, lx + 15, 79, fill=col, width=0)
            c.create_text(lx + 36, 72, text=name, anchor="w", font=self.f_small,
                          fill="#ffffff" if self.night else "#15301f")

        if self.state == "VALIDATE":
            c.create_text(SCENE_W // 2, 108, text="CHECKING SECOND SENSOR...",
                          fill="#b45309", font=self.f_pill)

    def _deterrent(self, c, active):
        px, py = 700, GROUND_Y - 96          # strobe lamp on its pole
        c.create_rectangle(px - 4, py, px + 4, GROUND_Y, fill="#39414f", width=0)
        if active:
            phase = (self.frame // 5) % 2
            lamp = "#ff3b3b" if phase else "#3b82f6"
            for r in (46, 34, 22):
                if (self.frame // 3 + r) % 18 < 9:
                    c.create_oval(px - r, py - r, px + r, py + r, outline=lamp, width=2)
            c.create_oval(px - 14, py - 14, px + 14, py + 14, fill=lamp, width=0)
            c.create_text(px, py - 62, text="SIREN + STROBE ON", fill="#ff3b3b",
                          font=self.f_pill)
            # buzzer, drawn as a little speaker throwing sound arcs
            bx, by = 470, GROUND_Y - 58
            c.create_rectangle(bx - 12, by - 12, bx, by + 12, fill="#2b2f38", width=0)
            c.create_polygon(bx, by - 12, bx + 16, by - 24, bx + 16, by + 24, bx, by + 12,
                             fill="#2b2f38", width=0)
            for k in range(3):
                if (self.frame // 4 + k) % 3:
                    rr = 12 + k * 11
                    c.create_arc(bx + 6 - rr, by - rr, bx + 6 + rr, by + rr,
                                 start=-52, extent=104, style="arc",
                                 outline="#f59e0b", width=2)
            c.create_text(bx + 14, by + 42, text="BUZZER", fill="#f59e0b",
                          font=self.f_small)
        else:
            c.create_oval(px - 12, py - 12, px + 12, py + 12, fill="#4b5563", width=0)

    def _draw_phone(self):
        p = self.phone
        p.delete("all")
        p.create_rectangle(16, 4, 284, 326, fill="#2a2f3a", width=0)
        p.create_rectangle(24, 26, 276, 306, fill="#0d1017", width=0)
        p.create_rectangle(118, 10, 182, 20, fill="#0d1017", width=0)
        p.create_text(150, 42, text=time.strftime("%H:%M"), fill="#f2f5fa",
                      font=self.f_pill)
        p.create_text(150, 62, text="HC-05  ·  SWIS", fill=MUTED, font=self.f_small)

        if not self.alerts:
            p.create_text(150, 170, text="no alerts", fill="#39414f", font=self.f_body)
            return

        y = 82
        for i, (text, clock) in enumerate(self.alerts[:5]):
            fresh = i == 0 and self.state in ("BREACH", "COOLDOWN")
            bg = "#3b1d1f" if fresh else "#191e28"
            edge = "#ef4444" if fresh else "#2a3140"
            p.create_rectangle(34, y, 266, y + 46, fill=bg, outline=edge, width=1)
            p.create_oval(44, y + 13, 60, y + 29,
                          fill="#ef4444" if fresh else "#6b7280", width=0)
            p.create_text(70, y + 14, anchor="w", text="SWIS Alert",
                          fill="#f2f5fa", font=self.f_small)
            p.create_text(70, y + 31, anchor="w", text=text[:26],
                          fill="#ffb4b4" if fresh else MUTED, font=self.f_small)
            p.create_text(256, y + 14, anchor="e", text=clock, fill=MUTED,
                          font=self.f_small)
            y += 52

    def _refresh_stats(self):
        colour, caption = STATE_COLOR.get(self.state, ("#6b7280", self.state))
        if self.state == "BREACH" and (self.frame // 8) % 2:
            colour = "#7f1d1d"
        self.pill.config(text=caption, bg=colour)
        self.stat_widgets["breaches"].config(text=str(self.breaches))
        self.stat_widgets["false"].config(text=str(self.false_alarms))
        self.stat_widgets["ldr"].config(text=str(self.ldr))
        self.stat_widgets["mode"].config(text="NIGHT" if self.night else "DAY")

    # -------------------------------------------------- input
    def _on_space(self, _evt):
        if isinstance(self.source, DemoSource):
            self.source.manual.set()
        return "break"

    def _on_key(self, evt):
        ch = evt.char.upper()
        if ch in ("?", "S", "R", "T", "D"):
            self.source.send(ch)

    def _on_close(self):
        self.source.close()
        if self.siren:
            self.siren.close()
        self.destroy()


# ----------------------------------------------------------------------------
def list_ports():
    try:
        from serial.tools import list_ports as lp
    except ImportError:
        print("pyserial is not installed.  pip install pyserial")
        return
    found = list(lp.comports())
    if not found:
        print("No serial ports found. Pair the HC-05 in Windows Bluetooth settings "
              "first;\nit then appears as an outgoing COM port.")
        return
    print("Serial ports:")
    for p in found:
        print("  %-8s %s" % (p.device, p.description))


def main():
    ap = argparse.ArgumentParser(description="SWIS live simulation GUI")
    ap.add_argument("--port", help="COM port of the paired HC-05, e.g. COM5")
    ap.add_argument("--baud", type=int, default=9600)
    ap.add_argument("--demo", action="store_true", help="run with no hardware")
    ap.add_argument("--list", action="store_true", help="list serial ports and exit")
    ap.add_argument("--sound", action="store_true", help="also beep the PC speaker")
    args = ap.parse_args()

    if args.list:
        list_ports()
        return 0
    if not args.port and not args.demo:
        ap.error("give --port COMx, or --demo to run without hardware "
                 "(--list shows the ports)")

    q = queue.Queue()
    source = DemoSource(q) if args.demo else SerialSource(args.port, args.baud, q)
    source.start()

    app = App(source, use_sound=args.sound)
    app.mainloop()
    return 0


if __name__ == "__main__":
    sys.exit(main())
