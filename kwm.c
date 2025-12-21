#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <stdlib.h>
#include <unistd.h>

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
        setsid();
        execlp(name, name, NULL);
    }
}

static void add_client(Window w) {
    clients = realloc(clients, sizeof(Window) * (nclients + 1));
    clients[nclients++] = w;
    current = nclients - 1;
    XSetWindowBorderWidth(dpy, w, 1);
    XSetWindowBorder(dpy, w, BlackPixel(dpy, screen));
    XSelectInput(dpy, w, EnterWindowMask);
    XGrabButton(dpy, 1, AnyModifier, w, True, ButtonPressMask,
                GrabModeSync, GrabModeAsync, None, None);
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

static void focus_next(void) {
    if (nclients == 0) return;
    current = (current + 1) % nclients;
    XRaiseWindow(dpy, clients[current]);
    XSetInputFocus(dpy, clients[current], RevertToPointerRoot, CurrentTime);
}

static void scan_existing_windows(void) {
    Window root_ret, parent_ret;
    Window *children;
    unsigned int nchildren;
    
    if (XQueryTree(dpy, root, &root_ret, &parent_ret, &children, &nchildren)) {
        for (unsigned int i = 0; i < nchildren; i++) {
            XWindowAttributes attr;
            if (XGetWindowAttributes(dpy, children[i], &attr) &&
                attr.map_state == IsViewable &&
                !attr.override_redirect) {
                add_client(children[i]);
            }
        }
        if (children) XFree(children);
    }
}

static void grab_keys(void) {
    unsigned int mods[] = {0, LockMask};
    
    for (int i = 0; i < 2; i++) {
        XGrabKey(dpy, XKeysymToKeycode(dpy, XK_q), Mod4Mask | mods[i], root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dpy, XKeysymToKeycode(dpy, XK_t), Mod4Mask | mods[i], root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dpy, XKeysymToKeycode(dpy, XK_space), Mod4Mask | mods[i], root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dpy, XKeysymToKeycode(dpy, XK_F1), Mod4Mask | mods[i], root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Tab), Mod4Mask | mods[i], root, True, GrabModeAsync, GrabModeAsync);
    }
    
    XGrabButton(dpy, 1, Mod4Mask, root, True, ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(dpy, 1, Mod4Mask | ShiftMask, root, True, ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                GrabModeAsync, GrabModeAsync, None, None);
}

int main(void) {
    XEvent ev;
    XWindowAttributes attr;
    
    if (!(dpy = XOpenDisplay(NULL))) return 1;
    
    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);
    
    XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask);
    XDefineCursor(dpy, root, XCreateFontCursor(dpy, XC_left_ptr));
    grab_keys();
    scan_existing_windows();
    
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
            if (ev.xkey.keycode == XKeysymToKeycode(dpy, XK_q) && (ev.xkey.state & Mod4Mask)) {
                Window focused;
                int revert;
                XGetInputFocus(dpy, &focused, &revert);
                if (focused != root && focused != PointerRoot)
                    XKillClient(dpy, focused);
            } else if (ev.xkey.keycode == XKeysymToKeycode(dpy, XK_t) && (ev.xkey.state & Mod4Mask)) {
              start("st");
            } else if (ev.xkey.keycode == XKeysymToKeycode(dpy, XK_space) && (ev.xkey.state & Mod4Mask)) {
              start("dmenu_run");
            } else if (ev.xkey.keycode == XKeysymToKeycode(dpy, XK_F1) && (ev.xkey.state & Mod4Mask)) {
                XCloseDisplay(dpy);
                free(clients);
                return 0;
            } else if (ev.xkey.keycode == XKeysymToKeycode(dpy, XK_Tab) && (ev.xkey.state & Mod4Mask)) {
                focus_next();
            }
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
