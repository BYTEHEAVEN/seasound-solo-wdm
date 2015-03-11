// $Id: minwave.h,v 1.16 2001/11/25 01:03:13 tekhedd Exp $
// Copyright(C) 2001 Tom Surace. All rights reserved.
// Except for the rights that attach to the sample code which is
// Copyright Microsoft Corporation.


#ifndef _SOLOWAVE_PRIVATE_H_
#define _SOLOWAVE_PRIVATE_H_

#include "stdunk.h"
#include "portcls.h"
#include "ksdebug.h"

// Prevent addition of yet more versions of operator new
#define _NEW_DELETE_OPERATORS_
#include "kcom.h"

// Local headers
// #include "adapter.h"

//
// CMiniportWaveCyclicSolo class
//

// Well, if you want to construct one, here's your function.
extern NTSTATUS CreateMiniportWaveCyclicSolo
(
    OUT PUNKNOWN     *  ppUnknown,
    IN  PUNKNOWN        pUnknownOuter  OPTIONAL,
    IN  POOL_TYPE       PoolType
);

class CMiniportWaveCyclicSolo
:   public IMiniportWaveCyclic,
    public CUnknown
{
public:
   // 
   // Name our pins. (Use an enum, what a concept.)
   //
   //        +---------------------------------+
   //  KS <--|CAPTURE_OUT        TOPOLOGY_IN   |<-- topology
   //  KS -->|RENDER_IN          TOPOLOGY_OUT  |--> topology
   //        +---------------------------------+
   //
   // These numbers match offsets in an array.
   enum
   {
      PIN_CAPTURE_OUT = 0,  // out to KS
      PIN_TOPOLOGY_IN = 1,  // in from topology
      PIN_RENDER_IN = 2,    
      PIN_TOPOLOGY_OUT = 3, // out to topology
      N_MINIPORT_PINS       // count (last)
   };

private:
	// Back reference to the driver device
   IAdapterSolo      * _adapter;               // Adapter common object.
   PPORTWAVECYCLIC     _portWave;              // Our MaxiPort.

   bool                _allocatedCapture;
   bool                _allocatedRender;

   // Internal helpers
   NTSTATUS ValidateFormat( IN      PKSDATAFORMAT   Format,
                             IN      BOOLEAN         isCapture);

   void CleanUp();

   // Property handler for the filter
public:
   static NTSTATUS PropertyHandlerDevSpecific_static
   (
     IN PPCPROPERTY_REQUEST   PropertyRequest
   );
private:
   NTSTATUS PropertyHandlerDevSpecific(IN PPCPROPERTY_REQUEST   PropertyRequest);

protected:
    //
    // protected methods for our Friends
    //
    friend class CMiniportWaveCyclicStreamSoloBase;

    // These are called by the ..CyclicStream classes when they go away
    // to tell us they've gone away.
    void DeallocateCapture();
    void DeallocateRender();

public:
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
    DEFINE_STD_CONSTRUCTOR(CMiniportWaveCyclicSolo);

    ~CMiniportWaveCyclicSolo();

    /*************************************************************************
     * This macro is from PORTCLS.H.  It lists all the interface's functions.
     */
    IMP_IMiniportWaveCyclic;

    /*************************************************************************
     * IWaveMiniportSolo methods (but it's not a base class --trs)
     */
    STDMETHODIMP_(void) ServiceRenderISR(void);
    STDMETHODIMP_(void) ServiceCaptureISR(void);
    
};


/*****************************************************************************
 * CMiniportWaveCyclicStreamSoloBase -- base class for 
 *   CMiniportWaveCyclicStreamSoloCapture and ...Render. Pure virtual
 * 
 * Solo wave miniport stream.  This object is associated with a streaming pin
 * and is created when a pin is created on the filter.  The class inherits
 * IMiniportWaveCyclicStream so it can expose this interface and CUnknown so
 * it automatically gets reference counting and aggregation support.
 *
 * OK, so the last paragraph tells you what you can figure out by reading the
 * DDK docs.
 *
 */
class CMiniportWaveCyclicStreamSoloBase
:   public IMiniportWaveCyclicStream,
    public CUnknown
{
protected:
    CMiniportWaveCyclicSolo *   _miniport;      // The Wave miniport that created us.
    IAdapterSolo  *             _adapter;       // hardware abstraction thing
    BOOLEAN                     _isCapture;     
    ULONG                       _channel;       // Index into channel list.
    USHORT                      _nChannels;     // 2, 10 or 12 channels
    USHORT                      _sampleBytes;   // 2 or 4
    KSSTATE                     _state;         // Stop, pause, run. You know.

    // Deallocate memory and stuff. Called from base class destructor
    // and Init()
    void CleanUp();

public:
    /*************************************************************************
     * The following two macros are from STDUNK.H.  DECLARE_STD_UNKNOWN()
     * defines inline IUnknown implementations that use CUnknown's aggregation
     * support.  NonDelegatingQueryInterface() is declared, but it cannot be
     * implemented generically.  Heck, if it could be implemented generically,
     * why would we do it as a macro anyway? Its definition appears in MINIPORT.CPP.
     * DEFINE_STD_CONSTRUCTOR() defines inline a constructor which accepts
     * only the outer unknown, which is used for aggregation.  The standard
     * create macro (in MINIPORT.CPP) uses this constructor.
     */
    DECLARE_STD_UNKNOWN();
    CMiniportWaveCyclicStreamSoloBase(PUNKNOWN pUnknownOuter, BOOLEAN isCapture)
       : CUnknown(pUnknownOuter),
         _isCapture(isCapture)
    {
       ; // do nothing
    }

    ~CMiniportWaveCyclicStreamSoloBase();


    // 
    // Implmementation of base class interfaces
    // 
    STDMETHODIMP SetFormat
    (   IN      PKSDATAFORMAT   DataFormat
    );
    STDMETHODIMP_(ULONG) SetNotificationFreq
    (   IN      ULONG           Interval,
        OUT     PULONG          FrameSize
    );
    STDMETHODIMP SetState
    (   IN      KSSTATE         State
    );
    STDMETHODIMP_(void) Silence
    (   IN      PVOID           Buffer,
        IN      ULONG           ByteCount
    );
    STDMETHODIMP NormalizePhysicalPosition
    (   IN OUT PLONGLONG        PhysicalPosition
    );

    // Functions that are called more often:
    STDMETHOD(GetPosition)
    (   THIS_
        OUT     PULONG          Position
    )   PURE;

    // Init -- if you override init you shoudl call the base
    //   class function too. :)
    STDMETHOD(Init)
    (   THIS_
        IN      CMiniportWaveCyclicSolo *   Miniport,
        IN      IAdapterSolo  *             adapter,
        IN      ULONG                       Channel,
        IN      PKSDATAFORMAT               DataFormat,
        IN      PPORTWAVECYCLIC             portWave,
        IN      PDMACHANNEL                 dmaChannel
    );

};

//
// TRS: Note on capture and render classes
//
// I just couldn't stand seeing the if (_isCapture) statements
// all through the wavecyclicstream miniports, especially in the
// functions that get called frequently. So for functions that
// are called a lot and are not difficult to maintain two different
// versions of, here are derived classes. :)
//
// Other functions may be moved here from the base class if 
// profiling shows that it would be good. :)
//
class CMiniportWaveCyclicStreamSoloCapture
:   public CMiniportWaveCyclicStreamSoloBase
{
public:
    CMiniportWaveCyclicStreamSoloCapture(PUNKNOWN pUnknownOuter)
       : CMiniportWaveCyclicStreamSoloBase(pUnknownOuter,
                                           TRUE /* isCapture */)
    {
       ;
    }

    STDMETHODIMP GetPosition
    (   
        OUT     PULONG          Position
    );

};

class CMiniportWaveCyclicStreamSoloRender
:   public CMiniportWaveCyclicStreamSoloBase
{
public:
    CMiniportWaveCyclicStreamSoloRender(PUNKNOWN pUnknownOuter)
       : CMiniportWaveCyclicStreamSoloBase(pUnknownOuter, 
                                           FALSE /* isCapture */)
    { ; }

    STDMETHODIMP GetPosition
    (   
        OUT     PULONG          Position
    );

};


#endif
