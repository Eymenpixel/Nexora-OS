# Kaya Taskbar (GTK + X11 helpers)

Minimal Linux taskbar written in C++ using GTK3. Designed to run via an X server (like VcXsrv on Windows).

Build
```
cd packages/taskbar
./build.sh
```

Run (example, on WSL or Linux with VcXsrv running on Windows):
```
export DISPLAY=localhost:0.0   # or set in PowerShell: $env:DISPLAY="localhost:0.0"
./taskbar
```

Notes
- The program looks for the logo in likely locations (e.g. `Main/logo.svg`). If you put the attached logo file in `Main/logo.svg` it will be used.
- `wmctrl` is used to list windows; install it if you want the "Windows" list to work: `sudo apt install wmctrl`.
