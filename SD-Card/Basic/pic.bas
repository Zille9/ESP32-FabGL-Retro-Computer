10 CUR0:CLS
20 PIC_L(0,"/globus.pic")
30 PIC_D(0)
40 PIC_L(1,"/iron.pic")
50 PIC_D(1)
60 PIC_L(2,"/simpson.pic")
70 PIC_D(2)
80 PIC_L(3,"/mandel.pic")
90 PIC_D(3)
100 PIC_L(4,"/spider.pic")
110 PIC_D(4)
120 PIC_L(5,"/lillith.pic")
130 PIC_D(5)
150 PAUSE 5000
160 FOR I=0 TO 5
170 PIC_D(I)
180 PAUSE 5000
190 NEXT I
200 GOTO 160
