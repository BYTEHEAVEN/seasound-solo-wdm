// $Id: drentry.cpp,v 1.6 2001/11/25 01:03:13 tekhedd Exp $
// Copyright(C) 2001 Tom Surace. All rights reserved.
//

// Don't declare DRIVER_MAIN so we can do our own DriverEntry. :(
#include <vdw.h>

// Include definitions for DWORD and other stupid macros
#include <windef.h>
#include <portcls.h>

#include "adapter.h"

#pragma hdrstop("SSSolo.pch")

// --- Local functions

extern "C" NTSTATUS AddDevice
(
    IN PDRIVER_OBJECT   DriverObject,
    IN PDEVICE_OBJECT   PhysicalDeviceObject
);
extern "C" NTSTATUS DispatchIoctl
(
    IN      PDEVICE_OBJECT  pDeviceObject,
    IN      PIRP            pIrp
);

// Set a default 32-bit tag value to be stored with each heap block
// allocated by operator new. 
POOLTAG DefaultPoolTag('oloS');

// Create the global driver trace object
// TODO:	Use KDebugOnlyTrace if you want trace messages
//			to appear only in debug builds.  Use KTrace if
//			you want trace messages to always appear.	
KTrace t("SSSolo");


//
// DriverEntry -- where it all begins
//
#pragma code_seg("INIT") // this function only used once
extern "C" NTSTATUS DriverEntry
(
    IN PDRIVER_OBJECT   DriverObject,
    IN PUNICODE_STRING  RegistryPathName
)
{
   PAGED_CODE();
   int fail = 0;

   if (fail)
      return STATUS_UNSUCCESSFUL;

#if DBG
   // Initialize the connection to BoundsChecker. If it doesn't exist, this
   // code is supposed to work anyway...
	BoundsChecker::Init(DriverObject);
#endif

	/*
    *Initialize the C++ Run time library in DriverEntry. This will call the
    *constructors for any global variables.
    */
	InitializeCppRunTime();	// calls constructors for globals
   // Ordinarily, you would also need to call TerminateCppRunTime() in the
   // unload function.  And once I find that, we will. Apparently the Port
   // class doesn't expose this...

   t << "In DriverEntry\n";

   //
   // Let the class driver do this. Unfortunately this doesn't
   // coexist nicely with DriverWorks...
   //
   NTSTATUS ntStatus = PcInitializeAdapterDriver( DriverObject,
                                                  RegistryPathName,
                                                  AddDevice );

#ifdef QQQ // comment out
	// Open the "Parameters" key under the driver
	KRegistryKey Params(RegistryPath, L"Parameters");
	if ( NT_SUCCESS(Params.LastError()) )
	{
		// Load driver data members from the registry
		LoadRegistryParameters(Params);
	}
#endif // QQQ

   fail = 1; // prevent optimization
	return ntStatus;
}
#pragma code_seg()


//
// AddDevice -- standard device-add function for multimedia classes
//   as specified by the port class docs
//
#pragma code_seg("PAGE")
extern "C" NTSTATUS AddDevice
(
    IN PDRIVER_OBJECT   DriverObject,         // FDO
    IN PDEVICE_OBJECT   PhysicalDeviceObject
)
{
    PAGED_CODE();

    NTSTATUS       status = STATUS_SUCCESS;

    //
    // Tell the class driver to add the device. If you don't call
    // this, other Pc functions will crash big time. So don't try
    // to get clever. Note that PortClass uses the device extension
    // too, ruling out the use of any other framework classes that
    // use it. (Can I use alternate DriverWorks constructors though?)
    //
    status = PcAddAdapterDevice( DriverObject,
                                 PhysicalDeviceObject,
                                 PCPFNSTARTDEVICE( CAdapterSoloStartDevice ),
                                 MAX_MINIPORTS,
                                 (PORT_CLASS_DEVICE_EXTENSION_SIZE +
                                  sizeof(CAdapterSolo *)));

    if(!NT_SUCCESS(status))
       goto bail;

    // Filter all Ioctl's before passing them to PortClass. We may have
    // other plans for these calls. <diabolical grin>

    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchIoctl;

    // Register an ASIO interface to the same driver? (Using the
    // ioctl dispatcher registered above, perhaps?)

    //status = IoRegisterDeviceInterface( PhysicalDeviceObject,
    //                                      GUID_ISoloAsio,
    //                                      NULL, /* ref string */
    //                                      &soloAsioLink );
    //
    // RtlFreeUnicodeString( soloAsioLink );                               

bail:
    return status;
}
#pragma code_seg()


// 
// DispatchSomething -- find the pointer and dispatch this to our
//   device class object thingy.
//
#pragma code_seg("PAGE")
extern "C" NTSTATUS DispatchIoctl
(
    IN      PDEVICE_OBJECT  pDeviceObject,
    IN      PIRP            pIrp
)
{
   PAGED_CODE();

   ASSERT(pDeviceObject);
   ASSERT(pIrp);

   NTSTATUS ntStatus = STATUS_SUCCESS;

   // I'm really hoping that PortCls doesn't get clever and hand
   // me a different device object. :/ At least we shouldn't get
   // this call before the object is created...I hope.
   CAdapterSolo * ptr = 
      *((CAdapterSolo **)( (PBYTE)pDeviceObject->DeviceExtension 
                           + PORT_CLASS_DEVICE_EXTENSION_SIZE ));

   // If the solo adapter wants to handle this one, let it. 
   // Otherwise, we absolutely MUST pass this on to portclass or
   // everything will be very bad.
   if (ptr->OnIoctl( KIrp(pIrp), &ntStatus))
      return ntStatus;


//    if( I.MinorFunction() == IRP_MN_FILTER_RESOURCE_REQUIREMENTS )
//    {
        //
        // Do your resource requirements filtering here!!
        //
//        t << "[AdapterDispatchPnp] - IRP_MN_FILTER_RESOURCE_REQUIREMENTS\n";

        // set the return status
//        I.Status() = ntStatus;

//    }

    //
    // Pass the IRPs on to PortCls
    //
    ntStatus = PcDispatchIrp( pDeviceObject,
                              pIrp );

    return ntStatus;
}


extern "C" int _cdecl _purecall();
#pragma code_seg()
int _cdecl _purecall()
{
	ASSERT(FALSE);	// attempt to invoke a pure virtual function
	return 0;
}
