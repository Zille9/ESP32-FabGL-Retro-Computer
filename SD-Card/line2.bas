90 INPUT"Ordnung :";N
100 CLS
110 XA=160+80*SIN(W):YA=100-50*COS(W)
120 XB=160+159*SIN(WW):YB=100-99*COS(WW)
130 LINE XA,YA,XB,YB
140 W=W+PI()/5:WW=WW+PI()/5/N
150 IF WW<2*PI() THEN GOTO 110