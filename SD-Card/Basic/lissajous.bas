10 CLS
20 INPUT"Y-Frequenz:";Y
30 INPUT"X-Frequenz:";X
50 S=X*Y*100
60 CLS
100 FOR A=0 TO 2*PI() STEP 2*PI()/S
110 PSET 75*SIN(A*X)+150,50*COS(A*Y)+100
120 NEXT A
200 END
