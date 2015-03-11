// -*-c++-*-
// $Id: ice.h,v 1.2 2001/08/06 03:16:23 tekhedd Exp $
// IC Ensemble interface header
// Copyright(C) 1999-2000 Seasound LLC
// Copyright(C) 2001 Tom Surace. All rights reserved.

#ifndef _ICE_H_
#define _ICE_H_

// Just until such time as I decide to put logging back into
// the assert function... --trs
#ifndef VASSERT
# define VASSERT(t,m) ASSERT(t)
#endif

// Bits in the CCSO_IRQSTATUS register:
#define ICE_IRQMIDIONEBIT (1 << 7)
#define ICE_IRQTIMERBIT   (1 << 6)
#define ICE_IRQMIDITWOBIT (1 << 5)
#define ICE_IRQMTBIT      (1 << 4) // Multi Track
#define ICE_IRQFMBIT      (1 << 3) 
#define ICE_IRQPLAYDMABIT (1 << 2) // direct sound
#define ICE_IRQRECDMABIT  (1 << 1) // direct sound
#define ICE_IRQNATIVEBIT  (1 << 0)

#define ICE_COOKIE_MAGIC (0x17121217)
#define IS_GOOD_COOKIE(cookie) ((cookie).magic == ICE_COOKIE_MAGIC)

//
// Offsets -- Might as well make macros:
//

// * PCI registers

// PCIO_CODEC register bits: (0x60)
//   Bit - Desc
//   7:6 - XIN2 (clock) 00: 22.5792MHz
//                      01: 16.9344MHz
//                      10: from external clock chip
//                      11: reserved
//   5   - 0: one MPU401
//         1: two MPU401 UARTs
//   4   - 1 if legacy AC97 codec is installed, 0 otherwise
//   3:2 - 00: 1 stereo codec
//         01: 2 
//         10: 3 
//         11: 4 
//   1:0 - 00: 1 stereo dac (etc, up to 11: 4 dac's)
//
// PCIO_ACLINK (reg 0x61)
//   7   - 0: AC97 converter on pro mult
//         1: I2S
//   6:2 - Reserved
//   1   - (AC97 only)  1: packed mode (SDATA_OUT packed in 3-10)
//                      0: split mode (SDATA_OUT split to different pins)
//   0   - (AC97 only) same as bit 1 but for SDATA_IN
//

// * CCS_* - General Controller Registers
// (IO region 0)
#define CCS_IRQMASK    (0x01)
#define CCS_IRQSTATUS  (0x02)
#define CCS_CCI_INDEX  (0x03)
#define CCS_CCI_DATA   (0x04)
#define CCS_UART_1_DAT (0x0c)
#define CCS_UART_1_CMD (0x0d)
#define CCS_GAMEPORT   (0x0f)
#define CCS_I2C_DEV    (0x10)
#define CCS_I2C_ADDR   (0x11)
#define CCS_I2C_DATA   (0x12)
#define CCS_I2C_CTRL   (0x13)
#define CCS_UART_2_DAT (0x1c)
#define CCS_UART_2_CMD (0x1d)
#define CCS_TIMER      (0x1e) // 1e-1f

//
// CCS_I2C_CTRL bits
//
#define I2C_BUSY_BIT          (1 << 0)
#define I2C_VENDOR_BIT        (1 << 1) // reread vendor ID?
#define I2C_EEPROM_DETECT_BIT (1 << 7)

//
// * CCIxx - offsets that are accessible through the
//     CCS offsets 03/04 (index/data)
//
#define CCI_GPIO_DATA      ((BYTE)(0x20))
#define CCI_GPIO_WRITEMASK ((BYTE)(0x21))
#define CCI_GPIO_DIRECTION ((BYTE)(0x22))

// Note that the solo driver doesn't use PCI I/O regions
// 1 and 2, since 2 has to do with the consumer section
// to which no hardware is attached, and 1 is, well, related
// to the directsound DMA configuration and the AC97 
// codec which, once again, does not exist on the Solo. :)

// * MTO - MultiTrackOffset
// IO range 3
#define MTO_IRQMASK     (0x00)
#define MTO_SAMPLERATE  (0x01)
#define MTO_I2SFORMAT   (0x02)
#define MTO_AC97INDEX   (0x04)
#define MTO_AC97COMSTAT (0x05)
#define MTO_AC97DATA    (0x06)
#define MTO_PLAYBASE    (0x10)
// SIZECOUNT - so named because writing sets the buffer size, reading
//   gets the current count:
#define MTO_PLAYSIZECOUNT (0x14)
// TERM - when the current pos reaches this value, we get an interrupt
#define MTO_PLAYTERM     (0x16)
#define MTO_PLAYCONTROL  (0x18)
#define MTO_RECBASE      (0x20)
#define MTO_RECSIZECOUNT (0x24)
#define MTO_RECTERM      (0x26)
#define MTO_RECCONTROL   (0x28)
#define MTO_PSDOUTROUTE  (0x30)
#define MTO_SPDOUTROUTE  (0x32)
#define MTO_PSDOUTSRC    (0x34) // 32 bits
#define MTO_VOLDATALEFT  (0x38)
#define MTO_VOLDATARIGHT (0x39)
#define MTO_VOLINDEX     (0x3A)
#define MTO_PEAKINDEX    (0x3e)
#define MTO_PEAKDATA     (0x3f)

// Bits in the MTO_IRQMASK register:
//   note that the irq mask is "clear to enable", so it's a true mask.
#define MTO_IRQPLAYEN  (1 << 6) // play irq mask bit
#define MTO_IRQRECEN   (1 << 7) // rec irq mask bit
#define MTO_IRQPLAYBIT (1 << 0) // play irq status bit
#define MTO_IRQRECBIT  (1 << 1) // rec irq status bit

//
// Values for the low nibble of MTO_SAMPLERATE:
//
#define MTSR_48_kHz     (0x00) // default?
#define MTSR_24_kHz     (0x01)
#define MTSR_12_kHz     (0x02)
#define MTSR_9_6_kHz    (0x03)
#define MTSR_32_kHz     (0x04)
#define MTSR_16_kHz     (0x05)
#define MTSR_96_kHz     (0x07)
#define MTSR_44_1_kHz   (0x08)
#define MTSR_22_05_kHz  (0x09)
#define MTSR_11_025_kHz (0x0a)
#define MTSR_88_2_kHz   (0x0b)

#define MTSR_SPDIF_SLAVE_BIT (1 << 4)

// The bit in the PCI configuration register that
// enables/disables the Error line (which is mapped to
// NMI on most PC systems).
#define PCI_SERR_BIT    (1 << 8)



// Seasound signal routings
enum
{
    ICEROUTE_NORMAL,            // regular old audio in and out
    ICEROUTE_CLONE,             // analog 0/1 goes to analog out and S/PDIF
    ICEROUTE_CONVERTER,         // S/PDIF converter mode
    ICEROUTE_SPDIFMIX,          // S/PDIF in is mixed into the audio out
    ICEROUTE_SPDIFMIX_CLONE,    // S/PDIF mix combined with clone mode.
    ICEROUTE_INVALID // should be last
};

// bits in the command register:
#define ICE_DSR_BIT        (1 << 7) // data set ready
#define ICE_DRR_BIT        (1 << 6) // data read ready


#endif // _ICE_H_

