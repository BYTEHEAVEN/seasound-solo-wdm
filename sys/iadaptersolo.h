// $Id: iadaptersolo.h,v 1.1 2001/11/25 01:03:13 tekhedd Exp $

#ifndef _IADAPTERSOLO_H_
#define _IADAPTERSOLO_H_

#include "stdunk.h"
#include "portcls.h"
#include "ksdebug.h"

// Prevent addition of yet more versions of operator new
#define _NEW_DELETE_OPERATORS_
#include "kcom.h"

// Types used by IAdapterSolo and children
enum DmaMode
{
   DMAMODE_UNINITIALIZED, // zero means we're in trouble
   DMAMODE_MULTICHANNEL,  // normal WDM interleaved DMA
   DMAMODE_LEGACY,        // 16-bit stereo reformatting
   DMAMODE_ASIO,          // asio has taken over
   DMAMODE_COUNT // must be last
};


// CAdapterSolo virtual base class, etc
// {267E96C2-956A-11d5-A39C-005004B5E330}
DEFINE_GUID(IID_IAdapterSolo, 
0x267e96c2, 0x956a, 0x11d5, 0xa3, 0x9c, 0x0, 0x50, 0x4, 0xb5, 0xe3, 0x30);

DECLARE_INTERFACE_(IAdapterSolo,IUnknown)
{
   DEFINE_ABSTRACT_UNKNOWN()           // For IUnknown

   STDMETHOD_(void, SetPlayMode)(DmaMode mode) PURE;
   STDMETHOD_(void, SetRecMode)(DmaMode mode) PURE;

   STDMETHOD_(NTSTATUS,SetSamplingFrequency)
   (
      THIS_ ULONG newFreq 
   ) PURE;
   
   STDMETHOD_(ULONG,GetSamplingFrequency) (THIS_) PURE;

   STDMETHOD_(NTSTATUS, GetRenderServiceGroup)(PSERVICEGROUP *gp_ret) PURE;
   STDMETHOD_(NTSTATUS, GetCaptureServiceGroup)(PSERVICEGROUP *gp_ret) PURE;

   // To suggest a new interrupt frequency...
   STDMETHOD_(void,      SetCaptureNotificationInterval)( ULONG interval ) PURE;
   STDMETHOD_(void,      SetRenderNotificationInterval)( ULONG interval ) PURE;
   STDMETHOD_(ULONG,     GetCaptureNotificationInterval)() PURE;
   STDMETHOD_(ULONG,     GetRenderNotificationInterval)() PURE;

   STDMETHOD_(NTSTATUS, StartCapture)() PURE;
   STDMETHOD_(NTSTATUS, PauseCapture)() PURE;
   STDMETHOD_(NTSTATUS, UnpauseCapture)() PURE;
   STDMETHOD_(NTSTATUS, StopCapture)() PURE;
   STDMETHOD_(NTSTATUS, StartRender)() PURE;
   STDMETHOD_(NTSTATUS, PauseRender)() PURE;
   STDMETHOD_(NTSTATUS, UnpauseRender)() PURE;
   STDMETHOD_(NTSTATUS, StopRender)() PURE;

   // Called by miniports to set addresses that will be accessed
   // by the busmaster dma. Note the 28-bit DMA engine...
   // Call with all parameters 0 to disable dma
   //STDMETHOD_(DWORD, SetRecDma)( PPHYSICAL_ADDRESS phys,
   //                              PULONG           virt,
   //                              ULONG            size ) PURE;
   //STDMETHOD_(DWORD, SetPlayDma)( PPHYSICAL_ADDRESS phys,
   //                               PULONG           virt,
   //                               ULONG            size ) PURE;

	STDMETHOD_(DWORD, PlayGetDMACount)( ) PURE;
	STDMETHOD_(DWORD, RecGetDMACount)( ) PURE;

   STDMETHOD_(DWORD, PlayGetDmaPageFrames)() PURE;
   STDMETHOD_(DWORD, RecGetDmaPageFrames)() PURE;

   STDMETHOD_(DWORD, PlayGetDmaPageCount)() PURE;
   STDMETHOD_(DWORD, RecGetDmaPageCount)() PURE;

   // Definitely don't call these after calling SetPlayDma, 
   // or you'll crash things. You must call these first and the
   // sizes must match. It's expected that the wave miniport
   // knows how to set these things up. :)
   STDMETHOD_(void, SetRecDmaPageFrames)(ULONG frames) PURE;
   STDMETHOD_(void, SetPlayDmaPageFrames)(ULONG frames) PURE;

   STDMETHOD_(void, SetRecDmaPageCount)(ULONG count) PURE;
   STDMETHOD_(void, SetPlayDmaPageCount)(ULONG count) PURE;

   // Where are we in the stream since the last start? 
   // (Start clears, pause/unpause does not.)
   STDMETHOD_(ULONG, RecGetPositionBytes)() PURE;
   STDMETHOD_(ULONG, PlayGetPositionBytes)() PURE;
   STDMETHOD_(ULONG, RecGetPositionFrames)() PURE;
   STDMETHOD_(ULONG, PlayGetPositionFrames)() PURE;

   STDMETHOD(GetPlayDmaChannelRef)
      ( THIS_
        PDMACHANNEL * channel
      ) PURE;
   STDMETHOD(GetRecDmaChannelRef)
      ( THIS_
        PDMACHANNEL * channel
      ) PURE;
   STDMETHOD(GetPlayLegacyDmaChannelRef)
      ( THIS_
        PDMACHANNEL * channel
      ) PURE;
};

typedef IAdapterSolo *PADAPTERSOLO;

#endif // _IADAPTERSOLO_H_
