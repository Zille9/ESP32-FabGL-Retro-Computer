10 A=TIMER
20 B=B+1
30 IF TIMER-A>999 THEN GOTO 60
40 GOTO 20
60 PRINT B*3;" Zeilen/sek."
70 END
