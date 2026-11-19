#!/usr/bin/env python3
"""Makes src/speech_data.c: the game's few spoken shouts.

    tools/speech.py          needs espeak (apt install espeak)

espeak speaks each phrase into a WAV; here it is cut at the silence, brought
to full level, halved to 11025 Hz and written out as bytes. The result is
checked in, so building the game needs neither espeak nor this script.
"""
import os, struct, subprocess, sys, tempfile, wave

HERE = os.path.dirname(os.path.abspath(__file__))
PHRASES = [                 # name, then for english and deutsch: voice, speed, text
    ("STOP",   ("en-us+m3", 165, "Stop! Police!"),         ("de+m3", 165, "Halt! Polizei!")),
    ("PULL",   ("en-us+m3", 170, "Pull over!"),            ("de+m3", 170, "Rechts ran!")),
    ("ARREST", ("en-us+m3", 160, "You are under arrest."), ("de+m3", 160, "Sie sind festgenommen.")),
    ("HEY",    ("en-us+m5", 190, "Hey!"),                  ("de+m5", 190, "Hey!")),
    ("WATCH",  ("en-us+f3", 190, "Watch it!"),             ("de+f3", 190, "Pass doch auf!")),
    ("TAXI",   ("en-us+m1", 170, "Taxi!"),                 ("de+m1", 170, "Taxi!")),
]

def speak(voice, speed, text):
    with tempfile.NamedTemporaryFile(suffix=".wav") as f:
        subprocess.run(["espeak", "-v", voice, "-s", str(speed), "-a", "200", "-w", f.name, text], check=True)
        w = wave.open(f.name)
        assert w.getnchannels() == 1 and w.getsampwidth() == 2, "espeak's WAV is not 16 bit mono"
        rate = w.getframerate()
        data = struct.unpack("<%dh" % w.getnframes(), w.readframes(w.getnframes()))
    step = rate / 11025.0                       # to 11025 Hz, averaging what falls together
    out, i = [], 0.0
    while int(i + step) <= len(data):
        chunk = data[int(i):int(i + step)] or data[int(i):int(i) + 1]
        out.append(sum(chunk) / len(chunk))
        i += step
    loud = [k for k, v in enumerate(out) if abs(v) > 600]
    out = out[max(0, loud[0] - 40):loud[-1] + 200] if loud else out
    peak = max(1.0, max(abs(v) for v in out))
    return bytes(max(0, min(255, int(128 + v / peak * 120))) for v in out)

def main():
    lines = ["/* speech_data.c -- written by tools/speech.py (espeak), do not edit.",
             " * Unsigned 8 bit, 11025 Hz. */", '#include "rt.h"', "", "#if RT_SPEECH", ""]
    table = [[], []]
    for name, *both in PHRASES:
        for lang, (voice, speed, text) in enumerate(both):
            pcm = speak(voice, speed, text)
            var = "say_%s_%s" % (name.lower(), "de" if lang else "en")
            lines.append("/* \"%s\" */" % text)
            lines.append("static const uint8_t %s[%d] = {" % (var, len(pcm)))
            for k in range(0, len(pcm), 20):
                lines.append("    " + ",".join(str(b) for b in pcm[k:k + 20]) + ",")
            lines.append("};")
            table[lang].append("{ %s, sizeof %s }" % (var, var))
            print("%-7s %s %5d bytes  %s" % (name, "de" if lang else "en", len(pcm), text))
    lines += ["", "const speech_t speech[2][SAY_COUNT] = {"]
    lines += ["    { " + ", ".join(t) + " }," for t in table]
    lines += ["};", "", "#endif", ""]
    open(os.path.join(HERE, "..", "src", "speech_data.c"), "w").write("\n".join(lines))

if __name__ == "__main__":
    sys.exit(main())
