/*********************************************************************
 *
 *                  Helper Functions for Microchip TCP/IP Stack
 *                 (Modified to work with CCS PCH, by CCS)
 *
 *********************************************************************
 * FileName:        Helpers.C
 * Dependencies:    compiler.h
 *                  helpers.h
 * Processor:       PIC18
 * Complier:        CCS PCH 3.181 or higher
 * Company:         Microchip Technology, Inc.
 *
 * Software License Agreement
 *
 * The software supplied herewith by Microchip Technology Incorporated
 * (the “Company”) for its PICmicro® Microcontroller is intended and
 * supplied to you, the Company’s customer, for use solely and
 * exclusively on Microchip PICmicro Microcontroller products. The
 * software is owned by the Company and/or its supplier, and is
 * protected under applicable copyright laws. All rights are reserved.
 * Any use in violation of the foregoing restrictions may subject the
 * user to criminal sanctions under applicable laws, as well as to
 * civil liability for the breach of the terms and conditions of this
 * license.
 *
 * THIS SOFTWARE IS PROVIDED IN AN “AS IS” CONDITION. NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
 * TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
 * IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
 * CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *
 * Author               Date     Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Nilesh Rajbharti     5/17/01  Original        (Rev 1.0)
 * Nilesh Rajbharti     2/9/02   Cleanup
 * Nilesh Rajbharti     6/25/02  Rewritten CalcIPChecksum() to avoid
 *                               multi-byte shift operation.
 * Darren Rook (CCS)    01/09/04 Initial CCS Public Release
 * Darren Rook (CCS)    05/24/04 swaps() and swapl() optimized
 ********************************************************************/

#include "drivers\helpers.h"


/*
int16 swaps(int16 v)
{
    WORD_VAL t;
    int8 b;

    t.Val   = v;
    b       = t.v[1];
    t.v[1]  = t.v[0];
    t.v[0]  = b;

    return t.Val;
}
*/

int16 swaps(WORD_VAL v)
{
    WORD_VAL new;
   
    new.v[0]=v.v[1];
    new.v[1]=v.v[0];

    return(new.Val);
}


/*
int32 swapl(int32 v)
{
    int8 b;
    int32 myV;
    DWORD_VAL *myP;

    myV     = v;
    myP     = (DWORD_VAL*)&myV;

    b       = myP->v[3];
    myP->v[3] = myP->v[0];
    myP->v[0] = b;

    b       = myP->v[2];
    myP->v[2] = myP->v[1];
    myP->v[1] = b;

    return myV;
}
*/

int32 swapl(DWORD_VAL v)
{
    DWORD_VAL new;

    new.v[0]=v.v[3];
    new.v[1]=v.v[2];    
    new.v[2]=v.v[1];    
    new.v[3]=v.v[0];

    return(new.Val);        
}


int16 CalcIPChecksum(int8* buff, int16 count)
{
    int16 i;
    int16 *val;

    union
    {
        int32 Val;
        int8 v[4];
        struct
        {
            WORD_VAL LSB;
            WORD_VAL MSB;
        } words;
    } tempSum, sum;

    sum.Val = 0;

    i = count >> 1;
    val = (int16 *)buff;

    while( i-- )
        sum.Val += *val++;

    if ( count & 1 )
        sum.Val += *(int8 *)val;

    tempSum.Val = sum.Val;

    while( (i = tempSum.words.MSB.Val) != 0 )
    {
        sum.words.MSB.Val = 0;
        sum.Val = (int32)sum.words.LSB.Val + (int32)i;
        tempSum.Val = sum.Val;
    }

    return (~sum.words.LSB.Val);
}


char *strupr (char *s)
{
    char c;
    char *t;

    t = s;
    while( (c = *t) )
    {
        if ( (c >= 'a' && c <= 'z') )
            *t -= ('a' - 'A');
    t++;
    }
    return s;
}

void delay_s(int8 s) {
   while(s) {
      restart_wdt();
      delay_ms(1000);
      s--;
   }
}
