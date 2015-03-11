// $Id: mcu.c,v 1.1 2001/08/24 18:24:16 tekhedd Exp $
//
// Copyright(C) 1999-2000 Seasound LLC.
// Copyright(C) 2001 Tom Surace. All rights reserved.

#include "mcu.h"


// Everything in this module is accessed at raised priority
#pragma code_seg()

// Time to wait for reply bytes from the attached device
#define UART_TIMEOUT          (50)  // milliseconds

const BYTE MCU_header[ SEASOUND_SYSEX_HEADER_SIZE ] =
{
    SEASOUND_SYSEX_HEADER
};

const BYTE MCU_footer[ SEASOUND_SYSEX_FOOTER_SIZE ] =
{
    SEASOUND_SYSEX_FOOTER
};

// -- Local functions --
static int _getByte( SoloInstance * solo, BYTE * byte_ret );
static int MCU_send( SoloInstance * solo,
                     const char *   msg,
                     DWORD          len );


//
// MCU_listenToSolo -- set the secondary midi input to listen to
//   the solo (primary attached device)
//
int MCU_listenToSolo( SoloInstance * solo )
{
    MidUart2SelectInput( solo, SSS_ATTACHEDDEV_SOLO );
}

//
// MCU_listenToExpander -- set the secondary midi input to listen
//   to the expander, A8, or whatever's attached to the internal
//   expansion header.
//
// Note: since the solo can send unsolicited messages, be sure to
// call "MCU_listenToSolo" after the message is recieved.
//
int MCU_listenToExpander( SoloInstance * solo )
{
    MidUart2SelectInput( solo, SSS_ATTACHEDDEV_EXPANDER );
}

//
// MCU_sendAttention -- send the "attention" command.
//
// Returns:
//   0 on success, nonzero otherwise
//
int MCU_sendAttention( SoloInstance * solo )
{
    BYTE cmd[2];
    cmd[0] = MCUCMD_ATTENTION;
    cmd[1] = MCU_PID_ALLCALL;
    
    return (MCU_send(solo, MCU_header, SEASOUND_SYSEX_HEADER_SIZE) ||
            MCU_send(solo, cmd,        2) ||
            MCU_send(solo, MCU_footer, SEASOUND_SYSEX_FOOTER_SIZE));
}

//
// MCU_sendLevelSwitchRequest -- request that the attached device
//   send all of its switch values.
//
// You will have to be listening to the expander to get its
// switch settings. This kind of sucks because you will miss
// any TCI messages that happen. Oh well.
//
// Returns:
//   0 on success, nonzero otherwise
//
int MCU_sendLevelSwitchRequest( SoloInstance * solo )
{
    BYTE cmd[2];
    cmd[0] = MCUCMD_LEVEL_SWITCH_REQ;
    cmd[1] = MCU_PID_ALLCALL;
    
    return (MCU_send(solo, MCU_header, SEASOUND_SYSEX_HEADER_SIZE) ||
            MCU_send(solo, cmd,        2) ||
            MCU_send(solo, MCU_footer, SEASOUND_SYSEX_FOOTER_SIZE));
}

//
// MCU_setInputGain - set the input gain for an input to
//   unity or greater.
//
// Parameters:
//   solo -
//   inputOffset - which input (0-7)
//   level - amount of gain (0 - 127, mostly)
//
int MCU_setInputGain( SoloInstance * solo,
                      int inputOffset,
                      int level )
{
    BYTE cmd[3];
    
    VASSERT( inputOffset >= 0 && inputOffset < 8, "range" );
    VASSERT( level >= 0 && level < 128, "range" );
    
    cmd[0] = MCUDAT_IN_GAIN;
    cmd[1] = (BYTE)inputOffset;
    cmd[2] = (BYTE)level;
    
    return (MCU_send(solo, MCU_header, SEASOUND_SYSEX_HEADER_SIZE) ||
            MCU_send(solo, cmd,        3) ||
            MCU_send(solo, MCU_footer, SEASOUND_SYSEX_FOOTER_SIZE));
}

//
// MCU_setInputAttenuation - set the input gain for an input to
//   unity or less
//
// Parameters:
//   solo -
//   inputOffset - which input (0-7)
//   level - amount of gain (0 - 127, where 0 is -infinity and
//     127 is unity gain.
//
int MCU_setInputAttenuation( SoloInstance * solo,
                             int inputOffset,
                             int level )
{
    BYTE cmd[3];
    
    VASSERT( inputOffset >= 0 && inputOffset < 8, "range" );
    VASSERT( level >= 0 && level < 128, "range" );
    
    cmd[0] = MCUDAT_IN_ATTEN;
    cmd[1] = (BYTE)inputOffset;
    cmd[2] = (BYTE)level;
    
    return (MCU_send(solo, MCU_header, SEASOUND_SYSEX_HEADER_SIZE) ||
            MCU_send(solo, cmd,        3) ||
            MCU_send(solo, MCU_footer, SEASOUND_SYSEX_FOOTER_SIZE));
}

//
// MCU_send -- send a message immediately (only for use during
//   init time with interrupts disabled).
//
// Returns:
//   0 on success
 //
int MCU_send( SoloInstance * solo, const char * msg, DWORD len )
{
    int i;
    DWORD startTime;

    // Each byte takes a little while to send
    for (i = 0; i < len; i++)
    {
        startTime = Get_System_Time();
        while (1)
        {
            if ((Get_System_Time() - startTime) > UART_TIMEOUT)
                return 1;       // ** give up **
            
            if (0 == iceMidiTwoDataOut(&solo->iceCookie, msg[i]))
                break;          // ** byte was sent, stop waiting **
        }
    }

    return 0;                   // success
}

//
// getImHere -- wait for "I'm Here" reply to "are you there"
//
// This waits until the entire message is recieved synchronously,
// and should therefore not be called when interrupts are enabled.
//
// Parameters:
//   solo -
//   product_id_ret - returned as: what is out there
//
// Returns:
//   0 on success
//
int MCU_getImHere( SoloInstance * solo, BYTE * product_id_ret )
{
   DWORD startTime;             // when we started this mess
   BYTE  aByte;                 // one byte from the uart
   int   i;

   // Format: SYSEX_HEADER, MCURSP_IM_HERE, product ID, F7
   BYTE  imHereHeader[5] =
   {
       SEASOUND_SYSEX_HEADER,   // 4 bytes
       MCURSP_IM_HERE           // 1 byte
   };

   // Outer loop to keep retrying for 100 ms or so. (it'd better respond
   // toot sweet).
   
   startTime = Get_System_Time();
   i = 0;
   while (1)
   {
       if ((Get_System_Time() - startTime) > 1000) // milliseconds
           return 1;            // What's the deal with this solo?
       
       if (0 != _getByte(solo, &aByte)) // error?
           return 1;            // ** quick exit **
           
       if (aByte == imHereHeader[i]) // got a match?
       {
           if (i >= 4)           // is that the whole header?
               break;
               
           ++i;                 // get the next one
       }
       else
       {
           i = 0;               // start over at the beginning
       }
   }
       
   // The next byte _is_ the product ID, we hope
   
   if (0 != _getByte(solo, product_id_ret)) // error?
       return 1;            // ** quick exit **

   // Now read the message terminator (clean up)
   _getByte( solo, &aByte );

   return 0;                    // success
}

//
// _getByte -- get a byte from uart 2
//
// Returns:
//   0 on success
//
int _getByte( SoloInstance * solo, BYTE * byte_ret )
{
   DWORD startTime = Get_System_Time();
   
   while ((Get_System_Time() - startTime) < UART_TIMEOUT)
   {
      if (0 == iceMidiTwoDataIn(&solo->iceCookie, byte_ret))
      {
          return 0;             // got a byte
      }
   }
   return 1;                    // timeout
}        

//
// getAnything -- wait for any incoming message, and return
//
// This waits until the entire message is recieved synchronously,
// and should therefore not be called when interrupts are enabled.
// This is really for debugging. Obviously it stops the system and
// that would suck.
//
// Basically, this just recieves data until the buffer is full or
// nothing is recieved for a while. Just keep calling it if you
// expect more message. :)
//
// Parameters:
//   solo -
//   message_ret - points to buffer where the message is to be stored
//   message_ret_size - size of the buffer pointed to by message_ret
//
// Returns:
//   Size of recieved buffer, or negative number on error.
//
int MCU_getAnything( SoloInstance * solo,
                     BYTE * message_ret,
                     int    message_ret_size )
{
   DWORD  startTime;             // when we started this mess
   BYTE * messagePtr = message_ret;
   int    length = 0;

   VASSERT( message_ret, "no buffer was supplied" );

   // Outer loop to keep retrying for 100 ms or so.
   
   startTime = Get_System_Time();
   while (1)
   {
       if ((Get_System_Time() - startTime) > 1000) // milliseconds
           break;               // What's the deal with this solo?

       if (length >= message_ret_size)
           break;
       
       if (0 != _getByte(solo, messagePtr)) // error?
           return -1;            // ** quick exit **

       ++ messagePtr;
       ++ length;
   }
       
   return length;
}
