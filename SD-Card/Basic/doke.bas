10 T=TIMER
20 FOR I=0 TO 32768 STEP 2
30 DOKE 1,I,0
40 NEXT I
50 PRINT (TIMER-T)/1000
