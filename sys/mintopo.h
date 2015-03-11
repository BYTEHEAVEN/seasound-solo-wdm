// $Id: mintopo.h,v 1.5 2001/09/03 15:37:04 tekhedd Exp $
// Copyright(C) 2001 Tom Surace. All rights reserved.
//
// (Sample code is also..)
// Copyright (c) 1997-2000 Microsoft Corporation. All Rights Reserved.
//

#ifndef _MINTOPO_H_
#define _MINTOPO_H_

#include "stdunk.h"
#include "portcls.h"
#include "ksdebug.h"

// Prevent addition of yet more versions of operator new
#define _NEW_DELETE_OPERATORS_
#include "kcom.h"

extern NTSTATUS CreateMiniportTopologySolo
(
    OUT     PUNKNOWN *  Unknown,
    IN      PUNKNOWN    UnknownOuter    OPTIONAL,
    IN      POOL_TYPE   PoolType
);

// Property handler for no-cpu nodes :)
extern NTSTATUS PropertyHandler_NoCpuResources
(
    IN      PPCPROPERTY_REQUEST   PropertyRequest
);


//
// CMiniportTopologySolo -- based loosely on the sb16 example
//
class CMiniportTopologySolo 
:   public IMiniportTopology, 
    public CUnknown
{
private:

public:
   enum {
      PIN_CAPTURE_SRC = 0,
      PIN_RENDER_DEST = 1,
      PIN_SOLOIN_DEST = 2,
      PIN_SOLOOUT_SRC = 3,
      N_MINIPORT_PINS 
   };

    /*************************************************************************
     * The following two macros are from STDUNK.H.  DECLARE_STD_UNKNOWN()
     * Supposedly this makes it easy to handle aggregation and such. :/
     */
    DECLARE_STD_UNKNOWN();
    DEFINE_STD_CONSTRUCTOR(CMiniportTopologySolo);

    ~CMiniportTopologySolo();

    /*************************************************************************
     * IMiniport methods
     */
    STDMETHODIMP 
    GetDescription
    (   OUT     PPCFILTER_DESCRIPTOR *  OutFilterDescriptor
    );
    STDMETHODIMP 
    DataRangeIntersection
    (   IN      ULONG           PinId
    ,   IN      PKSDATARANGE    DataRange
    ,   IN      PKSDATARANGE    MatchingDataRange
    ,   IN      ULONG           OutputBufferLength
    ,   OUT     PVOID           ResultantFormat     OPTIONAL
    ,   OUT     PULONG          ResultantFormatLength
    )
    {
        return STATUS_NOT_IMPLEMENTED;
    }

    /*************************************************************************
     * IMiniportTopology methods
     */
    STDMETHODIMP Init
    (
        IN      PUNKNOWN        UnknownAdapter,
        IN      PRESOURCELIST   ResourceList,
        IN      PPORTTOPOLOGY   Port
    );

};

#endif // _MINTOPO_H_

