// $Id: mintopo.cpp,v 1.5 2001/09/03 15:37:04 tekhedd Exp $
// 
// Copyright (C) 2001 Tom Surace. All rights reserved.
//
// Loosely based on the sb16 example, which is 
// Copyright (c) 1997-2000 Microsoft Corporation. All Rights Reserved.

#include <vdw.h>
#include "..\SolowdmDeviceinterface.h"

// Include definitions for DWORD and other stupid macros
#include <windef.h>
#include <portcls.h>		// PcGetTimeInterval() PcNewDmaChannel(imaginary)

#include "adapter.h"
#include "config.h"
#include "mintopo.h"

#pragma hdrstop("mintopo.pch")

// Forward declaration;
extern PCFILTER_DESCRIPTOR MiniportFilterDescriptor;

// --- Local functions

#pragma code_seg("PAGE")


/*****************************************************************************
 * CreateMiniportTopologySolo()
 */
NTSTATUS
CreateMiniportTopologySolo
(
    OUT     PUNKNOWN *  Unknown,
    IN      PUNKNOWN    UnknownOuter    OPTIONAL,
    IN      POOL_TYPE   PoolType
)
{
    PAGED_CODE();

    ASSERT(Unknown);

    // Wow, if this was any easier, it'd be completely unmaintainable! --trs
    STD_CREATE_BODY(CMiniportTopologySolo,Unknown,UnknownOuter,PoolType);
}

//
// CMiniportTopologySolo::NonDelegatingQueryInterface()
//
// Obtains an interface.  This function works just like a COM QueryInterface
// call and is used if the object is not being aggregated.
//
STDMETHODIMP
CMiniportTopologySolo::
NonDelegatingQueryInterface
(
    IN      REFIID  Interface,
    OUT     PVOID * Object
)
{
    PAGED_CODE();

    ASSERT(Object);

    t << "[CMiniportTopologySolo::NonDelegatingQueryInterface]\n";

    if (IsEqualGUIDAligned(Interface,IID_IUnknown))
    {
        *Object = PVOID(PUNKNOWN(this));
    }
    else
    if (IsEqualGUIDAligned(Interface,IID_IMiniport))
    {
        *Object = PVOID(PMINIPORT(this));
    }
    else
    if (IsEqualGUIDAligned(Interface,IID_IMiniportTopology))
    {
        *Object = PVOID(PMINIPORTTOPOLOGY(this));
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

/*****************************************************************************
 * CMiniportTopologySolo::~CMiniportTopologySolo()
 *****************************************************************************
 * Destructor.
 */
CMiniportTopologySolo::~CMiniportTopologySolo
(   void
)
{
    PAGED_CODE();

    t << "[CMiniportTopologySolo::~CMiniportTopologySolo]\n";

}

/*****************************************************************************
 * CMiniportTopologySolo::Init()
 *****************************************************************************
 * Initializes a the miniport.
 */
STDMETHODIMP
CMiniportTopologySolo::Init
(
    IN      PUNKNOWN        UnknownAdapter,
    IN      PRESOURCELIST   ResourceList,
    IN      PPORTTOPOLOGY   Port
)
{
   PAGED_CODE();

   // ASSERT(UnknownAdapter); // this is optional
   ASSERT(ResourceList);
   ASSERT(Port);

   t << "[CMiniportTopologySolo::Init]\n";

   NTSTATUS status = STATUS_SUCCESS;

   return status;
}

//
// CMiniportTopologySolo::GetDescription()
//
// Gets the topology. This is the reason this object exists, mostly.
//
STDMETHODIMP
CMiniportTopologySolo::GetDescription
(
    OUT     PPCFILTER_DESCRIPTOR *  OutFilterDescriptor
)
{
    PAGED_CODE();

    ASSERT(OutFilterDescriptor);

    t << "[CMiniportTopologySolo::GetDescription]\n";

    *OutFilterDescriptor = &MiniportFilterDescriptor;

    return STATUS_SUCCESS;
}

//
// topology data structures
//
// Initially for testing:
//  wave<0-----DAC--->out
// capture<1---ADC<---in
//
// We can deal with routing later...
//
/*  wave>-------VOL---------------------+
 *                                      |
 * synth>-------VOL--+------------------+
 *                   |                  |
 *                   +--SWITCH_2X2--+   |
 *                                  |   |
 *    cd>-------VOL--+--SWITCH----------+
 *                   |              |   |
 *                   +--SWITCH_2X2--+   |
 *                                  |   |
 *   aux>-------VOL--+--SWITCH----------+
 *                   |              |   |
 *                   +--SWITCH_2X2--+   |
 *                                  |   |
 *   mic>--AGC--VOL--+--SWITCH----------+--VOL--BASS--TREBLE--GAIN-->lineout
 *                   |              |
 *                   +--SWITCH_1X2--+-------------------------GAIN-->wavein
 *
 */
 
/*****************************************************************************
 * PinDataRangesBridge
 *****************************************************************************
 * Structures indicating range of valid format values for bridge pins.
 * Should be the same as bridge pins in the wave miniport.
 */
static
KSDATARANGE PinDataRangesBridge[] =
{
   {
      sizeof(KSDATARANGE),
      0,
      0,
      0,
      STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
      STATICGUIDOF(KSDATAFORMAT_SUBTYPE_ANALOG),
      STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE)
   }
};

/*****************************************************************************
 * PinDataRangePointersBridge
 *****************************************************************************
 * List of pointers to structures indicating range of valid format values
 * for audio bridge pins.
 */
static
PKSDATARANGE PinDataRangePointersBridge[] =
{
    &PinDataRangesBridge[0]
};

/*****************************************************************************
 * MiniportPins
 *****************************************************************************
 * List of pins.
 */
static
PCPIN_DESCRIPTOR 
MiniportPins[CMiniportTopologySolo::N_MINIPORT_PINS] =
{
    // Capture bridge pin (to waveminiport)
   // PIN_CAPTURE_SRC
   {
        0,0,0,  // InstanceCount
        NULL,   // AutomationTable
        {       // KsPinDescriptor
            0,                                          // InterfacesCount
            NULL,                                       // Interfaces
            0,                                          // MediumsCount
            NULL,                                       // Mediums
            SIZEOF_ARRAY(PinDataRangePointersBridge),   // DataRangesCount
            PinDataRangePointersBridge,                 // DataRanges
            KSPIN_DATAFLOW_OUT,                         // DataFlow
            KSPIN_COMMUNICATION_NONE,                   // Communication
            &KSNODETYPE_DIGITAL_AUDIO_INTERFACE,        // Category
            NULL,                                       // Name
            0                                           // Reserved
        }
    },

    // Render bridge pin (from waveminiport)
    // PIN_RENDER_DEST
   {
        0,0,0,  // InstanceCount
        NULL,   // AutomationTable
        {       // KsPinDescriptor
            0,                                          // InterfacesCount
            NULL,                                       // Interfaces
            0,                                          // MediumsCount
            NULL,                                       // Mediums
            SIZEOF_ARRAY(PinDataRangePointersBridge),   // DataRangesCount
            PinDataRangePointersBridge,                 // DataRanges
            KSPIN_DATAFLOW_IN,                          // DataFlow
            KSPIN_COMMUNICATION_NONE,                   // Communication
            &KSNODETYPE_DIGITAL_AUDIO_INTERFACE,        // Category
            NULL,                                       // Name
            0                                           // Reserved
        }
    },

    // Generic all inputs 
    // PIN_SOLOIN_DEST
    {
        0,0,0,  // InstanceCount
        NULL,   // AutomationTable
        {       // KsPinDescriptor
            0,                                          // InterfacesCount
            NULL,                                       // Interfaces
            0,                                          // MediumsCount
            NULL,                                       // Mediums
            SIZEOF_ARRAY(PinDataRangePointersBridge),   // DataRangesCount
            PinDataRangePointersBridge,                 // DataRanges
            KSPIN_DATAFLOW_IN,                          // DataFlow
            KSPIN_COMMUNICATION_NONE,                   // Communication
            &KSNODETYPE_ANALOG_CONNECTOR,               // Category
            &KSAUDFNAME_RECORDING_SOURCE,               // Name (this name shows up as
                                                        // the playback panel name in SoundVol)
            0                                           // Reserved
        }
    },

    // Generic all outputs 
    // PIN_SOLOOUT_SRC
    {
        0,0,0,  // InstanceCount
        NULL,   // AutomationTable
        {       // KsPinDescriptor
            0,                                          // InterfacesCount
            NULL,                                       // Interfaces
            0,                                          // MediumsCount
            NULL,                                       // Mediums
            SIZEOF_ARRAY(PinDataRangePointersBridge),   // DataRangesCount
            PinDataRangePointersBridge,                 // DataRanges
            KSPIN_DATAFLOW_OUT,                         // DataFlow
            KSPIN_COMMUNICATION_NONE,                   // Communication
            &KSNODETYPE_ANALOG_CONNECTOR,               // Category
            &KSAUDFNAME_WAVE_OUT_MIX,                   // Name (this name shows up as
                                                        // the playback panel name in SoundVol)
            0                                           // Reserved
        }
    }

};


//
// PropertiesNoCpu
// 
// Minimal properties for nodes where there's nothing to support
// but we want to indicate that it's not expensive to run. :)
//
static
PCPROPERTY_ITEM PropertiesNoCpu[] =
{
    {
        &KSPROPSETID_Audio,
        KSPROPERTY_AUDIO_CPU_RESOURCES,
        KSPROPERTY_TYPE_GET,
        PropertyHandler_NoCpuResources
    }
};


DEFINE_PCAUTOMATION_TABLE_PROP(AutomationADC,PropertiesNoCpu);
DEFINE_PCAUTOMATION_TABLE_PROP(AutomationDAC,PropertiesNoCpu);



/*****************************************************************************
 * TopologyNodes
 *****************************************************************************
 * List of node identifiers.
 * We need a/d and d/a nodes, as well as some switch nodes so we can have
 * general-purpose routing. Eventually.
 */
enum {
   TOPOLOGY_NODE_ADC,
   TOPOLOGY_NODE_DAC,
   N_TOPOLOGY_NODES
};
static
PCNODE_DESCRIPTOR TopologyNodes[N_TOPOLOGY_NODES] =
{
    // ADC (capture)
    {
        0,                      // Flags
        &AutomationADC,         // AutomationTable
        &KSNODETYPE_ADC,        // Type
        NULL                    // Name
    },
    // DAC (render)
    {
        0,                      // Flags
        &AutomationDAC,         // AutomationTable
        &KSNODETYPE_DAC,        // Type
        NULL                    // Name
    }
};

/*****************************************************************************
 * ConnectionTable
 */
static
PCCONNECTION_DESCRIPTOR MiniportConnections[] =
{   //  {FromNode,               FromPin,          
    //   ToNode,                 ToPin}

   { PCFILTER_NODE,            CMiniportTopologySolo::PIN_SOLOIN_DEST,  
     TOPOLOGY_NODE_ADC,        1 },    // analog in to adc in

   // ADC to stream pin (capture).
   { TOPOLOGY_NODE_ADC,        0,  
   PCFILTER_NODE,            CMiniportTopologySolo::PIN_CAPTURE_SRC },    

   { PCFILTER_NODE,            CMiniportTopologySolo::PIN_RENDER_DEST,  
     TOPOLOGY_NODE_DAC,        1 },    // Stream in to DAC.

   // DAC to Bridge.
   { TOPOLOGY_NODE_DAC,        0,  
   PCFILTER_NODE,            CMiniportTopologySolo::PIN_SOLOOUT_SRC }     
};

/*****************************************************************************
 * MiniportFilterDescription
 *****************************************************************************
 * Complete miniport description.
 */
static PCFILTER_DESCRIPTOR MiniportFilterDescriptor =
{
    0,                                  // Version
    NULL,                               // AutomationTable
    sizeof(PCPIN_DESCRIPTOR),           // PinSize
    SIZEOF_ARRAY(MiniportPins),         // PinCount
    MiniportPins,                       // Pins
    sizeof(PCNODE_DESCRIPTOR),          // NodeSize
    SIZEOF_ARRAY(TopologyNodes),        // NodeCount
    TopologyNodes,                      // Nodes
    SIZEOF_ARRAY(MiniportConnections),  // ConnectionCount
    MiniportConnections,                // Connections
    0,                                  // CategoryCount
    NULL                                // Categories
};


//
// Property handlers for this topology's nodes
//
// If a node is linked to the NoCpuResources handler, it never
// uses cpu resources, thus keeping this function minimal. :)
//
NTSTATUS PropertyHandler_NoCpuResources
(
    IN      PPCPROPERTY_REQUEST   PropertyRequest
)
{
    PAGED_CODE();

    ASSERT(PropertyRequest);

    t << "[mintopo.cpp::PropertyHandler_NoCpuResources]\n";

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    // validate node
    if(PropertyRequest->Node != ULONG(-1))
    {
        if(PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
        {
            if(PropertyRequest->ValueSize >= sizeof(LONG))
            {
                *(PLONG(PropertyRequest->Value)) = KSAUDIO_CPU_RESOURCES_NOT_HOST_CPU;
                PropertyRequest->ValueSize = sizeof(LONG);
                ntStatus = STATUS_SUCCESS;
            } else
            {
                ntStatus = STATUS_BUFFER_TOO_SMALL;
            }
        } 
    }

    return ntStatus;
}



#ifdef QQQ // comment out
//
// BasicSupportHandler() -- Helper function for level set/get requests
//
NTSTATUS BasicSupportHandler
(
    IN      PPCPROPERTY_REQUEST   PropertyRequest
)
{
    ASSERT(PropertyRequest);

    _DbgPrintF(DEBUGLVL_VERBOSE,("[BasicSupportHandler]"));

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    if(PropertyRequest->ValueSize >= (sizeof(KSPROPERTY_DESCRIPTION)))
    {
        // if return buffer can hold a KSPROPERTY_DESCRIPTION, return it
        PKSPROPERTY_DESCRIPTION PropDesc = PKSPROPERTY_DESCRIPTION(PropertyRequest->Value);

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

        // if return buffer cn also hold a range description, return it too
        if(PropertyRequest->ValueSize >= (sizeof(KSPROPERTY_DESCRIPTION) +
                                      sizeof(KSPROPERTY_MEMBERSHEADER) +
                                      sizeof(KSPROPERTY_STEPPING_LONG)))
        {
            // fill in the members header
            PKSPROPERTY_MEMBERSHEADER Members = PKSPROPERTY_MEMBERSHEADER(PropDesc + 1);

            Members->MembersFlags   = KSPROPERTY_MEMBER_STEPPEDRANGES;
            Members->MembersSize    = sizeof(KSPROPERTY_STEPPING_LONG);
            Members->MembersCount   = 1;
            Members->Flags          = 0;

            // fill in the stepped range
            PKSPROPERTY_STEPPING_LONG Range = PKSPROPERTY_STEPPING_LONG(Members + 1);

            switch(PropertyRequest->Node)
            {
                case WAVEOUT_VOLUME:
                case SYNTH_VOLUME:
                case CD_VOLUME:
                case LINEIN_VOLUME:
                case MIC_VOLUME:
                case LINEOUT_VOL:
                    Range->Bounds.SignedMaximum = 0;            // 0   (dB) * 0x10000
                    Range->Bounds.SignedMinimum = 0xFFC20000;   // -62 (dB) * 0x10000
                    Range->SteppingDelta        = 0x20000;      // 2   (dB) * 0x10000
                    break;

                case LINEOUT_GAIN:
                case WAVEIN_GAIN:
                    Range->Bounds.SignedMaximum = 0x120000;     // 18  (dB) * 0x10000
                    Range->Bounds.SignedMinimum = 0;            // 0   (dB) * 0x10000
                    Range->SteppingDelta        = 0x60000;      // 6   (dB) * 0x10000
                    break;

                case LINEOUT_BASS:
                case LINEOUT_TREBLE:
                    Range->Bounds.SignedMaximum = 0xE0000;      // 14  (dB) * 0x10000
                    Range->Bounds.SignedMinimum = 0xFFF20000;   // -14 (dB) * 0x10000
                    Range->SteppingDelta        = 0x20000;      // 2   (dB) * 0x10000
                    break;

            }
            Range->Reserved         = 0;

            _DbgPrintF(DEBUGLVL_BLAB, ("---Node: %d  Max: 0x%X  Min: 0x%X  Step: 0x%X",PropertyRequest->Node,
                                                                                       Range->Bounds.SignedMaximum,
                                                                                       Range->Bounds.SignedMinimum,
                                                                                       Range->SteppingDelta));

            // set the return value size
            PropertyRequest->ValueSize = sizeof(KSPROPERTY_DESCRIPTION) +
                                         sizeof(KSPROPERTY_MEMBERSHEADER) +
                                         sizeof(KSPROPERTY_STEPPING_LONG);
        } else
        {
            // set the return value size
            PropertyRequest->ValueSize = sizeof(KSPROPERTY_DESCRIPTION);
        }
        ntStatus = STATUS_SUCCESS;

    } else if(PropertyRequest->ValueSize >= sizeof(ULONG))
    {
        // if return buffer can hold a ULONG, return the access flags
        PULONG AccessFlags = PULONG(PropertyRequest->Value);

        *AccessFlags = KSPROPERTY_TYPE_BASICSUPPORT |
                       KSPROPERTY_TYPE_GET |
                       KSPROPERTY_TYPE_SET;

        // set the return value size
        PropertyRequest->ValueSize = sizeof(ULONG);
        ntStatus = STATUS_SUCCESS;

    }

    return ntStatus;
}
#endif // QQQ



#ifdef QQQ // not ready for properties yet
/*****************************************************************************
 * PropertiesVolume
 *****************************************************************************
 * Properties for volume controls.
 */
static
PCPROPERTY_ITEM PropertiesVolume[] =
{
    { 
        &KSPROPSETID_Audio, 
        KSPROPERTY_AUDIO_VOLUMELEVEL,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_Level
    },
    {
        &KSPROPSETID_Audio,
        KSPROPERTY_AUDIO_CPU_RESOURCES,
        KSPROPERTY_TYPE_GET,
        PropertyHandler_CpuResources
    }
};

/*****************************************************************************
 * AutomationVolume
 *****************************************************************************
 * Automation table for volume controls.
 */
DEFINE_PCAUTOMATION_TABLE_PROP(AutomationVolume,PropertiesVolume);

/*****************************************************************************
 * PropertiesAgc
 *****************************************************************************
 * Properties for AGC controls.
 */
static
PCPROPERTY_ITEM PropertiesAgc[] =
{
    { 
        &KSPROPSETID_Audio, 
        KSPROPERTY_AUDIO_AGC,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_OnOff
    },
    {
        &KSPROPSETID_Audio,
        KSPROPERTY_AUDIO_CPU_RESOURCES,
        KSPROPERTY_TYPE_GET,
        PropertyHandler_CpuResources
    }
};

/*****************************************************************************
 * AutomationAgc
 *****************************************************************************
 * Automation table for Agc controls.
 */
DEFINE_PCAUTOMATION_TABLE_PROP(AutomationAgc,PropertiesAgc);

/*****************************************************************************
 * PropertiesMute
 *****************************************************************************
 * Properties for mute controls.
 */
static
PCPROPERTY_ITEM PropertiesMute[] =
{
    { 
        &KSPROPSETID_Audio, 
        KSPROPERTY_AUDIO_MUTE,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_OnOff
    },
    {
        &KSPROPSETID_Audio,
        KSPROPERTY_AUDIO_CPU_RESOURCES,
        KSPROPERTY_TYPE_GET,
        PropertyHandler_CpuResources
    }
};

/*****************************************************************************
 * AutomationMute
 *****************************************************************************
 * Automation table for mute controls.
 */
DEFINE_PCAUTOMATION_TABLE_PROP(AutomationMute,PropertiesMute);

/*****************************************************************************
 * PropertiesTone
 *****************************************************************************
 * Properties for tone controls.
 */
static
PCPROPERTY_ITEM PropertiesTone[] =
{
    { 
        &KSPROPSETID_Audio, 
        KSPROPERTY_AUDIO_BASS,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_Level
    },
    { 
        &KSPROPSETID_Audio, 
        KSPROPERTY_AUDIO_TREBLE,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_Level
    },
    {
        &KSPROPSETID_Audio,
        KSPROPERTY_AUDIO_CPU_RESOURCES,
        KSPROPERTY_TYPE_GET,
        PropertyHandler_CpuResources
    }
};

/*****************************************************************************
 * AutomationTone
 *****************************************************************************
 * Automation table for tone controls.
 */
DEFINE_PCAUTOMATION_TABLE_PROP(AutomationTone,PropertiesTone);

/*****************************************************************************
 * PropertiesSupermix
 *****************************************************************************
 * Properties for supermix controls.
 */
static
PCPROPERTY_ITEM PropertiesSupermix[] =
{
    { 
        &KSPROPSETID_Audio, 
        KSPROPERTY_AUDIO_MIX_LEVEL_CAPS,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_SuperMixCaps
    },
    { 
        &KSPROPSETID_Audio, 
        KSPROPERTY_AUDIO_MIX_LEVEL_TABLE,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_SuperMixTable
    },
    {
        &KSPROPSETID_Audio,
        KSPROPERTY_AUDIO_CPU_RESOURCES,
        KSPROPERTY_TYPE_GET,
        PropertyHandler_CpuResources
    }
};

/*****************************************************************************
 * AutomationSupermix
 *****************************************************************************
 * Automation table for supermix controls.
 */
DEFINE_PCAUTOMATION_TABLE_PROP(AutomationSupermix,PropertiesSupermix);

#ifdef EVENT_SUPPORT
/*****************************************************************************
 * The Event for the Master Volume (or other nodes)
 *****************************************************************************
 * Generic event for nodes.
 */
static PCEVENT_ITEM NodeEvent[] =
{
    // This is a generic event for nearly every node property.
    {
        &KSEVENTSETID_AudioControlChange,   // Something changed!
        KSEVENT_CONTROL_CHANGE,             // The only event-property defined.
        KSEVENT_TYPE_ENABLE | KSEVENT_TYPE_BASICSUPPORT,
        CMiniportTopologySB16::EventHandler
    }
};

/*****************************************************************************
 * AutomationVolumeWithEvent
 *****************************************************************************
 * This is the automation table for Volume events.
 * You can create Automation tables with event support for any type of nodes
 * (e.g. mutes) with just adding the generic event above. The automation table
 * then gets added to every node that should have event support.
 */
DEFINE_PCAUTOMATION_TABLE_PROP_EVENT (AutomationVolumeWithEvent, PropertiesVolume, NodeEvent);
#endif

#endif // QQQ
