// $Id: adapter.cpp,v 1.14 2001/11/25 01:03:13 tekhedd Exp $
// Copyright (C) 2001 Tom Surace. All rights reserved.
//

#include <vdw.h> // DriverWorks

// #include "..\Solowdminterface.h"

// Include definitions for DWORD and other stupid macros
#include <windef.h>
#include <portcls.h>		   // PcGetTimeInterval()
#include "DMusicKS.h"

#include "adapter.h"
#include "config.h"        // Default buffer sizes, etc
#include "mintopo.h"
#include "minwave.h"       // CWaveCyclicStreamSolo, etc
// #include "SSSolo.h"

#include "..\solowdmioctl.h"

#include "eeprom.h"			// Eeprom format for the solo

// We can work out which functions are paged individually


//
// StartDevice -- called by the KS port class object when the
//   IRP_MN_START_DEVICE irp is sent
//
#pragma code_seg("PAGE")
extern "C" NTSTATUS CAdapterSoloStartDevice
(
    IN  PDEVICE_OBJECT  DeviceObject,   // Device object. (FDO)
    IN  PIRP            Irp,            // IO request packet.
    IN  PRESOURCELIST   ResourceList    // List of hardware resources.
)
{
    PAGED_CODE();

    t << "[CAdapterSoloStartDevice]\n";

    ASSERT(DeviceObject);
    ASSERT(Irp);
    ASSERT(ResourceList);

    NTSTATUS ntStatus = STATUS_SUCCESS;

    // Create a new solo adapter device and stick it up your ass. Oh?
    // Oh sorry, create a solo adapter device object and hand it to
    // the port class drivers :).
    CAdapterSolo * pAdapterSolo = NULL;


    // create a new adapter common object
    ntStatus = CAdapterSolo::New( &pAdapterSolo,
                                  NULL,
                                  NonPagedPool );

    if (!NT_SUCCESS(ntStatus))
       goto bail;

    ASSERT( pAdapterSolo );

     // Initialize the object
    ntStatus = pAdapterSolo->Init( DeviceObject, NULL /* pdo */, Irp, ResourceList );

    if (!NT_SUCCESS(ntStatus))
       goto bail;

    // register with PortCls for power-managment services
    ntStatus = PcRegisterAdapterPowerManagement( PUNKNOWN(PADAPTERSOLO(pAdapterSolo)),
                                                 DeviceObject );

    if (!NT_SUCCESS(ntStatus))
       goto bail;

    return ntStatus;

bail:
   // Jump here on failure to clean up

   // release the IUnknown on adapter common
   if (pAdapterSolo)
      pAdapterSolo->Release();

   return ntStatus;
}
#pragma code_seg()

//
// New
//
#pragma code_seg("PAGE")
NTSTATUS CAdapterSolo::New
(
    OUT     CAdapterSolo **  ppAdapter_ret,
    IN      PUNKNOWN    UnknownOuter    OPTIONAL,
    IN      POOL_TYPE   PoolType
)
{
    PAGED_CODE();

    t << "[CAdapterSolo::New]\n";

    ASSERT(ppAdapter_ret);

    CAdapterSolo * a = 
       new(PoolType) CAdapterSolo(UnknownOuter);

    if (!a)  // New can fail? I'll never get used to this.
       return STATUS_INSUFFICIENT_RESOURCES;                       

    *ppAdapter_ret = a;
    (*ppAdapter_ret)->AddRef();                                         

    return STATUS_SUCCESS;                                      
}   
#pragma code_seg()

//
// Init -- called after the constructor
//
#pragma code_seg("PAGE")
NTSTATUS CAdapterSolo::Init( PDEVICE_OBJECT device_obj,
                             PDEVICE_OBJECT Pdo,
                             PIRP           irp,
                             PRESOURCELIST  resources )
{
   PAGED_CODE();

   t << "[CAdapterSolo::Init]\n";

   _fdo = device_obj;
   // I don't know how to get the pdo into this object since the
   // port class object doesn't make it available to StartDevice...
   // Ew!
   _pdo = _fdo;

   _powerState = PowerDeviceD0;
   _portWave = NULL; // oh dear, circular reference to parent!

   _recMode = DMAMODE_MULTICHANNEL;
   _playMode = DMAMODE_MULTICHANNEL;
   _iceIsStarted = 0;

	// Fun local variables
   _ccsIrqMask = 0xff;
   _irqMaskOverride = 1; // turn 'em OFF by default
   _mtIrqMask = (MTO_IRQPLAYEN | MTO_IRQRECEN);

#ifdef QQQ // TODO: read the registry for init parameters
   // Stuff that's loaded from the registry
   SSSolo * solo = (SSSolo *)KDriver::DriverInstance();
   _loopbackMode = solo->m_Routing;
#endif // QQQ

   if ((int)_loopbackMode < 0 || (int)_loopbackMode > ICEROUTE_INVALID)
      _loopbackMode = ICEROUTE_SPDIFMIX;

   _playDmaPageFrames = 0;
   _recDmaPageFrames = 0;

   // TRS: hardcoded for now. Can be any multiple that divides evenly
   //      into the number of frames. Lost data is lost data, so why
   //      have a bigger buffer? 
   _recDmaPageCount = 0;
   _playDmaPageCount = 0;

   _recDmaChannel = 0;
   _playDmaChannel = 0;
   _playLegacyDmaChannel = 0;

   _recPosIrqs = 0;
   _playPosIrqs = 0;

   _playDmaBufferVirt = 0;
   _playBufferSize = 0;

   _recDmaBufferVirt = 0;
   _recBufferSize = 0;

   _renderServiceGroup = NULL;
   _captureServiceGroup = NULL;

   _samplingFrequency = 44100; // It isn't really
   _hardwareSync.Initialize(1); // level 1
  

   ASSERT( 0 == _iceIsStarted ); // Hmmm, startd twice?
   _iceIsStarted = 0;

   // Don't mess with hardware while I'm setting it up, OK?
   HardwareSyncAcquire();

	NTSTATUS status = STATUS_SUCCESS;

	// The default Pnp policy has already cleared the IRP with the lower device
	// Initialize the physical device object.

	// Get the list of raw resources from the IRP
	PCM_RESOURCE_LIST pResListRaw = resources->UntranslatedList();
	// Get the list of translated resources from the IRP
	PCM_RESOURCE_LIST pResListTranslated = resources->TranslatedList();


	// Create an instance of KPciConfiguration so we can map Base Address
	// Register indicies to ordinals for memory or I/O port ranges.
	// TRS: this kind of stuff is why NuMega kicks ass. :)
   //
	KPciConfiguration PciConfig(_pdo);
   ASSERT(PciConfig.IsValid());

	// For each I/O port mapped region, initialize the I/O port range using
	// the resources provided by NT. Once initialized, use member functions such as
	// inb/outb, or the array element operator to access the ports range.
	
	// Control port (offset 0) for general ICE control
	status = _ccsRange.Initialize
		(
		 pResListTranslated,
		 pResListRaw,
		 PciConfig.BaseAddressIndexToOrdinal(0)
		);

	if (!NT_SUCCESS(status))
		goto bail;

	// The more or less useless indexed port (directsound
	// stuff).
	status = _indexedRange.Initialize
		(
		 pResListTranslated,
		 pResListRaw,
		 PciConfig.BaseAddressIndexToOrdinal(1)
		);

	if (!NT_SUCCESS(status))
		goto bail;

	// Directsound dma control (ac97?)
	status = _dsdmaRange.Initialize
		(
		 pResListTranslated,
		 pResListRaw,
		 PciConfig.BaseAddressIndexToOrdinal(2)
		);

	if (!NT_SUCCESS(status))
		goto bail;

	// Multitrack controls (the meat!)
	status = _mtRange.Initialize
		(
		 pResListTranslated,
		 pResListRaw,
		 PciConfig.BaseAddressIndexToOrdinal(3)
		);

	if (!NT_SUCCESS(status))
		goto bail;

	// Initialize and connect the interrupt
	// TRS: More bad-assed stuff from DriverWorks. Yeah!
	status = _Irq.InitializeAndConnect( pResListTranslated, 
									            LinkTo(Isr_Irq), 
									            this );
	if (!NT_SUCCESS(status))
		goto bail;

	// Setup the DPC (Deferred Procedure Call, did you forget again?)
	// to be used for interrupt processing
	_DpcFor_Irq.Setup(LinkTo(DpcFor_Irq), this);

   // It's now safe to talk to the hardware and we will need to 
   // talk to it to turn it off if we fail:
   _iceIsStarted = TRUE;

   //
   // We need a service group for notifications. This is required for
   // the PortWave. --trs
   //
   // We will bind all the
   // streams that are created to this single service group.  All interrupt
   // notifications ask for service on this group, so all streams will get
   // serviced.  The PcNewServiceGroup() call returns an AddRefed pointer.
   //
   status = PcNewServiceGroup(&_renderServiceGroup,NULL);
   if (!NT_SUCCESS(status))
      goto bail;

   status = PcNewServiceGroup(&_captureServiceGroup,NULL);
   if (!NT_SUCCESS(status))
      goto bail;

	// Now that we have handlers we can init the ice chip. I know
   // this generates a UART interrupt on some chip revs, maybe 
   // other things as well. In fact, if you screw up you can
   // generate NMI interrupts and isn't that fun? :)
   if (0 != ICEinit()) 
		goto bail;

   // Detect the solo type so the miniports will be able to report how
   // many channels are useful. :)
   // Huh, this has to be instantiated before the miniports, but it
   // requires doing MIDI I/O. I guess I need my own tiny com engine
   // to do the initialization. Cut-and-paste time.
   // TODO: detect solo type here

   // Create the miniports, bonding them all to our resource list.
	// I don't see why this is necessary, but I guess if the PCI
	// adapter isn't there, we're hosed, and we don't want the resources
   // being released by the system because it thinks we're not using
   // them...
	//PRESOURCELIST allResources;
	//status = PcNewResourceList(&allResources,
   //                           NULL,
	//						         NonPagedPool,
	//						         pResListTranslated,
	//						         pResListRaw);
	//if (!NT_SUCCESS(status))
	//	goto bail;

   // Create the miniports, bonding them all to our resource list. 
   // Of course, they may not need the resource list, all they really
   // need is a reference to this adapter for the PCI regions...
	status = _CreateWdmStuff(irp, resources);
	if (!NT_SUCCESS(status))
	   goto bail;

   // Now that the portWave object exists we can allocate dma buffers.
   // Must do this before NewStream is called!
   status = _setUpDma();
	if (!NT_SUCCESS(status))
	   goto bail;

   // Might as well turn on interrupts now. 
   icemtRecIrqEnable();
   icemtPlayIrqEnable();
   iceIrqOverride(0);

   // The base class will handle completion
   HardwareSyncRelease();

   // Everything worked, so set our pointer in the device extension.
   // This allows the device object to (someday, possibly) set the _pdo
   // field in this object.
   *((CAdapterSolo **)( (PBYTE)device_obj->DeviceExtension
                        + PORT_CLASS_DEVICE_EXTENSION_SIZE )) = this;
   
   return status;

bail:
	Invalidate();
   HardwareSyncRelease();   // Not that there's anything to lock...
   return status;

}

//
// OnIoctl -- ioctl handler
//
// It is our responsibility to set I.status and the status
// parameter if we handle the ioctl. Otherwise, this
// function returns false and changes nothing.
//
#pragma code_seg("PAGE")
bool CAdapterSolo::OnIoctl( KIrp & I, NTSTATUS * status )
{
   PAGED_CODE();

   ASSERT( status );

   // Guess we don't want anyone disconnecting the PCI bus while
   // we do this. Er. Well, I suppose if they have an expansion 
   // chassis this would be useful, except of course that you can
   // always surprise-remove hardware, so why bother? Stupid stupid.

//   PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION) fdo->DeviceExtension;
//   NTSTATUS status = IoAcquireRemoveLock(&pdx->RemoveLock, Irp);
//   if (!NT_SUCCESS(status))
//      return CompleteRequest(Irp, status, 0);
  

   ULONG code = I.IoctlCode();
   switch (code)
   {
   case 1024:
//      IoReleaseRemoveLock(&pdx->RemoveLock, Irp);
      return true;

   default:
      break;
   };

   return false; // if we got here, it's not our problem
}


#pragma code_seg("PAGE")
void CAdapterSolo::SetPlayMode(DmaMode mode)
{
   ASSERT( mode > DMAMODE_UNINITIALIZED );
   ASSERT( mode < DMAMODE_COUNT );

   _playMode = mode;
}

#pragma code_seg("PAGE")
void CAdapterSolo::SetRecMode(DmaMode mode)
{
   ASSERT( mode > DMAMODE_UNINITIALIZED );
   ASSERT( mode < DMAMODE_COUNT );

   _recMode = mode;
}

#pragma code_seg("PAGE")
NTSTATUS CAdapterSolo::_setUpDma()
{
   PHYSICAL_ADDRESS phys; // a temp valud
   NTSTATUS ntStatus = STATUS_SUCCESS;
   ULONG    size;
   
   ntStatus = AllocDma(&_recDmaChannel, 1 /* capture */);
   if (!NT_SUCCESS(ntStatus))
      goto bail;

   t << " Record DMA size:" << _recDmaChannel->BufferSize() << "\n";

   ntStatus = AllocDma(&_playDmaChannel, 0 /* render */);
   if (!NT_SUCCESS(ntStatus))
      goto bail;

   t << " Play DMA size:" << _playDmaChannel->BufferSize() << "\n";

   // Now allocate legacy dma
   size = _playDmaChannel->BufferSize() * 4 / SSS_SAMPLE_BYTES / SSS_PLAY_FRAME_SIZE;
   ntStatus = _portWave->NewMasterDmaChannel
       ( 
        &_playLegacyDmaChannel,
        NULL,            // outer unknown 
        NULL,            // resources (optional)
        size,
        TRUE,            // not 32-bit (24-bit)
        FALSE,           // not 64-bit 
        Width32Bits,     // not used for busmaster dma
        MaximumDmaSpeed  // also not used for bustmaster
       );
                                    
   if (!NT_SUCCESS(ntStatus))
      goto bail;

   // Allocate memory in the dma adapter object. Ours must be 32-bit aligned
   // but fortunately this allocates page-aligned memory anyway.
   PHYSICAL_ADDRESS constraint;
   constraint.QuadPart = 0xffffffff; // 32-bit

   ntStatus = _playLegacyDmaChannel->AllocateBuffer(size, &constraint);
      
   if (!NT_SUCCESS(ntStatus))
      goto bail;

   // Uh, we don't want no strange noises
   RtlFillMemory(_playDmaChannel->SystemAddress(), 
                 _playDmaChannel->BufferSize(),
                 0);

   // Inform adapter of its new dma address. This may change the dma page
   // count. Alternatively, we may be screwed if the size is not a round
   // 
   phys = _recDmaChannel->PhysicalAddress();
   if (SetRecDma( &phys,
                  (PULONG)_recDmaChannel->SystemAddress(),
                  _recDmaChannel->BufferSize() ))
   {
      ntStatus = STATUS_UNSUCCESSFUL;
      goto bail;
   }

   phys = _playDmaChannel->PhysicalAddress();
   if (SetPlayDma( &phys,
                   (PULONG)_playDmaChannel->SystemAddress(),
                   _playDmaChannel->BufferSize() ))
   {
      ntStatus = STATUS_UNSUCCESSFUL;
      goto bail;
   }

bail:
   return ntStatus;
}

//
// A table of "good sizes" for the dma buffers
// in order of preference.
struct _sizesStruct_ 
{
   DWORD pageCount;   // number of pages
   DWORD pageFrames;  // number of frames in a page
};
   
#define N_SIZES (8)
struct _sizesStruct_ _niceSizes[N_SIZES] =
{
//   { 8,   0x0200 },    
//   { 4,   0x0200 },    
   { 4,   0x0100 },    
   { 2,   0x0100 },    
   { 2,   0x0080 },    
   { 2,   0x0040 },    
   { 2,   0x0020 },    
   { 2,   0x0010 },    
   { 2,   0x0008 },    
   { 2,   0x0004 },    // absolute smallest
};

//
// AllocDma -- an unusually large function for something
//   that just allocates a small contiguous memory buffer.
//
// This is suitable for allocating the buffers that are actually
// used by DMA. For other buffers (legacy 16-bit buffers) you
// should write your own stuff.
//
#pragma code_seg("PAGE")
NTSTATUS CAdapterSolo::AllocDma( PDMACHANNEL * channel_ret,
                                 BOOLEAN       isCapture )
{
   PAGED_CODE();

   t << "[CMiniportWaveCyclicSolo::AllocDma]\n";

   ASSERT(channel_ret);
   ASSERT(_portWave);   // just making sure...

   PDMACHANNEL channel = NULL; // what we are allocating
   NTSTATUS status = STATUS_UNSUCCESSFUL;
   ULONG    size;       // a nice general-purpose value
   int      i;          // your old friend i

   // Absolute maximum possible size (if this is too big, all allocation
   // attempts will fail for no apparent reason. :(
   if (isCapture)
      size = SSS_PAGE_FRAMES_MAX * SSS_REC_FRAME_BYTES * SSS_DMA_PAGE_COUNT_MAX;
   else
      size = SSS_PAGE_FRAMES_MAX * SSS_PLAY_FRAME_BYTES * SSS_DMA_PAGE_COUNT_MAX;

   // Notice that this is allocatd as big as it can ever be, but
   // there's no need for it to necessarily be that big.
   status = _portWave->NewMasterDmaChannel
       ( 
        &channel,
        NULL,            // outer unknown 
        NULL,            // resources (optional)
        size,
        FALSE,           // not 32-bit (24-bit)
        FALSE,           // not 64-bit 
        Width32Bits,     // not used for busmaster dma
        MaximumDmaSpeed  // also not used for bustmaster
       );
                                    
   if (!NT_SUCCESS(status))
      goto bail;

   // Allocate memory in the dma adapter object. Ours must be 32-bit aligned
   // but fortunately this allocates page-aligned memory anyway.
   PHYSICAL_ADDRESS constraint;
   constraint.HighPart = 0;
   constraint.LowPart = 0x0fffffff; // 28-bit

   // Loop to try to allocat a small enough buffer to make
   // the OS happy. :/ I have guidelines for a minimum page
   // size, so get the number of pages and work from there.

   for (i = 0; i < N_SIZES; i++)
   {
      struct _sizesStruct_ * s = &_niceSizes[i];

      size = s->pageCount * s->pageFrames;
      if (isCapture)
         size *= SSS_REC_FRAME_BYTES;
      else
         size *= SSS_PLAY_FRAME_BYTES;

      // If we can't get a simple kilobyte of mapped memory, fuck it.
      status = channel->AllocateBuffer(size, &constraint);
      if (NT_SUCCESS(status))
      {
         // Yay, we allocated a goddamn buffer! (can you tell how 
         // frustrating this was?)
         ASSERT( size > 0 );
         ASSERT( channel->BufferSize() > 0 );
         ASSERT( channel->SystemAddress() );

         // If the buffer size is not a round number of sample frames, the 
         // DMA engine will screw up, so set it now. This just sets a 
         // variable in the dma object, hence the assert.

         // This is what was _supposed_ to have happened
         ASSERT( channel->AllocatedBufferSize() >= size );

         // May need to call this, but then maybe not...
         channel->SetBufferSize(size);
         ASSERT( channel->BufferSize() == size );


         // Finally, tell the adapter about its new buffer params:
         if (isCapture)
         {
            SetRecDmaPageFrames( s->pageFrames );
            SetRecDmaPageCount( s->pageCount );
         }
         else
         {
            SetPlayDmaPageFrames( s->pageFrames );
            SetPlayDmaPageCount( s->pageCount );
         }

         (*channel_ret) = channel;
         return status;
      }
   }

   // If we got here, something failed.
bail:
   if (channel)
      channel->Release();

   return STATUS_UNSUCCESSFUL;
}
#pragma code_seg()


//
// dtor
//
CAdapterSolo::~CAdapterSolo()
{
    PAGED_CODE();

    t << "[CAdapterSolo::~CAdapterSolo]\n";

    Invalidate();
}

//
// NonDelegatingQueryInterface -- used in conjunction with
//   CUnknown base class to provide QueryInterface and aggregation
//
STDMETHODIMP
CAdapterSolo::NonDelegatingQueryInterface
(
    REFIID  Interface,
    PVOID * Object
)
{
    PAGED_CODE();

    t << "[CAdapterSolo::NonDelegatingQueryInterface]\n";

    ASSERT(Object);

    if (IsEqualGUIDAligned(Interface,IID_IUnknown))
    {
        *Object = PVOID(PUNKNOWN(PADAPTERSOLO(this)));
    }
    else if (IsEqualGUIDAligned(Interface,IID_IAdapterSolo))
    {
        *Object = PVOID((IAdapterSolo *)(this));
    }
    else if (IsEqualGUIDAligned(Interface,IID_IAdapterPowerManagment))
    {
        *Object = PVOID(PADAPTERPOWERMANAGMENT(this));
    }
    else
    {
        t << " Unknown Interface\n";
        *Object = NULL;
    }

    if (*Object)
    {
        PUNKNOWN(*Object)->AddRef();
        return STATUS_SUCCESS;
    }

    return STATUS_INVALID_PARAMETER;
}




//
// PowerChangeState -- one of those IAdapterPowerManagement virtual
//   functions called when state changes
//
STDMETHODIMP_(void)
CAdapterSolo::PowerChangeState
(
    IN      POWER_STATE     NewState
)
{
    t << "[CAdapterSolo::PowerChangeState]\n";
                
    t << "  Entering D" << ULONG(NewState.DeviceState) 
      << " from D" << ULONG(_powerState)
      << "\n";

    // is this actually a state change??
    if( NewState.DeviceState != _powerState )
    {
        // switch on old state (on exit state...)

        // switch on new state
        switch( NewState.DeviceState )
        {
            case PowerDeviceD0:
                // Insert your code here for entering the full power state (D0).
                // This code may be a function of the current power state.  Note that
                // property accesses such as volume and mute changes may occur when
                // the device is in a sleep state (D1-D3) and should be cached in
                // the driver to be restored upon entering D0.  However, it should
                // also be noted that new miniport and new streams will only be
                // attempted at D0 (portcls will place the device in D0 prior to the
                // NewStream call).
               break;

            case PowerDeviceD1:
                // This sleep state is the lowest latency sleep state with respect to the
                // latency time required to return to D0.  The driver can still access
                // the hardware in this state if desired.  If the driver is not being used
                // an inactivity timer in portcls will place the driver in this state after
                // a timeout period controllable via the registry.

               // If the solo had a low-power state, we could enter it here.

               // If interrupts were previously turned off, turn 'em on now
               if (_powerState == PowerDeviceD2 ||
                   _powerState == PowerDeviceD3)
               {
                  iceIrqOverride(0);
               }
               break;
                
            case PowerDeviceD2:
                // This is a medium latency sleep state.  In this state the device driver
                // cannot assume that it can touch the hardware so any accesses need to be
                // cached and the hardware restored upon entering D0 (or D1 conceivably).

               // FIXME: should block entry points from the WDM and other devices
               //        :/

               // OK, this break point is causing crashing in some end-user
               // systems, and anyway we should disable interrupts here:
               iceIrqOverride(1);
               break;
                
            case PowerDeviceD3:
                // This is a full hibernation state and is the longest latency sleep state.
                // The driver cannot access the hardware in this state and must cache any
                // hardware accesses and restore the hardware upon returning to D0 (or D1).

                // TRS: not that the word in the wdmaudio community is to save your 
                //      settings on this transition because the audio system under
                //      98SE does not call destructors. :/
                iceIrqOverride(1);
                break;
    
            default:
                t << "  Unknown Device Power State\n";
                break;
        }
    }

    // Save for later!
    _powerState = NewState.DeviceState;
}

//
// CAdapterSolo::QueryPowerChangeState()
//
// Can we change to this state? Another of those IAdapterPowerManagement
// functions
//
STDMETHODIMP_(NTSTATUS) CAdapterSolo::QueryPowerChangeState
(
    IN      POWER_STATE     NewStateQuery
)
{
    t << "[CAdapterSolo::QueryPowerChangeState]\n";

    // Check here to see of a legitimate state is being requested
    // based on the device state and fail the call if the device/driver
    // cannot support the change requested.  Otherwise, return STATUS_SUCCESS.
    // Note: A QueryPowerChangeState() call is not guaranteed to always preceed
    // a PowerChangeState() call.

    // Just in case you didn't know, even though you "fail" here, the
    // power state _will_ change. :)

    // We're ALWAYS ready
    return STATUS_SUCCESS;
}

//
// CAdapterSolo::QueryDeviceCapabilities()
//
// Called at startup to get the caps for the device.
//
// Another IAdapterPowerManagement virtual function
//
STDMETHODIMP_(NTSTATUS) CAdapterSolo::QueryDeviceCapabilities
(
    IN      PDEVICE_CAPABILITIES    PowerDeviceCaps
)
{
    t << "[CAdapterSolo::QueryDeviceCapabilities]\n";

    return STATUS_SUCCESS;
}

////////////////////////////////////////////////////////////////////////
//  CAdapterSolo::Invalidate
//
//	Routine Description:
//		Calls Invalidate methods for system resources
//
//	Parameters:
//		None
//
//	Return Value:
//		None
//
//	Comments:
//		This function is called from OnStopDevice, OnRemoveDevice and
//		OnStartDevice (in error conditions).  It calls the Invalidate
//		member funcitons for each resource to free the underlying system
//		resource if allocated.  It is safe to call Invalidate more than
//		once for a resource, or for an uninitialized resource.

VOID CAdapterSolo::Invalidate()
{	
   t << "[CAdapterSolo::Invalidate]\n";

   // Don't touch hardware if we haven't successfully started.
   if (_iceIsStarted)
   {
      // Turn off DMA and interrupts
      iceIrqOverride(1);
      icemtPlayStop();
      icemtRecStop();

      _iceIsStarted = FALSE;
   }

   if (_recDmaChannel)
   {
      _recDmaChannel->Release();
      _recDmaChannel = NULL;
   }
   if (_playDmaChannel)
   {
      _playDmaChannel->Release();
      _playDmaChannel = NULL;
   }
   if (_playLegacyDmaChannel)
   {
      _playLegacyDmaChannel->Release();
      _playLegacyDmaChannel = NULL;
   }

   // clean up the service groups (com objects)
   if( _renderServiceGroup )
   {
       _renderServiceGroup->Release();
       _renderServiceGroup = NULL;
   }
   if( _captureServiceGroup )
   {
       _captureServiceGroup->Release();
       _captureServiceGroup = NULL;
   }

	// For each I/O port mapped region, release the underlying system resource.
	_ccsRange.Invalidate();
	_indexedRange.Invalidate();
	_dsdmaRange.Invalidate();
	_mtRange.Invalidate();

	// For the interrupt, release the underlying system resource.
	_Irq.Invalidate();
}



//****************************************************************************
// InstallSubdevice()
//
// This monstrosity creates and initialized a miniport by calling its 
// port class driver. You supply the port and miniport class, name, and
// resources, we create them for you and optionally return the port 
// as an IUnknown interface. Spiffy. --trs
//
// Parameters:
//   Irp - Notification object. 
//   Name - name of class, I think
//   PortClassId - GUID of port class to instantiate
//   miniportAdopt - pointer to a miniport that will be released if
//     not used (so don't expect it to necessarily exist after this call)
//   MiniportClassId - GUID of miniport class to instantiate. Only used if
//     the MiniportCreate parameter is not supplied.
//   MiniportCreate - function to call to create miniport.
//   ResourceList - IPort::Init requires a list of resources for some reason
//   OutPortUnknown - Port (NOT mini) interface as an unknown.
//
#pragma code_seg("PAGE")
NTSTATUS CAdapterSolo::InstallSubdevice(
    IN      KIrp                I,
    IN      PWCHAR              Name,
    IN      REFGUID             PortClassId,
    IN      PUNKNOWN            miniportAdopt,
    IN      PRESOURCELIST       ResourceList,
    OUT     PUNKNOWN *          OutPortUnknown      OPTIONAL
)
{
    PAGED_CODE();

    t << "[CAdapterSolo::InstallSubdevice]\n";

    ASSERT(Name);
	 ASSERT(ResourceList);
    ASSERT(miniportAdopt);

    t << "InstallSubdevice" << Name << "\n";

	 //
    // Create the port driver object (eg IPortWave or IPortTopology object)
    //
    PPORT       port;
    NTSTATUS    ntStatus = PcNewPort(&port,PortClassId);

    if (NT_SUCCESS(ntStatus))
    {
         //
         // Init the port driver and miniport in one go.
         //
         ntStatus = port->Init( _fdo,
                                (PIRP)I,
                                miniportAdopt,
                                (PUNKNOWN)(PADAPTERSOLO)this,
                                ResourceList );

         if (NT_SUCCESS(ntStatus))
         {
             //
             // Register the subdevice (port/miniport combination).
             //
             ntStatus = PcRegisterSubdevice
                ( _fdo,
                  Name,
                  port );

             if (!(NT_SUCCESS(ntStatus)))
             {
                 t << "StartDevice: PcRegisterSubdevice failed\n";
             }
         }
         else
         {
             t << "InstallSubdevice: port->Init failed\n";
         }

         //
         // We don't need the miniport any more.  Either the port has it,
         // or we've failed, and it should be deleted.
         //
         miniportAdopt->Release();

        if (NT_SUCCESS(ntStatus))
        {
            //
            // Deposit the port as an unknown if it's needed.
            //
            if (OutPortUnknown)
            {
                //
                //  Failure here doesn't cause the entire routine to fail.
                //
                (void) port->QueryInterface
                (
                    IID_IUnknown,
                    (PVOID *) OutPortUnknown
                );
            }
        }
        

        //
        // Release the reference which existed when PcNewPort() gave us the
        // pointer in the first place.  This is the right thing to do
        // regardless of the outcome.
        //
        port->Release();
    }
    else
    {
        t << "InstallSubdevice: PcNewPort failed\n";
    }

    return ntStatus;
};
#pragma code_seg()

//
// _CreateWdmStuff -- create WDM-specific class objects as a
//   OnStartDevice() helper.
//
#pragma code_seg("PAGE")
NTSTATUS CAdapterSolo::_CreateWdmStuff(PIRP Irp,
                                        PRESOURCELIST resources)
{
   PAGED_CODE();

   t << "[CAdapterSolo::_CreateWdmStuff]\n";

   NTSTATUS status = STATUS_SUCCESS;
   PUNKNOWN unknownTopology   = NULL;
   PUNKNOWN unknownWave       = NULL;
   PUNKNOWN miniport          = NULL;
   PMINIPORT dmusicMiniport   = NULL;
   CMiniportWaveCyclicSolo * waveMiniport = NULL;
   PRESOURCELIST resourceListUart = NULL;
	
   ASSERT(Irp);
   ASSERT(resources); // bad parameter!

   // Create the Topology miniport, and incidentally retrieve
   // the WDM PortTopology as unknownTopology.

   // Create the Wave miniport for the solo
   status = CreateMiniportTopologySolo
   (
      &miniport,
      NULL, // unk outer
      NonPagedPool
   );

   if (!NT_SUCCESS(status))
      goto bail;

   // Install WavePort connected to our miniport
   status = InstallSubdevice( Irp,
                              L"Topology",
                              CLSID_PortTopology,
                              miniport,
                              resources,
                              &unknownTopology );


   if (!NT_SUCCESS(status))
      goto bail;

   // Create the Wave miniport for the solo and keep the 
   // reference to it.
   status = CreateMiniportWaveCyclicSolo
   (
      (PUNKNOWN *)&waveMiniport,
      NULL,
      NonPagedPool
   );
     
   if (!NT_SUCCESS(status))
   {            
      t << "CAdapterSolo::_CreateWdmStuff: PcNewMiniport failed (Wave)\n";
      goto bail;
   }


   // Install WavePort connected to our miniport This also
   // calls Release() on the miniport, so we shouldn't.
   status = InstallSubdevice( Irp,
                              L"Wave",
                              CLSID_PortWaveCyclic,
                              waveMiniport,
                              resourceListUart,
                              &unknownWave );


   // The port object will keep the current ref OR release it in
   // the previous call, so this may have been deleted (except for
   // the unknownWave ref of course, but I'm paranoid):
   waveMiniport = NULL;

   // Keep a reference to the IPortWaveCyclic interface for ourselves
   if (!unknownWave)
   {
      status = STATUS_UNSUCCESSFUL;
      goto bail;
   }
   status = unknownWave->QueryInterface( IID_IPortWaveCyclic,
                                         (PVOID *)&_portWave );

   if (!status)
      goto bail;

   //
   // Establish physical connections between audio filters as shown.
   //
   // TRS: There's a multiplexer and other stuff in the Topo, this
   //      diagram isn't really right. However, the Wave<-->Topo
   //      connections are pretty much straightforward:
   //
   //              +------+    +------+
   //              | Wave |    | Topo |
   //  Capture <---|0    1|<===|0   10|<--- S/PDIF In
   //              |      |    |      |
   //   Render --->|2    3|===>|1   11|---> S/PDIF Out
   //              +------+    |      |
   //                          |     2|<--- 0/1 In
   //                          |      |
   //                          |     3|---> 0/1 Out
   //                            ...         ...
   //
   //                          |     8|<--- 6/7 In
   //                          |      |
   //                          |     9|---> 6/7 Out
   //                          +------+
   //
   if (unknownTopology) // If the topology miniport
   {
      if (unknownWave) // ...and the wave miniport exist
      {
         // register wave <=> topology connections
         PcRegisterPhysicalConnection( _fdo,
                                       unknownTopology,
                                       CMiniportTopologySolo::PIN_CAPTURE_SRC,
                                       unknownWave,
                                       CMiniportWaveCyclicSolo::PIN_TOPOLOGY_IN );

         PcRegisterPhysicalConnection( _fdo,
                                       unknownWave,
                                       CMiniportWaveCyclicSolo::PIN_TOPOLOGY_OUT,
                                       unknownTopology,
                                       CMiniportTopologySolo::PIN_RENDER_DEST );
      }
   }


   ASSERT( 0 ); // TODO: walk through DMusic instantiation

   
   // 
   // Create built-in direct music uart miniport
   //

   resourceListUart = _FindUartResources( resources );
   if (NULL == resourceListUart)
   {
      t << "CAdapterSolo::_CreateWdmStuff: _FindUartResources failed\n";
      goto bail;
   }

   status = PcNewMiniport( (PMINIPORT*) &dmusicMiniport, 
                           CLSID_MiniportDriverDMusUART );

   if (! NT_SUCCESS( status ))
   {
      t << "CAdapterSolo::_CreateWdmStuff: PcNewMiniport failed (DMusic)\n";
      goto bail;
   }

   // Connect direct-music uart miniport to the direct-music port
   InstallSubdevice( Irp,
                     L"Uart",
                     CLSID_PortDMus,
                     dmusicMiniport,
                     resourceListUart,
                     NULL ); // interface is unnecessary

bail: // jump here on failure

  //
  // Release the unknowns.
  //
  if (unknownTopology)
  {
      unknownTopology->Release();
  }
  if (unknownWave)
  {
      unknownWave->Release();
  }

  return status;
}
#pragma code_seg()


//
// _FindUartResources -- 
//
// Parameters:
//   parent - parent resource list (all ICE1712 resources)
//
// Returns:
//   NULL on failure, a referenced IResourceList
//   representing the uart port on success
//
PRESOURCELIST CAdapterSolo::_FindUartResources( PRESOURCELIST parent )
{
   PRESOURCELIST theList = NULL;

   ASSERT( parent ); // must supply parent resource list!

   // UART resource notes (trs): 
   //   - Requires the port at offset 0 to be an MPU401 base
   //     address (which we know relative to the ICE addresses)
   //   - if one interrupt is supplied (two will not work), the
   //     MPU401 routines will use CallSychronizedRoutine. If that
   //     is what you want, put an interrupt in this list.
   //
   CM_PARTIAL_RESOURCE_DESCRIPTOR uartTranslated;
   CM_PARTIAL_RESOURCE_DESCRIPTOR uartUntranslated;
   uartTranslated.ShareDisposition = CmResourceShareDeviceExclusive;
   uartTranslated.Flags = CM_RESOURCE_PORT_IO;
   uartTranslated.u.Port.Start = _ccsRange.Base() + CCS_UART_1_DAT;
   uartTranslated.u.Port.Length = 2;
   uartTranslated.Type = CmResourceTypePort;

   uartUntranslated.u.Port.Start = _ccsRange.CpuPhysicalAddress() + CCS_UART_1_DAT;
   uartUntranslated.u.Port.Length = 2;
   uartUntranslated.Type = CmResourceTypePort;

FOO


   status =
      PcNewResourceSublist
      (
         &theList,
         NULL,
         PagedPool,
         resourceList,
         1
      );

   if (! NT_SUCCESS(status))
   {
      t << "PcNewResourceSublist failed\n";
      goto bail;
   }

   status = resourceListUart->AddEntry( &uartTranslated, 
                                        &uartUntranslated );

   if (! NT_SUCCESS(status))
   {
      t << "IResourceList::AddEntry failed\n";
      goto bail;
   }

   // resourceListUart)->AddInterruptFromParent(resourceList,0));

   return theList; // success

bail: // jump here on failure
   if (theList)
      theList->Release();

   return theList;
}

////////////////////////////////////////////////////////////////////////
//  CAdapterSolo::DpcFor_Irq
//
//	Routine Description:
//		Deferred Procedure Call (DPC) for Irq
//
//	Parameters:
//		Arg1 - User-defined context variable
//		Arg2 - User-defined context variable
//
//	Return Value:
//		None
//
//	Comments:
//		This function is called for secondary processing of an interrupt.
//		Most code that runs at elevated IRQL should run here rather than
//		in the ISR, so that other interrupt handlers can continue to run.
//

#pragma code_seg()
VOID CAdapterSolo::DpcFor_Irq(PVOID Arg1, PVOID Arg2)
{
// TODO:	Typically, the interrupt signals the end of a data transfer
//			operation for a READ or WRITE operation. The following code
//			assumes the driver will handle the completion of the IRP
//			associated with this operation here.  It further assumes that the
//			IRP to be completed is the current IRP on the driver managed queue.
//			Modify or replace the code here to handle the function of your DPC.

// TODO:	Enable further interrupts on the device. Well, maybe. :)

   // There's just times when you wonder if that ol' clock is advancing
   // at all...
   //t << " | " << _playDmaOldPos << " - " << _playPosBytes << " - " 
   //   << PlayGetPositionBytes() << " |\n";

	// The following macros simply allows compilation at Warning Level 4
	// If you reference these parameters in the function simply remove the macro.
	UNREFERENCED_PARAMETER(Arg1);
	UNREFERENCED_PARAMETER(Arg2);
}

////////////////////////////////////////////////////////////////////////
//  CAdapterSolo::Isr_Irq
//
//	Routine Description:
//		Interrupt Service Routine (ISR) for IRQ Irq
//
//	Parameters:
//		None
// 
//	Return Value:
//		BOOLEAN		True if this is our interrupt
//
//	Comments:
//
#pragma code_seg()
BOOLEAN CAdapterSolo::Isr_Irq(void)
{
   DWORD irqStatus = iceIrqStatus();
   
   if (0 == irqStatus)          // Not our interrupt?
      goto notOurIrq;           // use goto so branch is unusual

   // clear these interrupt bits because we are about to handle them.
   // Don't clear all bits (0xff) because that would cause a race
   // condition and potential dropped interrupts.

   iceIrqClear( (BYTE)irqStatus );				

   // Well, what interrupts do we have
   
   if (irqStatus & ICE_IRQMTBIT) // "pro" audio irq.
   {
      // We have to try to guess how many bytes have been 
      // played by counting buffers. For some reason the chip 
      // doesn't have hardware counters. (how hard could it be?)
      //
      // I think it's safe to write the _recPos* variables in the interrupt
      // handler because they are a) set when the interrupt
      // is disabled or b) only written by the IRQ handler when the
      // dma engine is running.

      // This is a special case: we clear the irq in a different register!
      DWORD mtIrqStatus = icemtIrqStatus();

      if (MTO_IRQRECBIT & mtIrqStatus)
      {
         icemtRecIrqClear();

         // Count bytes for posterity
         ++ _recPosIrqs;

         // Notify as necessary
         if (_portWave && _captureServiceGroup);
            _portWave->Notify(_captureServiceGroup);
      }

      if (MTO_IRQPLAYBIT & mtIrqStatus)
      {
         icemtPlayIrqClear();

         // Count bytes for posterity
         ++ _playPosIrqs;

         // if necessary, copy some more data from the "system" buffer
         // into the dma buffer
         switch (_playMode)
         {
         case DMAMODE_MULTICHANNEL:
         default:
            // do nothing
            break;

         case DMAMODE_LEGACY:
            DWORD pageDwords = (_playDmaPageFrames * SSS_PLAY_FRAME_SIZE);

            // Unless the dma engine seriously disses us, the dma 
            // position should be _past_ the end of the previous page. 
            DWORD page = (WORD)_mtRange[MTO_PLAYSIZECOUNT]; // current pos counts down...

            // Convert to 0-indexed downward-counting offset
            ASSERT( page > 0 );
            -- page;
            // page = (page - 1) % pageDwords;

            // Convert to page number (instead of dma offset). Truncation here
            // assumes 0-indexed dma offset.
            page /= pageDwords;
            ASSERT( page < _playDmaPageCount );

            // convert to 1-indexed dword counter
            page = _playDmaPageCount - page; 
            ASSERT( page <= _playDmaPageCount ); // negative?

            // convert to previous page, 0-indexed, and idiot-proof the range
            page = (page + _playDmaPageCount - 2) % _playDmaPageCount;
            _copyLegacyToPlay( page * _playDmaPageFrames,  // start frame
                               _playDmaPageFrames, // frame count
                               0 );                // out channel base offset
            break;

         }

         // Notify as necessary
         if (_portWave && _renderServiceGroup);
            _portWave->Notify(_renderServiceGroup);
      }
   }

   // If you need the timer, here it goes
   // if (irqStatus & ICE_IRQTIMERBIT)
   // {
   // }

   //
   // MIDI - The internal queue is big, no need to flush it on every
   // interrupt. (poll on timer just in case it drops an IRQ.)
   //
   if (irqStatus & (ICE_IRQMIDIONEBIT |
                    ICE_IRQMIDITWOBIT))
   {
	   ; // TODO: pass the buck on MIDI I/O as well.
   }

	// Request deferred procedure call if you want one
 	// The arguments to Request may be any values that you choose
//   if (!_DpcFor_Irq.Request(NULL, NULL))
//   {
      // Oh, request is already in the queue. This probably
      // means lost data for you!
//   }

	// Return TRUE to indicate that our device caused the interrupt
	return TRUE;

notOurIrq:
   return FALSE;
}

//
// _copyLegacyToPlay -- interleave stereo 16-bit audio into some output
//   dma channels. 
// 
// Parameters:
//   startFrame - 0-based offset of frame to start at
//   frameCount - number of frames to copy
//   baseOffset - 0-based channel offset for destination channel (0-9)
//
void CAdapterSolo::_copyLegacyToPlay( ULONG startFrame,
                                      ULONG frameCount,
                                      ULONG baseOffset )
{
   ASSERT( baseOffset < SSS_PLAY_FRAME_SIZE );

   // Think of source as a pointer to a stereo pair of 16-bit samples
   DWORD sourceFrame;
   PDWORD source = (PDWORD)_playLegacyDmaChannel->SystemAddress(); 
   source += startFrame;

   PDWORD sourceTerminator = source + frameCount;
   ASSERT( (DWORD)sourceTerminator <= ((DWORD)_playLegacyDmaChannel->SystemAddress() 
                                       + _playLegacyDmaChannel->BufferSize()) );

   PULONG dest = ((PDWORD)_playDmaBufferVirt) + (startFrame * SSS_PLAY_FRAME_SIZE);

   while (source < sourceTerminator)
   {
      sourceFrame = *source;

      *dest = (sourceFrame << 16);         // left from lower 16 bits
      ++ dest;
      *dest = (sourceFrame & 0xffff0000);  // right from upper 16 bits

      // Go to the next frame in both the source (stereo) and dest (10-channel)
      ++ source;                   
      dest += (SSS_PLAY_FRAME_SIZE - 1); 
   }
}

//
// SetRecDma -- called by the wave miniport 
//
// This function sets the addresses where we want the hardware
// to copy data on record. The virtual address is not technically
// necessary but is awfully useful for debugging.
//
// If size == 0, dma will be disabled for obvious reasons.
//
// Returns:
//   0 on success, nonzero otherwise
//
#pragma code_seg("PAGE")
DWORD CAdapterSolo::SetRecDma( PPHYSICAL_ADDRESS phys,
                               PULONG           virt,
                               ULONG            size )
{
   PAGED_CODE();

   t << "[CAdapterSolo::SetRecDma]\n";

   HardwareSyncAcquire();

   icemtRecStop(); // be paranoid

   if (!phys)
   {
      _recDmaBufferPhys.QuadPart = 0;
      _recDmaBufferVirt = 0;
      _recBufferSize = 0;
   }
   else
   {
      _recDmaBufferPhys = *phys;
      _recDmaBufferVirt = virt;
      _recBufferSize = size;

      // Doublecheck the stream miniport...
      ASSERT( size == (RecGetDmaPageFrames() * SSS_REC_FRAME_SIZE 
                       * SSS_SAMPLE_BYTES* _recDmaPageCount) );

      // Give the ice wrapper the buffers we allocated.

      if (0 != icemtRecSetDMABase( (PULONG)phys->LowPart ))
         goto bail;

      if (0 != icemtRecSetDMASize( size ))
         goto bail;

      // Set interrupt interval (in samples)
      icemtRecSetDMATerm( _recDmaPageFrames );
   
   }

   HardwareSyncRelease();
   return 0; // success

bail:
   // Something went wrong. Be sure to disable access to this memory.
   _recDmaBufferVirt = 0;
   _recBufferSize = 0;

   HardwareSyncRelease();
   return 1;
}
#pragma code_seg()

//
// SetPlayDma -- very similar to SetRecDma (above)
//
#pragma code_seg("PAGE")
DWORD CAdapterSolo::SetPlayDma( PPHYSICAL_ADDRESS phys,
                                PULONG           virt,
                                ULONG            size )
{
   PAGED_CODE();

   t << "[CAdapterSolo::SetRecDma]\n";

   HardwareSyncAcquire();

   icemtPlayStop();

   if (!phys)
   {
      _playDmaBufferPhys.QuadPart = 0;
      _playDmaBufferVirt = 0;
      _playBufferSize = 0;
   }
   else
   {
      _playDmaBufferPhys = *phys;
      _playDmaBufferVirt = virt;
      _playBufferSize = size;

      // The miniport is supposed to ensure this
      ASSERT( size == (PlayGetDmaPageFrames() * SSS_PLAY_FRAME_SIZE 
                       * SSS_SAMPLE_BYTES* _playDmaPageCount) );

      if (0 != icemtPlaySetDMABase( (PULONG)phys->LowPart ))
         goto bail;

      if (0 != icemtPlaySetDMASize( size ))
         goto bail;

      // Set interrupt interval (in samples)
      icemtPlaySetDMATerm( _playDmaPageFrames );

   }

   HardwareSyncRelease();
   return 0; // success

bail:
   _playDmaBufferVirt = 0;
   _playBufferSize = 0;

   // Something went wrong. Be sure to disable access to this memory.
   HardwareSyncRelease();
   return 1;
}
#pragma code_seg()


#pragma code_seg("PAGE")
NTSTATUS CAdapterSolo::GetPlayDmaChannelRef( PDMACHANNEL * channel )
{
   ASSERT(channel);
   ASSERT(_playDmaChannel);

   *channel = _playDmaChannel;
   (*channel)->AddRef();

   return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
NTSTATUS CAdapterSolo::GetRecDmaChannelRef( PDMACHANNEL * channel )
{
   ASSERT(channel);
   ASSERT(_recDmaChannel);

   *channel = _recDmaChannel;
   (*channel)->AddRef();

   return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
NTSTATUS CAdapterSolo::GetPlayLegacyDmaChannelRef( PDMACHANNEL * channel)
{
   ASSERT(channel);
   ASSERT(_playLegacyDmaChannel);

   *channel = _playLegacyDmaChannel;
   (*channel)->AddRef();

   return STATUS_SUCCESS;
}

//
// GetRenderServiceGroup -- return pointer to the service group. This
//   does not AddRef the pointer, that's your problem. 
//
#pragma code_seg("PAGE")
NTSTATUS CAdapterSolo::GetRenderServiceGroup(PSERVICEGROUP * gp_ret)
{
   PAGED_CODE();

   ASSERT( gp_ret ); // bad parameter

   if (_renderServiceGroup)
   {
      *gp_ret = _renderServiceGroup;
      return STATUS_SUCCESS;
   }

   return STATUS_UNSUCCESSFUL;
}
#pragma code_seg()


#pragma code_seg("PAGE")
NTSTATUS CAdapterSolo::GetCaptureServiceGroup(PSERVICEGROUP * gp_ret)
{
   PAGED_CODE();

   ASSERT( gp_ret ); // bad parameter

   if (_captureServiceGroup)
   {
      *gp_ret = _captureServiceGroup;
      return STATUS_SUCCESS;
   }

   return STATUS_UNSUCCESSFUL;
}
#pragma code_seg()


//
// SetSamplingFrequency
//
#pragma code_seg()
NTSTATUS CAdapterSolo::SetSamplingFrequency( ULONG newFreq )
{
   NTSTATUS status = STATUS_SUCCESS; // assume success

   // I suppose it's good there's a mutex around this because
   // the play and record wave devices might try to set
   // the freqency at exactly the same time. However, I wonder
   // if this is really sufficient. I suspect that if the wave
   // port chose to allocate play and record in different threads
   // you could end up with one thread running at a different
   // speed...
   HardwareSyncAcquire();

   // If it fails, it probably failed because of a bad frequency...
   // Keep holding the sync 'cause we're accessing hardware from
   // miniport entry points
   if (0 != icemtSetFreq(newFreq))
      status = STATUS_INVALID_PARAMETER;
   else
      _samplingFrequency = newFreq;

   HardwareSyncRelease();

   return status;
}

//
// GetSamplingFrequency -- get the current sample rate of the ICE
//   hardware.
//
#pragma code_seg()
ULONG CAdapterSolo::GetSamplingFrequency()
{
   HardwareSyncAcquire();
   ULONG freq_ret = _samplingFrequency;
   HardwareSyncRelease();

   return freq_ret;
}

//
// StartCapture -- start the record dma engine. Whee.
//
#pragma code_seg()
NTSTATUS CAdapterSolo::StartCapture()
{
   NTSTATUS status = STATUS_SUCCESS;

   HardwareSyncAcquire();

   _recPosIrqs = 0;

   if (0 == _recDmaBufferVirt)
   {
      ASSERT(0);
      status = STATUS_UNSUCCESSFUL;
   }
   else
   {
      icemtRecStart();
   }
   
   HardwareSyncRelease();
   return status;
}

NTSTATUS CAdapterSolo::PauseCapture()
{
   HardwareSyncAcquire();

   icemtRecPause();

   HardwareSyncRelease();

   return STATUS_SUCCESS;
}

NTSTATUS CAdapterSolo::UnpauseCapture()
{
   HardwareSyncAcquire();

   icemtRecUnpause();

   HardwareSyncRelease();

   return STATUS_SUCCESS;
}

NTSTATUS CAdapterSolo::StopCapture()
{
   HardwareSyncAcquire();

   icemtRecStop();

   HardwareSyncRelease();

   return STATUS_SUCCESS;
}

NTSTATUS CAdapterSolo::StartRender()
{
   NTSTATUS status = STATUS_SUCCESS;

   HardwareSyncAcquire();

   _playPosIrqs = 0;

   if (0 == _playDmaBufferVirt)
   {
      ASSERT(0);
      status = STATUS_UNSUCCESSFUL;
   }
   else
   {
      icemtPlayStart();
   }

   HardwareSyncRelease();
   return status;
}

NTSTATUS CAdapterSolo::PauseRender()
{
   HardwareSyncAcquire();

   icemtPlayPause();
   
   HardwareSyncRelease();
   return STATUS_SUCCESS;
}

NTSTATUS CAdapterSolo::UnpauseRender()
{
   HardwareSyncAcquire();

   icemtPlayUnpause();
   
   HardwareSyncRelease();
   return STATUS_SUCCESS;
}

NTSTATUS CAdapterSolo::StopRender()
{
   HardwareSyncAcquire();

   icemtPlayStop();
   
   HardwareSyncRelease();
   return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
void CAdapterSolo::SetRecDmaPageFrames(ULONG frames)
{
   PAGED_CODE();

   ASSERT( frames >= SSS_PAGE_FRAMES_MIN );
   ASSERT( frames <= SSS_PAGE_FRAMES_MAX );

   _recDmaPageFrames = frames;
}
#pragma code_seg()


#pragma code_seg("PAGE")
void CAdapterSolo::SetPlayDmaPageFrames(ULONG frames)
{
   PAGED_CODE();

   ASSERT( frames >= SSS_PAGE_FRAMES_MIN );
   ASSERT( frames <= SSS_PAGE_FRAMES_MAX );

   _playDmaPageFrames = frames;
}
#pragma code_seg()


#pragma code_seg("PAGE")
void CAdapterSolo::SetRecDmaPageCount(ULONG count)
{
   PAGED_CODE();

   ASSERT( count >= SSS_DMA_PAGE_COUNT_MIN );
   ASSERT( count <= SSS_DMA_PAGE_COUNT_MAX );
   _recDmaPageCount = count;

}

#pragma code_seg()


#pragma code_seg("PAGE")
void CAdapterSolo::SetPlayDmaPageCount(ULONG count)
{
   ASSERT( count > 1 );

   _playDmaPageCount = count;
}
#pragma code_seg()

   //
// SetCaptureNotificationInterval -- try to set the time interval at
//   which interrupts happen for input
//
// Parameters:
//   interval - suggested notification interval in milliseconds
//
// It may be possible to do this by changing the
// number of interrupts per page. We're depending on getting at least
// two interrupts per page so we can count pages to keep track of the
// current sample position. :)
//
// This could also be achieved by using the timer interrupt, but since
// there's only one, it would have to be the same for both play and
// record. This would be fine as well, but since there's no way to
// notify them that it's changed, we can't do that. 
//
#pragma code_seg("PAGE")
void CAdapterSolo::SetCaptureNotificationInterval( ULONG interval )
{
   PAGED_CODE();
   
   t << "[CAdapterSolo::SetCaptureNotificationInterval] " 
      << interval
      << "\n";

   if (interval < 1) // Just not possible;
      return;

   // How many bytes in the desired interval?
   //  (frames/s) * ms / (ms/s)
   ULONG pageSize= (GetSamplingFrequency() * interval / 1000 
                    * SSS_SAMPLE_BYTES * SSS_REC_FRAME_SIZE);

   if (pageSize == 0) // well, what if the sampling frequency is wrong?
      return;
   
   // How many of these fit into the buffer size?
   ULONG pageCount = _recBufferSize / pageSize;

   // Now divide this into the buffer size to get an exact size
   // for the notification interval in bytes.
   if (pageCount <= 1)   // Must be at least two buffers. :)
      pageCount = 2;

   pageSize = _recBufferSize / pageCount;

   _recDmaPageCount = pageCount;
   _recDmaPageFrames = pageSize / SSS_SAMPLE_BYTES / SSS_REC_FRAME_SIZE;

   // TRS: verbose
   t << " _recDmaPageFrames:" << _recDmaPageFrames
      << " _recDmaPageCount:" << _recDmaPageCount << "\n";


   // Set interrupt interval (in samples)
   icemtRecSetDMATerm( _recDmaPageFrames );
}
#pragma code_seg()

#pragma code_seg("PAGE")
void CAdapterSolo::SetRenderNotificationInterval( ULONG interval )
{
   PAGED_CODE();

   t << "[CAdapterSolo::SetRenderNotificationInterval] " 
      << interval
      << "\n";

   // Try to choose a page size to fit the interval. The buffer
   // must be an exact number of pages...

   if (interval < 1) // Just not possible;
      return;

   // How many bytes in the desired interval?
   //  (frames/s) * ms / (ms/s)
   ULONG pageSize= (GetSamplingFrequency() * interval / 1000 
                    * SSS_SAMPLE_BYTES * SSS_PLAY_FRAME_SIZE);

   if (pageSize == 0) // well, what if the sampling frequency is wrong?
      return;
   
   // How many of these fit into the buffer size?
   ULONG pageCount = _playBufferSize / pageSize;

   // Now divide this into the buffer size to get an exact size
   // for the notification interval in bytes.
   if (pageCount <= 1)   // Must be at least two buffers. :)
      pageCount = 2;

   pageSize = _playBufferSize / pageCount;

   _playDmaPageCount = pageCount;
   _playDmaPageFrames = pageSize / SSS_SAMPLE_BYTES / SSS_PLAY_FRAME_SIZE;

   // TRS: verbose
   t << " _playDmaPageFrames:" << _playDmaPageFrames
      << " _playDmaPageCount:" << _playDmaPageCount << "\n";

   // Set interrupt interval (in samples)
   icemtRecSetDMATerm( _recDmaPageFrames );
}
#pragma code_seg()

//
// GetCaptureNotificationInterval -- return the interrupt interval
//   in milliseconds
//
#pragma code_seg("PAGE")
ULONG CAdapterSolo::GetCaptureNotificationInterval()
{
   PAGED_CODE();

   HardwareSyncAcquire();

   // interval = page size(frames) / sample rate(frames/sec) * 1000(ms/sec)
   ULONG interval = _recDmaPageFrames * 1000 / _samplingFrequency;

   HardwareSyncRelease();
   return interval;
}
#pragma code_seg()

#pragma code_seg("PAGE")
ULONG CAdapterSolo::GetRenderNotificationInterval()
{
   PAGED_CODE();

   HardwareSyncAcquire();

   // interval = page size(frames) / sample rate(frames/sec) * 1000(ms/sec)
   ULONG interval = _playDmaPageFrames * 1000 / _samplingFrequency;

   HardwareSyncRelease();
   return interval;
}
#pragma code_seg()

//
// -- Local function protos --
//

//
// ICEinit -- Figure out what model of solo we have attached and
//   do appropriate processing
//
// Returns:
//   0 on success, nonzero otherwise
//
#pragma code_seg("PAGE")
DWORD CAdapterSolo::ICEinit()
{
   PAGED_CODE();

   // Windows(95?)-specific?:
   // This is VERY important.  If the PC's BIOS enabled the NMI line (SERR),
   // we must disable it.  (The NMI is used when virtualizing DMA in
   // DOS TSR's only.)  Otherwise the PC will hang on the first MIDI
   // byte recieved (at least until DOS-mode NMI handlers are installed).

   // This is nicely encapsulated in the DriverStudio objects. I wonder if
   // there's an easy way to do config cycles...

   // Old windows95 code:
   // PCIConfigRead( devnode, 0x04, &pciReg );
   // pciReg &= ~PCI_SERR_BIT;     // disable NMI (error)
   // PCIConfigWrite( devnode, 0x04, pciReg );

   // DriverWorks version: just create the configuration
   // using the physical device object. 
   KPciConfiguration pciConfig( _pdo );
   ASSERT(pciConfig.IsValid()); // Has lower been initialized?
   pciConfig.Control(cmdSystemError, 0);

   // Read the vendor ID (ours?)
   // PCIConfigRead( devnode, 0x2c, &pciReg );
   // switch (pciReg)
   // {
   // default:
       // Default is to assume standard solo card. This may
       // allow us to incorrectly load for other mfg's cards,
       // but it solves other problems (like programming
       // with a blank eeprom, etc).

	   // Fall through and hope it's seasound-compatible
       
   // case 0xeba01412:             // Solo (or SoloEX)
   //     break;
   // }

    iceGetCardType( &_cardType, &_cardVersion );

    switch (_cardType)
    {
    case SSS_PCICARDTYPE_SOLO:  
    case SSS_PCICARDTYPE_SOLOEX:  
    case SSS_PCICARDTYPE_SOLOIST:  
    case SSS_PCICARDTYPE_DIO:
        break;

    default:                    // unknown version
        return 1;               // ** quick exit **
    }
    
    // Disable Legacy DMA, which greatly reduces bus traffic.
    DWORD pciReg;                // scratch dword

	pciConfig.ReadDeviceSpecificConfig
		(
		 &pciReg,
		 0x40 - 0x40,	// offset from start of device data (0x40)
		 sizeof(pciReg) // buffer size
		);

    pciReg |= 0x00008000;
	pciConfig.WriteDeviceSpecificConfig
		(
		 &pciReg,
		 0x40 - 0x40,	// offset from start of device data (0x40)
		 sizeof(pciReg) // buffer size
		);

    // Set up the I2S interface.

    // Assume 4 stereo DAC's even though they may not be present
    pciReg =  (0x3f << 0);       // No legacy codec, 4 stereo AD/DA, 2 UARTs
    pciReg |= (0x80 << 8);       // "professional codec interface type" is I2S
    
    // pciReg |= (0x71 << 16);      // wrong(?) I2S format and ID
    pciReg |= (0x01 << 16);      // I2S format and ID

    // "cost-reduced" soloist card has no s/pdif
    if (SSS_PCICARDTYPE_SOLOIST == _cardType) 
        pciReg |= (0x00 << 24);
    else
        pciReg |= (0x03 << 24);      // S/PDIF in/out are present
   
    pciConfig.WriteDeviceSpecificConfig
		(
		 &pciReg,
         0x60 - 0x40,        // offset of CODEC config (PCI60-63)
         sizeof(pciReg)
		);


    // Set the initial state (disabled) for multitrack interrupts:
    _mtIrqMask = (MTO_IRQPLAYEN | MTO_IRQRECEN);
    _mtRange[MTO_IRQMASK] = (BYTE)_mtIrqMask;
   
    // disable all interrupts except MultiTrack interrupts (which
    // are disabled above)
    _ccsIrqMask = ~(ICE_IRQMTBIT);
    _setHwCcsIrqMask();

    // Make sure the status for these irq's is cleared so that they
    // can generate irq's...
    _ccsRange[CCS_IRQSTATUS] = (BYTE)0xff;

    // Initially disable the timer 
	// (warning: sets the timer irq status bit in some versions of the
	// hardware --trs)
    _ccsRange[CCS_TIMER] = (WORD)0;

    // Set a known default state for audio routing.
    _loopbackMode = ICEROUTE_SPDIFMIX;
    _setRoute();

    // Set up the GPIO, based on the card revision:

    switch (_cardType)
    {
    case SSS_PCICARDTYPE_SOLO:
        // Set up the GPIO directions to match the R1 Solo hardware.
        // Bits 0 and 3 are the outputs:
        iceGpioSetDirection( (1 << 0) | (1 << 3) );
        break;
        
    case SSS_PCICARDTYPE_SOLOEX:  // SoloEX
    case SSS_PCICARDTYPE_SOLOIST: // soloist (SoloEX with no S/PDIF)
        // ** SoloEX ** (PCI card Rev 2)
        // GPIO0 - input - not used
        // GPIO1 - output - MIDI IN 2 source select (1 = SOLO)
        // GPIO2 - output - MIDI out sync select (default to 1)
        // GPIO3 - input - not used
        // GPIO4 - input - PMODE (0 if A8 has channels 1&2, 1 otherwise)
        // GPIO5 - not used
        // GPIO6 - output - secondary MIDI OUT
        // GPIO7 - input - secondary MIDI IN

        iceGpioSetDirection( ((1 << 1) |
                              (1 << 2) |
                              (1 << 6)) );

        // Set the default values for the output bits:
        iceGpioWrite( (1 << 0) );

        // Inverted works best.
        iceSetMidiOutSync( 1 );
        break;
        
    case SSS_PCICARDTYPE_DIO: // ** DIO card ** (type 3)
        
        // write 1 for output, 0 for input
        iceGpioSetDirection
            ( ((0 << 0) |   // GPIO0 - input - not used                    
               (1 << 1) |   // GPIO1 - output - ext clock select (0 for S/P
               (1 << 2) |   // GPIO2 - output - hold error signal (adat)   
               (0 << 3) |   // GPIO3 - input - not used by driver          
               (1 << 4) |   // GPIO4 - output - Mutes incoming ADAT streams
               (0 << 5) |   // GPIO5 - input - not used                    
               (1 << 6) |   // GPIO6 - output - secondary MIDI OUT         
               (0 << 7)) ); // GPIO7 - input - secondary MIDI IN           

        // Set the default values for the output bits:
        iceGpioWrite( (0 << 0) ); // leave 'em alone

        // Inverted works best.
        iceSetMidiOutSync( 1 );
        break;

    default:
        VASSERT( 0, "checked this earlier" );
        break;
    }
    return 0;
}
#pragma code_seg()

//
// iceSetMidiOutSync -- Set/clear the "invert midi clock" bit
//
#pragma code_seg()
void CAdapterSolo::iceSetMidiOutSync(  int isInverted )
{
    BYTE values;
    
    switch (_cardType)
    {
    case SSS_PCICARDTYPE_SOLO:
        // Early cards didn't have this option. :(
        break;                  

    case SSS_PCICARDTYPE_SOLOEX:
    case SSS_PCICARDTYPE_SOLOIST:
        // Set GPIO2 for normal operation, clear bit 2 for
        // phase-inverted operation. :)
        
        // Read-modify-write
        
        iceGpioRead( &values );
        
        if (isInverted)             // for inverted operation
            values &= ~(1 << 2);    // clear bit 2
        else
            values |= (1 << 2);     // (normal)
        
        iceGpioWrite( values );
        break;

    case SSS_PCICARDTYPE_DIO:
        // DIO has no midi!
        break;
        
    default:
        VASSERT(0, "unexpected version" );
        break;
    }
}

//
// iceIrqOverride -- enable/disable the override
//
// When the override is on, the hardware irqs are masked off
// regardless of what mask is set by the software. This is
// useful for temporarily disabling irq's while setting up
// or shutting down the hardware.
//
// I have attempted to guarantee that NO interrupts will be
// generated after the override is enabled. Help!
//
#pragma code_seg()
void  CAdapterSolo::iceIrqOverride(  int blockIrqs )
{
   if (blockIrqs)
       _irqMaskOverride = 1;       // **last**
   else
       _irqMaskOverride = 0;     // **first**

   _setHwCcsIrqMask();
}

//
// _setHwCcsIrqMask -- local helper
//
// Only actually sets the hardware mask if the override is
// off.
//
#pragma code_seg()
void CAdapterSolo::_setHwCcsIrqMask()
{
   unsigned char dummy;

   // Note: this can be called for a cookie where the user hasn't
   //   called ICEInit yet, because you may want to disable interrupts
   //   initially.
   
   if (0 == _irqMaskOverride)
      _ccsRange[CCS_IRQMASK] = (BYTE)_ccsIrqMask;
   else
      _ccsRange[CCS_IRQMASK] = (BYTE)0xff; // turn 'em ALL off!

   // Make sure the status for irq's is cleared so that they
   // can generate irq's...or if we just disabled 'em, to clear
   // any pending irq to avoid crashing. :)
   
   _ccsRange[CCS_IRQSTATUS] = (BYTE)0xff;

   // If midi is enabled, drain its fifo...
   if (0 == (_ccsIrqMask & ICE_IRQMIDIONEBIT))
   {
       // Drain the FIFO to allow new midi irq's...
       while (0 == iceMidiOneDataIn(&dummy))
           /* do nothing */;
   }
       
   if (0 == (_ccsIrqMask & ICE_IRQMIDITWOBIT))
   {
       // Drain the FIFO to allow new midi irq's...
       while (0 == iceMidiTwoDataIn(&dummy))
           /* do nothing */;
   }
}


//
// icemtPlaySetVolume -- set volume for a play channel pair
//
// Parameters:
//   chnl - multi-track channel to set (0-indexed), 0 - 8 (9-10 S/PDIF)
//   leftVol - channel left volume.  0 = min, 0xffff = max
//   rightVol - right volume
//
// Notes:
//   Only the most significant 7 bits are used, and represent 1.5dB
//   attenuation.  
//
//   Perhaps I should scale our data so that the value 0x00 represents
//   the maximum attenuation (-144dB or binary 1100000)? 
//
// void icemtPlaySetVolume( WORD chnl, WORD leftVol, WORD rightVol )
// {
//    VASSERT( 0xffff != card->controlBase, "api not initialized" );
//    VASSERT( chnl < 10, "channel out of range" );

//    OUTB( MTO_VOLINDEX, (BYTE)chnl );
//    OUTB( MTO_VOLDATALEFT,  (BYTE)(((0xffff - leftVol) >> 9) & 0x7f) );
//    OUTB( MTO_VOLDATARIGHT, (BYTE)(((0xffff - rightVol) >> 9) & 0x7f) );
// }

// void icemtPlayGetVolume( WORD chnl, WORD * leftVol, WORD * rightVol )
// {
//    WORD left = INB( MTO_VOLDATALEFT );
//    WORD right = INB( MTO_VOLDATARIGHT );

//    VASSERT( 0xffff != card->controlBase, "api not initialized" );
   
//    *leftVol = 0xffff - (left << 9);
//    *rightVol = 0xffff - (right << 9);
// }

//
// icemtSetFreq -- set the frequency for the "pro MultiTrack" section
//
// Parameters:
//   newFreq - desired freqency in kiloherz.
//
// Returns:
//   0 on success, nonzero otherwise
//
#pragma code_seg()
DWORD CAdapterSolo::icemtSetFreq(  unsigned long newFreq )
{
   BYTE freqBits = 0;
   BYTE status;

   switch (newFreq)
   {
   case 48000L: freqBits = MTSR_48_kHz;     break;
   case 24000L: freqBits = MTSR_24_kHz;     break;
   case 12000L: freqBits = MTSR_12_kHz;     break;
   case  9600L: freqBits = MTSR_9_6_kHz;    break;
   case 32000L: freqBits = MTSR_32_kHz;     break;
   case 16000L: freqBits = MTSR_16_kHz;     break;
   case 96000L: freqBits = MTSR_96_kHz;     break;
   case 44100L: freqBits = MTSR_44_1_kHz;   break;
   case 22050L: freqBits = MTSR_22_05_kHz;  break;
   case 11025L: freqBits = MTSR_11_025_kHz; break;
   case 88200L: freqBits = MTSR_88_2_kHz;   break;

   default:
      return 1;                 // failure!  Aaugh!
   }

   // Erase the old sample rate, and replace it with the new one.
   // Yeah, that's right, the sample rate is in one nibble of a dword
   // containing lots of other crap.  Not my idea.  Yuck.
   
   status = (BYTE)_mtRange[MTO_SAMPLERATE];
   status &= 0xf0;
   status |= freqBits;
   _mtRange[MTO_SAMPLERATE] = (BYTE)status;
   
   return 0;
}

//
// icemtGetFreq -- returns the current frequency (as a DWORD)
//
#pragma code_seg()
DWORD CAdapterSolo::icemtGetFreq()
{
   BYTE status;

   status = (BYTE)_mtRange[MTO_SAMPLERATE];
   status &= 0x0f;
   
   switch (status)
   {
   case MTSR_48_kHz    :  return 48000L;
   case MTSR_24_kHz    :  return 24000L;
   case MTSR_12_kHz    :  return 12000L;
   case MTSR_9_6_kHz   :  return  9600L;
   case MTSR_32_kHz    :  return 32000L;
   case MTSR_16_kHz    :  return 16000L;
   case MTSR_96_kHz    :  return 96000L;
   case MTSR_44_1_kHz  :  return 44100L;
   case MTSR_22_05_kHz :  return 22050L;
   case MTSR_11_025_kHz:  return 11025L;
   case MTSR_88_2_kHz  :  return 88200L;

   default:
      break;
   }

   return 0;                 // unexpected frequency!
}

//
// setDMABase - set the base address for the DMA engine
//
// Parameters:
//   base - new DMA base.  only 27 bits are significant.  Must
//     be word32 aligned.  Address range must be below 256M
//     AFAIK Low bits are 0. --trs
//
// My documentation says:
//   "Channel-10 interleaves 10 slots, each with 32-bit from the
//   system memory."  Perhaps this means that we have 10 channels
//   interleaved. Could be.
//
// Returns:
//   0 on success
//
#pragma code_seg()
DWORD CAdapterSolo::icemtPlaySetDMABase(  PDWORD base )
{
#ifndef NDEBUG  // TRS: for testing only
   long b = (DWORD)_mtRange[MTO_PLAYBASE]; 
#endif 

   _mtRange[MTO_PLAYBASE] = (DWORD)base;

   return 0;
}

#pragma code_seg()
DWORD CAdapterSolo::icemtRecSetDMABase(  PDWORD base )
{
#ifndef NDEBUG  // TRS: for testing only
   long b = (DWORD)_mtRange[MTO_RECBASE];
#endif

   _mtRange[MTO_RECBASE] = (DWORD)base;

   return 0;
}

//
// ICEplaySetDMASize - set the "terminal count" for the DMA engine
//
// Parameters:
//   size - size of buffer at DMABase in bytes, must be an round
//     number of dwords.  Only the least 28 bits are significant.
//     Also, this must be a multiple of the "frame" size, which
//     is to say a multiple of (10 * sizeof(DWORD)).  If it is
//     not, you may not recieve interrupts.
//
// Returns:
//   0 on success
//
#pragma code_seg()
DWORD CAdapterSolo::icemtPlaySetDMASize(  DWORD size )
{
   if (size % (10 * sizeof(DWORD)) != 0)
      goto bail;
   else if ((size >> 28) != 0)
      goto bail;
   
   // convert to dwords, subtract 1, and write to thet size register.
   _mtRange[MTO_PLAYSIZECOUNT] = (WORD)((size / sizeof(DWORD)) - 1);
   return 0;

bail:
   _mtRange[MTO_PLAYSIZECOUNT] = (WORD)0; // Set the size to 0 for sanity
   return 1;
}

//
// ICErecSetDMASize - set the "terminal count" for the DMA engine
//
// Parameters:
//   size - size of buffer at DMABase in bytes, must be an round
//     number of dwords.  Only the least 28 bits are significant.
//     Also, this must be a multiple of the "frame" size, which
//     is to say a multiple of (12 * sizeof(DWORD)).  If it is
//     not, you may not recieve interrupts.
//
// Returns:
//   0 on success
//
#pragma code_seg()
DWORD CAdapterSolo::icemtRecSetDMASize(  DWORD size )
{
   if (size % (12 * sizeof(DWORD)) != 0)
      goto bail;
   if ((size >> 28) != 0)
      goto bail;
   
   // convert to dwords, subtract 1, and write to MT16, which
   // is the "terminating count" or something.
   _mtRange[MTO_RECSIZECOUNT] = (WORD)((size / sizeof(DWORD)) - 1);
   
   return 0;

bail:
   // Set size to 0 for sanity
   _mtRange[MTO_RECSIZECOUNT] = (WORD)0; 
   return 1;
}

//
// setDMATerm -- set the interrupt interval (in sample frames)
//
// Parameters:
//   terminator - interrupt frequency in samples.
//
// Notes:
//   You will get an interrupt at multiples of "terminator".  For
//   example, if "terminator" is 4, you will get an interrupt every 
//   fourth sample frame.
//
#pragma code_seg()
void CAdapterSolo::icemtPlaySetDMATerm(  DWORD terminator )
{
   terminator *= 10;

   if ((terminator >> 18) != 0)
      goto bail;
   
   // convert to dwords, subtract 1, and write to MT16, which
   // is the "terminating count" or something.
   _mtRange[MTO_PLAYTERM] = (WORD)(terminator - 1);

bail:
   return;
}

#pragma code_seg()
void CAdapterSolo::icemtRecSetDMATerm(  DWORD terminator )
{
   terminator *= 12;

   if ((terminator >> 18) != 0)
      goto bail;
   
   // convert to dwords, subtract 1, and write to MT16, which
   // is the "terminating count" or something.
   _mtRange[MTO_RECTERM] = (WORD)(terminator - 1);

bail:
   return;
}

#pragma code_seg()

//
// icemtIrqEnable/Disable -- en/disable the Multi-Track IRQ
//
// Note:
//   this enables both the record and playback IRQ's.  Hmmm.
//
void CAdapterSolo::icemtPlayIrqEnable(  )
{
   // Clear the play enable bit from the irq mask
   _mtIrqMask &= ~MTO_IRQPLAYEN;
   _mtRange[MTO_IRQMASK] = (BYTE)_mtIrqMask;
}

void CAdapterSolo::icemtRecIrqEnable(  )
{
   // Clear the play enable bit from the irq mask
   _mtIrqMask &= ~MTO_IRQRECEN;
   _mtRange[MTO_IRQMASK] = (BYTE)_mtIrqMask;
}

void CAdapterSolo::icemtPlayIrqDisable(  )
{
   _mtIrqMask |= MTO_IRQPLAYEN;
   _mtRange[MTO_IRQMASK] = (BYTE)_mtIrqMask;
}

void CAdapterSolo::icemtRecIrqDisable(  )
{
   _mtIrqMask |= MTO_IRQRECEN;
   _mtRange[MTO_IRQMASK] = (BYTE)_mtIrqMask;
}


//
// *IrqAsserted - see if irq is asserted
//
// Returns:
//   0 if the irq is not happening, 1 otherwise
//
DWORD CAdapterSolo::icemtPlayIrqAsserted(  )
{
   BYTE status = (BYTE)_mtRange[MTO_IRQMASK];

   if (status & MTO_IRQPLAYBIT)
      return 1;

   return 0;
}

DWORD CAdapterSolo::icemtRecIrqAsserted( )
{
   BYTE status = (BYTE)_mtRange[MTO_IRQMASK]; 

   if (status & MTO_IRQRECBIT)
      return 1;

   return 0;
}

//
// ICEtimerStart - start the interval timer:
//
// Parameters:
//   timeout - interval using the, er, 500kHz clock or something?
//
void CAdapterSolo::icetimerStart( WORD timeout )
{
   _ccsRange[CCS_TIMER] = (WORD)((1 << 15) | timeout);
}

void CAdapterSolo::icetimerIrqEnable(  )
{
   // Clear the timer irq bit so we can interrupt away!
   _ccsIrqMask &= ~(ICE_IRQTIMERBIT);
   _setHwCcsIrqMask();
}

void CAdapterSolo::icetimerIrqDisable(  )
{   
   // Block those darn timer IRQ's
   _ccsIrqMask |= ICE_IRQTIMERBIT;
   _setHwCcsIrqMask();
}


void CAdapterSolo::icemtPlayStart(  )
{
   BYTE playStatus = (BYTE)_mtRange[MTO_PLAYCONTROL];

   playStatus |= 0x01;
   _mtRange[MTO_PLAYCONTROL] = (BYTE)playStatus;
}

void CAdapterSolo::icemtPlayStop(  )
{
   BYTE playStatus = (BYTE)_mtRange[MTO_PLAYCONTROL];

   playStatus &= ~0x01;
   _mtRange[MTO_PLAYCONTROL] = (BYTE)playStatus ;
}

void CAdapterSolo::icemtPlayPause(  )
{
   BYTE playStatus = (BYTE)_mtRange[MTO_PLAYCONTROL];

   playStatus |= 0x02;
   _mtRange[MTO_PLAYCONTROL] = (BYTE)playStatus;
}

void CAdapterSolo::icemtPlayUnpause(  )
{
   BYTE playStatus = (BYTE)_mtRange[MTO_PLAYCONTROL];

   playStatus &= ~0x02;
   _mtRange[MTO_PLAYCONTROL] = (BYTE)playStatus;
}

void CAdapterSolo::icemtRecStart(  )
{
   BYTE recStatus = (BYTE)_mtRange[MTO_RECCONTROL];

   recStatus |= 0x01;
   _mtRange[MTO_RECCONTROL] = (BYTE)recStatus;
}

void CAdapterSolo::icemtRecStop(  )
{
   BYTE recStatus = (BYTE)_mtRange[MTO_RECCONTROL];

   recStatus &= ~0x01;
   _mtRange[MTO_RECCONTROL] = (BYTE)recStatus;
}

void CAdapterSolo::icemtRecPause(  )
{
   BYTE recStatus = (BYTE)_mtRange[MTO_RECCONTROL];

   recStatus |= 0x02;
   _mtRange[MTO_RECCONTROL] = (BYTE)recStatus;
}

void CAdapterSolo::icemtRecUnpause(  )
{
   BYTE recStatus = (BYTE)_mtRange[MTO_RECCONTROL];

   recStatus &= ~0x02;
   _mtRange[MTO_RECCONTROL] = (BYTE)recStatus;
}

//
// iceMidiOneIrqEnable -- enable midi irq's
//
void  CAdapterSolo::iceMidiOneIrqEnable(  )
{
   // Clear the irq bit and interrupt away!
   
   _ccsIrqMask &= ~(ICE_IRQMIDIONEBIT);
   _setHwCcsIrqMask();
}

//
// iceMidiOneIrqDisable -- disable...
//
void  CAdapterSolo::iceMidiOneIrqDisable(  )
{
   // Clear the irq bit and interrupt away!
   
   _ccsIrqMask |= ICE_IRQMIDIONEBIT;
   _setHwCcsIrqMask();
}

//
// iceMidiTwoIrqEnable -- enable midi irq's
//
void  CAdapterSolo::iceMidiTwoIrqEnable(  )
{
   // Clear the irq bit and interrupt away!
   
   _ccsIrqMask &= ~(ICE_IRQMIDITWOBIT);
   _setHwCcsIrqMask();
}

//
// iceMidiTwoIrqDisable -- disable...
//
void  CAdapterSolo::iceMidiTwoIrqDisable(  )
{
   // Clear the irq bit and interrupt away!
   
   _ccsIrqMask |= ICE_IRQMIDITWOBIT;
   _setHwCcsIrqMask();
}


// static void _checkByte(  BYTE byte );

//
// -- checks for an increasing sequence of note-on or note-off's
//
// void _checkByte(  BYTE byte )
// {
//     static BYTE _lastNote = 0;

//     // We expect only note-ons and only increasing note values:
//     if (byte & 0x80)            // status bit?
//     {
//         if (byte != 0x90)
//             VASSERT( 0, "not a note on" );       // stop!
        
//         ++ _lastNote;               // find next in sequence
//         if (127 == _lastNote)
//             _lastNote = 1;

//         return;                 // ** Quick Exit **
//     }

//     // compare note value and velocity (should be the same)
//     if (byte != _lastNote)      // following the sequence?
//     {
//         VASSERT( 0, "out of sequence byte" );
        
//         _lastNote = byte;       // sync back up to this note?
//     }
// }

//
// iceMidiOneCommandOut -- output an MPU401 command byte to midi port 1
//
// Parameters:
//   aByte - some data
//
// Returns:
//   0 if the byte was output
//   1 if the mpu401 data port is not ready
//
int   CAdapterSolo::iceMidiOneCommandOut(  BYTE aByte )
{
   BYTE status;

   status = (BYTE)_ccsRange[CCS_UART_1_CMD];
   if (status & ICE_DRR_BIT)    // If DRR *is* set
      return 1;                 // the uart is not ready!

   _ccsRange[CCS_UART_1_CMD] = (BYTE)aByte; // write byte to the data register
   return 0;
}

//
// iceMidiTwoCommandOut -- output an MPU401 command byte to midi port 1
//
// Parameters:
//   aByte - some data
//
// Returns:
//   0 if the byte was output
//   1 if the mpu401 data port is not ready
//
int   CAdapterSolo::iceMidiTwoCommandOut(  BYTE aByte )
{
   BYTE status;

   status = (BYTE)_ccsRange[CCS_UART_2_CMD];
   if (status & ICE_DRR_BIT)    // If DRR *is* set
      return 1;                 // the uart is not ready!

   // write byte to the data register
   _ccsRange[CCS_UART_2_CMD] = (BYTE)aByte; 
   return 0;
}

//
// icemtSPDIFSlaveOn -- slave internal clock to S/PDIF in
//
// Pretty obvious what it does, isn't it?
//
DWORD CAdapterSolo::icemtSPDIFSlaveOn()
{
   BYTE status;

   // Must do a read/modify/write because the SPDIF override bit is
   // nestled into the same byte as the current sample rate.  Yuck.
   
   status = (BYTE)_mtRange[MTO_SAMPLERATE];
   status |= MTSR_SPDIF_SLAVE_BIT;
   _mtRange[MTO_SAMPLERATE] = (BYTE)status;
   
   return 0;
}

//
// icemtSPDIFSlaveOff --
//
DWORD CAdapterSolo::icemtSPDIFSlaveOff()
{
   BYTE status;

   // Must do a read/modify/write because the SPDIF override bit is
   // nestled into the same byte as the current sample rate.  Yuck.
   
   status = (BYTE)_mtRange[MTO_SAMPLERATE];
   status &= (~ MTSR_SPDIF_SLAVE_BIT);
   _mtRange[MTO_SAMPLERATE] = (BYTE)status;
   
   return 0;
}

void  CAdapterSolo::iceSetRoute(  int route )
{
    VASSERT( route >= 0 && route < ICEROUTE_INVALID, "bad parameter" );

    _loopbackMode = route;
    _setRoute();
}

int CAdapterSolo::iceGetRoute(  )
{
    return _loopbackMode;
}

//
// _setRoute -- choose an appropriate routing scheme based on
//   the current state of the card.
//
// For example, if loopback is on, we ignore the SPDIF clone
// state.
//
DWORD CAdapterSolo::_setRoute()
{
   int i;
    
   // Default: turn the volume off on all channels in the dig mixer
   // (there are 20 streams...)
   for (i = 0; i < 20; i++)
   {
       _mtRange[MTO_VOLINDEX]     = (BYTE)i;
       _mtRange[MTO_VOLDATALEFT]  = (BYTE)0x7f; // mute
       _mtRange[MTO_VOLDATARIGHT] = (BYTE)0x7f; // mute
   }

   // This code is to loop 0&1-in to 0&1-out for testing and monitoring.
   // (This is weird.  You enable loopback from a PSD input in
   // one register, then you choose which input in another reg.)
   // OUTW( MTO_PSDOUTROUTE(*card), 0x0202 ); // set loopback from PSDIN
   // OUTW( MTO_PSDOUTSRC(*card), 0x00000010 ); // left to left, right to right

   switch (_loopbackMode)
   {
   default:                     // oops!
       VASSERT( 0, "invalid loopback mode!" );
       
       // fall through to normal for non-debug mode
       
   case ICEROUTE_NORMAL:
       // Route channels 0/1 straight out from DMA
       _mtRange[MTO_PSDOUTROUTE] = (WORD)0x0000;
      
       // Route SPDIF out from DMA 8/9
       _mtRange[MTO_SPDOUTROUTE] = (WORD)0x0000;
       break;
       
   case ICEROUTE_CLONE:             // not implemented
       // Route channels 0/1 straight out from DMA
       _mtRange[MTO_PSDOUTROUTE] = (WORD)0x0000;
      
       // Route SPDIF out from the monitor mixer
       _mtRange[MTO_SPDOUTROUTE] = (WORD)0x0005;

       // Turn only the analog output up in the monitor
       _mtRange[MTO_VOLINDEX]     = (BYTE)0; // analog out 0
       _mtRange[MTO_VOLDATALEFT]  = (BYTE)0x00; // -0dB
       _mtRange[MTO_VOLINDEX]     = (BYTE)1; // analog out 1
       _mtRange[MTO_VOLDATARIGHT] = (BYTE)0x00; // -0dB
       
       break;

   case ICEROUTE_CONVERTER:     // converter or "passthrough" mode
       // TRS: We convert SPDIF in into analog and vice versa, so the
       //      SOLO will work as an A/D and D/A standalone.
      
       // Route S/PDIF (8&9) to channels 0&1
       _mtRange[MTO_PSDOUTROUTE] = (WORD)0x0303; // set loopback out from S/PDIF in
       _mtRange[MTO_PSDOUTSRC] = (WORD)0x00000090; // left to left, right to right

       // Loop inputs 0&1 into 8&9 (input to S/PDIF out)
       _mtRange[MTO_SPDOUTROUTE] = (WORD)0x100a;
       break;

   case ICEROUTE_SPDIFMIX:
       // Route output 0/1, and S/PDIF input to the digital mixer
       // and route the digital mixer to the converters (0/1).

       // Route SPDIF out from DMA 8/9
       _mtRange[MTO_SPDOUTROUTE] = (WORD)0x0000 ;
       
       // route analog output from the digital mixer
       _mtRange[MTO_PSDOUTROUTE] = (WORD)0x0101; 

       // Mix the analog out and the spdif in into the output.
       _mtRange[MTO_VOLINDEX] = (BYTE)0; // analog out 0
       _mtRange[MTO_VOLDATALEFT] = (BYTE)0x00; // -0dB
       _mtRange[MTO_VOLINDEX] = (BYTE)1; // analog out 1
       _mtRange[MTO_VOLDATARIGHT] = (BYTE)0x00; // -0dB

       _mtRange[MTO_VOLINDEX] = (BYTE)0x12; // spdif in left
       _mtRange[MTO_VOLDATALEFT] = (BYTE)0x00; // -0dB
       _mtRange[MTO_VOLINDEX] = (BYTE)0x13; // spdif in right
       _mtRange[MTO_VOLDATARIGHT] = (BYTE)0x00; // -0dB

       break;

   case ICEROUTE_SPDIFMIX_CLONE:
       // exactly like SPDIFMIX except that the s/pdif out also gets
       // a copy of the digital mixer's output.

       // Route output 0/1, and S/PDIF input to the digital mixer
       // and route the digital mixer to the converters (0/1).

       // route analog output from the digital mixer
       _mtRange[MTO_PSDOUTROUTE] = (WORD)0x0101; 

       // Route SPDIF out from the monitor mixer
       _mtRange[MTO_SPDOUTROUTE] = (WORD)0x0005;
       
       // Mix the analog out and the spdif in into the output.
       _mtRange[MTO_VOLINDEX] = (BYTE)0; // analog out 0
       _mtRange[MTO_VOLDATALEFT] = (BYTE)0x00; // -0dB
       _mtRange[MTO_VOLINDEX] = (BYTE)1; // analog out 1
       _mtRange[MTO_VOLDATARIGHT] = (BYTE)0x00; // -0dB

       _mtRange[MTO_VOLINDEX] = (BYTE)0x12; // spdif in left
       _mtRange[MTO_VOLDATALEFT] = (BYTE)0x00; // -0dB
       _mtRange[MTO_VOLINDEX] = (BYTE)0x13; // spdif in right
       _mtRange[MTO_VOLDATARIGHT] = (BYTE)0x00; // -0dB

       break;
   }

   return 0;
}

//
// iceGpioSetDirection -- set the "direction" bits for the GPIO port.
//
// The high bit corresponds to the line GPIO[7], etcetera.  Write 1
// for output, 0 for input.
//
// Notes:
//   You really should know what this hardware is attached to before
//   you attempt to diddle the GPIO lines.  However, you can set
//   the Direction based on what's in the boot ROM.
//
void  CAdapterSolo::iceGpioSetDirection(  BYTE mask )
{
   // I guess we can assume that we want inputs to be read-only,
   // and outputs to be writable
   CCI_Write( CCI_GPIO_WRITEMASK, (BYTE)~mask );
    
   CCI_Write( CCI_GPIO_DIRECTION, mask );
}

//
// iceEepromRead
//
// Parameters:
//   offset - offset within EEPROm to read
//
// Returns:
//   0 on success
//
DWORD CAdapterSolo::iceEepromRead(  BYTE offset, BYTE * value_ret )
{
    // write eeprom offset.
    _ccsRange[CCS_I2C_ADDR] = (BYTE)offset;

    // Start read cycle by writing EEPROM device addr and 
    // read-enable bit.
    _ccsRange[CCS_I2C_DEV] = (BYTE)0xa0;
    
    // Wait until the eeprom thinks it's finished, but not for
    // more than a few milliseconds.
    ULONGLONG startTime = PcGetTimeInterval(0);
    while ((BYTE)_ccsRange[CCS_I2C_CTRL] & 1)
    {
		if (PcGetTimeInterval(startTime) >= GTI_MILLISECONDS(200))
        {
            return 0;           // timed out.
        }
    }

    // read in the data byte (data port at _iceBase + 0x12)
    *value_ret = (BYTE)_ccsRange[CCS_I2C_DATA];
    return 0;                   // success
}

//
// iceGetCardType --
//
// Returns:
//   version
//
void CAdapterSolo::iceGetCardType( int * type,
							        int * version )
{
    SSS_EEPROM soloEepromData;
    //BYTE       aByte;
    int        i;

    // Read the solo data into this struct...as an array (the struct
    // is known to be BYTE-packed).
    for (i = 0; i < sizeof(soloEepromData); i++)
    {
        if (0 != iceEepromRead( (BYTE)(i + SSS_EEPROM_OFFSET),
                                &((BYTE *)&soloEepromData)[i] ))
        {
            // Eeprom read failed. Aaaaugh!
            VASSERT( 0, "Can't read eeprom, ewww" );
            *type = 1;
            *version = 1;
            return;
        }
    }

    // Special case: original cards were uninitialized
    if (soloEepromData.cookie != SSS_EEPROM_COOKIE)
    {
        *type = 1;
        *version = 1;
        return;
    }

    *type = soloEepromData.type;
    if (*type < SSS_PCICARDTYPE_DIO)
        *version = 1;           // old eeprom didn't have version
    else
        *version = soloEepromData.version;
}


//
// _findAttachedDevices -- what things are attached to the
//   solo PCI card
//
// Since the midi miniport is not attached to the card at this
// point, we have to do low-level MIDI I/O stuff on our own 
// OR init the midi miniport separately before we know what's
// attached...
//
// Returns:
//   0 on success
//   nonzero on future versions
//
// Notes:
//   This assumes that interrupts are off and is not reentrant. It should
//   be called after the uarts are initialized.
//
#ifdef QQQ // comment out for now
int _findAttachedDevices( SoloInstance * solo )
{
    BYTE productID;
    
    iceGetCardType(&solo->iceCookie,
                   &solo->cardType,
                   &solo->cardVersion);

    switch (solo->cardType)
    {
    case SSS_PCICARDTYPE_SOLO:  // If this is version 1, it's just a solo.
        solo->attachedDevs = SSS_ATTACHEDDEV_SOLO;
        break;        

    case SSS_PCICARDTYPE_SOLOEX:
    case SSS_PCICARDTYPE_SOLOIST:

        solo->attachedDevs = 0; // assume nothing is there
        
        // If this is version 2, toggle the "UART 2 in" switch and
        // ask what's out there.
        MCU_listenToSolo(solo);
        if (0 != MCU_sendAttention(solo))
            return 1;           // MIDI error

        // If something responds...
        if (0 == MCU_getImHere(solo, &productID))
        {        
            switch (productID)  // Is it a solo?
            {
            case MCU_PID_SOLO:
				// If solo version >= 1.2, send level data,
                solo->attachedDevs |= SSS_ATTACHEDDEV_SOLO;
				break;

				// If soloist (iota) attached, send level data.
            case MCU_PID_IOTA:
                solo->attachedDevs |= SSS_ATTACHEDDEV_SOLO;
				solo->miscFlags |= SSS_MF_SENDMETERDATA;
                break;
                
            case 0:                 // nothing is attached. This is legal.
                break;
                
            default:
                // Wrong device is on this connector. Bitch!
                return 1;
            }
        }
        
        // -- Now select the header (a8 connector) and see what's out
        //    there.
        
        MCU_listenToExpander(solo);
        if (0 != MCU_sendAttention(solo))
            return 1;           // MIDI error

        if (0 == MCU_getImHere(solo, &productID))
        {
            switch (productID) // something is there. Is it a solo?
            {
            case MCU_PID_EXPANDER:
                solo->attachedDevs |= SSS_ATTACHEDDEV_EXPANDER;
				solo->miscFlags |= SSS_MF_SENDMETERDATA;
                break;
                
            case 0:                 // nothing is attached. This is legal.
                break;
                
            default:
                // Wrong device is on this connector. Bitch!
                return 1;
            }
        }

        // If nothing is attached, assume there's a classic Solo
        // because it doesn't respond. :(
        if (solo->attachedDevs == 0)
            solo->attachedDevs = SSS_ATTACHEDDEV_SOLO;

        // In any case, make sure the Solo is connected by default,
        // because the A8 never sends unsolicited input. :)
        MCU_listenToSolo(solo);
        
        break;

    case SSS_PCICARDTYPE_DIO:
        solo->attachedDevs = ( SSS_ATTACHEDDEV_SOLO |
                               SSS_ATTACHEDDEV_EXPANDER );
        break;
        
    default:
        // If this is a higher version, bail!
        return 1;
    }

    // TRS: this is TEST CODE TEST CODE! HACK ALERT!
//     {
//         BYTE buffer[100];
//         MCU_listenToExpander(solo);
        
//         VASSERT(0, "are you ready?" );
//         MCU_sendLevelSwitchRequest(solo);
//         while (MCU_getAnything(solo, buffer, 100) > 0)
//             /* do nothing */;
        
//         MCU_setInputGain( solo, 0, 40 );
//         MCU_setInputGain( solo, 1, 40 );
//         MCU_setInputGain( solo, 2, 20 );
//         MCU_setInputGain( solo, 3, 20 );
//         MCU_setInputAttenuation( solo, 4, 80 );
//         MCU_setInputAttenuation( solo, 5, 80 );
//         MCU_setInputAttenuation( solo, 6, 40 );
//         MCU_setInputAttenuation( solo, 7, 40 );
        
//     }
    
    MCU_listenToSolo(solo);
    return 0;                   // done
}
#endif // QQQ


// Confusing NuMega documentation to the contrary, 
// IRP_MN_QUERY_RESOURCE_REQUIREMENTS should be handled by the PDO. --trs
#ifdef QQQ // 
//
// OnQueryResourceRequirements -- Called by the PnP manager to see 
//   if we need any special resources
//
// Try to allocate a block of memory to use for DMA.
//
NTSTATUS CAdapterSolo::OnQueryResourceRequirements(KIrp I)
{
   PIO_RESOURCE_REQUIREMENTS_LIST requirements = 0;

DbgBreakPoint(); // walk through me

   // Try to allocate memory for the incredibly ugly structure used here
   requirements = (PIO_RESOURCE_REQUIREMENTS_LIST)ExAllocatePoolWithTag
      ( PagedPool, 
        sizeof(IO_RESOURCE_REQUIREMENTS_LIST),
        'oloS' );

   if (!requirements)
      goto bail;

   // Copy static data
   *requirements = _bufferRequirements;

   I.Information() = (ULONG)requirements;
   return STATUS_SUCCESS;

bail:
   I.Information() = 0;
   return STATUS_INSUFFICIENT_RESOURCES;
}
#endif // QQQ

#ifdef QQQ // don't need this any more
//
// OnFilterResourceRequirements -- add driver-specific requirements
//  to those indicated by the PCI bus.
//
NTSTATUS CAdapterSolo::OnFilterResourceRequirements(KIrp I)
{
DbgBreakPoint(); // Walk through me.
   NTSTATUS status = STATUS_SUCCESS;
   PIO_STACK_LOCATION stack = I.CurrentStackLocation();

   // This is the list of resource requirements detected by the
   // hardware. (I guess from the PCI bus or whatever)
   PIO_RESOURCE_REQUIREMENTS_LIST orig = 0;

   // This is the original list of requirements sent by higher
   // level filter drivers.
   PIO_RESOURCE_REQUIREMENTS_LIST filtered = 0;

   // Of the two, this is the one we will copy and modify
   PIO_RESOURCE_REQUIREMENTS_LIST source = 0;
   PIO_RESOURCE_REQUIREMENTS_LIST newList = 0;

   ULONG listSize = 0; // size of resource requirements list (bytes)
   ULONG nAddedRequirements = 1; // how many we will add (one memory buffer)
   PIO_RESOURCE_DESCRIPTOR resource; // pointer to appended resources

   // Now pass the irp on down the stack and wait for it to complete
   // But don't allow lowerDevice to actually complete the Irp:

   // Assuming we've made sure there's an extra Irp stack location,
   // we can wait for the lower device to complete, then we modify
   // the information field and complete this irp ourselves.
   //
   // If you have a copy of Walter Oney's book, note that setting the
   // second parameter to TRUE is equivalent to calling ForwardAndWait
   // to let the lower device complete without completing the Irp. --trs
   // 
   I.CopyParametersDown( TRUE ); // prepare next stack location
   status = _Lower.CallWaitComplete
      ( 
       I, 
       TRUE,   // wait for completion but don't complete
       NULL    // don't need copy if information
      );
       
   if (!NT_SUCCESS(status))
      goto bail;

   // OK, now we have the resource requirements for the PCI device.
   // Add other requirements like the DMA buffer.

   orig = stack->Parameters.FilterResourceRequirements.IoResourceRequirementList;
   filtered = (PIO_RESOURCE_REQUIREMENTS_LIST) I.Information();

   if (filtered)
      source = filtered;
   else
      source = orig;

   // We can't deal with multiple alternatives yet, although we should
   // because the PCI hardware could theoretically present such a 
   // situation. Unlikely, but it's still a TODO:.
   if (source->AlternativeLists != 1)
   {
      t << "[CAdapterSolo::OnFilterResourceRequirements] Should support alternative"
        << " resource lists\n";

      ASSERT(0); // trap in checked build only
      status = STATUS_UNSUCCESSFUL;
      goto completeAndBail;
   }

   // Allocate a new list so we can copy the old requirements and append
   // ours. How wasteful. :)
   listSize = source->ListSize;
   newList = (PIO_RESOURCE_REQUIREMENTS_LIST)ExAllocatePoolWithTag
         ( 
          PagedPool,
          listSize + (nAddedRequirements * sizeof(IO_RESOURCE_DESCRIPTOR)),
          'oloS'
         );

   if (!newList) // out of memory
   {
      status = STATUS_UNSUCCESSFUL;
      goto completeAndBail;
   }

   // -- Copy old requirements to our buffer --

   RtlCopyMemory( newList, source, listSize );
   newList->ListSize += (nAddedRequirements * sizeof(IO_RESOURCE_DESCRIPTOR));

   // -- 
   // -- Here is where we actually append new resources. 
   // -- 

   // Fill in the appended resource values, and increment the list size

   // One: allocate some low, contiguous memory for the DMA buffer.
   //      This is the size of both play and record buffers and can
   //      only be changed at boot time. (Values from the registry)
   ASSERT(_dmaPageFrames > 0);

   resource = &newList->List[0].Descriptors[newList->List[0].Count];
   ++ newList->List[0].Count;

   // TRS: question: does the allocated resource have the same offset
   //      as the list index? Hmmm.

   RtlZeroMemory(resource, sizeof(IO_RESOURCE_DESCRIPTOR));
   resource->Type = CmResourceTypeMemory;
   resource->ShareDisposition = CmResourceShareShared; // CmResourceShareDeviceExclusive;
   resource->Flags = CM_RESOURCE_MEMORY_READ_WRITE;
   resource->u.Memory.Length = 1024; // bytes? Pages?
   resource->u.Memory.Alignment = 0x0100L;         // dword aligned
   resource->u.Memory.MinimumAddress.LowPart = 0;  // anywhere
   resource->u.Memory.MinimumAddress.HighPart = 0; // anywhere
   resource->u.Memory.MaximumAddress.LowPart = 0x0fffffff; // below 256M on bus
   resource->u.Memory.MaximumAddress.HighPart = 0;

   // Save our new list as the new one to pass down the device
   // stack, and deallocate the old one (if it existed).

   I.Information() = (ULONG)newList;
   if (filtered) // old list was in Information?
      ExFreePool(filtered);


completeAndBail:
   // goto here if we've already passed the Irp to our lower device and
   // need to complete the irp before returning...

   // The Irp is not yet complete, even though the lower device is finished
   // with it. Mark it done asynchronously...
   return I.PnpComplete(this, status, IO_NO_INCREMENT);


bail:
   // Jump here if we failed and haven't yet passed the irp to our
   // lower device. 
   return DefaultPnp(I);
}
#endif // QQQ
