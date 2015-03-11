   // $Id: minwave.cpp,v 1.21 2001/11/25 01:03:13 tekhedd Exp $
// Copyright (C) 2001 Tom Surace. All rights reserved.
//
// Original sample code also carries microsoft's copyright:
// Copyright (c) 1997-2000 Microsoft Corporation.  All rights reserved.
//

#include <vdw.h>
#include "..\SolowdmDeviceInterface.h"

// Include definitions for DWORD and other stupid macros
#include <windef.h>
#include <portcls.h>		// PcGetTimeInterval() 

#include "adapter.h"    // for 't'
#include "config.h"
#include "mintopo.h"       // PropertyHandler_NoCpuResources
#include "minwave.h"       // CWaveCyclicStreamSolo and friends

//
// Local functions
//

static NTSTATUS PropertyHandler_DevSpecific
(
    IN PPCPROPERTY_REQUEST   PropertyRequest
);

#pragma hdrstop("minwave.pch")

//
// CreateMiniportWaveCyclicSolo()
//
// Creates a cyclic wave miniport object for the Solo adapter, and ties
// it to a specific SolowdmDevice. It was a pain unrolling the 
// inappropriat macros from the sample code. --trs
//
#pragma code_seg("PAGE")
NTSTATUS CreateMiniportWaveCyclicSolo
(
    OUT PUNKNOWN     *  ppUnknown,
    IN  PUNKNOWN        pUnknownOuter  OPTIONAL,
    IN  POOL_TYPE       PoolType
)
{
    PAGED_CODE();

    ASSERT(ppUnknown);

    CMiniportWaveCyclicSolo *p = 
       new(PoolType) CMiniportWaveCyclicSolo(pUnknownOuter);

    if (!p)  // New can fail? I'll never get used to this.
       return STATUS_INSUFFICIENT_RESOURCES;                       

    NTSTATUS ntStatus = p->QueryInterface( IID_IUnknown,
                                           (PVOID *)ppUnknown );
       
    // Not necessary if using QueryInterface
    // (*ppUnknown)->AddRef();                                         

    if (!NT_SUCCESS(ntStatus))
    {
       ASSERT(0); // bad: not derived from IUnknown? 
       return STATUS_UNSUCCESSFUL;
    }


    return STATUS_SUCCESS;                                      
}
#pragma code_seg()


/*****************************************************************************
 * CMiniportWaveCyclicSolo::NonDelegatingQueryInterface()
 *****************************************************************************
 * Obtains an interface.  This function works just like a COM QueryInterface
 * call and is used if the object is not being aggregated.
 */
#pragma code_seg("PAGE")
STDMETHODIMP CMiniportWaveCyclicSolo::NonDelegatingQueryInterface
(
    IN      REFIID  Interface,
    OUT     PVOID * Object
)
{
    PAGED_CODE();

    ASSERT(Object);

    t << "[CMiniportWaveCyclicSolo::NonDelegatingQueryInterface]\n";

    if (IsEqualGUIDAligned(Interface,IID_IUnknown))
    {
        *Object = PVOID(PUNKNOWN(this));
    }
    else if (IsEqualGUIDAligned(Interface,IID_IMiniport))
    {
        *Object = PVOID(PMINIPORT(this));
    }
    else if (IsEqualGUIDAligned(Interface,IID_IMiniportWaveCyclic))
    {
        *Object = PVOID(PMINIPORTWAVECYCLIC(this));
    }
    else
    {
        DbgBreakPoint();

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
#pragma code_seg()

/*****************************************************************************
 * CMiniportWaveCyclicSolo::~CMiniportWaveCyclicSolo()
 *****************************************************************************
 * Destructor.
 */
#pragma code_seg("PAGE")
CMiniportWaveCyclicSolo::~CMiniportWaveCyclicSolo( void )
{
   PAGED_CODE();

   t << "[CMiniportWaveCyclicSolo::~CMiniportWaveCyclicSolo]\n";

   CleanUp();
}
#pragma code_seg()

//
// CMiniportWaveCyclicSolo::Init() -- standard miniport Init function (virtual)
//
// Initializes the miniport. Well, actually the "Port" calls this and
// assumes that this is all that's needed, but the real initialization 
// occurs outside this function.
//
#pragma code_seg("PAGE")
STDMETHODIMP CMiniportWaveCyclicSolo::Init
(
    IN      PUNKNOWN        UnknownAdapter,
    IN      PRESOURCELIST   ResourceList,
    IN      PPORTWAVECYCLIC Port_
)
{
   NTSTATUS ntStatus;

   PAGED_CODE();

   ASSERT(UnknownAdapter); // not used
   ASSERT(ResourceList);
   ASSERT(Port_);

   t << "[MiniportWaveCyclicSolo::Init]\n";

   // These have to be initialized to zero or the call to
   // Invalidate() could crash and thus invalidate the whole OS. :)
   _allocatedCapture = 0;
   _allocatedRender = 0;
   _portWave = 0;
   _adapter = 0;

   // try to set everything up

   ntStatus = UnknownAdapter->QueryInterface( IID_IAdapterSolo,
                                              (PVOID *)&_adapter );

   if (!NT_SUCCESS(ntStatus))
      goto bail;

   //
   // AddRef() is required because we are keeping this pointer.
   //
   _portWave = Port_;
   _portWave->AddRef();

   return ntStatus;

bail:
   //
   // clean up our mess
   //
   CleanUp();
   return ntStatus;
}
#pragma code_seg()




//
// CleanUp - Helper function to release memory on destruction or
//   failure
//
#pragma code_seg() // destructor helper
void CMiniportWaveCyclicSolo::CleanUp()
{
   t << "[CMiniportWaveCyclicSolo::CleanUp]\n";

   if (_adapter)
   {
      _adapter->Release();
      _adapter = NULL;
   }

   // release the port
   if (_portWave)
   {
      _portWave->Release();
      _portWave = NULL;
   }

}


#pragma data_seg()

// 
// The range of formats supported for a pin. Note that
// we support one pin with 12 channels out and 10 channels
// in, 24 bits packed towards the high bit. (8 LSB unused)
//
// The formats are documented in ksmedia.h, right? --trs
//
// If we accept WAVEFORMATEX we can also expect to maybe see
// WAVEFORMATEXTENSIBLE
//
#define N_DATA_RANGES_STREAM (1)
static KSDATARANGE_AUDIO PinDataRangesStreamIn[N_DATA_RANGES_STREAM] =
{
#ifdef QQQ // comment out
   { // Legacy non-extensible mode
        {
            sizeof(KSDATARANGE_AUDIO),
            0, // ignored
            0, // ignored
            0, // reserved (must be 0)
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        2,      // Max number of channels.
        16,     // Minimum number of bits per sample.
        16,     // Maximum number of bits per channel.
        9600,   // Minimum rate.
        96000   // Maximum rate.
    },
#endif // QQQ
    { // General purpose multichannel stuff
        {
            sizeof(KSDATARANGE_AUDIO),
            0, // ignored
            0, // ignored
            0, // reserved (must be 0)
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        12,      // Max number of channels.
        32,     // Minimum number of bits per sample.
        32,     // Maximum number of bits per channel.
        9600,   // Minimum rate.
        96000   // Maximum rate.
    }
};

static KSDATARANGE_AUDIO PinDataRangesStreamOut[N_DATA_RANGES_STREAM] =
{
#ifdef QQQ // comment out
   { // Legacy non-extensible mode
        {
            sizeof(KSDATARANGE_AUDIO),
            0, // ignored
            0, // ignored
            0, // reserved (must be 0)
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        2,      // Max number of channels.
        16,     // Minimum number of bits per sample.
        16,     // Maximum number of bits per channel.
        9600,   // Minimum rate.
        96000   // Maximum rate.
    },
#endif // QQQ
   {
        {
            sizeof(KSDATARANGE_AUDIO),
            0, // ignored
            0, // ignored
            0, // reserved (must be 0)
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        10,      // Max number of channels.
        32,      // Minimum number of bits per sample.
        32,      // Maximum number of bits per channel.
        9600,    // Minimum rate.
        96000    // Maximum rate.
    }
};


/*****************************************************************************
 * PinDataRangePointersStream
 *****************************************************************************
 * List of pointers to structures indicating range of valid format values
 * for streaming pins.
 * 
 * TRS: Note the wide range of one format supported.
 */
#define N_DATA_RANGE_POINTERS_STREAM (1)
static PKSDATARANGE PinDataRangePointersStreamIn[N_DATA_RANGE_POINTERS_STREAM] =
{
    PKSDATARANGE(&PinDataRangesStreamIn[0])
};

static PKSDATARANGE PinDataRangePointersStreamOut[N_DATA_RANGE_POINTERS_STREAM] =
{
    PKSDATARANGE(&PinDataRangesStreamOut[0])
};

/*****************************************************************************
 * PinDataRangesBridge
 *****************************************************************************
 * Structures indicating range of valid format values for bridge pins.
 * TRS: it's conceptual, so generic "audio" is fine.
 */
#define N_DATA_RANGES_BRIDGE (1)
static
KSDATARANGE PinDataRangesBridge[N_DATA_RANGES_BRIDGE] =
{
   {
      sizeof(KSDATARANGE),
      0, 
      0,
      0,
      STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
      STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
      STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE)
   }
};

/*****************************************************************************
 * PinDataRangePointersBridge
 *****************************************************************************
 * List of pointers to structures indicating range of valid format values
 * for bridge pins.
 */
#define N_DATA_RANGE_POINTERS_BRIDGE (1)
static
PKSDATARANGE PinDataRangePointersBridge[N_DATA_RANGE_POINTERS_BRIDGE] =
{
    &PinDataRangesBridge[0]
};

/*****************************************************************************
 * MiniportPins
 *****************************************************************************
 * List of pins.
 *
 * !! If you add or remove pins !! 
 * Be sure to update the enum of pin names. :)
 */
static PCPIN_DESCRIPTOR MiniportPins[CMiniportWaveCyclicSolo::N_MINIPORT_PINS] =
{
     // Wave In Streaming Pin (Capture)
    // PIN_CAPTURE_OUT 
    {
        1,1,0,  // instances: max global/max filter/min
        NULL,
        {
            0,
            NULL,
            0,
            NULL,
            N_DATA_RANGE_POINTERS_STREAM,
            PinDataRangePointersStreamIn,
            KSPIN_DATAFLOW_OUT,
            KSPIN_COMMUNICATION_SINK,
            (GUID *) &PINNAME_CAPTURE,
            &KSAUDFNAME_RECORDING_CONTROL,  // this name shows up as the recording panel name in SoundVol.
            0
        }
    },
    // Wave In Bridge Pin (Capture - From Topology)
    // PIN_TOPOLOGY_IN
    {
        0,0,0,
        NULL,
        {
            0,
            NULL,
            0,
            NULL,
            N_DATA_RANGE_POINTERS_BRIDGE,
            PinDataRangePointersBridge,
            KSPIN_DATAFLOW_IN,
            KSPIN_COMMUNICATION_NONE,
            (GUID *) &KSCATEGORY_AUDIO,
            NULL,
            0
        }
    },
    // Wave Out Streaming Pin (Renderer)
    // PIN_RENDER_IN
    {
        1,1,0,
        NULL,
        {
            0,
            NULL,
            0,
            NULL,
            N_DATA_RANGE_POINTERS_STREAM,
            PinDataRangePointersStreamOut,
            KSPIN_DATAFLOW_IN,
            KSPIN_COMMUNICATION_SINK,
            (GUID *) &KSCATEGORY_AUDIO,
            &KSAUDFNAME_STEREO_MIX,
            0
        }
    },
    // Wave Out Bridge Pin (Renderer)
    // PIN_TOPOLOGY_OUT
    {
        0,0,0,
        NULL,
        {
            0,
            NULL,
            0,
            NULL,
            N_DATA_RANGE_POINTERS_BRIDGE,
            PinDataRangePointersBridge,
            KSPIN_DATAFLOW_OUT,
            KSPIN_COMMUNICATION_NONE,
            (GUID *) &KSCATEGORY_AUDIO,
            NULL,
            0
        }
    }
};

//
// PropertiesDevSpecific
// 
// The properties of our "dummy" node that allows the miniport to
// be instantiated without crashing kmixer.
//
static
PCPROPERTY_ITEM PropertiesDevSpecific[] =
{
   {  // Required for KSNODETYPE_DEV_SPECIFIC
      &KSPROPSETID_Audio,
      KSPROPERTY_AUDIO_DEV_SPECIFIC,
      KSPROPERTY_TYPE_BASICSUPPORT,
      PropertyHandler_DevSpecific
   },
   {
      &KSPROPSETID_Audio,
      KSPROPERTY_AUDIO_CPU_RESOURCES,
      KSPROPERTY_TYPE_GET,
      PropertyHandler_NoCpuResources
   }
};
DEFINE_PCAUTOMATION_TABLE_PROP(AutomationDevSpecific,PropertiesDevSpecific);

//
// Here's an automation table for the wave miniport so we can 
// talk directly to it:
//
static
PCPROPERTY_ITEM PropertiesWaveCyclicSolo[] =
{
   {  // Required for KSNODETYPE_DEV_SPECIFIC
      &KSPROPSETID_Audio,
      KSPROPERTY_AUDIO_DEV_SPECIFIC,
      KSPROPERTY_TYPE_BASICSUPPORT,
      CMiniportWaveCyclicSolo::PropertyHandlerDevSpecific_static
   },
   {
      &KSPROPSETID_Audio,
      KSPROPERTY_AUDIO_CPU_RESOURCES,
      KSPROPERTY_TYPE_GET,
      PropertyHandler_NoCpuResources
   }
};
DEFINE_PCAUTOMATION_TABLE_PROP(AutomationWaveCyclicSolo,PropertiesWaveCyclicSolo);

/*****************************************************************************
 * TopologyNodes
 *****************************************************************************
 * List of nodes -- we have no nodes, we're a straight passthrough --
 */
enum {
   NODE_INBOUND,
   NODE_OUTBOUND,
   N_MINIPORT_NODES
};
static PCNODE_DESCRIPTOR MiniportNodes[N_MINIPORT_NODES] =
{
   // Unconnected pins causing a problem? Create device-specific
   // ones!
   {
      0,
      &AutomationDevSpecific,
      &KSNODETYPE_DEV_SPECIFIC,       // Type
      NULL
   },
   {
      0,
      &AutomationDevSpecific,
      &KSNODETYPE_DEV_SPECIFIC,       // Type
      NULL
   }
};

//
// MiniportConnections
//
// List of connections.
//
// Our wave device is simply a dma engine that passes channels directly to
// the ice topology.
//
#define N_MINIPORT_CONNECTIONS (4)
static
PCCONNECTION_DESCRIPTOR MiniportConnections[N_MINIPORT_CONNECTIONS] =
{ 
   { PCFILTER_NODE, CMiniportWaveCyclicSolo::PIN_TOPOLOGY_IN,  // fromNode, fromPin
      NODE_INBOUND,  1 },

   { NODE_INBOUND,   0, 
     PCFILTER_NODE, CMiniportWaveCyclicSolo::PIN_CAPTURE_OUT }, // toNode,   toPin

   { PCFILTER_NODE, CMiniportWaveCyclicSolo::PIN_RENDER_IN,    
     NODE_OUTBOUND,  1 },

   { NODE_OUTBOUND,  0, 
     PCFILTER_NODE, CMiniportWaveCyclicSolo::PIN_TOPOLOGY_OUT }
};


/*****************************************************************************
 * MiniportFilterDescriptor
 *****************************************************************************
 * Complete miniport description.
 */
static
PCFILTER_DESCRIPTOR MiniportFilterDescriptor =
{
    0,                                  // Version
    &AutomationWaveCyclicSolo,          // AutomationTable
    sizeof(PCPIN_DESCRIPTOR),           // PinSize
    CMiniportWaveCyclicSolo::N_MINIPORT_PINS, // PinCount
    MiniportPins,                       // Pins
    sizeof(PCNODE_DESCRIPTOR),          // Node array size
    N_MINIPORT_NODES,                   // NodeCount
    MiniportNodes,                      // Nodes
    N_MINIPORT_CONNECTIONS,             // ConnectionCount
    MiniportConnections,                // Connections
    0,                                  // CategoryCount
    NULL                                // Categories
};
#pragma code_seg()

/*****************************************************************************
 * CMiniportWaveCyclicSolo::GetDescription()
 *****************************************************************************
 * Gets the topology of the wave miniport. Huh?
 */
#pragma code_seg("PAGE")
STDMETHODIMP CMiniportWaveCyclicSolo::GetDescription
(
    OUT     PPCFILTER_DESCRIPTOR *  OutFilterDescriptor
)
{
    PAGED_CODE();

    ASSERT(OutFilterDescriptor);

    t << "[CMiniportWaveCyclicSolo::GetDescription]\n";

    // Simply return the array of arrays of arrays of arrays defined above
    *OutFilterDescriptor = &MiniportFilterDescriptor;

    return STATUS_SUCCESS;
}
#pragma code_seg()

/*****************************************************************************
 * CMiniportWaveCyclicSolo::DataRangeIntersection()
 *****************************************************************************
 * Tests a data range intersection.
 *
 * Yet another useful comment from the sb16 sample. OK, given the data range
 * requested by the ClientDataRange, return something that fits that range
 * that's also accptable to us.
 */
#pragma code_seg() // not paged
STDMETHODIMP 
CMiniportWaveCyclicSolo::DataRangeIntersection
(   
    IN      ULONG           PinId,
    IN      PKSDATARANGE    ClientDataRange,
    IN      PKSDATARANGE    MyDataRange,
    IN      ULONG           OutputBufferLength,
    OUT     PVOID           ResultantFormat,
    OUT     PULONG          ResultantFormatLength
)
{
    BOOLEAN                         DigitalAudio;
    ULONG                           RequiredSize;
    ULONG                           returnedFrequency;
  
   t << "[CMiniportWaveCyclicSolo::DataRangeIntersection]\n";

   if (!_adapter)
    {
        ASSERT(0);                  // Call before construction or
                                    // after destruction? Ewww!
        return STATUS_NO_MATCH;
    }

    //
    // Let's do the complete work here. Yes, let's --trs
    //
    if (IsEqualGUIDAligned(ClientDataRange->Specifier,KSDATAFORMAT_SPECIFIER_NONE)) 
    {
        DigitalAudio = FALSE;
        RequiredSize = sizeof(KSDATAFORMAT);
    }
    else // format specified
    {
        //
        // The miniport did not resolve this format.  If the dataformat
        // is not PCM audio and requires a specifier, bail out.
        //
        if ( !IsEqualGUIDAligned(ClientDataRange->MajorFormat, KSDATAFORMAT_TYPE_AUDIO ) 
          || !IsEqualGUIDAligned(ClientDataRange->SubFormat, KSDATAFORMAT_SUBTYPE_PCM )) 
        {
            return STATUS_INVALID_PARAMETER;
        }
        DigitalAudio = TRUE;
        
        //
        // weird enough, the specifier here does not define the format of ClientDataRange
        // but the format that is expected to be returned in ResultantFormat.
        //
        if (IsEqualGUIDAligned(ClientDataRange->Specifier,KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)) 
        {
            // We will return an extensible format so the driver will know
            // we kick ass. :)
            RequiredSize = sizeof(KSDATAFORMAT) + sizeof(WAVEFORMATEXTENSIBLE);
        }            
        else
        {
           return STATUS_INVALID_PARAMETER;
        }
    } 
            
    //
    // Validate return buffer size, if the request is only for the
    // size of the resultant structure, return it now.
    //
    if (!OutputBufferLength) 
    {
        *ResultantFormatLength = RequiredSize;
        return STATUS_BUFFER_OVERFLOW;
    } 
    else if (OutputBufferLength < RequiredSize) 
    {
        return STATUS_BUFFER_TOO_SMALL;
    }
    
    // There was a specifier ...
    if (DigitalAudio) 
    {     
        // Convenience pointers since several major format specifiers
        // share the inevitable waveFormatEx structure.
        PWAVEFORMATEXTENSIBLE extensible = NULL;
        PWAVEFORMATEX         waveFormatEx = NULL;

        PKSDATARANGE_AUDIO  AudioRange = (PKSDATARANGE_AUDIO) MyDataRange;

        // At this point we have also confirmed that the data range is 
        // major format AUDIO
        PKSDATARANGE_AUDIO clientDataRangeAudio = (PKSDATARANGE_AUDIO)ClientDataRange;
        
        // Fill the structure
        if (IsEqualGUIDAligned(ClientDataRange->Specifier,KSDATAFORMAT_SPECIFIER_DSOUND)) 
        {
            PKSDATAFORMAT_DSOUND    DSoundFormat;
            
            ASSERT(0); // not supported?

            DSoundFormat = (PKSDATAFORMAT_DSOUND) ResultantFormat;
            
            t << "returning KSDATAFORMAT_DSOUND format intersection\n";
            
            DSoundFormat->BufferDesc.Flags = 0 ;
            DSoundFormat->BufferDesc.Control = 0 ;
            DSoundFormat->DataFormat = *ClientDataRange;
            DSoundFormat->DataFormat.Specifier = KSDATAFORMAT_SPECIFIER_DSOUND;
            DSoundFormat->DataFormat.FormatSize = RequiredSize;

            waveFormatEx = &DSoundFormat->BufferDesc.WaveFormatEx;
            
            *ResultantFormatLength = RequiredSize;
        } 
        else  if (IsEqualGUIDAligned(ClientDataRange->Specifier,KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)) 
        {
            PKSDATAFORMAT_WAVEFORMATEX  DataFormat;
        
            DataFormat = (PKSDATAFORMAT_WAVEFORMATEX) ResultantFormat;
            
            t << "returning KSDATAFORMAT_WAVEFORMATEX format intersection\n";
 
            // Don't really validate, just return it. Why bother then? --trs

            DataFormat->DataFormat = *ClientDataRange;
            DataFormat->DataFormat.Specifier = KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;
            DataFormat->DataFormat.FormatSize = RequiredSize;

            waveFormatEx = &DataFormat->WaveFormatEx;
            extensible = (PWAVEFORMATEXTENSIBLE) waveFormatEx;
            
            *ResultantFormatLength = RequiredSize; // how big?
        }
        else
        {
           ASSERT(0); // no format specified, logic error
           return STATUS_UNSUCCESSFUL;
        }
        
        //
        // Return a format that intersects the given audio range, 
        // using our maximum support as the "best" format.
        // 
        // Note that we can't support less than our maximum channels. :(
        //
        
        ASSERT(waveFormatEx); // oops?

        // We support two formats, the extensible one and one two-channel
        // 16-bit one for legacy directsound support and stuff. For now.
        if (2 == AudioRange->MaximumChannels)
        {
           ASSERT( 16 == AudioRange->MaximumBitsPerSample);
         
           waveFormatEx->wFormatTag = WAVE_FORMAT_PCM;
        }
        else
        {
           waveFormatEx->wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        }

        // Since there's no way to force a minimum number of channels,
        // we have to try to force it here. :/ -1 means that any number
        // of channels is acceptable. :)
        if ((AudioRange->MaximumChannels != clientDataRangeAudio->MaximumChannels) 
            &&
            (clientDataRangeAudio->MaximumChannels != 0xffffffff))
        {
           return STATUS_INVALID_PARAMETER;
        }
        waveFormatEx->nChannels = (USHORT)AudioRange->MaximumChannels;
        
        //
        // Check if the pin is still free
        //
        switch (PinId)
        {
        case PIN_CAPTURE_OUT:
           if (_allocatedCapture)
           {
               return STATUS_NO_MATCH;
           }
           break;

        case PIN_RENDER_IN:
           if (_allocatedRender)
           {
               return STATUS_NO_MATCH;
           }
           break;

        default:
            // It's not one of our pins. Aaaugh!
            return STATUS_INVALID_PARAMETER;
            break;
        }

        //
        // Check if one pin is in use -> use same sample frequency
        // for the other one.
        //
        if (_allocatedCapture || _allocatedRender)
        {
            returnedFrequency = _adapter->GetSamplingFrequency();;
            if ( (returnedFrequency > ((PKSDATARANGE_AUDIO) ClientDataRange)->MaximumSampleFrequency) 
              || (returnedFrequency < ((PKSDATARANGE_AUDIO) ClientDataRange)->MinimumSampleFrequency))
            {
                return STATUS_NO_MATCH;
            }
        }
        else // Not playing back, anything we support is OK
        {
            returnedFrequency = 
                min( AudioRange->MaximumSampleFrequency,
                     ((PKSDATARANGE_AUDIO) ClientDataRange)->MaximumSampleFrequency );

        }

        waveFormatEx->nSamplesPerSec = returnedFrequency;

        // Check range of sample size
        if ((AudioRange->MinimumBitsPerSample > 
             ((PKSDATARANGE_AUDIO) ClientDataRange)->MaximumBitsPerSample) ||
            (AudioRange->MaximumBitsPerSample  < 
             ((PKSDATARANGE_AUDIO) ClientDataRange)->MinimumBitsPerSample))
        {
            return STATUS_NO_MATCH;
        }

        // Fill out your old friend the WaveFormatEx structure.
        // Even with the port/miniport architecture, we're still basically doing
        // everything in every driver. Except now there's more code in between
        // to have obscure bugs. A bigger buggy black box, so to speak.

        waveFormatEx->wBitsPerSample = (USHORT)AudioRange->MaximumBitsPerSample;
        waveFormatEx->nBlockAlign = (waveFormatEx->wBitsPerSample * waveFormatEx->nChannels) / 8;
        waveFormatEx->nAvgBytesPerSec = (waveFormatEx->nSamplesPerSec * waveFormatEx->nBlockAlign);

        if (extensible) // if not direct-sound
        {
            // Set up the new frind the WAVEFORMATEXTENSIBLE
            ASSERT(extensible); // dang well better exist

            waveFormatEx->cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
            extensible->Samples.wValidBitsPerSample = 24; // about 15 useful...
            extensible->dwChannelMask       = 0; // only for surround systems. :/

            // TRS: channel mask should be 0, but I keep getting output rendered
            //      to the wrong outputs: (stereo out looks like this).
            //      (doesn't help). Fixed by applying service pack 2. :)
            //
            //extensible->dwChannelMask       = (SPEAKER_FRONT_LEFT |
            //                                   SPEAKER_FRONT_RIGHT); 

            extensible->SubFormat           = KSDATAFORMAT_SUBTYPE_PCM;

            // ((PKSDATAFORMAT) ResultantFormat)->SampleSize = waveFormatEx->nBlockAlign;
        }
        else
        {
           // DirectSound, no extension
           waveFormatEx->cbSize = 0;
        }

        t << "Channels = " << waveFormatEx->nChannels << "\n";
        t << "Samples/sec = " << waveFormatEx->nSamplesPerSec << "\n";
        t << "Bits/sample = " << waveFormatEx->wBitsPerSample << "\n";
        
    } 
    else  // not DIGITAL AUDIO -trs
    {   
        // There was no specifier. Return only the KSDATAFORMAT structure.
        //
        // Copy the data format structure.
        //
        t << "returning non-digital-audio default format intersection\n";
            
        RtlCopyMemory(ResultantFormat, ClientDataRange, sizeof( KSDATAFORMAT ) );
        *ResultantFormatLength = sizeof( KSDATAFORMAT );
    }
    
    return STATUS_SUCCESS;
}
#pragma code_seg()

/*****************************************************************************
 * CMiniportWaveCyclicSolo::NewStream()
 *****************************************************************************
 * Creates a new stream.  This function is called when a streaming pin is
 * created.
 * If you got here, be happy!
 */
#pragma code_seg("PAGE")
STDMETHODIMP CMiniportWaveCyclicSolo::NewStream
(
    OUT     PMINIPORTWAVECYCLICSTREAM * OutStream,
    IN      PUNKNOWN                    OuterUnknown,
    IN      POOL_TYPE                   PoolType,
    IN      ULONG                       Channel,
    IN      BOOLEAN                     Capture,
    IN      PKSDATAFORMAT               DataFormat,
    OUT     PDMACHANNEL *               OutDmaChannel,
    OUT     PSERVICEGROUP *             OutServiceGroup
)
{
    PAGED_CODE();

    ASSERT(OutStream);
    ASSERT(DataFormat);
    ASSERT(OutDmaChannel);
    ASSERT(OutServiceGroup);

    t << "[CMiniportWaveCyclicSolo::NewStream]\n";

    NTSTATUS            ntStatus = STATUS_SUCCESS;
    PWAVEFORMATEX       waveFormat = NULL;
    PDMACHANNEL         dmaChannel = NULL;

    CMiniportWaveCyclicStreamSoloBase * stream = NULL;

    if (!_adapter)
    {
       // Call before construction (impossible) or after destruction
       // of our hardware (which should have removed the ref)?
       ASSERT(FALSE);

       ntStatus = STATUS_INVALID_DEVICE_REQUEST;
       goto bail;
    }

    //
    // Make sure the hardware is not already in use.
    //
    if (Capture)
    {
        if (_allocatedCapture)
        {
            ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        }
    }
    else
    {
        if (_allocatedRender)
        {
            ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        }
    }

    if (!NT_SUCCESS(ntStatus))
       goto bail;

    //
    // Determine if the format is valid.
    //
    ntStatus = ValidateFormat(DataFormat, Capture);

    if(!NT_SUCCESS(ntStatus))
       goto bail;

    // No matter what the format is, there's a WAVEFORMATEX 
    // attached. 
    waveFormat = PWAVEFORMATEX(DataFormat + 1);

    // if we're trying to start a full-duplex stream.
    if(_allocatedCapture || _allocatedRender)
    {
        // make sure the requested sampling rate is the
        // same as the currently running one...
        if( _adapter->GetSamplingFrequency() != waveFormat->nSamplesPerSec )
        {
            // Bad format....
            t << " Sampling rate mismatch with open channel\n";
            ntStatus = STATUS_INVALID_PARAMETER;
            goto bail;
        }
    }

    // If this is a multichannel stream, instantiate a normal WDM device,
    // otherwise instantiate a converting adapter for playback...

    //
    // Instantiate a stream. Or just new one. Any way you like.
    //
    if (Capture)
    {
       stream = new(PoolType) CMiniportWaveCyclicStreamSoloCapture(OuterUnknown);
       ntStatus = _adapter->GetRecDmaChannelRef(&dmaChannel);
       if (!NT_SUCCESS(ntStatus))
          goto bail;

       _adapter->SetRecMode( DMAMODE_MULTICHANNEL );
    }
    else // Hmmm, render
    {
       stream = new(PoolType) CMiniportWaveCyclicStreamSoloRender(OuterUnknown);

       if (2 == waveFormat->nChannels)
       {
          _adapter->SetPlayMode( DMAMODE_LEGACY );
          ntStatus = _adapter->GetPlayLegacyDmaChannelRef(&dmaChannel);
       }
       else
       {
          _adapter->SetPlayMode( DMAMODE_MULTICHANNEL );
          ntStatus = _adapter->GetPlayDmaChannelRef(&dmaChannel);
       }

       if (!NT_SUCCESS(ntStatus))
          goto bail;
    }
    ASSERT(dmaChannel);

    if (! stream) // allocation failed?
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto bail;
    }

    // Don't want it going away before we're through with it:
    stream->AddRef();

    ntStatus =
        stream->Init
          (
              this,
              _adapter,
              Channel,
              DataFormat,
              _portWave,
              dmaChannel
          );

    if (!NT_SUCCESS(ntStatus))
       goto bail;

    if (Capture)
    {
       ASSERT( !_allocatedCapture ); // well, you never know
        _allocatedCapture = TRUE;
    }
    else
    {
        ASSERT( ! _allocatedRender ); // is the logic above faulty?
        _allocatedRender = TRUE;
    }

    *OutStream = PMINIPORTWAVECYCLICSTREAM(stream);
    stream->AddRef();

    // Functions adds ref for us
    *OutDmaChannel = dmaChannel;
    (*OutDmaChannel)->AddRef();
            
    if (!NT_SUCCESS(ntStatus))
       goto bail;

    if (Capture)
    {
       ntStatus = _adapter->GetCaptureServiceGroup(OutServiceGroup);
       if (!NT_SUCCESS(ntStatus))
          goto bail;
    }
    else
    {
       ntStatus = _adapter->GetRenderServiceGroup(OutServiceGroup);
       if (!NT_SUCCESS(ntStatus))
          goto bail;
    }
    (*OutServiceGroup)->AddRef(); // ref for the caller

    //
    // The stream, the DMA channel, and the service group have
    // references now for the caller.  The caller expects these
    // references to be there.
    //

    // Jump here on failure to clean up and exit.
bail:
    //
    // This is our private reference to the stream to prevent its
    // disappearing during this function call.  The caller has
    // its own.
    //
    if (stream)
       stream->Release();

    // And a private ref to the dma channel
    if (dmaChannel)
       dmaChannel->Release();

    return ntStatus;
}
#pragma code_seg()

//
// DeallocateCapture -- called when the stream miniport goes away
//
#pragma code_seg()
void CMiniportWaveCyclicSolo::DeallocateCapture()
{
   t << "[CMiniportWaveCyclicSolo::DeallocateCapture]\n";

   // We have no pointer to the stream, so we don't have to
   // release it or anything...at least in this initial rev.

   _allocatedCapture = FALSE;
}

//
// DeallocateCapture -- called when the rendering stream miniport goes away
//
#pragma code_seg() // can this be called at raised priority?
void CMiniportWaveCyclicSolo::DeallocateRender()
{
   t << "[CMiniportWaveCyclicSolo::DeallocateRender]\n";

   _allocatedRender = FALSE;
}

/*****************************************************************************
 * CMiniportWaveCyclicStreamSoloBase::NonDelegatingQueryInterface()
 *****************************************************************************
 * Obtains an interface.  This function works just like a COM QueryInterface
 * call and is used if the object is not being aggregated.
 *
 * Note that while we don't delegate, we do aggregate. :)
 */
#pragma code_seg("PAGE")
STDMETHODIMP
CMiniportWaveCyclicStreamSoloBase::
NonDelegatingQueryInterface
(
    IN      REFIID  Interface,
    OUT     PVOID * Object
)
{
    PAGED_CODE();

    ASSERT(Object);

    t << "[CMiniportWaveCyclicStreamSoloBase::NonDelegatingQueryInterface]\n";

    if (IsEqualGUIDAligned(Interface,IID_IUnknown))
    {
        *Object = PVOID(PUNKNOWN(this));
        PUNKNOWN(*Object)->AddRef();
        return STATUS_SUCCESS;
    }
    else
    if (IsEqualGUIDAligned(Interface,IID_IMiniportWaveCyclicStream))
    {
        *Object = PVOID(PMINIPORTWAVECYCLICSTREAM(this));
        PUNKNOWN(*Object)->AddRef();
        return STATUS_SUCCESS;
    }
    else
    {
        DbgBreakPoint();

        *Object = NULL;
    }

    return STATUS_INVALID_PARAMETER;
}
#pragma code_seg()

//
// CMiniportWaveCyclicStreamSoloBase::~CMiniportWaveCyclicStreamSoloBase()
//
CMiniportWaveCyclicStreamSoloBase::~CMiniportWaveCyclicStreamSoloBase( void )
{
    PAGED_CODE();

    t << "[CMiniportWaveCyclicStreamSoloBase::~CMiniportWaveCyclicStreamSoloBase]\n";

    CleanUp();
}

//
// CMiniportWaveCyclicStreamSoloBase::Init()
//
// Set up the stream, allocate DMA, etc. 
//
#pragma code_seg("PAGE")
NTSTATUS
CMiniportWaveCyclicStreamSoloBase::Init
(
    IN      CMiniportWaveCyclicSolo *   Miniport_,
    IN      IAdapterSolo  *             adapter,
    IN      ULONG                       Channel_,
    IN      PKSDATAFORMAT               DataFormat,
    IN      PPORTWAVECYCLIC             portWave,
    IN      PDMACHANNEL                 dmaChannel
)
{
    PAGED_CODE();

    t << "[CMiniportWaveCyclicStreamSoloBase::Init]\n";

    ASSERT(Miniport_);
    ASSERT(adapter);
    ASSERT(DataFormat);

    ULONG size = 0;
    PWAVEFORMATEX waveFormat = PWAVEFORMATEX(DataFormat + 1);

    _adapter = 0;
    _miniport = 0;
    // _isCapture = 0; // set by derived class constructor
    _channel = 0;
    _nChannels = 0;
    _state           = KSSTATE_STOP;

    // Save references to our friends

    _adapter = adapter;
    _adapter->AddRef();

    _miniport = Miniport_;
    _miniport->AddRef();

    _channel         = Channel_;
    _nChannels       = waveFormat->nChannels;
    _sampleBytes     = waveFormat->wBitsPerSample / 8;

    t << "   isCapture:" << _isCapture << "\n";
    t << "   nChannels:" << _nChannels << "\n";
    t << " sampleBytes:" << _sampleBytes << "\n";

    NTSTATUS status = STATUS_SUCCESS;

    status = _miniport->ValidateFormat(DataFormat,_isCapture);
    if (!NT_SUCCESS(status))
       goto bail;

    SetFormat( DataFormat );

    return status ;

bail:
    // Jump here on failure
    CleanUp();
    return status;
}
#pragma code_seg()

//
// CleanUp -- destructor and failed-Init() helper function that
//   deallocates everything
//
#pragma code_seg() // it's a helper, who knows when it will be called
void CMiniportWaveCyclicStreamSoloBase::CleanUp()
{
   // Make sure the adapter isn't still talking to our
   // DMA.
    if (_adapter) 
    {
       if (_isCapture)
       {
          _adapter->StopCapture();
       }
       else
       {
          _adapter->StopRender();
       }

       _adapter->Release();
       _adapter = 0;
    }

    // Tell the miniport we're gone
    if (_miniport)
    {
       if (_isCapture)
          _miniport->DeallocateCapture();
       else
          _miniport->DeallocateRender();

       _miniport->Release();
       _miniport = 0;
    }

}
#pragma code_seg()


/*****************************************************************************
 * CMiniportWaveCyclicStreamSoloBase::SetNotificationFreq()
 *****************************************************************************
 * Sets the notification frequency
 * 
 * TRS: (I Hope) Not called very often, so I left it in the base class
 */
#pragma code_seg("PAGE")
STDMETHODIMP_(ULONG)
CMiniportWaveCyclicStreamSoloBase::SetNotificationFreq
(
    IN      ULONG   Interval,
    OUT     PULONG  FramingSize    
)
{
    PAGED_CODE();

    t << "[CMiniportWaveCyclicStreamSoloBase::SetNotificationFreq]\n";

    ULONG actualInterval;

    if (!_adapter)
    {
       ASSERT(0); // call before Init()??
       return 0;
    }

    // Pass the buck
    if (_isCapture)
    {
       _adapter->SetCaptureNotificationInterval( Interval );
       actualInterval = _adapter->GetCaptureNotificationInterval();
    }
    else
    {
       _adapter->SetRenderNotificationInterval( Interval );
       actualInterval = _adapter->GetRenderNotificationInterval();
    }

    // Return number of bytes in a frame (heck, the fractional milliseconds
    // are quite significant here. How lame.
    if (_isCapture)
       *FramingSize = _adapter->RecGetDmaPageFrames() * _nChannels * _sampleBytes;
    else
       *FramingSize = _adapter->PlayGetDmaPageFrames() * _nChannels * _sampleBytes;

    // TRS: verbose 
    t << " interval:" << actualInterval 
       << " framingSize" << *FramingSize
       << " capture:" << _isCapture 
       << "\n";

    // Return what it was actually set to
    return actualInterval;
}
#pragma code_seg()

//
// CMiniportWaveCyclicStreamSolo::SetFormat()
//
// Sets up the adapter to teliver this format. It's assumed that
// you validated the format before calling this, I mean really, do
// I have to do everything for you?
//
#pragma code_seg("PAGE")
STDMETHODIMP
CMiniportWaveCyclicStreamSoloBase::SetFormat( IN PKSDATAFORMAT   Format )
{
    PAGED_CODE();

    t << "[CMiniportWaveCyclicSoloBase::SetFormat]\n";

    NTSTATUS ntStatus = STATUS_SUCCESS;
    PWAVEFORMATEX waveFormat = NULL;

    ASSERT(Format);
    if (!_adapter)
    {
       ASSERT(0); // call before construction?
       ntStatus = STATUS_UNSUCCESSFUL;
       goto bail;
    }

    waveFormat = PWAVEFORMATEX(Format + 1);

    if (KSSTATE_RUN == _state)
    {
       ntStatus = STATUS_INVALID_DEVICE_REQUEST;
       goto bail;
    }

    ntStatus = _miniport->ValidateFormat(Format,_isCapture);
    if (!NT_SUCCESS(ntStatus))
       goto bail;

    ntStatus = _adapter->SetSamplingFrequency( waveFormat->nSamplesPerSec );

    if (!NT_SUCCESS(ntStatus))
       goto bail;

    t << "  SampleRate: " << waveFormat->nSamplesPerSec << "\n";

    return ntStatus;

bail:
    t << "  failed\n";
    return ntStatus;
}
#pragma code_seg()

//
//CMiniportWaveCyclicStreamSolo::SetState() -- Set the state of the channel.
//
// There's four states, or actually three. Run, pause, and stop. What do
// you do with acquire, exactly?
//
#pragma code_seg("PAGE")
STDMETHODIMP
CMiniportWaveCyclicStreamSoloBase::SetState( IN      KSSTATE     NewState )
{
   PAGED_CODE();

   t << "[CMiniportWaveCyclicStreamSoloBase::SetState] ";

   if (_isCapture)
      t << "(capture)\n";
   else
      t << "(render)\n";

   if (!_adapter)
   {
      ASSERT(0); // call before Init?
      return STATUS_UNSUCCESSFUL;
   }

   // Pull the same trick used by some other drivers: we only need
   // three states (stop, pause, run). We don't want to waste time
   // entering stop twice:
   if (KSSTATE_ACQUIRE == NewState)
      NewState = KSSTATE_STOP;   // Modify stack varible--bad form but legal

   if (_state == NewState)
      return STATUS_SUCCESS;     // *quick exit* no state change

   // If we don't flag it otherwise, assume success
   NTSTATUS ntStatus = STATUS_SUCCESS;

   // We can assume state transitions, but if there's no 
   // transition just skip it and return success
   switch (NewState)
   {
   case KSSTATE_RUN:
      t << " +KSSTATE_RUN\n";

      if (_isCapture)            
      {
         ntStatus = _adapter->UnpauseCapture();
      }
      else
      {
         ntStatus = _adapter->UnpauseRender();
      }
      break;

   case KSSTATE_PAUSE: 
      t << " +KSSTATE_PAUSE\n";

      if (_isCapture)
      {
         ntStatus = _adapter->PauseCapture();

         // If we were not running, start the DMA. Don't call this
         // when pausing from the "RUN" state because it clears the
         // counters.
         if (KSSTATE_STOP == _state)
            ntStatus = _adapter->StartCapture();
      }
      else
      {
         ntStatus = _adapter->PauseRender();

         if (KSSTATE_STOP == _state)
            ntStatus = _adapter->StartRender();
      }
      break;

   // As far as we're concerned, stop and acquire are the same
   case KSSTATE_STOP:      
      t << " +KSSTATE_STOP/ACQUIRE\n";

      if (_isCapture)
         ntStatus = _adapter->StopCapture();
      else
         ntStatus = _adapter->StopRender();
      break;

   default: 
      ASSERT(0); // unexpected case
      break;
   }
 
   if (NT_SUCCESS(ntStatus))
      _state = NewState;

   return ntStatus;
}
#pragma code_seg()


//
// CMiniportWaveCyclicStreamSoloCapture/Render::GetPosition()
//
// Called frequently, so hey let's let the virtual table do the work:
//
#pragma code_seg()
STDMETHODIMP
CMiniportWaveCyclicStreamSoloCapture::
GetPosition
(
    OUT     PULONG  Position
)
{
   ASSERT(Position);

   if (!_adapter)
   {
      ASSERT(0); // Hmmm, call before construction?
      return STATUS_UNSUCCESSFUL;
   }

   // I _think_ the ICE hardware is reporting the DMA position
   // at the time we last called stop, but if we turn it on
   // while it is paused, it is actually reset to 0 and will
   // start from 0 when we unpause it. Therefore...

   if (_state != KSSTATE_RUN)
   {
      *Position = 0;
      return STATUS_SUCCESS;
   }

   ULONG pos = _adapter->RecGetPositionFrames();  // get pos in frames
   pos *= (_nChannels * _sampleBytes);       // convert to bytes
   *Position = pos;
   //t << " +" << *Position << "\n";

   return STATUS_SUCCESS;
}

//
//
//
#pragma code_seg()
STDMETHODIMP
CMiniportWaveCyclicStreamSoloRender::
GetPosition
(
    OUT     PULONG  Position
)
{
   ASSERT(Position);

   if (!_adapter)
   {
      ASSERT(0); // Hmmm, call before construction?
      return STATUS_UNSUCCESSFUL;
   }

   if (_state != KSSTATE_RUN)
   {
      *Position = 0;
      return STATUS_SUCCESS;
   }

   ULONG pos = _adapter->PlayGetPositionFrames();  // get pos in frames
   pos *= (_nChannels * _sampleBytes);       // convert to bytes
   *Position = pos;

   // TRS: Dangerously verbose (in irq handler) but interesting
   //t << " -" << *Position << "\n";

   return STATUS_SUCCESS;
}




/*++

Routine Description:
    Given a physical position based on the actual number of bytes transferred,
    this function converts the position to a time-based value of 100ns units.

Arguments:
    IN OUT PLONGLONG PhysicalPosition -
        value to convert.

Return:
    STATUS_SUCCESS or an appropriate error code.

--*/
STDMETHODIMP
CMiniportWaveCyclicStreamSoloBase::NormalizePhysicalPosition(
    IN OUT PLONGLONG PhysicalPosition
)
{         
   if (!_adapter)
   {
      ASSERT(0); // call before construction?
      return STATUS_UNSUCCESSFUL;
   }

   // TRS: editorial comment: I guess I see why this is done here in the
   //      driver instead of, eg, looking at the format that was approved
   //      in previous calls. :) 

   // absoluteTime = PhysicalPos(bytes) / frameSize(bytes/Frame) / freq(seconds/Frame) * Kns/sec

    *PhysicalPosition =
            (_100NS_UNITS_PER_SECOND                  //   ns/sec 
             / (_sampleBytes * _nChannels)            // * frames/bytes
             * *PhysicalPosition                      // * bytes
             / _adapter->GetSamplingFrequency());     // * sec/frames
                                                      // = ns 

    return STATUS_SUCCESS;
}
    
/*****************************************************************************
 * CMiniportWaveCyclicStreamSolo::Silence()
 *****************************************************************************
 * Fills a buffer with silence.
 */
#pragma code_seg()
STDMETHODIMP_(void)
CMiniportWaveCyclicStreamSoloBase::
Silence
(
    IN      PVOID   Buffer,
    IN      ULONG   ByteCount
)
{
   // Just don't log this, it's too intrusive and called at wierd
   // levels...
   // t << "[CMiniportWaveCyclicSoloBase::Silence]\n";

   // 0 is always silence since all pro audio is signed. :)
   RtlFillMemory(Buffer, ByteCount, 0);
}

/*****************************************************************************
 * CMiniportWaveCyclicSolo::ValidateFormat()
 *****************************************************************************
 * Validates a wave format.
 * TRS: Oh yeah, what a great description. Thanks. How helpful.
 *
 * Determines whether the supplied format is supported by our hardware. 
 * Here's an example from MSDN (thanks guys!):
 *
 * Note:
 *   The system will call this with any format requested, so be ready to
 *   reject things that _aren't part of your dataformatintersection_. :(
 *   Aaaaugh! But this is part of the hack to let you bypass KMixer 
 *   latency, and it's also a reason we have to support 16 bit stereo 
 *   output. :(
 *
  DataFormat.FormatSize = sizeof(KSDATAFORMAT) + sizeof(WAVEFORMATEXTENSIBLE);
  DataFormat.Flags      = 0;
  DataFormat.SampleSize = 0;
  DataFormat.Reserved   = 0;
  DataFormat.MajorFormat = STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO);
  DataFormat.SubFormat   = STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM);
  DataFormat.Specifier   = STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX);
  Format.wFormatTag      = WAVE_FORMAT_EXTENSIBLE;
  Format.nChannels       = 4;
  Format.nSamplesPerSec  = 44100;
  Format.nAvgBytesPerSec = 352800;
  Format.nBlockAlign     = 8; // wrong!
  Format.wBitsPerSample  = 24;
  Format.cbSize          = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
  Format.wValidBitesPerSample = 20;
  Format.dwChannelMask   = KSAUDIO_SPEAKER_SURROUND;
  Format.SubFormat       = KSDATAFORMAT_SUBTYPE_PCM;

  We just have to accept this (or something more restrictive that makes us happy)
 */
#pragma code_seg()
NTSTATUS
CMiniportWaveCyclicSolo::ValidateFormat
(
    IN      PKSDATAFORMAT   Format,
    IN      BOOLEAN         isCapture
)
{
   PAGED_CODE();

   ASSERT(Format);

   t << "[CMiniportWaveCyclicSolo::ValidateFormat]\n";

   //
   // A WAVEFORMATEX or WAVEFORMATEXTENSIBLE structure should appear 
   // after the generic KSDATAFORMAT if the GUIDs turn out as we expect.
   //
   // The extensible format begins with a waveformatex structure, so
   // we should be able to use the same pointer for both. I hope.
   //
   PWAVEFORMATEXTENSIBLE extensible = PWAVEFORMATEXTENSIBLE(Format + 1);
   PWAVEFORMATEX         waveFormat = PWAVEFORMATEX(Format + 1);

   // First check the WAVEFORMATEX structures, common to both formats

   if (! (Format->FormatSize >= sizeof(KSDATAFORMAT_WAVEFORMATEX)) )
   {
      t << " too small to hold WAVEFFORMATEX\n";
      goto bail;
   }

   // Major format
   if (!IsEqualGUIDAligned(Format->MajorFormat,KSDATAFORMAT_TYPE_AUDIO))
   {
      t << " major format wrong\n";
      goto bail;
   }

   // Subformat
   if (!IsEqualGUIDAligned(Format->SubFormat,KSDATAFORMAT_SUBTYPE_PCM))
   {
      t << " subformat wrong\n";
      goto bail;
   }
        
   if (!IsEqualGUIDAligned(Format->Specifier,KSDATAFORMAT_SPECIFIER_WAVEFORMATEX))
   {
      t << " format Specifier wrong\n";
      goto bail;
   }

   // If it's 16-bit, we only support stereo
   if (16 == waveFormat->wBitsPerSample)
   {
      if (isCapture) // 16-bit capture not supported for now
         goto bail;

      if (2 != waveFormat->nChannels)
         goto bail;
   }
   else if ((SSS_SAMPLE_BYTES * 8) == waveFormat->wBitsPerSample)
   {
      if (isCapture)
      {
         if (12 != waveFormat->nChannels)
         {
            t << " nChannels:" << waveFormat->nChannels << " wrong (capture)\n";
            goto bail;
         }
      }
      else
      {
         if (10 != waveFormat->nChannels)
         {
            t << " nChannels:" << waveFormat->nChannels << " wrong (render)\n";
            goto bail;
         }
      }
   }
   else
   {
      t << " wBitsPerSample:" << waveFormat->wBitsPerSample << " wrong\n";
      goto bail;
   }

   switch (waveFormat->nSamplesPerSec)
   {
      // Valid formats
   case 48000L: 
   case 24000L: 
   case 12000L: 
   case  9600L: 
   case 32000L: 
   case 16000L: 
   case 96000L: 
   case 44100L: 
   case 22050L: 
   case 11025L: 
   case 88200L: 
      break;

   default:          // bad sample rate
      t << " nSamplesPerSec:" << waveFormat->nSamplesPerSec << " bad\n";
      goto bail;
   }

   // Now check: either it's PCM or EXTENSIBLE
   switch (waveFormat->wFormatTag)
   {
   case WAVE_FORMAT_PCM:      // That's all, we're done
      break;

   case WAVE_FORMAT_EXTENSIBLE:
      // Check size again, extensible has of course an extension 
      // on the end (cbSize > 0)
      if (waveFormat->cbSize < (sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)))
      {
         t << " EXTENSIBLE format but too small\n";
         goto bail;
      }

      // Who cares if these are wrong?
      // if (24 != extensible->Samples.wValidBitsPerSample) // but do I care?
      // {
      //    t << " wValidBitsPerSample:" << extensible->Samples.wValidBitsPerSample << "\n";
      // }

      if (0 != extensible->dwChannelMask)
      {
         t << "       dwChannelMask:" << extensible->dwChannelMask << "\n";
      }

      if (extensible->SubFormat != KSDATAFORMAT_SUBTYPE_PCM)
      {
         t << " EXTENSIBLE subformat wrong\n";
         goto bail;
      }

      break;

   default:
      goto bail;
   }

   return STATUS_SUCCESS;

bail:
   return STATUS_INVALID_PARAMETER;
}

#pragma code_seg("PAGE")
NTSTATUS CMiniportWaveCyclicSolo::PropertyHandlerDevSpecific_static
(
    IN PPCPROPERTY_REQUEST   PropertyRequest
)
{
   PAGED_CODE();

   ASSERT(PropertyRequest);

   t << "[CMiniportWaveCyclicSolo::PropertyHandler_DevSpecific]\n";

   // How convenient!
   CMiniportWaveCyclicSolo *that =
        (CMiniportWaveCyclicSolo *) PropertyRequest->MajorTarget;

   ASSERT(that);

   return that->PropertyHandlerDevSpecific(PropertyRequest);
}

#pragma code_seg("PAGE")
NTSTATUS CMiniportWaveCyclicSolo::PropertyHandlerDevSpecific
(
    IN PPCPROPERTY_REQUEST   PropertyRequest
)
{
   PAGED_CODE();

   if (PropertyRequest->PropertyItem->Id != KSPROPERTY_AUDIO_DEV_SPECIFIC)
      return STATUS_INVALID_DEVICE_REQUEST;

   // Later...later...
   // return that->PropertyHandler(PropertyRequest);

   // validate node
   if(PropertyRequest->Node == ULONG(-1))
      return STATUS_INVALID_DEVICE_REQUEST;

   if(PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
   {
      // Here's where we'd return custom stuff, or something.
      if(PropertyRequest->ValueSize < sizeof(LONG))
          return STATUS_BUFFER_TOO_SMALL;

      *(PLONG(PropertyRequest->Value)) = 0;
      PropertyRequest->ValueSize = sizeof(LONG);

      return STATUS_SUCCESS;
   } 
   else if(PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
   {
      // if return buffer can hold a KSPROPERTY_DESCRIPTION, return it
      if(PropertyRequest->ValueSize >= (sizeof(KSPROPERTY_DESCRIPTION)))
      {
         PKSPROPERTY_DESCRIPTION PropDesc = PKSPROPERTY_DESCRIPTION(PropertyRequest->Value);

         // Return size of this structure plus required size to return all
         // device specific info in subsequent fields
         PropDesc->DescriptionSize   = sizeof(KSPROPERTY_DESCRIPTION);

         PropDesc->AccessFlags       = KSPROPERTY_TYPE_BASICSUPPORT;
                                       // | KSPROPERTY_TYPE_GET;

         PropDesc->PropTypeSet.Set   = KSPROPTYPESETID_General; // only allowed type
         PropDesc->PropTypeSet.Id    = VT_I4; 
         PropDesc->PropTypeSet.Flags = 0;
         PropDesc->MembersListCount  = 0;
         PropDesc->Reserved          = 0;

         // set the return value size
         PropertyRequest->ValueSize = sizeof(KSPROPERTY_DESCRIPTION);
         return STATUS_SUCCESS;
      } 
      else if(PropertyRequest->ValueSize >= sizeof(ULONG)) 
      {
         // Just return the capabilities

         PULONG AccessFlags = PULONG(PropertyRequest->Value);
  
         *AccessFlags = KSPROPERTY_TYPE_BASICSUPPORT |
                        KSPROPERTY_TYPE_GET |
                        KSPROPERTY_TYPE_SET;
  
         // set the return value size
         PropertyRequest->ValueSize = sizeof(ULONG);
         return STATUS_SUCCESS;                    
      }
      else
      {
         // Buffer is too small to be useful:
         return STATUS_INVALID_DEVICE_REQUEST;
      }
   }

   // If we got here, we're unhappy.
   return STATUS_INVALID_DEVICE_REQUEST;
}


//
// PropertyHandler_DevSpecific
// 
// Property handler for devspecific dummy node. (We just "support" it but
// don't do anything. Yet.)
//
// typedef struct {
//    KSNODEPROPERTY NodeProperty;
//    ULONG   DevSpecificId;
//    ULONG   DeviceInfo;
//    ULONG   Length;
//} KSNODEPROPERTY_AUDIO_DEV_SPECIFIC, *PKSNODEPROPERTY_AUDIO_DEV_SPECIFIC;

#pragma code_seg("PAGE")
NTSTATUS PropertyHandler_DevSpecific
(
    IN PPCPROPERTY_REQUEST   PropertyRequest
)
{
   PAGED_CODE();

   ASSERT(PropertyRequest);

   t << "[minwave.cpp::PropertyHandler_DevSpecific]\n";

   // How convenient!
   CMiniportWaveCyclicSolo *that =
        (CMiniportWaveCyclicSolo *) PropertyRequest->MajorTarget;

   if (PropertyRequest->PropertyItem->Id != KSPROPERTY_AUDIO_DEV_SPECIFIC)
      return STATUS_INVALID_DEVICE_REQUEST;

   // Later...later...
   // return that->PropertyHandler(PropertyRequest);

   // validate node
   if(PropertyRequest->Node == ULONG(-1))
      return STATUS_INVALID_DEVICE_REQUEST;

   if(PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
   {
      // Here's where we'd return custom stuff, or something.
      if(PropertyRequest->ValueSize < sizeof(LONG))
          return STATUS_BUFFER_TOO_SMALL;

      *(PLONG(PropertyRequest->Value)) = 0;
      PropertyRequest->ValueSize = sizeof(LONG);

      return STATUS_SUCCESS;
   } 
   else if(PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
   {
      // if return buffer can hold a KSPROPERTY_DESCRIPTION, return it
      if(PropertyRequest->ValueSize >= (sizeof(KSPROPERTY_DESCRIPTION)))
      {
         PKSPROPERTY_DESCRIPTION PropDesc = PKSPROPERTY_DESCRIPTION(PropertyRequest->Value);

         // Return size of this structure plus required size to return all
         // device specific info in subsequent fields
         PropDesc->DescriptionSize   = sizeof(KSPROPERTY_DESCRIPTION);

         PropDesc->AccessFlags       = KSPROPERTY_TYPE_BASICSUPPORT;
                                       // | KSPROPERTY_TYPE_GET;

         PropDesc->PropTypeSet.Set   = KSPROPTYPESETID_General; // only allowed type
         PropDesc->PropTypeSet.Id    = VT_I4; 
         PropDesc->PropTypeSet.Flags = 0;
         PropDesc->MembersListCount  = 0;
         PropDesc->Reserved          = 0;

         // set the return value size
         PropertyRequest->ValueSize = sizeof(KSPROPERTY_DESCRIPTION);
         return STATUS_SUCCESS;
      } 
      else if(PropertyRequest->ValueSize >= sizeof(ULONG)) 
      {
         // Just return the capabilities

         PULONG AccessFlags = PULONG(PropertyRequest->Value);
  
         *AccessFlags = KSPROPERTY_TYPE_BASICSUPPORT |
                        KSPROPERTY_TYPE_GET |
                        KSPROPERTY_TYPE_SET;
  
         // set the return value size
         PropertyRequest->ValueSize = sizeof(ULONG);
         return STATUS_SUCCESS;                    
      }
      else
      {
         // Buffer is too small to be useful:
         return STATUS_INVALID_DEVICE_REQUEST;
      }
   }

   // If we got here, we're unhappy.
   return STATUS_INVALID_DEVICE_REQUEST;
}

#ifdef QQQ // comment out

// Extended property stuff in basic_support from ac97 properties example

    // if there is enough space for a KSPROPERTY_DESCRIPTION information
    if (PropertyRequest->ValueSize >= (sizeof(KSPROPERTY_DESCRIPTION)))
    {
        // we return a KSPROPERTY_DESCRIPTION structure.
        PKSPROPERTY_DESCRIPTION PropDesc = (PKSPROPERTY_DESCRIPTION)PropertyRequest->Value;

        PropDesc->AccessFlags = KSPROPERTY_TYPE_BASICSUPPORT |
                                KSPROPERTY_TYPE_GET |
                                KSPROPERTY_TYPE_SET;
        PropDesc->DescriptionSize   = sizeof(KSPROPERTY_DESCRIPTION) +
                                      sizeof(KSPROPERTY_MEMBERSHEADER) +
                                      sizeof(KSPROPERTY_STEPPING_LONG);
        PropDesc->PropTypeSet.Set   = KSPROPTYPESETID_General;
        PropDesc->PropTypeSet.Id    = VT_I4;
        PropDesc->PropTypeSet.Flags = 0;
        PropDesc->MembersListCount  = 1;
        PropDesc->Reserved          = 0;

        // if return buffer can also hold a range description, return it too
        if (PropertyRequest->ValueSize >= (sizeof(KSPROPERTY_DESCRIPTION) +
            sizeof(KSPROPERTY_MEMBERSHEADER) + sizeof(KSPROPERTY_STEPPING_LONG)))
        {
            // fill in the members header
            PKSPROPERTY_MEMBERSHEADER Members = (PKSPROPERTY_MEMBERSHEADER)(PropDesc + 1);

            Members->MembersFlags   = KSPROPERTY_MEMBER_STEPPEDRANGES;
            Members->MembersSize    = sizeof(KSPROPERTY_STEPPING_LONG);
            Members->MembersCount   = 1;
            Members->Flags          = 0;

            // fill in the stepped range
            PKSPROPERTY_STEPPING_LONG Range = (PKSPROPERTY_STEPPING_LONG)(Members + 1);
#endif // QQQ