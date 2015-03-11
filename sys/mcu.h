// -*-c++-*-
//
// $Id: mcu.h,v 1.1 2001/08/24 18:24:16 tekhedd Exp $
//
// MCU functions -- secondary midi port commands that talk
//   to the Microcomputer C??? Unit
//

#ifndef _MCU_H_
#define _MCU_H_

// Commands to select input from either the primary (solo) or
// secondary (expander) attached device. It's important to select
// the input before sending a command because the ACK may be
// missed otherwise.
extern int MCU_listenToSolo( SoloInstance * solo );
extern int MCU_listenToExpander( SoloInstance * solo );

//
// Commands that send a message to the solo (synchronously)
//
extern int MCU_sendAttention(SoloInstance * solo);
extern int MCU_sendLevelSwitchRequest( SoloInstance * solo );
extern int MCU_setInputGain( SoloInstance * solo,
                             int inputOffset,
                             int level );
extern int MCU_setInputAttenuation( SoloInstance * solo,
                                    int inputOffset,
                                    int level );

//
// Commands that wait for a "response" message (synchronously)
//
extern int MCU_getImHere( SoloInstance * solo, BYTE * product_id_ret );
extern int MCU_getAnything( SoloInstance * solo,
                            BYTE * message_ret,
                            int    message_ret_size );

#endif // _MCU_H_
