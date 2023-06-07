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
 * @defgroup lcdio lcdio
 * @brief Character LCD Input/Output functions
 * @{
 */

/*============================================================================*/
/*!
@file lcd_io.c

    Input/Output functions for the character based display

    The lcd_io module provides low level input/output functions
    for controlling the character based LCD display assuming the
    display is connected via a PCF8574 serial-to-parallel I/O expander.

    It supports activities like, opening and closing the I2C device,
    reading and writing data via the bus to the PCF8574,
    and providing low level access to the LED backlight, and the
    instruction and data registers of the LCD character display.

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
#include <sys/ioctl.h>
#include "lcd_ctrl.h"
#include "lcd_io.h"

/*==============================================================================
        Definitions
==============================================================================*/

#ifndef EOK
#define EOK 0
#endif

/*==============================================================================
        Data Types
==============================================================================*/

/*! The Ctrl Register defines the bit-mapping for the LCD controller */
typedef struct _ctrlReg
{
    /*! Register Select: 0=Instruction Register, 1=Data Register */
    uint8_t RS:1;

    /*! Read/Write: 0=Write, 1=Read */
    uint8_t RW:1;

    /*! Enable: starts data read/write */
    uint8_t EN:1;

    /*! LED control */
    uint8_t LED:1;

    /*! 4-bit data register */
    uint8_t D4:4;
} CtrlReg;

/*! The LCDDev type provides the context used when reading/writing the
 *  LCD character display */
struct _LCDDev
{
    /*! the I2C device */
    char *device;

    /*! handle to the I2C device */
    int fd;

    /*! busy flag */
    bool busy;

    /*! exclusive mode flag */
    bool exclusive;

    /*! PCF8574 device address */
    uint8_t address;

    /*! address counter */
    uint8_t AddressCounter;

    /*! cursor X position */
    int cx;

    /*! cursor Y position */
    int cy;

    /*! register */
    union
    {
        /*! register bitmap */
        CtrlReg reg;

        /*! register value */
        uint8_t regval;
    };

};

/*==============================================================================
        Function Definitions
==============================================================================*/

/*============================================================================*/
/*  InitDev                                                                   */
/*!
    Initialize the LCD device context

    The InitDev function creates and intializes an LCD device context.
    It sets the following defaults:

    - device address: 0x27
    - device bus: /dev/i2c-1
    - backlight: ON
    - device state: closed

    @retval pointer to the new LCDDev device context
    @retval NULL if the device context could not be created

==============================================================================*/
LCDDev *InitDev( void )
{
    LCDDev *pDev = calloc( 1, sizeof( LCDDev ) );
    if ( pDev != NULL )
    {
        /* initialize default file descriptor */
        pDev->fd = -1;

        /* initialize default address */
        pDev->address = 0x27;

        /* initialize default I2C device name */
        pDev->device = "/dev/i2c-1";

        /* backlight is on */
        pDev->reg.LED = 1;
    }

    return pDev;
}

/*============================================================================*/
/*  LCDOpen                                                                   */
/*!
    Open a connection to the LCD device

    The LCDOpen function opens a file descriptor to communicate with the
    LCD device via the i2c device driver.  The file descriptor is opened
    in R/W mode as a bus master.  The file descriptor is cached for
    subsequent input/output operations.

    If a connection to the LCD device is already available (previously opened)
    then this function has no effect.

    @param[in]
        pDev
            pointer to the LCDDev controller state object

    @retval EOK a file descriptor to the device is available and cached
    @retval EINVAL invalid arguments
    @retval ENXIO cannot use ioctl to set the device as a slave device
    @retval ENODEV no i2c device has been specified
    @retval other error as reported by open()

==============================================================================*/
int LCDOpen( LCDDev *pDev )
{
    int fd;
    int result = EINVAL;

    if ( pDev != NULL )
    {
        if ( pDev->fd == -1 )
        {
            if ( pDev->device != NULL )
            {
                /* open the i2c device for reading */
                fd = open( pDev->device, O_RDWR );
                if( fd != -1 )
                {
                    /* set up the device slave address */
                    if (ioctl( fd, I2C_SLAVE, pDev->address ) >= 0 )
                    {
                        /* set up the channel to read */
                        write( fd, &(pDev->regval), 1 );

                        /* save the file descriptor */
                        pDev->fd = fd;

                        /* success */
                        result = EOK;
                    }
                    else
                    {
                        close( fd );
                        result = ENXIO;
                    }
                }
                else
                {
                    result = errno;
                }
            }
            else
            {
                result = ENODEV;
            }
        }
        else
        {
            /* LCD is already open */
            result = EOK;
        }
    }

    return result;
}

/*============================================================================*/
/*  LCDClose                                                                  */
/*!
    Close the connection to the LCD device

    The LCDClose function closes the connection to the LCD device via
    the i2c device driver if the connecion is open and not opened for
    exclusive access.

    To close a connection which was opened with exclusive access, you
    must first clear the "exclusive" flag using the SetExclusive()
    function before closing the connection.

    If a connection to the LCD device is not established, this
    function has no effect.

    @param[in]
        pDev
            pointer to the LCDDev controller state object

    @retval EOK no error occurred
    @retval EINVAL invalid arguments

==============================================================================*/
int LCDClose( LCDDev *pDev )
{
    int result = EINVAL;

    if ( pDev != NULL )
    {
        /* don't close if we are in exclusive-open mode */
        if ( ( pDev->fd != -1 ) &&
             ( pDev->exclusive == false ) )
        {
            close( pDev->fd );
            pDev->fd = -1;

            result = EOK;
        }
        else
        {
            /* nothing to do */
            result = EOK;
        }
    }

    return result;
}

/*============================================================================*/
/*  Set4BitMode                                                               */
/*!
    Set the LCD interface to communicate using 4-bit mode

    The Set4BitMode sets the LCD interface to communicate using 4-bit mode
    requiring two writes to send a command or character data.
    This is necessary since the 8-bit PCF8574 is configured such that
    3 of the bits are used for control, 1 for backlight, and the remaining
    4 for data.

    In case we re-starting, and to make sure we do not get out of
    synch when performing 4-bit writes (this can easily happen), we
    first set the interface to 8-bit mode, then set it into 4 bit mode,
    then configure the display mode (2 lines), and then select 5x8 c
    characters.

    @param[in]
        pDev
            pointer to the LCDDev controller state object

    @retval EOK no error occurred
    @retval EINVAL invalid arguments
    @retval other error from writeByte()

==============================================================================*/
int Set4BitMode( LCDDev *pDev )
{
    int result = EINVAL;
    int rc1;
    int rc2;

    if ( pDev != NULL )
    {
        result = ENXIO;

        /* set up 8-bit writes first in case we are in 4-bit mode */
        rc1 = writeByte( pDev, 0, 0x38 );

        /* now set up 4-bit mode with a single write */
        pDev->reg.RS = 0;
        pDev->reg.RW = 0;
        pDev->reg.D4 =  0x02;
        writeReg( pDev );

        /* latch the output */
        latch( pDev );

        /* set up Function status 4-bit mode, 2 line display, 5x8 char */
        rc2 = writeByte( pDev, 0, 0x28 );

        if ( ( rc1 == EOK ) && ( rc2 == EOK ) )
        {
            result = EOK;
        }
    }

    return result;
}

/*============================================================================*/
/*  latch                                                                     */
/*!
    Latch the PCF8574 outputs into the LCD display interface

    The latch function toggles the EN pin (PCF8574 bit 2) to latch the
    RS, RW, and D7-D4 bits into the LCD display interface.
    The EN pin is written high and then low to latch the data.

    @param[in]
        pDev
            pointer to the LCDDev controller state object

    @retval EOK no error occurred
    @retval EINVAL invalid arguments

==============================================================================*/
int latch( LCDDev *pDev )
{
    int result = EINVAL;

    if ( pDev != NULL )
    {
        pDev->reg.EN = 1;
        writeReg( pDev );

        pDev->reg.EN = 0;
        writeReg( pDev );

        result = EOK;
    }

    return result;
}

/*============================================================================*/
/*  writeByte                                                                 */
/*!
    Write an 8-bit data value to the LCD instruction or data register

    The writeByte function writes a byte of data to either the instruction
    or data RAM depending on the value of the RS bit.  The LCD device
    must be operating in 4-bit mode.

    RW is set to 0

    RS is set to 1 for Data RAM or 0 for Control Commands

    The D7-D4 pins are used to transfer 4-bits of the byte to be written.

    The Most Significant bits (d7-d4) and sent first followed by the Least
    Significant bits (d3-d0).

    Between each nibble, the data is latched using the latch() function which
    toggles the EN bit.

    After the write the BUSY status is polled, making the writeByte function
    synchronous, that is, it does not return until the write is completed.

    As a side effect of the busy polling, this function updates the cursor
    cx,cy coordinates in the device object.  These can be retrieved using
    the GetCursorX() and GetCursorY() functions.

    @param[in]
        pDev
            pointer to the LCDDev controller state object

    @param[in]
        rs
            register select: 1 = Data RAM, 0 = Instruction Register

    @param[in]
        val
            8-bit value to write

    @retval EOK no error occurred
    @retval EINVAL invalid arguments

==============================================================================*/
int writeByte( LCDDev *pDev, uint8_t rs, uint8_t val )
{
    int result = EINVAL;

    if ( pDev != NULL )
    {
        /* set up to write to control (rs = 0) or data (rs = 1) registers */
        pDev->reg.RS = rs ? 1 : 0;

        /* set up to write (rw = 0) */
        pDev->reg.RW = 0;

        /* write most significant nibble */
        pDev->reg.D4 = (val & 0xF0) >> 4;
        writeReg( pDev );

        /* latch the output */
        latch( pDev );

        /* write least significant nibble */
        pDev->reg.D4 = (val & 0x0F);
        writeReg( pDev );

        /* latch the output */
        latch( pDev );

        /* poll hardware for write completion */
        do
        {
            result = GetStatus( pDev );
        } while( pDev->busy );
    }

    return result;
}

/*============================================================================*/
/*  readByte                                                                  */
/*!
    Reads an 8-bit data value from the LCD controller

    The readByte function reads a byte of data from the LCD controller IC.
    The read status byte contains the busy flag, and the current data
    address (the address in the LCD RAM of the next write).

    The LCD controller must be operating in 4-bit data transfer mode

    Since the PCF8574 is a pseudo I/O chip, the data outputs have to be
    driven high during a read operation.

    RS is set to 0 to select the status register
    RW is set to 1 to read the data
    D7-D4 are set HIGH

    The EN is set high to enable the LCD outputs, the data is read back
    from the PCF8574, and then the EN is set low to disable the LCD outputs.

    This process is repeated twice, the first time to get the Most Significant
    Bits of the status register, and the second time to get the Least
    Significant bits of the status register.

    The two halves of the status register are ORed together to get the final
    result

    @param[in]
        pDev
            pointer to the LCDDev controller state object

    @param[in]
        val
            pointer to the location to store the 8-bit status value

    @retval EOK no error occurred
    @retval EINVAL invalid arguments
    @retval EBADF invalid device file descriptor

==============================================================================*/
int readByte( LCDDev *pDev, uint8_t *val )
{
    int result = EINVAL;
    uint8_t data_high = 0;
    uint8_t data_low = 0;
    uint8_t data = 0;

    if ( pDev != NULL )
    {
        if ( pDev->fd != -1 )
        {
            /* set up register for reading status */
            pDev->reg.RS = 0;
            pDev->reg.RW = 1;

            /* set data registers high for PCF8574 so we can read them back */
            pDev->reg.D4 = 0x0F;

            /* Update PCF8574 outputs */
            write( pDev->fd, &(pDev->regval), 1 );

            pDev->reg.EN = 1;
            write( pDev->fd, &(pDev->regval), 1 );

            /* read back upper nibble of status byte */
            read( pDev->fd, &data, 1 );
            data_high = ( data & 0xF0 );

            pDev->reg.EN = 0;
            write( pDev->fd, &(pDev->regval), 1 );

            /* Update PCF8574 outputs */
            pDev->reg.EN = 1;
            write( pDev->fd, &(pDev->regval), 1 );

            /* read back lower nibble of status byte */
            read( pDev->fd, &data, 1 );
            data_low = ( data & 0xF0 ) >> 4;

            /* Update PCF8574 outputs */
            pDev->reg.EN = 0;
            write( pDev->fd, &(pDev->regval), 1 );

            *val = data_high | data_low;

            result = EOK;
        }
        else
        {
            result = EBADF;
        }
    }

    return result;
}


/*============================================================================*/
/*  writeReg                                                                  */
/*!
    Write the register to the I2C device

    The writeReg function writes an 8-bit value to the PCF8574 serial to
    parallel I/O expander.

    If the I2C interface is not opened (for exclusive access), this function
    will open/close the I2C interface for each write.

    @param[in]
        pDev
            pointer to the LCDDev controller state object

    @retval EOK the write was successful
    @retval ENODEV no I2C device was specified
    @retval ENXIO ioctl failed
    @retval other error from open()
    @retval EINVAL invalid arguments

==============================================================================*/
int writeReg( LCDDev *pDev )
{
    int result = EINVAL;
    int fd = -1;

    if ( pDev != NULL )
    {
        fd = pDev->fd;

        if( fd != -1 )
        {
            write( fd, &(pDev->regval), 1 );
            result = EOK;
        }
        else if ( pDev->device != NULL )
        {
            /* open the i2c device for reading */
            fd = open( pDev->device, O_RDWR );
            if( fd != -1 )
            {
                /* set up the device slave address */
                if (ioctl( fd, I2C_SLAVE, pDev->address ) >= 0 )
                {
                    /* set up the channel to read */
                    write( fd, &(pDev->regval), 1 );

                    /* success */
                    result = EOK;
                }
                else
                {
                    result = ENXIO;
                }

                close( fd );
            }
            else
            {
                result = errno;
            }
        }
        else
        {
            result = ENODEV;
        }
    }

    return result;
}

/*============================================================================*/
/*  GetStatus                                                                 */
/*!
    Get the status of the LCD

    The GetStatus function gets the busy status and address counter
    from the LCD display via the PCF8574 4-bit interface

    If the I2C interface is not opened (for exclusive access), this function
    will open/close the I2C interface for each status read.

    @param[in]
        pDev
            pointer to the LCDDev controller state object

    @retval EOK the device is ready
    @retval EBUSY the device is busy
    @retval ENODEV no I2C device was specified
    @retval ENXIO ioctl failed
    @retval other error from open()
    @retval EINVAL invalid arguments

==============================================================================*/
int GetStatus( LCDDev *pDev )
{
    int result = EINVAL;
    int fd = -1;
    uint8_t val;
    uint8_t ac;

    if ( pDev != NULL )
    {
        fd = pDev->fd;

        if( fd != -1 )
        {
            /* get the LCD status */
            result = readStatus( pDev );
        }
        else if ( pDev->device != NULL )
        {
            /* open the i2c device for reading */
            fd = open( pDev->device, O_RDWR );
            if( fd != -1 )
            {
                /* set up the device slave address */
                if (ioctl( fd, I2C_SLAVE, pDev->address ) >= 0 )
                {
                    /* get the LCD status */
                    pDev->fd = fd;
                    result = readStatus( pDev );
                }
                else
                {
                    result = ENXIO;
                }

                pDev->fd = -1;
                close( fd );
            }
            else
            {
                result = errno;
            }
        }
        else
        {
            result = ENODEV;
        }
    }

    return result;
}

/*============================================================================*/
/*  readStatus                                                                */
/*!
    Read the status register of the LCD

    The readStatus function reads the busy status and address counter
    from the LCD display via the PCF8574 4-bit interface

    @param[in]
        pDev
            pointer to the LCDDev controller state object

    @retval EOK the device is ready
    @retval EBUSY the device is busy
    @retval EBADF invalid file descriptor
    @retval EINVAL invalid arguments

==============================================================================*/
int readStatus( LCDDev *pDev )
{
    int result = EINVAL;
    uint8_t val;

    if ( pDev != NULL )
    {
        if ( pDev->fd != -1 )
        {
            /* read a byte by 4-bit read */
            result = readByte( pDev, &val );
            if ( result == EOK )
            {
                pDev->busy = val & 0x80 ? true : false;
                pDev->AddressCounter = val & 0x7F;
                pDev->cx = ( pDev->AddressCounter & 0x3F ) + 1;
                pDev->cy = pDev->AddressCounter >= 0x40 ? 2 : 1;
            }
        }
        else
        {
            result = EBADF;
        }
    }

    return result;
}

/*============================================================================*/
/*  GetExclusive                                                              */
/*!
    Check if the module has exclusive access to the I2C bus

    The GetExclusive function checks to see if the module has exclusive
    access to the I2C bus, and has an open persistent connection to the
    driver.

    @param[in]
        pDev
            pointer to the LCDDev controller state object

    @param[in]
        exclusive
            pointer to a boolean to hold the results of the query

    @retval EOK the query was successful
    @retval EINVAL invalid arguments

==============================================================================*/
int GetExclusive( LCDDev *pDev, bool *exclusive )
{
    int result = EINVAL;

    if ( ( pDev != NULL ) &&
         ( exclusive != NULL ) )
    {
        *exclusive = pDev->exclusive;
        result = EOK;
    }

    return result;
}

/*============================================================================*/
/*  SetExclusive                                                              */
/*!
    Set flag to request exclusive access to the I2C bus

    The SetExclusive function sets a flag to request exclusive access
    to the bus.  Note that this function has no effect after the application
    has started its main loop.  It must be set as part of command line
    argument processing before the main loop begins.

    @param[in]
        pDev
            pointer to the LCDDev controller state object

    @param[in]
        exclusive
            boolean request for exclusive (true) or non-exclusive (false) access

    @retval EOK the set was successful
    @retval EINVAL invalid arguments

==============================================================================*/
int SetExclusive( LCDDev *pDev, bool exclusive )
{
    int result = EINVAL;

    if ( pDev != NULL )
    {
        pDev->exclusive = exclusive;
    }

    return result;
}

/*============================================================================*/
/*  GetBacklight                                                              */
/*!
    Check if the LCD backlight is enabled

    The GetBacklight function checks to see if the LCD backlight is enabled

    @param[in]
        pDev
            pointer to the LCDDev controller state object

    @param[in]
        backlight
            pointer to a boolean to hold the results of the query

    @retval EOK the query was successful
    @retval EINVAL invalid arguments

==============================================================================*/
int GetBacklight( LCDDev *pDev, bool *backlight )
{
    int result = EINVAL;

    if ( ( pDev != NULL ) &&
         ( backlight != NULL ) )
    {
        *backlight = ( pDev->reg.LED );
        result = EOK;
    }

    return result;
}

/*============================================================================*/
/*  SetBacklight                                                              */
/*!
    Set the state of the LCD backlight

    The SetBacklight function sets the state (on or off) of the LCD backlight

    @param[in]
        pDev
            pointer to the LCDDev controller state object

    @param[in]
        backlight
            true - turn on the backlight
            false - turn off the backlight

    @retval EOK the set was successful
    @retval EINVAL invalid arguments

==============================================================================*/
int SetBacklight( LCDDev *pDev, bool backlight )
{
    int result = EINVAL;

    if ( pDev != NULL )
    {
        pDev->reg.LED = backlight == true ? 1 : 0;
        writeReg( pDev );
    }

    return result;
}

/*============================================================================*/
/*  GetCursorX                                                                */
/*!
    Get the x coordinate of the cursor

    The GetCursorX function gets the X coordinate of the cursor.
    The cursor indicates where the next character will be displayed.
    x = 1 is the far left of the screen
    x = 16 is the far right of the screen (on a 16x2 display)

    @param[in]
        pDev
            pointer to the LCDDev controller state object

    @param[in]
        pX
            pointer to the location to store the X coordinate

    @retval EOK the query was successful
    @retval EINVAL invalid arguments

==============================================================================*/
int GetCursorX( LCDDev *pDev, int *pX )
{
    int result = EINVAL;

    if ( ( pDev != NULL ) &&
         ( pX != NULL ) )
    {
        *pX = pDev->cx;
        result = EOK;
    }

    return result;
}

/*============================================================================*/
/*  GetCursorY                                                                */
/*!
    Get the y coordinate of the cursor

    The GetCursorY function gets the Y coordinate of the cursor.
    The cursor indicates where the next character will be displayed.
    y = 1 is the top of the screen (line1)
    y = 2 is the bottom of the screen (line2) on a 16x2 display

    @param[in]
        pDev
            pointer to the LCDDev controller state object

    @param[in]
        pX
            pointer to the location to store the X coordinate

    @retval EOK the query was successful
    @retval EINVAL invalid arguments

==============================================================================*/
int GetCursorY( LCDDev *pDev, int *pY )
{
    int result = EINVAL;

    if ( ( pDev != NULL ) &&
         ( pY != NULL ) )
    {
        *pY = pDev->cy;
        result = EOK;
    }

    return result;
}

/*============================================================================*/
/*  GetAddress                                                                */
/*!
    Get address of the PCF8574 device on the i2c bus

    The GetAddress function gets the address of the PCF8574 device on the
    i2c bus.  By default this is 0x27.

    @param[in]
        pDev
            pointer to the LCDDev controller state object

    @param[in]
        address
            pointer to the location to store the device address

    @retval EOK the query was successful
    @retval EINVAL invalid arguments

==============================================================================*/
int GetAddress( LCDDev *pDev, uint8_t *address )
{
    int result = EINVAL;

    if ( ( pDev != NULL ) &&
         ( address != NULL ) )
    {
        *address = pDev->address;
        result = EOK;
    }

    return result;
}

/*============================================================================*/
/*  SetAddress                                                                */
/*!
    Set the address of the PCF8574 device on the i2C bus

    The SetAddress function sets the address of the PCF8574 device on the
    i2c bus.  The address is used on the next call to LCDOpen()

    @param[in]
        pDev
            pointer to the LCDDev controller state object

    @param[in]
        address
            address of the PCF8574 device on the i2c bus

    @retval EOK the set was successful
    @retval EINVAL invalid arguments

==============================================================================*/
int SetAddress( LCDDev *pDev, uint8_t address )
{
    int result = EINVAL;

    if ( pDev != NULL )
    {
        pDev->address = address;
        result = EOK;
    }

    return result;
}

/*============================================================================*/
/*  GetDeviceName                                                             */
/*!
    Get the name of the i2c device

    The GetDeviceName function gets the name of the i2c bus device
    eg /dev/i2c-1

    Note that this function retrieves the pointer to the device name
    which was specified (probably during startup) as part of the
    initialiation.  As such, the pointer to the name should be
    considered constant.  Do not perform any in-place modification
    on the device name retrieved with this function.

    @param[in]
        pDev
            pointer to the LCDDev controller state object

    @param[in]
        name
            pointer to the location to store the pointer to the device name

    @retval EOK the query was successful
    @retval EINVAL invalid arguments

==============================================================================*/
int GetDeviceName( LCDDev *pDev, char **name )
{
    int result = EINVAL;

    if ( ( pDev != NULL ) &&
         ( name != NULL ) )
    {
        *name = pDev->device;
        result = EOK;
    }

    return result;
}

/*============================================================================*/
/*  SetDeviceName                                                             */
/*!
    Set the name of the i2c device

    The SetDeviceName function sets the name of the i2c device
    eg /dev/i2c-1.  The name is used on the next call to LCDOpen()

    @param[in]
        pDev
            pointer to the LCDDev controller state object

    @param[in]
        name
            pointer to the i2c device name

    @retval EOK the set was successful
    @retval EINVAL invalid arguments

==============================================================================*/
int SetDeviceName( LCDDev *pDev, char *name )
{
    int result = EINVAL;

    if ( pDev != NULL )
    {
        pDev->device = name;
        result = EOK;
    }

    return result;
}

/*! @}
 * end of lcdio group */
