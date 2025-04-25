# How to compile

Terminal
```
gcc vte-terminal.c $(pkg-config --cflags --libs gtk+-3.0 vte-2.91) -o vte-terminal
```

Statusbar
```
gcc hyprbar.c $(pkg-config --cflags --libs gtk4 gtk4-layer-shell-0) -o hyprbar
```
