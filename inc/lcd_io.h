/*==============================================================================
MIT License

Copyright (c) 2023 Trevor Monk

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
==============================================================================*/

#ifndef LCD_IO_H
#define LCD_IO_H

/*==============================================================================
        Includes
==============================================================================*/

#include <stdint.h>

/*==============================================================================
        Public Definitions
==============================================================================*/

typedef struct _LCDDev LCDDev;

/*==============================================================================
        Public Function Declarations
==============================================================================*/

LCDDev *InitDev( void );
int LCDOpen( LCDDev *pDev );
int LCDClose( LCDDev *pDev );
int Set4BitMode( LCDDev *pDev );
int readByte( LCDDev *pDev, uint8_t *val );
int writeByte( LCDDev *pDev, uint8_t rs, uint8_t val );
int latch( LCDDev *pDev );
int writeReg( LCDDev *pDev );
int readStatus( LCDDev *pDev );
int readReg( LCDDev *pDev );

int GetCursorX( LCDDev *pDev, int *pX );
int GetCursorY( LCDDev *pDev, int *pY );
int GetBacklight( LCDDev *pDev, bool *backlight );
int SetBacklight( LCDDev *pDev, bool backlight );
int GetExclusive( LCDDev *pDev, bool *exclusive );
int SetExclusive( LCDDev *pDev, bool exclusive );
int GetAddress( LCDDev *pDev, uint8_t *address );
int SetAddress( LCDDev *pDev, uint8_t address );
int SetDeviceName( LCDDev *pDev, char *name );
int GetDeviceName( LCDDev *pDev, char **name );

#endif
