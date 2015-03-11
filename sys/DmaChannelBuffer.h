// $Id: DmaChannelBuffer.h,v 1.2 2001/08/11 05:43:51 tekhedd Exp $
// Copyright(C) 2001 Tom Surace. All rights reserved.

#ifndef _DmaChannelBuffer_h_
#define _DmaChannelBuffer_h_

#include "stdunk.h"            // CUnknown
//
// CDmaChannelBuffer -- kernel-COM (quote unquote) DMA channel
//   object that supports more specific memory allocation.
//
// Since this allocates contiguous, locked memory, you should
// create early in the driver load to avoid fragmenting the heap, etc.
//
// Notes:
//   I created this because I have a PCI card that does bus-mastered
//   memory writes, but cannot write to memory above 256M. The
//   IPortWave classes all require the use of an IDmaChannel object,
//   but the DmaChannel objects do not allow you to allocate your 
//   own buffer. And of course CDmaChannel is not available, or 
//   I'd just extend it.
//
//   Also, several standard functions are _not_ supported, for the 
//   same reason. There's no need for a dma adapter object, for 
//   example, so that call always fails. Etc.


//
// IDmaChannelBuffer
// 
// Interface for CDmaChannelBuffer for your convenience
//
// {EE895381-8C05-11d5-A39C-005004B5E330}
DEFINE_GUID(IID_IDmaChannelBuffer, 
0xee895381, 0x8c05, 0x11d5, 0xa3, 0x9c, 0x0, 0x50, 0x4, 0xb5, 0xe3, 0x30);
//
DECLARE_INTERFACE_(IDmaChannelBuffer,IDmaChannel)
{
   STDMETHOD(AllocateContiguousMemory)
   (  THIS_
      IN ULONG            NumberOfBytes,
      IN PHYSICAL_ADDRESS HighestAcceptableAddress 
   )  PURE;
};



// CDmaChannelBuffer implements and exposes the IDmaChannel interface
//   plus about two more functions for more specific memory allocation.
//
class CDmaChannelBuffer 
: public IDmaChannelBuffer,
  public CUnknown
{
private:
   PVOID             _bufferVirtual;
   PHYSICAL_ADDRESS  _bufferPhysical;

public:
   // Create function ala COM
   static NTSTATUS Create
     (
      OUT PUNKNOWN     *   ppUnknown,
      IN  PUNKNOWN         pUnknownOuter  OPTIONAL,
      IN  POOL_TYPE        PoolType,
      IN  PHYSICAL_ADDRESS buffer,
      IN  ULONG            bufferLength
     );

   // My extensions to the IDmaChannel interface

   // IDmaChannel interface implementation:

   // !!Warning!!
   // AllocateBuffer will always fail. Call AllocateContiguousMemory
   // above instead. :)
   STDMETHODIMP AllocateBuffer    
     (   
      IN      ULONG               BufferSize,
      IN      PPHYSICAL_ADDRESS   PhysicalAddressConstraint   OPTIONAL
     );

   STDMETHODIMP_(void) FreeBuffer();

   STDMETHODIMP_(ULONG) TransferCount();

   STDMETHODIMP_(ULONG) MaximumBufferSize();

   STDMETHODIMP_(ULONG) AllocatedBufferSize();

   STDMETHODIMP_(ULONG) BufferSize();

   STDMETHODIMP_(void) SetBufferSize( IN      ULONG   BufferSize );

   STDMETHODIMP_(PVOID) SystemAddress();

   STDMETHODIMP_(PHYSICAL_ADDRESS) PhysicalAddress();

   STDMETHODIMP_(PADAPTER_OBJECT) GetAdapterObject();

   STDMETHODIMP_(void) CopyTo
   (   
       IN      PVOID   Destination,
       IN      PVOID   Source,
       IN      ULONG   ByteCount
   ) ;

   STDMETHODIMP_(void) CopyFrom
   (   
       IN      PVOID   Destination,
       IN      PVOID   Source,
       IN      ULONG   ByteCount
   );

   /*************************************************************************
    * The following two macros are from STDUNK.H.  DECLARE_STD_UNKNOWN()
    * defines inline IUnknown implementations that use CUnknown's aggregation
    * support.  NonDelegatingQueryInterface() is declared, but it cannot be
    * implemented generically.  Its definition appears in MINIPORT.CPP.
    * DEFINE_STD_CONSTRUCTOR() defines inline a constructor which accepts
    * only the outer unknown, which is used for aggregation.  The standard
    * create macro (in MINIPORT.CPP) uses this constructor.
    */
   DECLARE_STD_UNKNOWN();

   CDmaChannelBuffer( PUNKNOWN pUnknownOuter,
                      PHYSICAL_ADDRESS bufferBase,
                      ULONG            bufferLength )
    :   CUnknown(pUnknownOuter),
        _bufferVirtual(NULL),
        _bufferPhysical(bufferBase)
    {
      _bufferVirtual = 
      ; // Do nothing
    }

   ~CDmaChannelBuffer();

private:

};

#endif // _DmaChannelBuffer_h_
