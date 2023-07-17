10 COL 63,0:CLS
20 N=6:D=20
30 R=120
40 A=PI()/N:B=A
100 FOR I=A TO 2*PI()-PI()/180 STEP 2*PI()/N
110 X=SIN(I):Y=COS(I):U=0:V=0
120 W=SIN(I-B):Z=COS(I-B):GOSUB 300
130 X=SIN(I):Y=COS(I):U=0:V=0
140 W=SIN(I+B):Z=COS(I+B):GOSUB 300
150 NEXT I
200 GOTO 20
300 FOR K=D TO 1 STEP -1
310 DRAW 150+(X*R),120+(Y*R),0
312 DRAW 150+(U*R),120+(V*R),1
314 DRAW 150+(W*R),120+(Z*R),1
320 XO=X:YO=Y:X=X-(X-U)/K:Y=Y-(Y-V)/K
330 U=U+(W-U)/K:V=V+(Z-V)/K
340 W=W+(XO-W)/K:Z=Z-(Z-YO)/K
350 NEXT K
355 F=RND(63)
356 IF F=0 THEN F=32
358 PEN F
360 RETURN