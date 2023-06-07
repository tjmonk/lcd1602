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
 * @defgroup lcd1602 lcd1602
 * @brief Map LCD 16x2 display with PCF8574 interface to system variables
 * @{
 */

/*============================================================================*/
/*!
@file smartlcd.c

    Smart LCD 16x2 display

    The lcd1602 Application maps variables to the 16 x 2 LCD Character
    display driven via a PCF8574 I2C Serial to Parallel 8-bit I/O
    expander.

    It maps the following variables

    /HW/LCD1602/LINE1
    /HW/LCD1602/LINE2
    /HW/LCD1602/BACKLIGHT
    /HW/LCD1602/STATUS

*/
/*============================================================================*/

/*==============================================================================
        Includes
==============================================================================*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <varserver/varserver.h>
#include "lcd_io.h"
#include "lcd_ctrl.h"

/*==============================================================================
        Private definitions
==============================================================================*/

/*==============================================================================
        Type definitions
==============================================================================*/

/*! the LCD1602 structure manages the interface to the
 *  16 char by 2 line LCD display via the PCF8574 8-bit serial to
 *  parallel I/O expander */
typedef struct _LCD1602
{
    /*! instance identifier */
    uint32_t instanceID;

    /*! verbose mode */
    bool verbose;

    /*! handle to the variable server */
    VARSERVER_HANDLE hVarServer;

    /*! line 1 */
    char line1[17];

    /*! line 2 */
    char line2[17];

    /*! LCD Device */
    LCDDev *pDev;

    /*! handle to backlight system variable */
    VAR_HANDLE hVarBacklight;

    /*! handle to line1 system variable */
    VAR_HANDLE hVarLine1;

    /*! handle to line2 system variable */
    VAR_HANDLE hVarLine2;
} LCD1602;

/*==============================================================================
        Private file scoped variables
==============================================================================*/

/*! pointer to the LCD1602 state object */
LCD1602 *pLCD;

/*==============================================================================
        Private function declarations
==============================================================================*/

void main(int argc, char **argv);
static int ProcessOptions( int argC, char *argV[], LCD1602 *pLCD );
static void usage( char *cmdname );
static void SetupTerminationHandler( void );
static void TerminationHandler( int signum, siginfo_t *info, void *ptr );
static int run( LCD1602 *pLCD );
static int WaitSignal( int *signum, int *id );
static int HandleSignal( LCD1602 *pLCD, int signum, int id );
static int SetupPrintNotifications( LCD1602 *pLCD );
static int PrintStatus (LCD1602 *pLCD, int fd );


static int SetupNotifications( LCD1602 *pLCD );
static int SetupModifiedNotifications( LCD1602 *pLCD );
static int SetupModifiedNotification( VARSERVER_HANDLE hVarServer,
                                      char *name,
                                      VAR_HANDLE *hVar );

static int OnChange( LCD1602 *pLCD, VAR_HANDLE hVar );
static int UpdateBacklight( LCD1602 *pLCD, VAR_HANDLE hVar );
static int UpdateLine1( LCD1602 *pLCD );
static int UpdateLine2( LCD1602 *pLCD );

/*==============================================================================
        Private function definitions
==============================================================================*/

/*============================================================================*/
/*  main                                                                      */
/*!
    Main entry point for the smartlcd_1602 application

    The main function starts the smartlcd_1602 application

    @param[in]
        argc
            number of arguments on the command line
            (including the command itself)

    @param[in]
        argv
            array of pointers to the command line arguments

    @return none

==============================================================================*/
void main(int argc, char **argv)
{
    LCD1602 state;
    int rc;
    bool exclusive;

    /* clear the smartlcd_1602 state object */
    memset( &state, 0, sizeof( LCD1602 ) );

    /* set default state */
    state.instanceID = 0;
    state.pDev = InitDev();

    pLCD = &state;

    /* set up an abnormal termination handler */
    SetupTerminationHandler();

    /* process the command line options */
    ProcessOptions( argc, argv, &state );

    /* get a handle to the VAR server */
    state.hVarServer = VARSERVER_Open();
    if( state.hVarServer != NULL )
    {
        /* open the LCD in exclusive mode if requested */
        GetExclusive( pLCD->pDev, &exclusive );
        rc = (exclusive == true) ? LCDOpen( pLCD->pDev ) : EOK;
        if ( rc == EOK )
        {
            /* set up notifications */
            if ( SetupNotifications( &state ) == EOK )
            {
                if (LCDInit( pLCD->pDev ) == EOK )
                {
                    UpdateLine1( pLCD );
                    UpdateLine2( pLCD );

                    /* run the LCD1602 controller */
                    run( &state );
                }
            }

            /* close the LCD device (if it is still open) */
            SetExclusive( pLCD->pDev, false );
            LCDClose( pLCD->pDev );
        }

        /* close the variable server */
        VARSERVER_Close( state.hVarServer );
    }
}

/*============================================================================*/
/*  usage                                                                     */
/*!
    Display the application usage

    The usage function dumps the application usage message
    to stderr.

    @param[in]
       cmdname
            pointer to the invoked command name

    @return none

==============================================================================*/
static void usage( char *cmdname )
{
    if( cmdname != NULL )
    {
        fprintf(stderr,
                "usage: %s [-a address] [-i instanceID] [-h] [-v]\n"
                " [-h] : display this help\n"
                " [-a address] : set PCF8574 device address\n"
                " [-i instanceID] : set LCD instance ID\n"
                " [-v] : verbose output\n",
                cmdname );
    }
}

/*============================================================================*/
/*  ProcessOptions                                                            */
/*!
    Process the command line options

    The ProcessOptions function processes the command line options and
    populates the iotsend state object

    @param[in]
        argC
            number of arguments
            (including the command itself)

    @param[in]
        argv
            array of pointers to the command line arguments

    @param[in]
        pLCD
            pointer to the LCD1602 object

    @return 0

==============================================================================*/
static int ProcessOptions( int argC,
                           char *argV[],
                           LCD1602 *pLCD )
{
    int c;
    int result = EINVAL;
    const char *options = "a:i:hve";

    if( ( pLCD != NULL ) &&
        ( argV != NULL ) )
    {
        while( ( c = getopt( argC, argV, options ) ) != -1 )
        {
            switch( c )
            {
                case 'a':
                    SetAddress( pLCD->pDev, atoi( optarg ) );
                    break;

                case 'i':
                    pLCD->instanceID = atoi( optarg );
                    break;

                case 'e':
                    /* open I2C interface in exclusive mode */
                    SetExclusive( pLCD->pDev, true );
                    break;

                case 'v':
                    pLCD->verbose = true;
                    break;

                case 'h':
                    usage( argV[0] );
                    break;

                default:
                    break;

            }
        }
    }

    return 0;
}

/*============================================================================*/
/*  SetupTerminationHandler                                                   */
/*!
    Set up an abnormal termination handler

    The SetupTerminationHandler function registers a termination handler
    function with the kernel in case of an abnormal termination of this
    process.

==============================================================================*/
static void SetupTerminationHandler( void )
{
    static struct sigaction sigact;

    memset( &sigact, 0, sizeof(sigact) );

    sigact.sa_sigaction = TerminationHandler;
    sigact.sa_flags = SA_SIGINFO;

    sigaction( SIGTERM, &sigact, NULL );
    sigaction( SIGINT, &sigact, NULL );

}

/*============================================================================*/
/*  TerminationHandler                                                        */
/*!
    Abnormal termination handler

    The TerminationHandler function will be invoked in case of an abnormal
    termination of this process.  The termination handler closes
    the connection with the variable server.

@param[in]
    signum
        The signal which caused the abnormal termination (unused)

@param[in]
    info
        pointer to a siginfo_t object (unused)

@param[in]
    ptr
        signal context information (ucontext_t) (unused)

==============================================================================*/
static void TerminationHandler( int signum, siginfo_t *info, void *ptr )
{
    syslog( LOG_ERR, "Abnormal termination of statemachine\n" );

    if ( pLCD != NULL )
    {
        if ( pLCD->hVarServer != NULL )
        {
            VARSERVER_Close( pLCD->hVarServer );
            pLCD->hVarServer = NULL;

            SetExclusive( pLCD->pDev, false );
            LCDClose( pLCD->pDev );
            pLCD = NULL;
        }
    }

    exit( 1 );
}

/*============================================================================*/
/*  run                                                                       */
/*!
    Run the LCD1602 controller

    The run function loops forever waiting for signals from the
    variable server or timer events.

    @param[in]
        pLCD
            pointer to the LCD1602 controller state object

    @retval EOK the LCD1602 controller completed successfully
    @retval EINVAL invalid arguments

==============================================================================*/
static int run( LCD1602 *pLCD )
{
    int result = EINVAL;
    int signum;
    int id;
    if ( pLCD != NULL )
    {
        result = EOK;

        while( true )
        {
            WaitSignal( &signum, &id );
            HandleSignal( pLCD, signum, id );
        }
    }

    return result;
}

/*============================================================================*/
/*  WaitSignal                                                                */
/*!
    Wait for a signal from the system

    The WaitSignal function waits for either a variable calculation request
    or timer expired signal from the system

@param[in,out]
    signum
        Pointer to a location to store the received signal

@param[in,out]
    id
        Pointer to a location to store the signal identifier

@retval 0 signal received successfully
@retval -1 an error occurred

==============================================================================*/
static int WaitSignal( int *signum, int *id )
{
    sigset_t mask;
    siginfo_t info;
    int result = EINVAL;
    int sig;

    if( ( signum != NULL ) &&
        ( id != NULL ) )
    {
        /* create an empty signal set */
        sigemptyset( &mask );


        /* calc notification */
        sigaddset( &mask, SIG_VAR_MODIFIED );

        /* print notification */
        sigaddset( &mask, SIG_VAR_PRINT );

        /* apply signal mask */
        sigprocmask( SIG_BLOCK, &mask, NULL );

        /* wait for the signal */
        sig = sigwaitinfo( &mask, &info );

        /* return the signal information */
        *signum = sig;
        *id = info._sifields._timer.si_sigval.sival_int;

        /* indicate success */
        result = EOK;
    }

    return result;
}

/*============================================================================*/
/*  HandleSignal                                                              */
/*!
    Handle Received Signals

    The HandleSignal function handles signals received from the system,
    such as one of the following:
        - SIG_VAR_PRINT
        - SIG_VAR_MODIFIED

    @param[in]
        pLCD
            pointer to the LCD1602 controller state object

    @param[in]
        signum
            the number of the received signal. One of:
            SIG_VAR_PRINT
            SIG_VAR_MODIFIED

    @param[in]
        id
            the id of the signal source

    @retval EOK the signal was handled successfully
    @retval ENOTSUP the signal was not supported
    @retval ENOENT the channel was invalid
    @retval EINVAL invalid arguments

==============================================================================*/
static int HandleSignal( LCD1602 *pLCD, int signum, int id )
{
    int sig;
    VAR_HANDLE hVar;
    int fd = -1;
    int result = EINVAL;
    int ch;

    if ( pLCD != NULL )
    {
        if( sig == SIG_VAR_MODIFIED )
        {
            /* get a handle to the ADC channel associated with
             * the specified variable */
            hVar = (VAR_HANDLE)id;
            result = OnChange( pLCD, hVar );
        }
        else if ( sig == SIG_VAR_PRINT )
        {
            /* open a print session */
            VAR_OpenPrintSession( pLCD->hVarServer,
                                  id,
                                  &hVar,
                                  &fd );

            /* print the file variable */
            PrintStatus( pLCD, fd );

            /* Close the print session */
            VAR_ClosePrintSession( pLCD->hVarServer,
                                   id,
                                   fd );

            result = EOK;
        }
        else
        {
            /* unsupported notification type */
            result = ENOTSUP;
        }
    }

    return result;
}

/*============================================================================*/
/*  SetupNotifications                                                        */
/*!
    Set up notifications for the LCD1602 controller

    The SetupNotifications function sets up the modified and render
    notifications for the LCD1602 controller.

    @param[in]
        pLCD
            pointer to the LCD1602 controller state which contains a handle
            to the variable server for requesting the notifications.

    @retval EOK the notification was successfully requested
    @retval ENOENT one or more of the requested variables was not found
    @retval EINVAL invalid arguments

==============================================================================*/
static int SetupNotifications( LCD1602 *pLCD )
{
    int result = EINVAL;
    int rc;

    if ( pLCD != NULL )
    {
        result = SetupPrintNotifications( pLCD );
        if ( result == EOK )
        {
            result = SetupModifiedNotifications( pLCD );
        }
    }

    return result;
}

/*============================================================================*/
/*  SetupPrintNotifications                                                   */
/*!
    Set up a render notifications for the LCD1602 controller

    The SetupPrintNotifications function sets up the render notifications
    for the LCD1602 controller.

    @param[in]
        pLCD
            pointer to the LCD1602 controller state which contains a handle
            to the variable server for requesting the notifications.

    @retval EOK the notification was successfully requested
    @retval ENOENT the requested variable was not found
    @retval EINVAL invalid arguments

==============================================================================*/
static int SetupPrintNotifications( LCD1602 *pLCD )
{
    int result = EINVAL;
    VAR_HANDLE hVar;

    if ( pLCD != NULL )
    {
        /* get a handle to the STATUS variable */
        hVar = VAR_FindByName( pLCD->hVarServer, "/HW/LCD1602/STATUS" );
        if( hVar != VAR_INVALID )
        {
            /* request render notification for the status variable */
            result = VAR_Notify( pLCD->hVarServer,
                                 hVar,
                                 NOTIFY_PRINT );
        }
        else
        {
            /* requested variable not found */
            result = ENOENT;
        }
    }

    return result;
}

/*============================================================================*/
/*  SetupModifiedNotifications                                                */
/*!
    Set up modified notifications for the LCD1602 controller

    The SetupModifiedNotifications function sets up the render notifications
    for the LCD1602 controller.

    The variables being monitored are:

    /HW/LCD1602/BACKLIGHT
    /HW/LCD1602/LINE1
    /HW/LCD1602/LINE2

    @param[in]
        pLCD
            pointer to the LCD1602 controller state which contains a handle
            to the variable server for requesting the notifications.

    @retval EOK the notifications were successfully requested
    @retval ENOENT the requested variable was not found
    @retval EINVAL invalid arguments

==============================================================================*/
static int SetupModifiedNotifications( LCD1602 *pLCD )
{
    int result = EINVAL;
    int rc;

    if ( pLCD != NULL )
    {
        result = EOK;

        rc = SetupModifiedNotification( pLCD->hVarServer,
                                        "/HW/LCD1602/BACKLIGHT",
                                        &(pLCD->hVarBacklight ) );
        if ( rc != EOK )
        {
            result = rc;
        }

        rc = SetupModifiedNotification( pLCD->hVarServer,
                                        "/HW/LCD1602/LINE1",
                                        &(pLCD->hVarLine1 ) );
        if ( rc != EOK )
        {
            result = rc;
        }


        rc = SetupModifiedNotification( pLCD->hVarServer,
                                        "/HW/LCD1602/LINE2",
                                        &(pLCD->hVarLine2 ) );
        if ( rc != EOK )
        {
            result = rc;
        }
    }

    return result;
}

/*============================================================================*/
/*  SetupModifiedNotification                                                 */
/*!
    Set up a modified notification for the LCD1602 controller

    The SetupModifiedNotification function sets up a modified notification
    for the LCD1602 controller for the specified system variable.

    @param[in]
        hVarServer
            handle to the variable server

    @param[in]
        name
            name of the variable to monitor

    @param[in]
        hVar
            pointer to a location to store the variable handle of
            the variable being monitored

    @retval EOK the notification was successfully requested
    @retval ENOENT the requested variable was not found
    @retval EINVAL invalid arguments

==============================================================================*/
static int SetupModifiedNotification( VARSERVER_HANDLE hVarServer,
                                      char *name,
                                      VAR_HANDLE *hVar )
{
    int result = EINVAL;
    VAR_HANDLE hdl;

    if ( ( pLCD != NULL ) &&
         ( hVar != NULL ) &&
         ( name != NULL ) )
    {
        /* get the variable handle given its name */
        hdl = VAR_FindByName( hVarServer, name );
        if( hdl != VAR_INVALID )
        {
            /* request MODIFIED notification */
            result = VAR_Notify( hVarServer,
                                 hdl,
                                 NOTIFY_MODIFIED );
            if ( result == EOK )
            {
                *hVar = hdl;
            }
        }
        else
        {
            result = ENOENT;
        }
    }

    return result;
}

/*============================================================================*/
/*  PrintStatus                                                               */
/*!
    Output the status of the LCD1602

    The PrintStatus function prints the status of the LCD1602 controller.

    @param[in]
        pLCD
            pointer to the LCD1602 controller state

    @retval EOK the status was output successfully
    @retval EINVAL invalid arguments

==============================================================================*/
static int PrintStatus (LCD1602 *pLCD, int fd )
{
    int result = EINVAL;
    bool backlight = false;
    int cx = 0;
    int cy = 0;
    char *device = "none";
    uint8_t address = 0;
    bool exclusive = false;

    if ( ( pLCD != NULL ) &&
         ( fd != -1 ) )
    {
        GetBacklight( pLCD->pDev, &backlight );
        GetExclusive( pLCD->pDev, &exclusive );
        GetCursorX( pLCD->pDev, &cx );
        GetCursorY( pLCD->pDev, &cy );
        GetDeviceName( pLCD->pDev, &device );
        GetAddress( pLCD->pDev, &address );

        dprintf(fd, "LCD1602 Status:\n");
        dprintf(fd, "Device: %s\n", device );
        dprintf(fd, "Address: 0x%02x\n", address );
        dprintf(fd, "Exclusive: %s\n", exclusive ? "true" : "false" );
        dprintf(fd, "Verbose: %s\n", pLCD->verbose ? "true" : "false" );
        dprintf(fd, "Backlight: %s\n", backlight ? "ON" : "OFF" );
        dprintf(fd, "Line1: %s\n", pLCD->line1 );
        dprintf(fd, "Line2: %s\n", pLCD->line2 );
        dprintf(fd, "Cursor X: %d\n", cx );
        dprintf(fd, "Cursor Y: %d\n", cy );
    }

    return result;
}

/*============================================================================*/
/*  OnChange                                                                  */
/*!
    Handle a change to a system variable

    The OnChange function handles a change to one of the following
    variables:

    /HW/LCD1602/BACKLIGHT
    /HW/LCD1602/LINE1
    /HW/LCD1602/LINE2

    Any change to these variables will cause an update to the
    attached LCD1602 hardware

    @param[in]
        pLCD
            pointer to the LCD1602 controller state

    @param[in]
        hVar
            handle to the variable which changed

    @retval EOK the change was handled successfully
    @retval EINVAL invalid arguments

==============================================================================*/
static int OnChange( LCD1602 *pLCD, VAR_HANDLE hVar )
{
    int result = EINVAL;

    if ( pLCD != NULL )
    {
        if ( hVar == pLCD->hVarBacklight )
        {
            result = UpdateBacklight( pLCD, hVar );
        }
        else if ( hVar == pLCD->hVarLine1 )
        {
            result = UpdateLine1( pLCD );
        }
        else if ( hVar == pLCD->hVarLine2 )
        {
            result = UpdateLine2( pLCD );
        }
        else
        {
            result = ENOENT;
        }
    }

    return result;
}

/*============================================================================*/
/*  UpdateBacklight                                                           */
/*!
    Handle a change to the /HW/LCD1602/BACKLIGHT system variable

    The UpdateBacklight function handles a change to the backlight
    system variable and updates the state of the backlight on the
    LCD 16x2 module

    @param[in]
        pLCD
            pointer to the LCD1602 controller state

    @param[in]
        hVar
            handle to the backlight variable

    @retval EOK the backlight was updated successfully
    @retval EINVAL invalid arguments

==============================================================================*/
static int UpdateBacklight( LCD1602 *pLCD, VAR_HANDLE hVar )
{
    int result = EINVAL;
    VarObject obj;
    bool backlight;

    if ( pLCD != NULL )
    {
        /* get the value of the backlight variable */
        if ( VAR_Get( pLCD->hVarServer, hVar, &obj ) == EOK )
        {
            /* get the requested backlight status */
            backlight = obj.val.ui == 0 ? false : true;

            result = SetBacklight( pLCD->pDev, backlight );
        }
    }

    return result;
}

/*============================================================================*/
/*  UpdateLine1                                                               */
/*!
    Handle a change to the /HW/LCD1602/LINE1 system variable

    The UpdateLine1 function handles a change to the LCD Screen line 1
    system variable and updates the contents of line 1 on the
    LCD 16x2 module

    @param[in]
        pLCD
            pointer to the LCD1602 controller state

    @retval EOK the contents of line 1 on the display were updated successfully
    @retval EINVAL invalid arguments

==============================================================================*/
static int UpdateLine1( LCD1602 *pLCD )
{
    int result = EINVAL;
    VarObject obj;

    if ( pLCD != NULL )
    {
        memset( pLCD->line1, 0, 17 );
        obj.len = 16;
        obj.type = VARTYPE_STR;
        obj.val.str = pLCD->line1;

        /* get the value of the backlight variable */
        result = VAR_Get( pLCD->hVarServer, pLCD->hVarLine1, &obj );
        if ( result == EOK )
        {
            result = DisplayLine( pLCD->pDev, 0x00, pLCD->line1 );
        }
    }

    return result;
}

/*============================================================================*/
/*  UpdateLine2                                                               */
/*!
    Handle a change to the /HW/LCD1602/LINE2 system variable

    The UpdateLine2 function handles a change to the LCD Screen line 2
    system variable and updates he contents of line 2 on the
    LCD 16x2 module

    @param[in]
        pLCD
            pointer to the LCD1602 controller state

    @retval EOK the contents of line 2 on the display were updated successfully
    @retval EINVAL invalid arguments

==============================================================================*/
static int UpdateLine2( LCD1602 *pLCD )
{
    int result = EINVAL;
    VarObject obj;

    if ( pLCD != NULL )
    {
        memset( pLCD->line2, 0x20, 17 );
        obj.len = 16;
        obj.type = VARTYPE_STR;
        obj.val.str = pLCD->line2;

        /* get the value of the backlight variable */
        result = VAR_Get( pLCD->hVarServer, pLCD->hVarLine2, &obj );
        if ( result == EOK )
        {
            result = DisplayLine( pLCD->pDev, 0x40, pLCD->line2 );
        }
    }

    return result;
}


/*! @}
 * end of lcd1602 group */

