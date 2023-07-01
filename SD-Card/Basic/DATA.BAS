10 CLS 
20 RESTORE
30 INPUT"Alter:";ALTER
35 PRINT"Aelter als ";ALTER;" Jahre sind:"
36 PRINT
40 READ NAME$,JAHR
50 IF ALTER<JAHR THEN PRINT NAME$;TAB(12);JAHR
60 B=B+1
70 IF B<12 THEN GOTO 40
100 DATA "Zille",54,"Moni",58
110 DATA "Roger",50,"Christian",52
120 DATA "Micha",30,"Enrico",35
130 DATA "Kalle",64,"Thomas",42
140 DATA "Stefanie",32,"Julia",25
150 DATA "Frank",25,"Harald",21
