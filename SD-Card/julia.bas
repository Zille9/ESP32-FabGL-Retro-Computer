5 TI=TIMER
7 CUR 0
10 CLS
20 A=-1.3
30 B=-0.98
40 C=-0.78
50 D=-0.20
60 E=80
70 F=240/320
80 G=2.80
90 H=G/320
100 I=G*F:I=I/240
110 FOR X=0 TO 319
120 CX=X*H+A
130 FOR Y=0 TO 239
140 CY=Y*I+B
150 ZR=CX
160 ZI=CY
170 N=0
180 IF N>E THEN GOTO 300
190 IF (ZR^2)-(ZI^2)>4 THEN GOTO 300
200 NZ=(ZR^2)-(ZI^2)+C
210 NI=2*ZR*ZI+D
220 ZR=NZ
230 ZI=NI
240 N=N+1
255 Z=ZR*ZR-ZI*ZI
280 GOTO 180
300 PSET X,Y,N+19
330 NEXT Y
340 NEXT X
345 PEN 62
350 PRINT (TIMER-TI)/1000/60;" Minuten"
355 CUR 1
360 GOTO 360
