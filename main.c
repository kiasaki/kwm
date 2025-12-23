#include <stdlib.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>

#define stk(s)      XKeysymToKeycode(dpy, XStringToKeysym(s))
#define keys(k, _)  XGrabKey(dpy, stk(k), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
#define map(k, x)   if (ev.xkey.keycode == stk(k)) { x; }
#define TBL(x)  x("F1", XCloseDisplay(dpy);free(clients);exit(0)) \
  x("Tab", focus_next()) \
  x("q", kill_window()) \
  x("m", maximize_window()) \
  x("t", start("st")) \
  x("w", start("mychromium")) \
  x("space", start("dmenu_run")) \
  x("y", start("amixer -q set Master toggle")) \
  x("u", start("amixer -q set Master 5%%- unmute")) \
  x("i", start("amixer -q set Master 5%%+ unmute")) \
  x("o", start("bri -")) \
  x("p", start("bri +"))

static Display *dpy;
static Window root;
static int screen;
static Window *clients = NULL;
static int nclients = 0;
static int current = -1;
static int drag_start_x, drag_start_y;
static int win_start_x, win_start_y;
static unsigned int win_start_w, win_start_h;
static Window drag_win = None;
static int resizing = 0;

static void start(char *name) {
  if (fork() == 0) {
    if (dpy) close(ConnectionNumber(dpy));
    setsid();
    system(name);
  }
}

static void add_client(Window w) {
  clients = realloc(clients, sizeof(Window) * (nclients + 1));
  clients[nclients++] = w;
  current = nclients - 1;
  XSetWindowBorderWidth(dpy, w, 1);
  XSetWindowBorder(dpy, w, BlackPixel(dpy, screen));
  XSelectInput(dpy, w, EnterWindowMask);
  XGrabButton(dpy, 1, AnyModifier, w, True, ButtonPressMask, GrabModeSync, GrabModeAsync, None, None);
}

static void remove_client(Window w) {
  for (int i = 0; i < nclients; i++) {
    if (clients[i] == w) {
      for (int j = i; j < nclients - 1; j++)
        clients[j] = clients[j + 1];
      nclients--;
      if (current >= nclients) current = nclients - 1;
      return;
    }
  }
}

static void maximize_window() {
  Window focused;
  int revert;
  XGetInputFocus(dpy, &focused, &revert);
  if (focused != root && focused != PointerRoot) {
    XMoveResizeWindow(dpy, focused, 0, 0,
      DisplayWidth(dpy, screen), DisplayHeight(dpy, screen));
  }
}

static void kill_window() {
  Window focused;
  int revert;
  XGetInputFocus(dpy, &focused, &revert);
  if (focused != root && focused != PointerRoot)
    XKillClient(dpy, focused);
}

static void focus_next() {
  if (nclients == 0) return;
  current = (current + 1) % nclients;
  XRaiseWindow(dpy, clients[current]);
  XSetInputFocus(dpy, clients[current], RevertToPointerRoot, CurrentTime);
}

int main() {
  XEvent ev;
  XWindowAttributes attr;
  if (!(dpy = XOpenDisplay(NULL))) return 1;
  screen = DefaultScreen(dpy);
  root = RootWindow(dpy, screen);
  XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask);
  XDefineCursor(dpy, root, XCreateFontCursor(dpy, XC_left_ptr));
  // grab events
  TBL(keys);
  XGrabButton(dpy, 1, Mod4Mask, root, True, ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
              GrabModeAsync, GrabModeAsync, None, None);
  XGrabButton(dpy, 1, Mod4Mask | ShiftMask, root, True, ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
              GrabModeAsync, GrabModeAsync, None, None);
  // scan existing windows
  Window root_ret, parent_ret;
  Window *children;
  unsigned int nchildren;
  if (XQueryTree(dpy, root, &root_ret, &parent_ret, &children, &nchildren)) {
    for (unsigned int i = 0; i < nchildren; i++) {
      XWindowAttributes attr;
      if (XGetWindowAttributes(dpy, children[i], &attr)
          && attr.map_state == IsViewable && !attr.override_redirect) {
        add_client(children[i]);
      }
    }
    if (children) XFree(children);
  }
  
  while (1) {
    XNextEvent(dpy, &ev);
    switch (ev.type) {
    case MapRequest:
        XMapWindow(dpy, ev.xmaprequest.window);
        add_client(ev.xmaprequest.window);
        XSetInputFocus(dpy, ev.xmaprequest.window, RevertToPointerRoot, CurrentTime);
        break;
    case DestroyNotify:
        remove_client(ev.xdestroywindow.window);
        break;
    case UnmapNotify:
        remove_client(ev.xunmap.window);
        break;
    case KeyPress:
        TBL(map);
        break;
    case ButtonPress:
        if (ev.xbutton.subwindow != None) {
          XGetWindowAttributes(dpy, ev.xbutton.subwindow, &attr);
          drag_start_x = ev.xbutton.x_root;
          drag_start_y = ev.xbutton.y_root;
          win_start_x = attr.x;
          win_start_y = attr.y;
          win_start_w = attr.width;
          win_start_h = attr.height;
          drag_win = ev.xbutton.subwindow;
          resizing = (ev.xbutton.state & ShiftMask) ? 1 : 0;
          XRaiseWindow(dpy, drag_win);
        } else if (ev.xbutton.window != root) {
          XRaiseWindow(dpy, ev.xbutton.window);
          XSetInputFocus(dpy, ev.xbutton.window, RevertToPointerRoot, CurrentTime);
          XAllowEvents(dpy, ReplayPointer, CurrentTime);
        }
        break;
    case MotionNotify:
      if (drag_win != None) {
        int xdiff = ev.xmotion.x_root - drag_start_x;
        int ydiff = ev.xmotion.y_root - drag_start_y;
        if (resizing) {
          int neww = win_start_w + xdiff;
          int newh = win_start_h + ydiff;
          if (neww > 10 && newh > 10)
            XResizeWindow(dpy, drag_win, neww, newh);
        } else {
          XMoveWindow(dpy, drag_win, win_start_x + xdiff, win_start_y + ydiff);
        }
      }
      break;
    case ButtonRelease:
      drag_win = None;
      break;
    case EnterNotify:
      if (ev.xcrossing.window != root) {
        XSetInputFocus(dpy, ev.xcrossing.window, RevertToPointerRoot, CurrentTime);
      }
      break;
    }
  }
  return 0;
}
