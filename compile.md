# How to compile

Terminal
```
gcc indoterm.c $(pkg-config --cflags --libs gtk+-3.0 vte-2.91) -o indoterm
```

Statusbar
```
gcc hyprbar.c $(pkg-config --cflags --libs gtk4 gtk4-layer-shell-0) -o hyprbar
```
