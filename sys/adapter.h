// $Id: adapter.h,v 1.9 2001/11/25 01:03:13 tekhedd Exp $
// Copyright (C) 2001 Tom Surace. All rights reserved.

// Adapter object for device as required by the wdm streaming
// port class hierarchy. Only included because I can't use the
// DriverWorks KPnpDevice classes. :(

#ifndef _ADAPTER_H_
#define _ADAPTER_H_

// Prevent addition of yet more versions of operator new
#define _NEW_DELETE_OPERATORS_

#include "stdunk.h"
#include "portcls.h"

#include "config.h"        // Other macros
#include "ice.h"				// Macros for offsets, etc
#include "iadaptersolo.h"

#include "kcom.h"

// In adapter.cpp or something
extern KTrace t;

// Wave, topology, midi. 
#define MAX_MINIPORTS (3)


// Forward declaration
class CMiniportWaveCyclicSolo;
class CMiniportTopologySolo;

extern "C" NTSTATUS CAdapterSoloStartDevice 
(
 IN  PDEVICE_OBJECT  DeviceObject,   // Device object.
 IN  PIRP            Irp,            // IO request packet.
 IN  PRESOURCELIST   ResourceList    // List of hardware resources.
);


//
// CAdapterSolo class
//
class CAdapterSolo
:   public IAdapterSolo,
    public IAdapterPowerManagement,
    public CUnknown
{
public:
   bool OnIoctl( KIrp & I, NTSTATUS * status );

private:
   // Data members
   PDEVICE_OBJECT _pdo; // the pdo beneath us. Mostly.
   PDEVICE_OBJECT _fdo; // our fuctional device object

   DEVICE_POWER_STATE      _powerState;

   // 
   // The ISR macros define these functions:
   //   BOOLEAN Isr_Irq(void); 
	//   VOID DpcFor_Irq(PVOID Arg1, PVOID Arg2); 
   // 
   // The ISR (interrupt service routine)
	MEMBER_ISR(CAdapterSolo, Isr_Irq);
   // The DPC (deferred procedure call) for the ISR
	MEMBER_DPC(CAdapterSolo, DpcFor_Irq);


   // TRS: For those of you familiar with the WDM audio example: This
   //      has been moved here since the whole pro multitrack section
   //      runs at the same frequency. :)
   ULONG    _samplingFrequency; // Frames per second.

	// The following members correspond to hardware resources in the
	// device.
   BOOLEAN        _iceIsStarted;    // true if hw is touchable
	KIoRange			_ccsRange;			// PCI range 0
	KIoRange			_indexedRange;		// PCI range 1
	KIoRange			_dsdmaRange;		// PCI range 2
	KIoRange			_mtRange;			// PCI range 3
	KInterrupt		_Irq;
	KDeferredCall	_DpcFor_Irq;

   PSERVICEGROUP     _renderServiceGroup;    // For notification.
   PSERVICEGROUP     _captureServiceGroup;   // For notification.

   PPORTWAVECYCLIC   _portWave;


   // DMA buffer bookkeeping:
   PHYSICAL_ADDRESS  _recDmaBufferPhys;  // physical address of dma
   PULONG            _recDmaBufferVirt;  // systm address of dma
   ULONG             _recBufferSize;     // in bytes...

   PHYSICAL_ADDRESS  _playDmaBufferPhys; // physical address of dma
   PULONG            _playDmaBufferVirt; // systm address of dma
   ULONG             _playBufferSize;    // in bytes...

   // Set by wave miniport constructor
   ULONG             _recDmaPageCount;   // Probably 2,4,or 8
   ULONG             _playDmaPageCount;  // Probably 2,4,or 8

   ULONG             _recDmaPageFrames;  // # of samples in a dma page
   ULONG             _playDmaPageFrames; // # of samples in a dma page

   PDMACHANNEL       _recDmaChannel;  
   PDMACHANNEL       _playDmaChannel; 
   PDMACHANNEL       _playLegacyDmaChannel; // 16-bit version
   NTSTATUS _setUpDma();
   NTSTATUS AllocDma( PDMACHANNEL * channel,
                      BOOLEAN       isCapture );

   void     _copyLegacyToPlay( ULONG startFrame,
                               ULONG frameCount,
                               ULONG baseOffset );

   // WAVE system bookkeeping
   // This is used when reformatting the buffers for happy low-latency
   // legacy action. (And ASIO?)
   DmaMode        _recMode;
   DmaMode        _playMode;
   ULONG          _recPosIrqs;   // Count irqs. Dropped irqs are evil.
   ULONG          _playPosIrqs;

	// Mask of enabled irq's. This is cached in memory because..er...
	// there's a good reason really. Oh yeah, to allow temporarily 
	// disabling interrupts and then restoring the old mask. 
	BYTE  _ccsIrqMask;
	int   _irqMaskOverride; // if nonzero, 0xff is used instead of ccsIrqMask

	// Maintain the multitrack IRQ mask for play/record. This accelerates
	// things because, get this, the mask of bits to disable play and record
	// separately are in the same byte as the write-to-clear bits to clear
	// the interrupt. In the same *BYTE*???? Aaaaaugh!
	BYTE  _mtIrqMask;
 
	// This stuff is stored in the registry, and may be found
	// the SSSolo object, right?
   int _loopbackMode;			// routing
   int _cardVersion;           // 1 for most cards
   int _cardType;              // see ../util/tsyn/common/eeprom.h

   // This mutex prevents access to the ICE registers from multiple
   // threads. It does not protect us from the interrupt handler, but
   // I am assuming (cross fingers) that the wave miniport won't be 
   // trying anything tricky from within its notify functions. :)
   KMutex   _hardwareSync;      


public:
   static NTSTATUS New
   (
    OUT     CAdapterSolo ** ppAdapter,
    IN      PUNKNOWN        UnknownOuter    OPTIONAL,
    IN      POOL_TYPE       PoolType
   );
   
   static NTSTATUS AdapterDispatchPnp
   (
    IN  PDEVICE_OBJECT  pDeviceObject,
    IN  PIRP            pIrp
   );


   // CUnknown stuff from stdunk.h, etc
   DECLARE_STD_UNKNOWN();
   DEFINE_STD_CONSTRUCTOR(CAdapterSolo);
   ~CAdapterSolo();

   //  IAdapterPowerManagement declarations from portcls.h
   IMP_IAdapterPowerManagement;

   NTSTATUS Init( PDEVICE_OBJECT deviceObj,
                  PDEVICE_OBJECT pdo,
                  PIRP           irp,
                  PRESOURCELIST resources );

protected:
   // IAdapterSolo implementation
   STDMETHOD_(void, SetPlayMode)(DmaMode mode);
   STDMETHOD_(void, SetRecMode)(DmaMode mode);

   STDMETHOD_(NTSTATUS,SetSamplingFrequency)( ULONG newFreq );
   STDMETHOD_(ULONG,GetSamplingFrequency)();

   STDMETHOD_(NTSTATUS, GetRenderServiceGroup)(PSERVICEGROUP *gp_ret);
   STDMETHOD_(NTSTATUS, GetCaptureServiceGroup)(PSERVICEGROUP *gp_ret);

   // To suggest a new interrupt frequency...
   STDMETHOD_(void,SetCaptureNotificationInterval)( ULONG interval );
   STDMETHOD_(void,SetRenderNotificationInterval)( ULONG interval );
   STDMETHOD_(ULONG,GetCaptureNotificationInterval)();
   STDMETHOD_(ULONG,GetRenderNotificationInterval)();

   STDMETHOD_(NTSTATUS,StartCapture)();
   STDMETHOD_(NTSTATUS,PauseCapture)();
   STDMETHOD_(NTSTATUS,UnpauseCapture)();
   STDMETHOD_(NTSTATUS,StopCapture)();
   STDMETHOD_(NTSTATUS,StartRender)();
   STDMETHOD_(NTSTATUS,PauseRender)();
   STDMETHOD_(NTSTATUS,UnpauseRender)();
   STDMETHOD_(NTSTATUS,StopRender)();

   // Called by miniports to set addresses that will be accessed
   // by the busmaster dma. Note the 28-bit DMA engine...
   // Call with all parameters 0 to disable dma
   STDMETHOD_(DWORD,SetRecDma)( PPHYSICAL_ADDRESS phys,
                   PULONG           virt,
                   ULONG            size );
   STDMETHOD_(DWORD,SetPlayDma)( PPHYSICAL_ADDRESS phys,
                                 PULONG           virt,
                                 ULONG            size );

	STDMETHOD_(DWORD,PlayGetDMACount)( );
	STDMETHOD_(DWORD,RecGetDMACount)( );

   STDMETHOD_(DWORD,PlayGetDmaPageFrames)();
   STDMETHOD_(DWORD,RecGetDmaPageFrames)();

   STDMETHOD_(DWORD, PlayGetDmaPageCount)();
   STDMETHOD_(DWORD, RecGetDmaPageCount)();

   STDMETHOD_(void, SetRecDmaPageFrames)(ULONG frames);
   STDMETHOD_(void, SetPlayDmaPageFrames)(ULONG frames);

   STDMETHOD_(void, SetRecDmaPageCount)(ULONG count);
   STDMETHOD_(void, SetPlayDmaPageCount)(ULONG count);


   // Where are we in the stream since the last start? 
   // (Start clears, pause/unpause does not.)
   STDMETHOD_(ULONG,RecGetPositionBytes)();
   STDMETHOD_(ULONG,PlayGetPositionBytes)();
   STDMETHOD_(ULONG,RecGetPositionFrames)();
   STDMETHOD_(ULONG,PlayGetPositionFrames)();

   STDMETHOD(GetPlayDmaChannelRef)( PDMACHANNEL * channel );
   STDMETHOD(GetRecDmaChannelRef)( PDMACHANNEL * channel );
   STDMETHOD(GetPlayLegacyDmaChannelRef) ( PDMACHANNEL * channel);

private:
   // Private functions

   void HardwareSyncAcquire()
   {
      _hardwareSync.Wait(KernelMode,
                         FALSE,
                         NULL,        // wait forever
                         Executive);  // don't care about status
   }
   
   void HardwareSyncRelease() { _hardwareSync.Release(); }
   BOOLEAN HardwareSyncIsAcquired() { return (TRUE != _hardwareSync.State()); };

	// Function called on driver start to set up the WDM port objects
   NTSTATUS _CreateWdmStuff(PIRP Irp,
                            PRESOURCELIST resources);
   
   PRESOURCELIST _FindUartResources( PRESOURCELIST parent );

	// _createWdmStuff helper function.
	NTSTATUS InstallSubdevice
	(
    IN      KIrp                Irp,
    IN      PWCHAR              Name,
    IN      REFGUID             PortClassId,
    IN      PUNKNOWN            miniportAdopt,
    IN      PRESOURCELIST       ResourceList,
    OUT     PUNKNOWN *          OutPortUnknown      OPTIONAL
   );

   void Invalidate();

	// Solo functions essentially cribbed from the ice.c file
	// in the Solo driver. Most of these are not specific to the
	// Seasound hardware, but some are, especially those that
	// manipulate the S/PDIF settings or initialize the card.
	DWORD ICEinit();

	// Note: card types in ../util/tsyn/common/eeprom.h
	void  iceGetCardType( int * type,
                          int * version );

	void  iceIrqOverride( int blockIrqs );

	void  iceIrqClear( BYTE irqMask );

	DWORD icemtSetFreq( unsigned long newFreq );

	DWORD icemtGetFreq();
	DWORD icemtSPDIFSlaveOn();
	DWORD icemtSPDIFSlaveOff();
	DWORD icemtIsSPDIFSlaveOn();


	void  iceSetRoute( int route );
	int   iceGetRoute( );

	void  icemtPlayIrqEnable( );
	void  icemtPlayIrqDisable( );
	DWORD icemtPlayIrqAsserted( );
	void  icemtPlayIrqClear( );
   BYTE  icemtIrqStatus();
	DWORD icemtPlaySetDMABase( PDWORD base );
	DWORD icemtPlaySetDMASize( DWORD size );
	void  icemtPlaySetDMATerm( DWORD terminator );
	void  icemtPlayStart( );
	void  icemtPlayStop( );
	void  icemtPlayPause( );
	void  icemtPlayUnpause( );

	void  icemtRecIrqEnable( );
	void  icemtRecIrqDisable( );
	DWORD icemtRecIrqAsserted( );
	void  icemtRecIrqClear( );
	DWORD icemtRecSetDMABase( PDWORD base );
	DWORD icemtRecSetDMASize( DWORD size );
	void  icemtRecSetDMATerm( DWORD terminator );
	void  icemtRecStart( );
	void  icemtRecStop( );
	void  icemtRecPause( );
	void  icemtRecUnpause( );
	
	void  icetimerStart( WORD timeout );
	void  icetimerIrqEnable( );
	void  icetimerIrqDisable( );
	
	void  iceGpioSetDirection( BYTE mask );
	void  iceGpioWrite( BYTE mask );
	void  iceGpioRead( BYTE * value );

	void  iceSetMidiOutSync( int isInverted );

	void  iceMidiOneIrqEnable( );
	void  iceMidiOneIrqDisable( );
	int   iceMidiOneDataOut( BYTE aByte );
	int   iceMidiOneDataIn( BYTE * byteRet );
	int   iceMidiOneCommandOut( BYTE aByte );
	void  iceMidiTwoIrqEnable( );
	void  iceMidiTwoIrqDisable( );
	int   iceMidiTwoDataOut( BYTE aByte );
	int   iceMidiTwoDataIn( BYTE * byteRet );
	int   iceMidiTwoCommandOut( BYTE aByte );

	BYTE  iceJoyRead();

	DWORD iceEepromRead( BYTE offset, BYTE * value_ret );

	void iceReadPeakMeter( BYTE            offset,
			               BYTE          * value_ret );

	BYTE iceIrqStatus( );

	// ICE Helper functions (were "static" in ice.c)
	void CCI_Write( BYTE index, BYTE data );
	void CCI_Read( BYTE index, BYTE * data );

	DWORD _iceInitSolo();

	DWORD _setRoute();
	void _setHwCcsIrqMask();

};


//
// iceIrqGetStatus -- get irq status mask
//
// Notes:
//   This returns the mask of irq's.  They are all returned together
//   for performance reasons.  Active irq's will have their bits set.
//
// trs: don't use the cached copy                                                                       
// otherwise there is a race condition...
//
inline BYTE CAdapterSolo::iceIrqStatus( )
{
   // Retrieve the IRQ mask and mask out IRQ's that the hardware
   // is not reporting to us:

   return ((BYTE)_ccsRange[CCS_IRQSTATUS] & (~ (BYTE)_ccsRange[CCS_IRQMASK]));
}

//
// *IrqClear -- clear the named irq, enabling more.  Eek!
//
inline void CAdapterSolo::icemtPlayIrqClear( )
{
   // Yes, the mask bits and the write-to-clear bits are packed into
   // the same byte.  :/
   
   _mtRange[MTO_IRQMASK] = (BYTE)(_mtIrqMask | MTO_IRQPLAYBIT);
}

inline void CAdapterSolo::icemtRecIrqClear( )
{
   // Yes, the mask bits and the write-to-clear bits are packed into
   // the same byte.  :/
   
   _mtRange[MTO_IRQMASK] = (BYTE)(_mtIrqMask | MTO_IRQRECBIT);
}

//
// icemtIrqStatus -- get the multitrack interrupt bits
//
inline BYTE CAdapterSolo::icemtIrqStatus()
{
   return (BYTE)_mtRange[MTO_IRQMASK];
}


//
// iceIrqClear -- clear one or more irq's
//
// Set the bits you want to clear using the macros provided.  Note that
// the record and play multitrack IRQ's can not be cleared using this
// function.
//
inline void CAdapterSolo::iceIrqClear( BYTE irqMask )
{
	_ccsRange[CCS_IRQSTATUS] = (BYTE)irqMask; // write-one-to-clear:
}

//
// iceMidiOneDataIn -- get a byte from uart 1
//
// Parameters:
//   byteRet -- pointer to byte that is assigned return value on success
//
// Returns:
//   0 on success, nonzero if the fifo is empty
//
inline int CAdapterSolo::iceMidiOneDataIn( BYTE * byteRet )
{
   // If the status bit is set, there's nothing to read.
   if ((BYTE)_ccsRange[CCS_UART_1_CMD] & ICE_DSR_BIT)    // If DSR *is* set
      return 1;                 // the uart is empty!

   // Read from the uart!
   *byteRet = (BYTE)_ccsRange[CCS_UART_1_DAT]; 

   return 0;
}

//
// iceMidiOneDataOut -- output a byte to midi port 1
//
// Parameters:
//   aByte - some data
//
// Returns:
//   0 if the byte was output
//   1 if the mpu401 data port is not ready
//
inline int CAdapterSolo::iceMidiOneDataOut( 
                                       BYTE aByte )
{
   // If DRR *is* set
   if ((BYTE)_ccsRange[CCS_UART_1_CMD] & ICE_DRR_BIT) 
      return 1;                 // the uart is not ready!

   // write byte to the data register
   _ccsRange[CCS_UART_1_DAT] = (BYTE) aByte; 
   
   return 0;
}

//
// iceMidiTwoDataIn -- get a byte from uart 2
//
// Parameters:
//   byteRet -- pointer to byte that is assigned return value on success
//
// Returns:
//   0 on success, nonzero if the fifo is empty
//
inline int CAdapterSolo::iceMidiTwoDataIn( 
                                      BYTE * byteRet )
{
   BYTE status = (BYTE)_ccsRange[CCS_UART_2_CMD];

   if (status & ICE_DSR_BIT)    // If DSR *is* set
      return 1;                 // the uart is empty!

   // Read from the uart!
   *byteRet = (BYTE)_ccsRange[CCS_UART_2_DAT]; 
   return 0;
}

//
// iceMidiTwoDataOut -- output a byte to midi port 2
//
// Parameters:
//   aByte - some data
//
// Returns:
//   0 if the byte was output
//   1 if the mpu401 data port is not ready
//
inline int CAdapterSolo::iceMidiTwoDataOut( BYTE aByte )
{
   // If DRR *is* set
   if ((BYTE)_ccsRange[CCS_UART_2_CMD] & ICE_DRR_BIT)    
      return 1;                 // the uart is not ready!

   // write byte to the data register
   _ccsRange[CCS_UART_2_DAT] = (BYTE)aByte;

   return 0;
}


//
// icemt*GetDMACount -- get the current DMA offset in sample frames
//
// Caveats:
//   Because interrupts happen when the counter is on the last
//   sample frame, the count will be past the end, usually.
//
//   Note that the counter counts downwards.
//
//   The ICE docs say that the dma counter resets to the buffer
//   size when it reaches 0. Therefore, this returns a 1-indexed
//   count.
//
inline DWORD CAdapterSolo::PlayGetDMACount( )
{
   // TRS: don't need hardware sync to read I/O space

   DWORD count = (WORD)_mtRange[MTO_PLAYSIZECOUNT];

   // Convert to count of completed frames (ignore any
   // current incomplete one.)
   count = (count + 10) / 10;

   return count;
}

//
// RecGetDMACount -- return dma count in sample frames
//   
// Rounded down to the nearest complete frame.
//
inline DWORD CAdapterSolo::RecGetDMACount( )
{
   // TRS: don't need hardware sync to read I/O space

   DWORD count = (WORD)_mtRange[MTO_RECSIZECOUNT];

   // truncate (ignore incomplete frame) and convert
   // to frame count.
   count = (count + 12) / 12;
   return count;
}

//
// PlayGetDmaPageFrames -- return the number of bytes in one dma page
//
// Note that the acutal dma buffer is this size times irqsPerBuffer, 
// or at least twice this big. :)
//
inline DWORD CAdapterSolo::PlayGetDmaPageFrames()
{
   // TRS: don't get sync because this is called at irql
   return _playDmaPageFrames ;
}

inline DWORD CAdapterSolo::RecGetDmaPageFrames()
{
   // TRS: don't get sync because this is called at irql
   return _recDmaPageFrames;
}

// 
// PlayGetDmaPageCount -- return the number of dma pages currently
//   used by the play engine.
//
// This number will change depending on the buffer size. Eventually.
//
inline DWORD CAdapterSolo::PlayGetDmaPageCount()
{
   return _playDmaPageCount;
}

inline DWORD CAdapterSolo::RecGetDmaPageCount()
{
   return _recDmaPageCount;
}


inline BYTE  CAdapterSolo::iceJoyRead()
{
   return (BYTE)_ccsRange[CCS_GAMEPORT];
}

inline ULONG CAdapterSolo::RecGetPositionFrames()
{
   // Total frames in all pages of the DMA buffer.
   DWORD recBufferFrames = _recBufferSize / (SSS_SAMPLE_BYTES * SSS_REC_FRAME_SIZE);

   // DMA pos is reported as a count of DWORDS left in the buffer. :/
   // Convert to frames:

   DWORD pos = (WORD)_mtRange[MTO_RECSIZECOUNT] / SSS_REC_FRAME_SIZE;
   pos = pos % recBufferFrames; // it returns size and 0 sometimes

   pos = recBufferFrames - pos - 1; // convert to 0-indexed pos

   // If I understand this right, we don't have to remember the 
   // absolute buffer position for a WaveCyclic miniport--so the 
   // returned value represents DMA circular buffer pos. To change
   // this, do something like this:
   // pos += (_recPosIrqs / _recDmaPageCount) * recBufferFrames;

   return pos;
}

//
// Note: called at interrupt time.
//
// Returns "buffer position" in bytes, which is the same
// as the dma count only it counts up and is 0-indexed.
//
inline ULONG CAdapterSolo::RecGetPositionBytes()
{
   return RecGetPositionFrames() * SSS_SAMPLE_BYTES * SSS_REC_FRAME_SIZE;
}


inline ULONG CAdapterSolo::PlayGetPositionFrames()
{
   // Total frames in all pages of the DMA buffer.
   DWORD playBufferFrames = _playBufferSize / (SSS_SAMPLE_BYTES * SSS_PLAY_FRAME_SIZE);

   // DMA pos is reported as a count of DWORDS left in the buffer. :/
   // Convert to frames:

   DWORD pos = (WORD)_mtRange[MTO_PLAYSIZECOUNT] / SSS_PLAY_FRAME_SIZE;
   pos = pos % playBufferFrames; // it returns size and 0 sometimes

   pos = playBufferFrames - pos - 1; // convert to 0-indexed pos

   // If I understand this right, we don't have to remember the 
   // absolute buffer position for a WaveCyclic miniport--so the 
   // returned value represents DMA circular buffer pos. To change
   // this, do something like this:
   // pos += (_playPosIrqs / _playDmaPageCount) * playBufferFrames;

   return pos;
}

inline ULONG CAdapterSolo::PlayGetPositionBytes()
{
   return PlayGetPositionFrames() * SSS_SAMPLE_BYTES * SSS_REC_FRAME_SIZE;
}

//
// iceReadPeakMeter -- read peak level
//
// Parameters:
//   offset - offset within dma (see ice documentation)
//
inline void CAdapterSolo::iceReadPeakMeter(BYTE   offset,
                                            BYTE * value_ret )
{
    _mtRange[MTO_PEAKINDEX] = (BYTE)offset;    // choose meter
    *value_ret = (BYTE)_mtRange[MTO_PEAKDATA]; // read value
}

//
// CCI_Write -- write a data byte to the CCI register at the
//   named index.
//
// Notes:
//   use the #defined values for the index, make us all happy.
//   If it's not defined yet, type it in.  Don't be lazy.  Well,
//   not lazier than necessary.
//
inline void CAdapterSolo::CCI_Write( BYTE index,
                                      BYTE data )
{
   // Set the address, then set the data.  I don't know if we
   // need to wait between those stages.
   
   _ccsRange[CCS_CCI_INDEX] = (BYTE)index;
   _ccsRange[CCS_CCI_DATA] = (BYTE)data;

   // Thrilling, isn't it?
}

//
// CCI_Read -- read a data byte from the CCI register at the
//   named index.
//
inline void CAdapterSolo::CCI_Read( BYTE index,
                                     BYTE * data )
{
   _ccsRange[CCS_CCI_INDEX] = (BYTE)index; // Set the address
   *data = (BYTE)_ccsRange[CCS_CCI_DATA];  // Read the value.

   // Also thrilling.
}

//
// iceGpioWrite -- Set the state of the GPIO lines.  Well, the
//   ones that are set as output lines, anyway.
//
inline void  CAdapterSolo::iceGpioWrite( BYTE mask )
{
   CCI_Write( CCI_GPIO_DATA, mask );
}

//
// iceGpioRead -- read the current GPIO state.
//
// Caveat:
//   I just like saying the word "Caveat".  Also, only the hardware
//   manufacturer knows what all of these mean.
//
inline void  CAdapterSolo::iceGpioRead( BYTE * value )
{
   VASSERT( value, "bad pointer!" ); // how dare you!
   
   CCI_Read( CCI_GPIO_DATA, value );
}

//
// ...SPDIFSlaveIsOn -- is the slave currently enabled?
//
// Returns:
//   0 if the internal clock is master, nonzero if the S/PDIF
//   clock is master
//
inline DWORD CAdapterSolo::icemtIsSPDIFSlaveOn()
{
   // If the slave bit is currently set, we are slaved.
   
   return (BYTE)_mtRange[MTO_SAMPLERATE] & MTSR_SPDIF_SLAVE_BIT; 
}


#endif  //_ADAPTER_H_

