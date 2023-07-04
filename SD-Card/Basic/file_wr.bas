10 CLS
11 A$="Das ist Nummer:"
20 OPEN"/Neu.txt",W
30 FOR I=1 TO 255
40 FILE_WR A$,I,SIN(I)
45 PRINT I
50 NEXT I
55 FILE_PS(0)
56 FILE_WR"Diese Zeile wurde ueberschrieben"
60 CLOSE
