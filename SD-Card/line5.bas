100 INPUT"Symmetrie:";N
110 CLS:RA=99:RB=50:DW=PI()/N
120 FOR I=0 TO 5
130 RC=99-RA:RD=99-RB
140 FOR W=DW*2 TO 2*PI()+DW*2 STEP DW*4
150 LINE 160+RC*COS(W),100-RC*SIN(W),160+RD*COS(W+DW),100-RD*SIN(W+DW)
155 LINE 160+RC*COS(W),100-RC*SIN(W),160+RD*COS(W-DW),100-RD*SIN(W-DW)
160 NEXT W
170 RA=RA/2:RB=RB/2:DW=DW/2:NEXT I