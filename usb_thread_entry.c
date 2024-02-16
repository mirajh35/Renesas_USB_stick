/***********************************************************************************************************************
* DISCLAIMER
* This software is supplied by Renesas Electronics Corporation and is only intended for use with Renesas products. No
* other uses are authorized. This software is owned by Renesas Electronics Corporation and is protected under all
* applicable laws, including copyright laws.
* THIS SOFTWARE IS PROVIDED "AS IS" AND RENESAS MAKES NO WARRANTIES REGARDING
* THIS SOFTWARE, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. ALL SUCH WARRANTIES ARE EXPRESSLY DISCLAIMED. TO THE MAXIMUM
* EXTENT PERMITTED NOT PROHIBITED BY LAW, NEITHER RENESAS ELECTRONICS CORPORATION NOR ANY OF ITS AFFILIATED COMPANIES
* SHALL BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR ANY REASON RELATED TO THIS
* SOFTWARE, EVEN IF RENESAS OR ITS AFFILIATES HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
* Renesas reserves the right, without notice, to make changes to this software and to discontinue the availability of
* this software. By using this software, you agree to the additional terms and conditions found by accessing the
* following link:
* http://www.renesas.com/disclaimer
*
* Copyright (C) 2017 Renesas Electronics Corporation. All rights reserved.
***********************************************************************************************************************/
#include "usb_thread.h"
#include "board_cfg.h"



#define SEMI_HOSTING

#ifdef SEMI_HOSTING
    #ifdef __GNUC__
    extern void initialise_monitor_handles(void);
    #endif
#endif

#define     MAX_NUM_OF_TRY              (1000000)
#define     UX_STORAGE_BUFFER_SIZE      (64*1024)

#define     EVENT_USB_PLUG_IN           (1UL << 0)
#define     EVENT_USB_PLUG_OUT          (1UL << 1)

static char        local_buffer[UX_STORAGE_BUFFER_SIZE];
static bsp_leds_t   tLeds;
static FX_FILE      my_file;

static void set_led_on(int iIdx);
//static void set_led_off(int iIdx);
//static void blink_led_blink(int iIdx, int iCount);

UINT usb_host_plug_event_notification(ULONG usb_event, UX_HOST_CLASS * host_class, VOID * instance);


static void set_led_on(int iIdx) {
    if ( (iIdx >= 0) && (iIdx <= tLeds.led_count) ) {
        g_ioport_on_ioport.pinWrite(tLeds.p_leds[iIdx], LED_ON);
    }
}


static void led_clear(void)
{
    uint16_t led_count = tLeds.led_count;
    uint8_t led_number = 0;
    while(led_count)
    {
        g_ioport_on_ioport.pinWrite(tLeds.p_leds[led_number], LED_OFF);
        led_number++;
        led_count--;
    }
}

//callback function for USB_MSC_HOST
UINT usb_host_plug_event_notification(ULONG usb_event, UX_HOST_CLASS * host_class, VOID * instance)
{
    /* variable to hold the UX calls return values */
    UINT ux_return;

    UX_HOST_CLASS_STORAGE_MEDIA * p_ux_host_class_storage_media;

    // Check if host_class is for Mass Storage class.
    if (UX_SUCCESS == _ux_utility_memory_compare (
            _ux_system_host_class_storage_name,
            host_class,
            _ux_utility_string_length_get (_ux_system_host_class_storage_name)))
    {
        // Get the pointer to the media
        ux_return = ux_system_host_storage_fx_media_get (instance, &p_ux_host_class_storage_media, &g_fx_media0_ptr);

        if(ux_return != UX_SUCCESS)
        {
            /* This is a fairly simple error handling - it holds the
               application execution. In a more realistic scenarios
               a more robust and complex error handling solution should
               be provided. */
               #ifdef SEMI_HOSTING
                   if (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)
                   {
                       /* Debugger is connected */
                       /* Call this before any calls to printf() */
                       printf("Could not get the pointer to the media, error:%d\n", ux_return);
                   }
               #endif
                   tx_thread_sleep(TX_WAIT_FOREVER);
        }
        //Check the usb_event type
        switch (usb_event) {
            case EVENT_USB_PLUG_IN:
                // Notify the insertion of a USB Mass Storage device.
                tx_event_flags_set (&g_usb_plug_events, EVENT_USB_PLUG_IN, TX_OR);
                break;
            case EVENT_USB_PLUG_OUT:
                // Notify the removal of a USB Mass Storage device.
                tx_event_flags_set (&g_usb_plug_events, EVENT_USB_PLUG_OUT, TX_OR);
                break;
            default:
                //ignore this unsupported event
                break;
        }
    }
    return UX_SUCCESS;
}



/* USB Thread entry function */
/* The application project demonstrates the typical use of the USBX Host Class Mass Storage module APIs.
 * The application project main thread entry waits for the connection from the callback function, reads the
 * firstdir\counter.txt file in the USB memory, and updates the described number. If firstdir does not exist,
 * the directory information is ignored. If this file does not exist, creates a file. After updating the file,
 * the application waits until the USB memory is unplugged. The application uses LED2 LED1 and LED0 in sequence
 * to provide status of the application. (When all three or two (depends upon evaluation board) lights up, it
 * indicates operation is complete and user can disconnect the attached mass storage device.The application uses
 * SEMI-HOSTING feature, it displays relevant messages and errors to the user. */
void usb_thread_entry(void)
{

    CHAR                        volume[32];
    FX_MEDIA                    * p_media;
    ULONG                       actual_length = 0;
    ULONG                       actual_flags;
    int                         iValue = 0;
    UINT                        tx_return;
    UINT                        fx_return;
    // Get LED information for this board
    R_BSP_LedsGet(&tLeds);



    #ifdef SEMI_HOSTING
    #ifdef __GNUC__
        if (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)
        {
            initialise_monitor_handles();
        }
    #endif
    #endif


    while (1)
    {
        /* clear leds */
        led_clear();

        // Wait until device inserted.
        tx_return = tx_event_flags_get (&g_usb_plug_events, EVENT_USB_PLUG_IN, TX_OR_CLEAR, &actual_flags, TX_WAIT_FOREVER);
        if(tx_return!=TX_SUCCESS)
        {
            /* This is a fairly simple error handling - it holds the
               application execution. In a more realistic scenarios
               a more robust and complex error handling solution should
               be provided. */
               #ifdef SEMI_HOSTING
                   if (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)
                   {
                       /* Debugger is connected */
                       /* Call this before any calls to printf() */
                       printf("Could not get the event flags status, error:%d\n", tx_return);
                   }
               #endif
                   tx_thread_sleep(TX_WAIT_FOREVER);
        }
        set_led_on(2);
        // Get the pointer to FileX Media Control Block for a USB flash device
        p_media = g_fx_media0_ptr;

        // Retrieve the volume name of the opened media from the Data sector
        fx_return = fx_media_volume_get(p_media, volume, FX_DIRECTORY_SECTOR);
        //fx_return = FX_SUCCESS;
        if (fx_return == FX_SUCCESS )
        {
            // Set the default directory in the opened media, arbitrary name called "firstdir"
            fx_directory_default_set(p_media, "firstdir");
            // Suspend this thread for 200 time-ticks
            tx_thread_sleep(100);

            // Try to open the file, 'counter.txt'.
            fx_return = fx_file_open(p_media, &my_file, "counter.txt",
                                  FX_OPEN_FOR_READ | FX_OPEN_FOR_WRITE);
            if (fx_return != FX_SUCCESS)
            {
                //The 'counter.txt' file is not found, so create a new file
                fx_return = fx_file_create(p_media, "counter.txt");
                if (fx_return != FX_SUCCESS) {
                    // Blink the LED 1 to report an error

                    break;
                }
                // Open that file
                fx_return = fx_file_open(p_media, &my_file, "counter.txt",
                                      FX_OPEN_FOR_READ | FX_OPEN_FOR_WRITE);
                if (fx_return != FX_SUCCESS) {

                    break;
                }
            }
            /* File open successful */
            set_led_on(1);
            // Already open a file, then read the file in blocks
            // Set a specified byte offset for reading
            fx_return = fx_file_seek(&my_file, 0);
            if (fx_return == FX_SUCCESS)
            {
                fx_return = fx_file_read(&my_file, local_buffer, UX_STORAGE_BUFFER_SIZE, &actual_length);
                if ((fx_return == FX_SUCCESS) || (fx_return == FX_END_OF_FILE) )
                {
                    if (actual_length <= 0) {
                        //empty file
                        actual_length = 1;
                        iValue = 1;
                    } else {
                        iValue = atoi(local_buffer);
                        iValue++;
                    }
                    actual_length = (ULONG) sprintf(local_buffer, "%d", iValue);


                    // Set the specified byte offset for writing
                    fx_return = fx_file_seek(&my_file, 0);
                    if (fx_return == FX_SUCCESS) {
                        // Write the file in blocks
                        fx_return = fx_file_write(&my_file, local_buffer, actual_length);
                        if (fx_return == FX_SUCCESS)
                        {

                        } else
                        {
                            /* This is a fairly simple error handling - it holds the
                               application execution. In a more realistic scenarios
                               a more robust and complex error handling solution should
                               be provided. */
                               #ifdef SEMI_HOSTING
                                   if (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)
                                   {
                                       /* Debugger is connected */
                                       /* Call this before any calls to printf() */
                                       printf("failed to write, error:%d\n", fx_return);
                                   }
                               #endif
                                   tx_thread_sleep(TX_WAIT_FOREVER);

                        }
                    }
                }
            }
            else
            {
                /* This is a fairly simple error handling - it holds the
                   application execution. In a more realistic scenarios
                   a more robust and complex error handling solution should
                   be provided. */
                   #ifdef SEMI_HOSTING
                       if (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)
                       {
                           /* Debugger is connected */
                           /* Call this before any calls to printf() */
                           printf("fx_file_seek()or fx_file_read() is failed, error:%d\n", fx_return);
                       }
                   #endif
                       tx_thread_sleep(TX_WAIT_FOREVER);

            }
            //Close already opened file
            fx_return = fx_file_close(&my_file);
            if(fx_return != FX_SUCCESS)
            {
                /* This is a fairly simple error handling - it holds the
                   application execution. In a more realistic scenarios
                   a more robust and complex error handling solution should
                   be provided. */
                   #ifdef SEMI_HOSTING
                       if (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)
                       {
                           /* Debugger is connected */
                           /* Call this before any calls to printf() */
                           printf("Could not close the file, error:%d\n", fx_return);
                       }
                   #endif
                       tx_thread_sleep(TX_WAIT_FOREVER);
            }

            tx_thread_sleep(200);
        }
        else
        {
            /* This is a fairly simple error handling - it holds the
               application execution. In a more realistic scenarios
               a more robust and complex error handling solution should
               be provided. */
               #ifdef SEMI_HOSTING
                   if (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)
                   {
                       /* Debugger is connected */
                       /* Call this before any calls to printf() */
                       printf("Could not get the media volume, error:%d\n", fx_return);
                   }
               #endif
                   tx_thread_sleep(TX_WAIT_FOREVER);
        }
        /* flush the media */
        fx_return = fx_media_flush(p_media);
        if(fx_return != FX_SUCCESS)
        {
            /* This is a fairly simple error handling - it holds the
               application execution. In a more realistic scenarios
               a more robust and complex error handling solution should
               be provided. */
               #ifdef SEMI_HOSTING
                   if (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)
                   {
                       /* Debugger is connected */
                       /* Call this before any calls to printf() */
                       printf("Could not flush the media, error:%d\n", fx_return);
                   }
               #endif
                   tx_thread_sleep(TX_WAIT_FOREVER);
        }

        /* close the media */
        fx_return = fx_media_close(p_media);
        if(fx_return != FX_SUCCESS)
        {
            /* This is a fairly simple error handling - it holds the
               application execution. In a more realistic scenarios
               a more robust and complex error handling solution should
               be provided. */
               #ifdef SEMI_HOSTING
                   if (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)
                   {
                       /* Debugger is connected */
                       /* Call this before any calls to printf() */
                       printf("Could not close the media, error:%d\n", fx_return);
                   }
               #endif
                   tx_thread_sleep(TX_WAIT_FOREVER);
        }
        #ifdef SEMI_HOSTING
            if (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)
            {
                /* Debugger is connected */
                /* Call this before any calls to printf() */
                printf("Disconnect the device \n");
            }
        #endif
        set_led_on(0);

        // Wait for unplugging the USB
        tx_event_flags_get (&g_usb_plug_events, EVENT_USB_PLUG_OUT, TX_OR_CLEAR, &actual_flags, TX_WAIT_FOREVER);
    } //while(1)
}
