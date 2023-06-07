10 B=TIMER
30 K=K+1
40 A=K/2*3+4-5
50 IF K<1000 THEN GOTO 30
55 C=TIMER-B
60 PRINT"Ausfuehrungszeit:";C;" millisek."
65 PRINT"das entspricht ";INT((1000/C)*K*3);" Zeilen/sek."
