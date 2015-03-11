// $Id: SoloDriver.h,v 1.1 2001/11/25 01:03:13 tekhedd Exp $
// Copyright (c) 2001 Tom Surace All rights reserved.
//
// Generated by DriverWizard version DriverStudio 2.0.1 (Build 53)
// Requires Compuware's DriverWorks classes
//

#ifndef __SoloDriver_h__
#define __SoloDriver_h__

// Global driver trace object
extern	KDebugOnlyTrace	t;

class SoloDriver : public KDriver
{
	SAFE_DESTRUCTORS
public:
	virtual NTSTATUS DriverEntry(PUNICODE_STRING RegistryPath);
	virtual NTSTATUS AddDevice(PDEVICE_OBJECT Pdo);

   NTSTATUS StartWdmDevice( 
       PDEVICE_OBJECT  DeviceObject,   // Device object. (FDO)
       PIRP            Irp,            // IO request packet.
       PRESOURCELIST   ResourceList    // List of hardware resources.
   );

   void    LoadRegistryParameters(KRegistryKey &Params);


	// The following data members are loaded from the registry during DriverEntry
	ULONG   _bBreakOnEntry;
	
private:
	int	         _unit;   // index of next unit to create
   PDEVICE_OBJECT _thePdo; // hack to work around port class :(
};

#endif			// __BHSoloDriver_h__
