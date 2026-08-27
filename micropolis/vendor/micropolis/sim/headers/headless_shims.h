#pragma once
/* Headless (embedded) replacement for the X11/Tcl/Tk stack.
   The Micropolis simulation core never touches these GUI fields; the
   typedefs only need to exist so the GUI-flavoured structs in view.h
   still compile when X11/Tk are absent. */
#include <stdint.h>

/* X11 scalar handles */
typedef unsigned long Window, Colormap, Pixmap, Drawable, Atom, Cursor, KeySym, XID;
typedef void *GC;

/* X11 opaque structs (referenced only via pointer in the sim build) */
typedef struct _Display  Display;
typedef struct _Visual   Visual;
typedef struct _Screen   Screen;
typedef struct _XImage   XImage;
typedef struct _XColor   XColor;
typedef struct _XEvent   XEvent;
typedef struct _XFontStruct XFontStruct;

/* Tk / Tcl opaque */
typedef struct _TkDisplay TkDisplay;
typedef void *Tk_Window;
typedef struct Tcl_Interp Tcl_Interp;

/* additional GUI types referenced by value/array in view.h */
typedef unsigned long Time;
typedef void *Tk_TimerToken;
typedef void *Tk_3DBorder;
typedef struct { short x, y; } XPoint;
typedef struct { int shmid; char *shmaddr; int readOnly; } XShmSegmentInfo;

/* Tk internal type touched only by the chalk/annotation tool (never called
   in the headless build) — just needs to satisfy the compiler. */
typedef struct { unsigned long lastEventTime; } _TkDispShim;
typedef struct { _TkDispShim *dispPtr; } TkWindow;

/* Pointer-returning frontend stub(s) the vendored tool layer calls without a
   prototype — declare here so the return value isn't truncated to int. */
extern char *Tk_PathName();
