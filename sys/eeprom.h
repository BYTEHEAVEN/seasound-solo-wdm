// -*-c++-*-
// $Id: eeprom.h,v 1.3 2001/08/10 06:13:27 tekhedd Exp $
/*
 *
 * Copyright (C) 1998-1999 IC Ensemble, Inc. All Rights Reserved.
 * Modifications copyright (C) 1999-2000 SeaSound LLC. All rights reserved.
 * WDM Port copyright(C) 2001 Tom Surace. All rights reserved.
 *
 * Information in this file is the intellectual property of
 * IC Ensemble, Inc., and contains trade secrets that must be
 * stored and viewed confidentially.
 * By viewing, using, modifying and compiling the programming code below
 * you acknowledge you have read, accepted and executed the Software
 * licensing agreement your company has established with IC Ensemble.
 * The following software can only be used in conjuction with IC Ensemble
 * products.        
 *
 * Abstract:
 *
 *		This include file defines the EEPROM images. 
 */

#ifndef _EEPROM_H_
#define _EEPROM_H_

// Note: Customers should not change the definition of this EEPROM image.
// However, customer can append to the structure below any vendor specific
// fields. Please also adjust the value of "bSize" accordingly.
//

#define MAX_EEPROM_SIZE     (256)     // in bytes

// TRS: offsets from 128 on are used by seasound on their cards :)

// definition for version 1 EEPROM

#pragma pack(1)                     // assume byte packing

//
// EEPROM - this data is located at offset 0 in the eeprom
//
typedef struct _EEPROM {
    DWORD   dwSubVendorID;  // PCI[2C-2F]
    BYTE    bSize;          // size of EEPROM image in bytes
    BYTE    bVersion;       // version equal 1 for this structure.
    BYTE    bCodecConfig;   // PCI[60]
    BYTE    bACLinkConfig;  // PCI[61]
    BYTE    bI2SID;         // PCI[62]
    BYTE    bSpdifConfig;   // PCI[63]
    BYTE    bGPIOInitMask;  // Corresponding bits need to be init'ed
                            // 0 means write bit and 1 means cannot write
    BYTE    bGPIOInitState; // Initial state of GPIO pins
    BYTE    bGPIODirection; // GPIO Direction State
    WORD    wAC97MainVol;
    WORD    wAC97PCMVol;
    WORD    wAC97RecVol;
    BYTE    bAC97RecSrc;
    BYTE    bDACID[4];      // I2S IDs for DACs
    BYTE    bADCID[4];      // I2S IDs for ADCs
} EEPROM, *PEEPROM;

#define EEPROM_SIZE    sizeof(EEPROM)
#define EEPROM_SIZE_OFFSET  4   // offset of size

typedef union {
    EEPROM      Eeprom;        
    BYTE        Bytes[EEPROM_SIZE];
} EEPROM_U;

// I2S converter properties:
// Since each I2S Codec is unique in its bit width, 96k sampling rate support,
// and volume control support, the following structure is used to store the
// various properties.
// A global table is built, and each codec is assigned an I2S id which is used 
// as an index to this table. In a particular implementation, the I2S id is 
// programmed in EEPROM for driver to retrieve.

typedef struct {
    WORD    wBitWidth;      // max. bit width of codec
    WORD    w96kSupport;    // enum. of 96k support
    WORD    wVolSupport;    // enum. of volume support
} I2S_CODEC;

// I2S IDs: I2SID_vvv_ppp
// where vvv is vendor name, and ppp specifies part no.

typedef enum {
    I2SID_None,         // ID 0 is not assigned
    I2SID_AKM_AK4393,   // DAC   
    I2SID_AKM_AK5393,   // ADC   
    I2SID_MAX
} I2S_ID;

// 
typedef enum {
    I2S_96K_NONE = 0,       // no 96k support
    I2S_96K_GPIO,           // 96k sample rate is programmed via GPIO pins
                            // pin 0 = 1 when SR = 96k, pin 0 = 0 otherwise
    I2S_96K_MAX
} I2S_96K_SUPPORT;

typedef enum {
    I2S_VOL_NONE = 0,       // no volume control
    I2S_VOL_MAX
} I2S_VOL_SUPPORT;

//
// SSS_EEPROM -- seasound solo eeprom data, located at offset
//   128 in the eeprom.
//
// The original solo had uninitialized data there, which reads
// back as all 1's (0xffffffff)
//
#define SSS_EEPROM_OFFSET (128)
#define SSS_EEPROM_COOKIE (0x1412eba0)
typedef struct SSS_EEPROM_STRUCT
{
    DWORD cookie;               // contains SSS_EEPROM_COOKIE

    // Version was added first when I didn't realize there'd
    // be several types of cards based on the ICE chipset,
    // it being relatively lame and all. Therefore, the
    // first field has been renamed "type" and the second
    // field is now "version". Which is fine and fully backwards
    // compatible if you use the types as defined below:
    BYTE  type;                 // see below
    BYTE  version;              // usually 1 but "0xff" for (version<3)
    
    // The remaining 123 bytes are reserved for expansion.
} SSS_EEPROM;

#define SSS_PCICARDTYPE_SOLO    (1) // or 0xff
#define SSS_PCICARDTYPE_SOLOEX  (2) // (featuring the EX connector)
#define SSS_PCICARDTYPE_DIO     (3) // ADAT & S/PDIF test card
#define SSS_PCICARDTYPE_SOLOIST (4) // Has no S/PDIF installed

#pragma pack()              // revert to default packing
/*
 * Revision History:
 *
 * $Log: eeprom.h,v $
 * Revision 1.3  2001/08/10 06:13:27  tekhedd
 * Builds but does not link and I am very tired. I feel I am getting close to a working wave port.
 *
 * Revision 1.2  2001/08/06 05:36:19  tekhedd
 * It builds. Does it work?
 *
 * Revision 1.3  2000/11/18 05:40:55  toms
 * New version tags becasue we keep coming out with new variations of PCI cards.
 *
 * Revision 1.2  2000/03/03 03:31:11  toms
 * Created per-instance data structures for multi-card support. A8 support. PCI card v2 support.
 *
 * Revision 1.1  1999/12/08 03:46:41  toms
 * Eeprom low-level utilities and such.
 *
 * Revision 1.1  1999/12/04 02:44:19  toms
 * Added the core of the test program...some of it anyway.
 *
 * Revision 1.1  1999/10/10 23:54:58  toms
 * The eeprom programmer actually works. Yay.
 *
 * Revision 1.1  1999/04/13 00:34:10  toms
 * Added code to load default settings on boot (from registry)
 *
 * 
 * 5     3/31/99 3:20p Winston
 * Enclose in #ifndef, #define, etc. to avoid multiple inclusions.
 * 
 * 4     2/11/99 1:57p Winston
 * Added copyright and comments.
 *
 */

#endif // _EEPROM_H_

