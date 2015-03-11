// $Id: SSSolo.cpp,v 1.7 2001/08/23 21:06:30 tekhedd Exp $
// Copyright(C) 2001 Tom Surace. All rights reserved.
//
// Generated by DriverWizard version DriverStudio 2.0.1 (Build 53)
// Requires Compuware's DriverWorks classes
// And that would be that, if it just stopped there. However, now I'm
// folding in the multimedia examples from Micrsoft (the ones written
// by a guy who only worked there for a year and which are specific
// to the soundblaster and which have comments which don't help you
// unless you already know what everything does anyway). --trs
//

#define VDW_MAIN
#include <vdw.h>

// Include definitions for DWORD and other stupid macros
#include <windef.h>

#include "SSSolo.h"
// #include "SolowdmDevice.h" // NOPE
#include "adapter.h"

#pragma hdrstop("SSSolo.pch")

// Generated by DriverWizard version DriverStudio 2.0.1 (Build 53)

// Set a default 32-bit tag value to be stored with each heap block
// allocated by operator new. Use BoundsChecker to view the memory pool.
// This value can be overridden using the global function SetPoolTag().
POOLTAG DefaultPoolTag('oloS');

// Create the global driver trace object
// TODO:	Use KDebugOnlyTrace if you want trace messages
//			to appear only in debug builds.  Use KTrace if
//			you want trace messages to always appear.	
KTrace t("SSSolo");

/////////////////////////////////////////////////////////////////////
// Begin INIT section
#pragma code_seg("INIT")

DECLARE_DRIVER_CLASS(SSSolo, NULL)

/////////////////////////////////////////////////////////////////////
//  SSSolo::DriverEntry
//
//	Routine Description:
//		This is the first entry point called by the system when the
//		driver is loaded.
// 
//	Parameters:
//		RegistryPath - String used to find driver parameters in the
//			registry.  To locate SSSolo look for:
//			HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\SSSolo
//
//	Return Value:
//		NTSTATUS - Return STATUS_SUCCESS if no errors are encountered.
//			Any other indicates to the system that an error has occured.
//
//	Comments:
//

NTSTATUS SSSolo::DriverEntry(PUNICODE_STRING RegistryPath)
{
	t << "In DriverEntry\n";


	// Open the "Parameters" key under the driver
	KRegistryKey Params(RegistryPath, L"Parameters");
	if ( NT_SUCCESS(Params.LastError()) )
	{
#if DBG
		ULONG bBreakOnEntry = FALSE;
		// Read "BreakOnEntry" value from registry
		Params.QueryValue(L"BreakOnEntry", &bBreakOnEntry);
		// If requested, break into debugger
		if (bBreakOnEntry) DbgBreakPoint();
#endif
		// Load driver data members from the registry
		LoadRegistryParameters(Params);
	}
	m_Unit = 0;

	return STATUS_SUCCESS;
}


/////////////////////////////////////////////////////////////////////
//  SSSolo::LoadRegistryParameters
//
//	Routine Description:
//		Load driver data members from the registry.
// 
//	Parameters:
//		Params - Open registry key pointing to "Parameters"
//
//	Return Value:
//		None
//			
//	Comments:
//		Member variables are updated with values read from registry.
//
//		The parameters are found as values under the "Parameters" key,	
//		HKLM\SYSTEM\CurrentControlSet\Services\SSSolo\Parameters\...
//

void SSSolo::LoadRegistryParameters(KRegistryKey &Params)
{

	m_bBreakOnEntry = FALSE;
	Params.QueryValue(L"BreakOnEntry", &m_bBreakOnEntry);
	t << "m_bBreakOnEntry loaded from registry, resulting value: [" << m_bBreakOnEntry << "]\n";

	m_IsSlaveEnabled = 0x0000;
	Params.QueryValue(L"IsSlaveEnabled", &m_IsSlaveEnabled);
	t << "m_IsSlaveEnabled loaded from registry, resulting value: [" << m_IsSlaveEnabled << "]\n";

	m_Routing = 0x0000;
	Params.QueryValue(L"Routing", &m_Routing);
	t << "m_Routing loaded from registry, resulting value: [" << m_Routing << "]\n";

	m_DmaPageFrames = 0x0000;
	Params.QueryValue(L"DmaPageFrames", &m_DmaPageFrames);
	t << "m_DmaPageFrames loaded from registry, resulting value: [" << m_DmaPageFrames << "]\n";

}
// End INIT section
/////////////////////////////////////////////////////////////////////
#pragma code_seg()

/////////////////////////////////////////////////////////////////////
//  SSSolo::AddDevice
//
//	Routine Description:
//		Called when the system detects a device for which this
//		driver is responsible.
//
//	Parameters:
//		Pdo - Physical Device Object. This is a pointer to a system device
//			object that represents the physical device.
//
//	Return Value:
//		NTSTATUS - Success or failure code.
//
//	Comments:
//		This function creates the Functional Device Object, or FDO. The FDO
//		enables this driver to handle requests for the physical device. 
//
NTSTATUS SSSolo::AddDevice(PDEVICE_OBJECT Pdo)
{
   NTSTATUS status = STATUS_SUCCESS;
      
	t << "AddDevice called\n";

   //
   // Tell the class driver to add the device. You have to use
   // this function to init the miniports, or at least do some 
   // serious hacking to construct a KDevice object that's
   // actually attached to the FDO created by the
   // port class functions... (Not impossible, should consider
   // it later 'cause the KDevice class hierarchy rocks.)
   //
   status = PcAddAdapterDevice( DriverObject(), // StartDevice context1
                                Pdo,            // StartDevice context2 sort of
                                PCPFNSTARTDEVICE( CAdapterSoloStartDevice ),
                                MAX_MINIPORTS,
                                0 );


	return status;
}

