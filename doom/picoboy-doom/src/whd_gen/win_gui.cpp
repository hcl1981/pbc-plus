// Windows-only: nativer "Datei auswaehlen"-Dialog + Meldungsbox ueber die
// EINGEBAUTEN System-DLLs (comdlg32 / user32). Nichts muss nachinstalliert
// werden. Auf anderen Plattformen ist diese Datei leer.
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>

// Zeigt den Datei-Oeffnen-Dialog. Bei Auswahl: 1 + Pfad in buf. Abbruch: 0.
extern "C" int whd_win_pick_wad(char *buf, int buflen) {
    if (buflen > 0) buf[0] = 0;
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = NULL;
    ofn.lpstrFilter = "Doom-WAD (*.wad)\0*.wad;*.WAD\0Alle Dateien (*.*)\0*.*\0";
    ofn.lpstrFile   = buf;
    ofn.nMaxFile    = (DWORD)buflen;
    ofn.lpstrTitle  = "Doom-WAD auswaehlen";
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    return GetOpenFileNameA(&ofn) ? 1 : 0;
}

extern "C" void whd_win_msgbox(const char *title, const char *text) {
    MessageBoxA(NULL, text, title, MB_OK);
}
#endif
