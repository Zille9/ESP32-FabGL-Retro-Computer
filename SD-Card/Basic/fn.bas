10 DEFN A(A,B)=[SIN(A)*COS(B)]
20 DEFN B(C,D)=[C*D]
25 FOR I=5 TO 1 STEP-1
26 FOR F=1 TO 255
30 Z=FN A(I,F):W=FN B(F,I)
35 P=GET(1)
40 POS 13,P:PRINT W,Z
50 NEXT F
60 NEXT I
