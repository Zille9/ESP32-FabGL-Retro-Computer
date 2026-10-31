* VERSION BY DEFAULT PLAYS FROM GROM AT >6000
* CHECK CODE FOR 'GROM' TAGS TO CHANGE TO ROM/RAM

	DEF MAIN
*volatile bool step;	// Has to be volatile since it's modified in the NMI handler.
*unsigned char *pPlayList=NULL;
*unsigned char nPlayDelay=0;

	
STEP EQU >8300
PPLAYLIST EQU >8302
NPLAYDELAY EQU >8304
GROMBASE EQU >8306

	AORG >6000
* ROM HEADER
	DATA >AA01, 0, 0, TITLE, 0
TITLE
	DATA 0, MAIN
	BYTE 6
	TEXT 'PLAYER'
	EVEN

ONEBYTE DATA >0100

VWAD
	SWPB R0
	MOVB R0,@>8C02
	SWPB R0
	MOVB R0,@>8C02
	B *R11

*// code starts
*void main(void)
MAIN
*{
*	stopmusic();
	BL @STOPMUSIC

* Not part of the original C program here
* Setup bitmap mode and load the image from GROM!
* Routine from my old 'MOS' attempt ;)
* Note that this overwrites vectors at the top of
* VDP RAM that the disk system needs to use. I 
* used to save off >80 bytes from >3780 to work
* around that.
GFERIT 
	LI R0,>81A0
	BL @VWAD
	LI R0,>8002
	BL @VWAD
	LI R0,>8206
	BL @VWAD
	LI R0,>83FF
	BL @VWAD
	LI R0,>8403
	BL @VWAD
	LI R0,>8536
	BL @VWAD
	LI R0,>5B00
	BL @VWAD
	LI R1,>D000
	MOVB R1,@>8C00

	LI R0,>5800
	BL @VWAD
	CLR R2
NQ# 
	CLR R1
LP# 
	MOVB R1,@>8C00
	AI R1,>0100
	CI R1,>0000
	JNE LP#
	INC R2
	CI R2,3
	JNE NQ#

* Screen is disabled
* P data at VDP >0000 from GROM base 15 >6000
* C data at VDP >2000 from GROM base 15 >8000
* GROM BASE	
	LI R4,>983C
* VDP 'BASE'
	LI R3,>8C00
* SET GROM ADDRESS
	LI R2,>6000
	MOVB R2,@>0402(R4)
	SWPB R2
	MOVB R2,@>0402(R4)
	SWPB R2
* SET VDP ADDRESS
	LI R2,>4000
	SWPB R2
	MOVB R2,@>0002(R3)
	SWPB R2
	MOVB R2,@>0002(R3)
* COPY 6K (>1800 BYTES)
* QUESTION - VDP IS OFF - HOW FAST CAN WE WRITE ON A REAL TI?
* UNROLLING THE LOOP A BIT FOR NOW. EVEN SO, THE SLOW GROM
* SHOULD BE ENOUGH FOR THE VDP, EH?
	LI R0,1536
CP1
	MOVB *R4,*R3
	MOVB *R4,*R3
	MOVB *R4,*R3
	MOVB *R4,*R3
	DEC R0
	JNE CP1

* SET GROM ADDRESS
	LI R2,>8000
	MOVB R2,@>0402(R4)
	SWPB R2
	MOVB R2,@>0402(R4)
	SWPB R2
* SET VDP ADDRESS
	LI R2,>6000
	SWPB R2
	MOVB R2,@>0002(R3)
	SWPB R2
	MOVB R2,@>0002(R3)
* COPY 6K (>1800 BYTES)
	LI R0,1536
CP2
	MOVB *R4,*R3
	MOVB *R4,*R3
	MOVB *R4,*R3
	MOVB *R4,*R3
	DEC R0
	JNE CP2

* ENABLE VIDEO
	LI R0,>81E2
	BL @VWAD

*	cv_set_vint_handler(nmi);
	LI R0,NMI
	MOV R0,@>83C4

*	startmusic(music);
*	LI R0,MUSIC
* GROM
	LI R0,>6000
*
	BL @STARTMUSIC

*	// run forever
*	for (;;) {
LP1
*		waitforstep();
	BL @WAITFORSTEP

* RESET SCREEN TIMEOUT (any value ok)
	LI R0,>4000
	MOV R0,@>83D6

	JMP LP1
*		}
*
*	}
*}

*// called on vblank from VDP
*void nmi(void)
NMI

*{
*	unsigned char nDat;
*
*	// release any waiting code
*	step = true;
	SETO @STEP

*	// handle music - format ripped from an MSX emulator:
*	// first byte: frame count (8 bits)
*	// second byte: data
*	// third byte: only if tone data in second byte
*	// repeat until frame count is in the future
*	if (NULL != pPlayList) {
	MOV @PPLAYLIST,R2
	JEQ NMIOUT

*		nPlayDelay++;
	AB @ONEBYTE,@NPLAYDELAY

* GROM - SET BASE ADDRESS
	MOV @GROMBASE,R4
	MOVB R2,@>0402(R4)
	SWPB R2
	MOVB R2,@>0402(R4)
	SWPB R2
*

*		if (nPlayDelay == *(pPlayList)) {

	MOVB @NPLAYDELAY,R1
	CLR R0

*	MOVB *R2,R0
* GROM
	MOVB *R4,R0
*

	CB R0,R1
	JNE NMIOUT
	JMP NMIPOSTLP

*			while (nPlayDelay==*(pPlayList)) {
NMILOOP
	CLR R0

*	MOVB *R2,R0
* GROM
	MOVB *R4,R0
*
	CB R0,R1
	JNE NMIOUT
NMIPOSTLP
	
*				nDat=*(++pPlayList);
	INC R2
	JNE NOLOOP1
* WRAP AROUND TO NEXT GROM BASE
	INCT R4
	INCT R4
	LI R2,>6000
	MOVB R2,@>0402(R4)
	SWPB R2
	MOVB R2,@>0402(R4)
	SWPB R2
	MOV R4,@GROMBASE
NOLOOP1

*	MOVB *R2,R3
* GROM
	MOVB *R4,R3	

*				if (0 == nDat) {
	JNE NOTDONE

*					pPlayList=NULL;
	CLR R2
*					break;
*				}
	JMP NMIOUT
NOTDONE

*				switch (nDat&0x90) {
	MOV R3,R0
	ANDI R0,>9000

*					case 0x80:		// load sample
	CI R0,>8000
	JNE NOT8000

*						audport=nDat;
	MOVB R3,@>8400

*						if ((nDat&0xe0) != 0xe0) {
	MOV R3,R0
	ANDI R0,>E000
	CI R0,>E000
	JEQ DONESWITCH

*							audport=*(++pPlayList);
	INC R2
	JNE NOLOOP2
* WRAP AROUND TO NEXT GROM BASE
	INCT R4
	INCT R4
	LI R2,>6000
	MOVB R2,@>0402(R4)
	SWPB R2
	MOVB R2,@>0402(R4)
	SWPB R2
	MOV R4,@GROMBASE
NOLOOP2

* GROM
*	MOVB *R2,@>8400
	MOVB *R4,@>8400
*

*						}
*						break;
	JMP DONESWITCH

*					case 0x90:		// load volume
NOT8000
	CI R0,>9000
	JNE NOT9000

*						audport=nDat;
	MOVB R3,@>8400

*						break;
	JMP DONESWITCH

*					default:		// don't know what this is, should abort
NOT9000

*						pPlayList=NULL;
	CLR R2
	JMP NMIOUT
*						break;
*				}

DONESWITCH
*				// point to next frame count
*				pPlayList++;
	INC R2
	JNE NOLOOP3
* WRAP AROUND TO NEXT GROM BASE
	INCT R4
	INCT R4
	LI R2,>6000
	MOVB R2,@>0402(R4)
	SWPB R2
	MOVB R2,@>0402(R4)
	SWPB R2
	MOV R4,@GROMBASE
NOLOOP3

	JMP NMILOOP
*			}
*		}
*	}
NMIOUT
	MOV R2,@PPLAYLIST
	B *R11
*}

*// stops sfx too
*void stopmusic() {
STOPMUSIC

*	pPlayList=NULL;
	CLR @PPLAYLIST

*	cv_set_attenuation(CV_SOUNDCHANNEL_0, VOL_OFF);
	LI R0,>9F00
	MOVB R0,@>8400

*	cv_set_attenuation(CV_SOUNDCHANNEL_1, VOL_OFF);
	LI R0,>BF00
	MOVB R0,@>8400

*	cv_set_attenuation(CV_SOUNDCHANNEL_2, VOL_OFF);
	LI R0,>DF00
	MOVB R0,@>8400

*	cv_set_attenuation(CV_SOUNDCHANNEL_NOISE, VOL_OFF);
	LI R0,>FF00
	MOVB R0,@>8400
*}
	B *R11

*void startmusic(unsigned char *pDat) {
STARTMUSIC

*	// order here is important
*	if (NULL != pPlayList) {
	MOV @PPLAYLIST,R1
	JEQ START2

*		stopmusic();
	MOV R11,R1
	BL @STOPMUSIC
	MOV R1,R11

*	}
START2

*	nPlayDelay=(*pDat)-1;
	CLR R1

* GROM
*	MOVB *R0,R1
	LI R1,>9800
	MOV R1,@GROMBASE
	MOVB R0,@>0402(R1)
	SWPB R0
	MOVB R0,@>0402(R1)
	SWPB R0
	MOVB *R1,R1
*

	SWPB R1
	DEC R1
	MOV R1,@NPLAYDELAY

*	pPlayList=pDat;
	MOV R0,@PPLAYLIST
*}
	B *R11

*void waitforstep() {
WAITFORSTEP

*	while (!step);
WAITLP
	LIMI 2
	LIMI 0
	MOV @STEP,R0
	JEQ WAITLP

*	step=false;
	CLR @STEP

*}
	B *R11

	END