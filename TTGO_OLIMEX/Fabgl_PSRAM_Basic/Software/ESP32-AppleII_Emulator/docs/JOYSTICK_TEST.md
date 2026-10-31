# Joystick test program

The Apple II joystick is emulated with a **PS/2 mouse** plugged into the board's
second PS/2 jack (the mouse port, GPIO 26/27 on the TTGO VGA32 v1.4).

| Mouse | Apple II |
|---|---|
| move left / right | paddle 0 (`PDL(0)`), 0 to 255 |
| move up / down | paddle 1 (`PDL(1)`), 0 at the top, 255 at the bottom |
| left button | pushbutton 0 (`$C061`), same as Left Alt |
| right button | pushbutton 1 (`$C062`), same as Right Alt |
| middle button, or **F3** | centre the stick (127, 127) |

A mouse does not spring back to the middle the way a joystick does: the stick
stays where you left it until you centre it.

## The program

Type it in at the Applesoft `]` prompt on the ][+ or the //e, then `RUN`.

```basic
10 GR : HOME : PX = 0 : PY = 0
20 X = PDL(0) : Y = PDL(1)
30 B0 = PEEK(-16287) > 127 : B1 = PEEK(-16286) > 127
40 NX = INT(X * 39 / 255) : NY = INT(Y * 39 / 255)
50 COLOR= 0 : PLOT PX,PY
60 C = 15 : IF B0 THEN C = 1
70 IF B1 THEN C = 6
80 COLOR= C : PLOT NX,NY : PX = NX : PY = NY
90 VTAB 22 : HTAB 1 : PRINT "X=";X;"  ";TAB(10);"Y=";Y;"  ";TAB(20);"B0=";B0;" B1=";B1;"  "
100 GOTO 20
```

`PEEK(-16287)` is `$C061` (pushbutton 0) and `PEEK(-16286)` is `$C062`
(pushbutton 1); bit 7 set means pressed. Press **Ctrl+C** to stop it and `TEXT`
to get the full text screen back.

## What you should see

- A white dot in the lo-res area, starting in the middle (`X=127 Y=127`).
- Moving the mouse right and down takes the dot to the right edge and the
  bottom, and `X` / `Y` up to 255; left and up take them down to 0.
- Holding the **left** button turns the dot magenta and shows `B0=1`.
- Holding the **right** button turns the dot dark blue and shows `B1=1`.
- The **middle** button (or **F3**) puts the dot back in the middle.

If `X` and `Y` stay at 127 and the buttons do nothing, check the boot log on the
serial port for `[mouse] joystick: absent` — the mouse was not found at power-on.
PS/2 devices are only detected at boot, so plug the mouse in before powering up.
