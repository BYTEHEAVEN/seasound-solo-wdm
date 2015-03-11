# Microsoft Developer Studio Project File - Name="solowdm" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=solowdm - Win32 Checked
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "solowdm.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "solowdm.mak" CFG="solowdm - Win32 Checked"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "solowdm - Win32 Free" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "solowdm - Win32 Checked" (based on\
 "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "solowdm - Win32 Free"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Free"
# PROP BASE Intermediate_Dir "Free"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Free"
# PROP Intermediate_Dir "Free"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /Gz /W3 /Oy /Gy /D "WIN32" /YX /Oxs /c
# ADD CPP /nologo /Gz /W3 /Oy /Gy /I "$(BASEDIR)\inc\ddk\wdm" /I "$(BASEDIR)\inc\ddk" /I "$(BASEDIR)\inc" /I "$(BASEDIR)\inc\win98" /I "$(CPU)\\" /I "." /I "..\inc" /I "$(DRIVERWORKS)\include" /I "$(DRIVERWORKS)\source" /I "$(DRIVERWORKS)\include\dep_vxd" /D WIN32=100 /D "STD_CALL" /D CONDITION_HANDLING=1 /D NT_UP=1 /D NT_INST=0 /D _NT1X_=100 /D WINNT=1 /D _WIN32_WINNT=0x0400 /D WIN32_LEAN_AND_MEAN=1 /D DEVL=1 /D FPO=1 /D "_IDWBUILD" /D "NDEBUG" /D _DLL=1 /D _X86_=1 /D $(CPU)=1 /D NTVERSION='WDM' /D "_WIN32" /D "UNICODE" /D "_UNICODE" /FR /YX /FD /Oxs /QI0f    /Zel -cbstring /QIfdiv- /QIf /GF           /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o NUL /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o NUL /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /i "$(BASEDIR)\inc\ddk\wdm" /i "$(BASEDIR)\inc\ddk" /i "$(BASEDIR)\inc" /i "$(BASEDIR)\inc\Win98" /i "..\inc" /i "$(DRIVERWORKS)\include" /i "$(DRIVERWORKS)\source" /i "$(DRIVERWORKS)\include\dep_vxd" /d "NDEBUG" /d NTVERSION='WDM' /d "_WIN32" /d "UNICODE" /d "_UNICODE"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo /o"objfre\i386/solowdm.bsc"
LINK32=link.exe
# ADD BASE LINK32 /nologo /machine:IX86
# ADD LINK32 wdm.lib wmilib.lib       free\vdw_wdm.lib  \portcls.lib  \libcntpr.lib  \ntoskrnl.lib  ..\lib\\stdunk.lib  /nologo /base:"0x10000" /version:5.0 /stack:0x40000,0x1000 /entry:"DriverEntry" /map /machine:IX86 /nodefaultlib /out:"objfre\i386\solowdm.sys" /libpath:"$(BASEDIR)\libfre\i386" /libpath:"$(BASEDIR)\lib\i386\free" /driver:WDM /debug:FULL /IGNORE:4001,4037,4039,4065,4070,4078,4087,4089,4096   /MERGE:_PAGE=PAGE /MERGE:_TEXT=.text /SECTION:INIT,d /MERGE:.rdata=.text  /FULLBUILD /RELEASE /FORCE:MULTIPLE /OPT:REF /OPTIDATA   /align:0x20  /osversion:5.00       /subsystem:native,1.10

!ELSEIF  "$(CFG)" == "solowdm - Win32 Checked"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Checked"
# PROP BASE Intermediate_Dir "Checked"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Checked"
# PROP Intermediate_Dir "Checked"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /Gz /W3 /Zi /Od /D "WIN32" /YX /FD /c
# ADD CPP /nologo /Gz /W3 /Z7 /Oi /Gy /I "$(BASEDIR)\inc\ddk\wdm" /I "$(BASEDIR)\inc\ddk" /I "$(BASEDIR)\inc" /I "$(BASEDIR)\inc\win98" /I "$(CPU)\\" /I "." /I "..\inc" /I "$(DRIVERWORKS)\include" /I "$(DRIVERWORKS)\source" /I "$(DRIVERWORKS)\include\dep_vxd" /D WIN32=100 /D "RDRDBG" /D "SRVDBG" /D "STD_CALL" /D CONDITION_HANDLING=1 /D NT_UP=1 /D NT_INST=0 /D _NT1X_=100 /D WINNT=1 /D _WIN32_WINNT=0x0400 /D WIN32_LEAN_AND_MEAN=1 /D DBG=1 /D DEVL=1 /D FPO=0 /D "NDEBUG" /D _DLL=1 /D _X86_=1 /D $(CPU)=1 /D NTVERSION='WDM' /D "_WIN32" /D "UNICODE" /D "_UNICODE" /FR /YX /FD /QI0f    /Zel -cbstring /QIfdiv- /QIf /GF           /c
# ADD BASE MTL /nologo /mktyplib203 /o NUL /win32
# ADD MTL /nologo /mktyplib203 /o NUL /win32
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409 /i "$(BASEDIR)\inc\ddk\wdm" /i "$(BASEDIR)\inc\ddk" /i "$(BASEDIR)\inc" /i "$(BASEDIR)\inc\Win98" /i "..\inc" /i "$(DRIVERWORKS)\include" /i "$(DRIVERWORKS)\source" /i "$(DRIVERWORKS)\include\dep_vxd" /d NTVERSION='WDM' /d "_WIN32" /d "UNICODE" /d "_UNICODE"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo /o"objchk\i386/solowdm.bsc"
LINK32=link.exe
# ADD BASE LINK32 /nologo /machine:IX86
# ADD LINK32 wdm.lib wmilib.lib       checked\vdw_wdm.lib  \portcls.lib  \libcntpr.lib  \ntoskrnl.lib  ..\lib\\stdunk.lib  /nologo /base:"0x10000" /version:5.0 /stack:0x40000,0x1000 /entry:"DriverEntry" /incremental:no /map /machine:IX86 /nodefaultlib /out:"objchk\i386\solowdm.sys" /libpath:"$(BASEDIR)\libchk\i386" /libpath:"$(BASEDIR)\lib\i386\checked" /driver:WDM /debug:notmapped,FULL /IGNORE:4001,4037,4039,4065,4070,4078,4087,4089,4096   /MERGE:_PAGE=PAGE /MERGE:_TEXT=.text /SECTION:INIT,d /MERGE:.rdata=.text  /FULLBUILD /RELEASE /FORCE:MULTIPLE /OPT:REF /OPTIDATA   /align:0x20  /osversion:5.00       /subsystem:native,1.10

!ENDIF 

# Begin Target

# Name "solowdm - Win32 Free"
# Name "solowdm - Win32 Checked"
# Begin Group "Source Files"

# PROP Default_Filter ".c;.cpp"
# Begin Source File

SOURCE=.\adapter.cpp
# End Source File
# Begin Source File

SOURCE=.\drentry.cpp
# End Source File
# Begin Source File

SOURCE=.\guids.cpp
# End Source File
# Begin Source File

SOURCE=.\mcu.c
# End Source File
# Begin Source File

SOURCE=.\mintopo.cpp
# End Source File
# Begin Source File

SOURCE=.\minwave.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter ".h"
# Begin Source File

SOURCE=.\adapter.h
# End Source File
# Begin Source File

SOURCE=.\config.h
# End Source File
# Begin Source File

SOURCE=.\DmaChannelBuffer.h
# End Source File
# Begin Source File

SOURCE=.\eeprom.h
# End Source File
# Begin Source File

SOURCE=.\function.h
# End Source File
# Begin Source File

SOURCE=.\iadaptersolo.h
# End Source File
# Begin Source File

SOURCE=.\ice.h
# End Source File
# Begin Source File

SOURCE=.\mcu.h
# End Source File
# Begin Source File

SOURCE=.\mintopo.h
# End Source File
# Begin Source File

SOURCE=.\minwave.h
# End Source File
# Begin Source File

SOURCE=.\SoloDriver.h
# End Source File
# Begin Source File

SOURCE=.\SoloWdmDevice.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter ".rc;.mc;.mof"
# Begin Source File

SOURCE=.\SSSolo.rc

!IF  "$(CFG)" == "solowdm - Win32 Free"

!ELSEIF  "$(CFG)" == "solowdm - Win32 Checked"

!ENDIF 

# End Source File
# End Group
# End Target
# End Project
