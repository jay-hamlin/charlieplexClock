
void WriteSegment(char seg);
void WriteNumber(char num);

#define   REVERSE_VIEW      0
#define   WRITE_WHOLE_BYTES 1

unsigned char    segment;
unsigned char    number;
int     loopCounter;

/* A 7 segment LED display is Charlie plexed on Arduino pins 2,3,4,5,6,7 as follows:
                anode(+)    cathode (-)
    segment A   PIN 5       PIN 4
    segment B   PIN 4       PIN 3
    segment C   PIN 3       PIN 2
    segment D   PIN 7       PIN 2
    segment E   PIN 6       PIN 7
    segment F   PIN 5       PIN 6
    segment G   PIN 6       PIN 3

    The arduino pin numbers 2-7 map to PORTD bits 2-7

*/

/*  Working out the swaps for reversing (looking from the back side) of the display
 * The reverse view lookup swaps (F and B)  and (E and C)
 *          F E   C B
 *  0:  0 0 1 1 1 1 1 1   = 0x3F
 *          same
 *  1:  0 0 0 0 0 1 1 0   = 0x06
 *   rev0 0 1 1 0 0 0 0   = 0x30
 *  2:  0 1 0 1 1 0 1 1   = 0x5B
 *   rev0 1 1 0 1 1 0 1   = 0x6D
 *  3:  0 1 0 0 1 1 1 1   = 0x4F
 *   rev0 1 1 1 1 0 0 1   = 0x79
 *  4:  0 1 1 0 0 1 1 0   = 0x66
 *   rev0 1 1 1 0 0 1 0   = 0x72
 *  5:  0 1 1 0 1 1 0 1   = 0x6D
 *   rev0 1 0 1 1 0 1 1   = 0x5B
 *  6:  0 1 1 1 1 1 0 1   = 0x7D
 *   rev0 1 0 1 1 1 1 1   = 0x5F
 *  7:  0 0 0 0 0 1 1 1   = 0x07
 *   rev0 0 1 1 0 0 0 1   = 0x31
 *  8:  0 1 1 1 1 1 1 1   = 0x7F
 *          same
 *  9:  0 1 1 0 0 1 1 1   = 0x67
 *   rev0 1 1 1 0 0 1 1   = 0x73
 *  A:  0 1 1 1 0 1 1 1   = 0x77
 *           same
 *  B:  0 1 1 1 1 1 0 0   = 0x7C
 *   rev0 1 0 1 1 1 1 0   = 0x5E
 *  C:  0 0 1 1 1 0 0 1   = 0x39
 *   rev0 0 0 0 1 1 1 1   = 0x0F
 *  D:  0 1 0 1 1 1 1 0   = 0x5E
 *   rev0 1 1 1 1 1 0 0   = 0xEC
 *  E:  0 1 1 1 1 0 0 1   = 0x79
 *   rev0 1 0 0 1 1 1 1   = 0x4F
 *  F:  0 1 1 1 0 0 0 1   = 0x71
 *   rev0 1 0 0 0 1 1 1   = 0x47
 */

// this is the character map.  '1' is segment ON
// sevenSegmentEncoder is the classic 7 segment display encoding
 #if REVERSE_VIEW
unsigned char    sevenSegmentEncoder[17] = {
    0x3F, // 0
    0x30, // 1
    0x6D, // 2
    0x79, // 3
    0x72, // 4
    0x5B, // 5
    0x5F, // 6
    0x31, // 7
    0x7F, // 8
    0x73, // 9
    0x77, // A
    0x5E, // B
    0x0F, // C
    0xEC, // D
    0x4F, // E
    0x47,  // F
    0x00  // All off
    };
  #else
unsigned char    sevenSegmentEncoder[17] = {
    0x3F, // 0
    0x06, // 1
    0x5B, // 2
    0x4F, // 3
    0x66, // 4
    0x6D, // 5
    0x7D, // 6
    0x07, // 7
    0x7F, // 8
    0x67, // 9
    0x77, // A
    0x7C, // B
    0x39, // C
    0x5E, // D
    0x79, // E
    0x71,  // F
    0x00  // All off
    };

  #endif

void setup(void) {
  // put your setup code here, to run once:
  int   pin;
  
  // initialize gpios
  pin=2;
  while(pin<8) {    // 2,3,4,5,6,7
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
    pin++;
    }
    // debug pin 13
    pinMode(13, OUTPUT);
    digitalWrite(13, LOW);

  segment=0;
  number =0;
  loopCounter=0;
}

void loop() {
    // put your main code here, to run repeatedly:
    unsigned char segmentBit;
                            // we go thru this loop about once per millisecond
    if(loopCounter>200){    // loopCounter is incremented once every 7 loops
                            // so the 200 counts here represent about 1.4 seconds
        loopCounter=0;
        number++;           // the next number to display
        if(number>9)
            number=0;
    }

    digitalWrite(13, HIGH);  // pulse high for the oscilloscope

    if(loopCounter<100){
        WriteSegment(7); // off
    } else {
        segmentBit = (1<<segment); // 0,1,2,3,4,5,6
        if((sevenSegmentEncoder[number]&segmentBit))
            // a new segment is written each millisecond
            WriteSegment(segment);
        else
            WriteSegment(7); // All off
    }
    
    segment++;
    if(segment>6){    // there are 7 segments so we go 0,1,2,3,4,5,6,(repeat)
        segment=0;
        loopCounter++;
    }

    digitalWrite(13, LOW);  // pulse low

   delay(1);  // 1 millisecond sleep

}

/*
        segmentStates is the Charlieplexed bit map using the
        Ardunio pin numbers and segment as the index into the array
        index = (8*segment)+pin;
        
        Used only when we write 1 bit at a time using the Arduino digitalWrite() function
*/
unsigned char    segmentStates[64]={
// pin 0 1 2 3 4 5 6 7
       1,1,1,1,0,1,1,1,    //segment 0x01, 'A' + pin 5, - pin 4
       1,1,0,0,1,0,0,0,    //segment 0x02, 'B' + pin 4, - pin 3
       1,1,0,1,1,0,0,0,    //segment 0x04, 'C' + pin 3, - pin 2
       1,1,0,0,0,0,0,1,    //segment 0x08, 'D' + pin 2, - pin 7
       1,1,1,1,1,1,1,0,    //segment 0x10, 'E' + pin 6, - pin 7
       1,1,1,1,1,1,0,0,    //segment 0x20, 'F' + pin 5, - pin 6
       1,1,1,0,0,0,1,1,    //segment 0x40, 'G' + pin 6, - pin 3
       1,1,1,1,1,1,1,1     // all off
};

/*
        segmentStatesBytes is the Charlieplexed bit map using the
        when wrting to PORTD directly as a whole byte.
        the segment number is the indexx into the arraay.
        
        About 100 times faster than writing 1 bit at a time method.
*/
unsigned char    segmentStatesBytes[8]={
//         pin 0 1 2 3 4 5 6 7
      0xEC, // 1,1,1,0,1,1,0,,    //segment 0x01, 'A' + pin 5, - pin 4
      0x10, // 0,0,0,1,0,0,0,0    //segment 0x02, 'B' + pin 4, - pin 3
      0x18, // 0,0,0,1,1,0,0,0    //segment 0x04, 'C' + pin 3, - pin 2
      0x80, // 1,0,0,0,0,0,0,0    //segment 0x08, 'D' + pin 2, - pin 7
      0x7C, // 0,1,1,1,1,1,0,0    //segment 0x10, 'E' + pin 6, - pin 7
      0x3C, // 0,0,1,1,1,1,0,0    //segment 0x20, 'F' + pin 5, - pin 6
      0xC4, // 1,1,0,0,0,1,0,0    //segment 0x40, 'G' + pin 6, - pin 3
      0xFC  // 1,1,1,1,1,1,0,0    // all off
};


#if WRITE_WHOLE_BYTES
void WriteSegment(char seg)
{

    int pin;    // pin number
    int idx;    // index

    (PORTD) = segmentStatesBytes[seg];
 
}

#else
void WriteSegment(char seg)
{

    int pin;    // pin number
    int idx;    // index
    
    pin=2;
    idx = 8*seg+pin;
    
    while(pin<8){  // one bit at a time
        if(segmentStates[idx])
            digitalWrite(pin, HIGH);
        else
            digitalWrite(pin, LOW);
        idx++;
        pin++;
    }
}
#endif
