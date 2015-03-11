// $Id: DmaChannelBuffer.cpp,v 1.2 2001/08/11 05:43:51 tekhedd Exp $
// Copyright(C) 2001 Tom Surace. All rights reserved.

// CDmaChannelBuffer implementation.

#include <vdw.h>

// Include definitions for DWORD and other stupid macros
#include <wdm.h>
#include <windef.h>

// #include <portcls.h>		         // PcGetTimeInterval()

#include "SolowdmDevice.h"          // for "t"
#include "DmaChannelBuffer.h"

#pragma hdrstop("SSSolo.pch")

#pragma code_seg("PAGE")

//
// CDmaChannelBuffer::Create -- standard create function ala COM
//
// Returns an AddRef()'ed buffer object.
//
NTSTATUS CDmaChannelBuffer::Create
(
    OUT PUNKNOWN     *   ppUnknown,
    IN  PUNKNOWN         pUnknownOuter  OPTIONAL, // for aggregation
    IN  POOL_TYPE        PoolType,
    IN  PHYSICAL_ADDRESS buffer,
    IN  ULONG            bufferLength
)
{
    PAGED_CODE();

    ASSERT(ppUnknown);

    CDmaChannelBuffer *p = 
       new(PoolType) CDmaChannelBuffer(pUnknownOuter);

    if (p)  // New can fail? I'll never get used to this.
    {                                                                   
        *ppUnknown = PUNKNOWN(p);                               
        (*ppUnknown)->AddRef();                                         

        return STATUS_SUCCESS;                                      
    }                                                                   

    return STATUS_INSUFFICIENT_RESOURCES;                       
}


CDmaChannelBuffer::~CDmaChannelBuffer()
{
   // Just make sure to free our buffer, and we're done.
   FreeBuffer();
}

//
// CDmaChannelBuffer::NonDelegatingQueryInterface
//
// Kernel COM is fun.
//
// Obtains an interface.  This function works just like a COM QueryInterface
// call and is used if the object is not being aggregated.
//
STDMETHODIMP
CDmaChannelBuffer::
NonDelegatingQueryInterface
(
    IN      REFIID  Interface,
    OUT     PVOID * Object
)
{
    PAGED_CODE();

    ASSERT(Object);

    t << "[CDmaChannelBuffer::NonDelegatingQueryInterface]\n";

    if (IsEqualGUIDAligned(Interface,IID_IUnknown))
    {
        *Object = PVOID(PUNKNOWN(this));
    }
    else if (IsEqualGUIDAligned(Interface,IID_IDmaChannel))
    {
        *Object = PVOID(PDMACHANNEL(this));
    }
    else if (IsEqualGUIDAligned(Interface,IID_IDmaChannelBuffer))
    {
        *Object = PVOID((IDmaChannelBuffer *)(this));
    }
    else
    {
        *Object = NULL;
    }

    if (*Object)
    {
        //
        // We reference the interface for the caller.
        //
        PUNKNOWN(*Object)->AddRef();
        return STATUS_SUCCESS;
    }

    return STATUS_INVALID_PARAMETER;
}

//
// AllocateContiguousMemory -- allocate buffer for use by
//   bus mastering dma
//
// See MmAllocateContiguousMemory for parameters.
//
// Must run at PASSIVE_LEVEL
//
STDMETHODIMP CDmaChannelBuffer::AllocateContiguousMemory
  ( 
   IN ULONG NumberOfBytes,
   IN PHYSICAL_ADDRESS HighestAcceptableAddress 
  )
{
   NTSTATUS status;

   if (_bufferVirtual)
      FreeBuffer();        // That's right, no more buffer
   
   _bufferVirtual = MmAllocateContiguousMemory( NumberOfBytes, 
                                                HighestAcceptableAddress );

   if (NULL == _bufferVirtual)
   {
      status = STATUS_INSUFFICIENT_RESOURCES;
      goto bail;
   }

   // Now figure out what else we know about this address (phys address, etc)
   _bufferPhysical = MmGetPhysicalAddress( _bufferVirtual );

bail:
   return status;
}

//
// CDmaChannelBuffer::AllocateBuffer -- standard interface to allocate buffer
//
// !!Warning!!
//   AllocateBuffer will always fail. Call AllocateContiguousMemory
//   above instead. :)
//
// This is required to support IDmaChannel's interface, but it is not called
// by the PortWave classes and in fact doesn't do what we want anyway.
//
STDMETHODIMP CDmaChannelBuffer::AllocateBuffer    
  (   
   IN      ULONG               /* BufferSize */,
   IN      PPHYSICAL_ADDRESS   /* PhysicalAddressConstraint */  OPTIONAL
  )
{
   ASSERT(0);                 // I assume this is never called

   return STATUS_NOT_SUPPORTED;
}


   STDMETHODIMP_(void) CDmaChannelBuffer::FreeBuffer();

   STDMETHODIMP_(ULONG) CDmaChannelBuffer::TransferCount();

   STDMETHODIMP_(ULONG) CDmaChannelBuffer::MaximumBufferSize();

   STDMETHODIMP_(ULONG) CDmaChannelBuffer::AllocatedBufferSize();

   STDMETHODIMP_(ULONG) CDmaChannelBuffer::BufferSize();

   STDMETHODIMP_(void) CDmaChannelBuffer::SetBufferSize( IN      ULONG   BufferSize );

   STDMETHODIMP_(PVOID) CDmaChannelBuffer::SystemAddress();

   STDMETHODIMP_(PHYSICAL_ADDRESS) CDmaChannelBuffer::PhysicalAddress();

      // I don't believe we need this, so I think I'll let it always fail
   STDMETHODIMP_(PADAPTER_OBJECT) CDmaChannelBuffer::GetAdapterObject();

   STDMETHODIMP_(void) CDmaChannelBuffer::CopyTo
   (   
       IN      PVOID   Destination,
       IN      PVOID   Source,
       IN      ULONG   ByteCount
   ) ;

   STDMETHODIMP_(void) CDmaChannelBuffer::CopyFrom
   (   
       IN      PVOID   Destination,
       IN      PVOID   Source,
       IN      ULONG   ByteCount
   );


