5 TI=TIMER
10 CLS
20 FOR R=50 TO 100
30 FOR T=0 TO 2*PI() STEP 0.007
40 RT=8+R*(SIN(T*6+R*PI()/2))
50 X=RT*COS(T)
60 Y=RT*SIN(T)
65 F=32
66 PEN T*10+1
70 PSET 160+X,120+Y
80 NEXT T
90 NEXT R
100 PRINT AT(0,0);(TIMER-TI)/1000;" sek."
