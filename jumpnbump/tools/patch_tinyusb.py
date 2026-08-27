# =============================================================================
#  USBCollection - lokaler TinyUSB-Fork
# =============================================================================
#  Das Problem
#  -----------
#  Im RP2040/RP2350-USB-Geraetetreiber von TinyUSB steht in
#      hw_endpoint_abort_xfer()   (dcd_rp2040.c)
#  eine Warteschleife OHNE Zeitgrenze:
#
#      usb_hw_set->abort = abort_mask;
#      while ((usb_hw->abort_done & abort_mask) != abort_mask) {}
#
#  Diese Funktion wird u.a. aus reset_ep0() im USB-INTERRUPT aufgerufen - also
#  bei JEDEM eintreffenden SETUP-Paket. Setzt die Hardware das Flag abort_done
#  nicht, dreht sich der Interrupt endlos und der GESAMTE Chip steht still:
#  Bild eingefroren, USB tot, Hauptschleife nie wieder erreicht.
#
#  Auf der hier verwendeten Verbindung (zwei RP2350 direkt ueber D+/D-, ohne
#  VBUS, mit doppelten Serienwiderstaenden) kann ein verfaelschtes SETUP-Paket
#  genau das ausloesen. An einem PC passiert es praktisch nie - deshalb faellt
#  der Fehler im Originalcode nicht auf.
#
#  Die Loesung
#  -----------
#  patches/dcd_rp2040.c enthaelt die korrigierte Fassung (Schleife mit
#  Zeitgrenze). Da die Datei zum Framework-Paket gehoert und dort bei einer
#  Neuinstallation ueberschrieben wuerde, spielt dieses Skript sie VOR JEDEM
#  BUILD wieder ein. Beim ersten Mal wird das Original als .orig gesichert.
#
#  Wichtig: Massgeblich ist die Fassung in
#      libraries/Adafruit_TinyUSB_Arduino/src/portable/raspberrypi/rp2040/
#  denn mit -DUSE_TINYUSB wird DIESE kompiliert (die Kopie in pico-sdk/ liegt
#  vorkompiliert in libpico.a und kommt hier gar nicht zum Einsatz).
#
#  Rueckgaengig machen
#  -------------------
#  dcd_rp2040.c.orig zurueckkopieren und extra_scripts aus platformio.ini
#  entfernen - oder das Framework neu installieren (pio pkg install --force).
# =============================================================================
import os
import shutil

Import("env")  # noqa: F821  (von PlatformIO bereitgestellt)

SRC_ROOT = os.path.join("libraries", "Adafruit_TinyUSB_Arduino", "src")
RP = os.path.join("portable", "raspberrypi", "rp2040")

# Dateiname -> Unterverzeichnis. Alle Stellen, an denen der Stack unbegrenzt
# warten konnte (jede davon legt bei einem Haenger den GESAMTEN Chip still):
#   dcd_rp2040.c  Geraeteseite: abort_done-Schleife (aus dem SETUP-Interrupt),
#                 Puffer-Schleifen im Interrupt (innen UND aussen) + Schutz
#                 gegen Interrupt-Sturm
#   rp2040_usb.c  gemeinsam: STOP_TRANS- und abort_done-Schleife
#   hcd_rp2040.c  Hostseite: sie_stop_xfer (im Original als "not safe, can be
#                 racing" kommentiert), EPX-Schleife + Interrupt-Sturm-Schutz
#   usbh.c        Hostkern: blockierender Control-Transfer OHNE Timeout - im
#                 Original als "TODO probably some timeout to prevent hanged"
#                 vermerkt. Bleibt eine Antwort aus, haengt tuh_task() ewig.
FILES = {
    "dcd_rp2040.c": RP,
    "rp2040_usb.c": RP,
    "hcd_rp2040.c": RP,
    "usbh.c":       "host",
}
MARKER = "USBCollection-PATCH"


def _log(msg):
    print("[tinyusb-fork] %s" % msg)


def _sync(patched, target, name):
    """Eine Datei einspielen. Rueckgabe: True = geaendert."""
    with open(patched, "r", encoding="utf-8", errors="replace") as fh:
        wanted = fh.read()
    if MARKER not in wanted:
        _log("WARNUNG: patches/%s enthaelt die Korrektur nicht!" % name)
        return False
    with open(target, "r", encoding="utf-8", errors="replace") as fh:
        current = fh.read()
    if current == wanted:
        return False
    backup = target + ".orig"
    if not os.path.exists(backup) and MARKER not in current:
        shutil.copyfile(target, backup)
        _log("Original gesichert: %s.orig" % name)
    shutil.copyfile(patched, target)
    _log("eingespielt: %s" % name)
    return True


def main():
    project_dir = env["PROJECT_DIR"]                     # noqa: F821
    try:
        pkg_dir = env.PioPlatform().get_package_dir("framework-arduinopico")  # noqa: F821
    except Exception as exc:                             # pragma: no cover
        _log("Framework-Pfad nicht ermittelbar (%s)" % exc)
        return
    if not pkg_dir:
        _log("Framework nicht installiert")
        return

    changed = 0
    for name, subdir in FILES.items():
        patched = os.path.join(project_dir, "patches", name)
        target = os.path.join(pkg_dir, SRC_ROOT, subdir, name)
        if not os.path.isfile(patched):
            _log("patches/%s fehlt - uebersprungen!" % name)
            continue
        if not os.path.isfile(target):
            _log("Zieldatei nicht gefunden: %s" % target)
            continue
        if _sync(patched, target, name):
            changed += 1

    if changed:
        _log("%d Datei(en) gepatcht - keine unbegrenzten USB-Warteschleifen mehr" % changed)
    else:
        _log("Fork aktiv (alle USB-Warteschleifen zeitbegrenzt) - nichts zu tun")


main()
