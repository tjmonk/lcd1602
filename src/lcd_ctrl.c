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

/*!
 * @defgroup lcdctrl lcdctrl
 * @brief Character LCD Control functions
 * @{
 */

/*============================================================================*/
/*!
@file lcd_ctrl.c

    Control functions for the character based display

    The lcd_ctrl module provides high level functions for controlling
    the character based LCD display.

    It supports activities like, clearing the screen, positioning the
    cusor, controlling cursor display modes, and writing to the display
    among other things.

*/
/*============================================================================*/

/*==============================================================================
        Includes
==============================================================================*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include "lcd_ctrl.h"
#include "lcd_io.h"

/*==============================================================================
        Definitions
==============================================================================*/

#ifndef EOK
#define EOK 0
#endif

/*============================================================================*/
/*  LCDInit                                                                   */
/*!
    Initialize the LCD device

    The LCDInit function intializes the LCD device and sets it up
    to use 4-bit operating mode.

    @param[in]
        pDev
            pointer to the LCDDev controller state object

    @retval EOK the initialization was successful
    @retval ENODEV no I2C device was specified
    @retval ENXIO ioctl failed
    @retval other error from open()
    @retval EINVAL invalid arguments

==============================================================================*/
int LCDInit( LCDDev *pDev )
{
    int result = EINVAL;

    if ( pDev != NULL )
    {
        result = LCDOpen( pDev );
        if ( result == EOK )
        {
            result = Set4BitMode( pDev );
            if ( result == EOK )
            {
                ClearDisplay( pDev );
                CursorHome( pDev );
                Cursor( pDev );
            }

            LCDClose( pDev );
        }
    }

    return result;
}

/*============================================================================*/
/*  ClearDisplay                                                              */
/*!
    Clear the character LCD display

    The ClearDisplay function writes the clear display (code 0x01)
    command to the hardware and waits for the operation to complete
    before continuing.

    The command is written to the hardware using the writeByte()
    function.

    @param[in]
        pDev
            pointer to the LCD device object

    @retval EOK the command was successful
    @retval EINVAL invalid arguments
    @retval other error from writeByte()

==============================================================================*/
int ClearDisplay( LCDDev *pDev )
{
    int result = EINVAL;

    if ( pDev != NULL )
    {
        /* clear the display */
        result = writeByte( pDev, 0, 0x01 );
    }

    return result;
}

/*============================================================================*/
/*  CursorHome                                                                */
/*!
    Move the cursor to the Home position of the character LCD display

    The CursorHome function writes the cursor home (code 0x02)
    command to the hardware and waits for the operation to complete
    before continuing.

    The command is written to the hardware using the writeByte()
    function.

    @param[in]
        pDev
            pointer to the LCD device object

    @retval EOK the command was successful
    @retval EINVAL invalid arguments
    @retval other error from writeByte()

==============================================================================*/
int CursorHome( LCDDev *pDev )
{
    int result = EINVAL;

    if ( pDev != NULL )
    {
        /* clear the display */
        result = writeByte( pDev, 0, 0x02 );
    }

    return result;
}

/*============================================================================*/
/*  SetADD                                                                    */
/*!
    Set the Display Data Address

    The SetADD function writes the display data address to the
    hardware using the Set DDRAM address command.  This effectively
    sets the location on (or off) the screen where the next
    displayed character will be written.

    The command is written to the hardware using the writeByte()
    function.

    @param[in]
        pDev
            pointer to the LCD device object

    @retval EOK the command was successful
    @retval EINVAL invalid arguments
    @retval other error from writeByte()

==============================================================================*/
int SetADD( LCDDev *pDev, uint8_t loc )
{
    int result = EINVAL;

    if ( pDev != NULL )
    {
        /* Set the Dislay Data Address */
        result = writeByte( pDev, 0, 0x80 | loc );
    }

    return result;
}

/*============================================================================*/
/*  Cursor                                                                    */
/*!
    Control the cursor display mode

    The Cursor function writes the cursor display mode to the
    hardware using the Display On/Off control command.
    Currently this is hard coded to turn the display on, cursor on, and
    enable cursor blink.

    The command is written to the hardware using the writeByte()
    function.

    @param[in]
        pDev
            pointer to the LCD device object

    @retval EOK the command was successful
    @retval EINVAL invalid arguments
    @retval other error from writeByte()

==============================================================================*/
int Cursor( LCDDev *pDev )
{
    int result = EINVAL;

    if ( pDev != NULL )
    {
        /* blinking cursor */
        result = writeByte( pDev, 0, 0x0F );
    }

    return result;
}

/*============================================================================*/
/*  DisplayLine                                                               */
/*!
    Display a line of text on the display

    The DisplayLine function writes the specified text to the
    display data RAM at the specified offset.  For example, on the
    2x16 LCD hardware, offset 0 is the start of the first line,
    and offset 0x40 is the start of the second line.

    The character write start location is specified using the SetADD()
    function.

    The command is written to the hardware using the writeByte()
    function.

    If the interface to the hardware is not open, this function will
    open it and closed it upon completion of the write.

    @param[in]
        pDev
            pointer to the LCD device object

    @retval EOK the command was successful
    @retval EINVAL invalid arguments
    @retval other error from SetADD() or writeByte()

==============================================================================*/
int DisplayLine( LCDDev *pDev, int offset, char *line )
{
    int result = EINVAL;
    char ch;
    int i;
    int rc;

    if ( ( pDev != NULL ) &&
         ( line != NULL ) )
    {
        /* open the LCD device if it isn't open already */
        result = LCDOpen( pDev );
        if ( result == EOK )
        {
            /* Set the Display Data Address */
            result = SetADD( pDev, offset );
            if ( result == EOK )
            {
                /* iterate through the input data */
                for( i = 0; i < 17; i++ )
                {
                    ch = line[i];
                    if ( ch == 0 )
                    {
                        ch = 0x20;
                    }

                    /* write a character to the display memory */
                    rc = writeByte(pDev, 1, ch);
                    if ( rc != EOK )
                    {
                        result = rc;
                    }
                }
            }

            LCDClose( pDev );
        }
    }

    return result;
}

/*! @}
 * end of lcdctrl group */
