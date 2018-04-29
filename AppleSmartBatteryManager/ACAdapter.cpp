//
//  ACAdapter.cpp
//  ACPIBatteryManager
//
//  Created by RehabMan on 11/16/13.
//
//  Code/ideas liberally borrowed from VoodooBattery...
//

#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/pwr_mgt/IOPMPowerSource.h>
#include "IOPMPrivate.h"
#include "ACAdapter.h"

OSDefineMetaClassAndStructors(ACPIACAdapter, IOService)

bool ACPIACAdapter::init(OSDictionary* dict)
{
    if (!IOService::init(dict))
        return false;
    
    fTracker = NULL;
    fProvider = NULL;
    
    return true;
}

bool ACPIACAdapter::start(IOService* provider)
{
    if (!IOService::start(provider))
    {
        AlwaysLog("ACPIACAdapter: IOService::start failed!\n");
        return false;
    }
    
    fProvider = OSDynamicCast(IOACPIPlatformDevice, provider);
    if (!fProvider)
    {
        AlwaysLog("ACPIACAdapter: provider not IOACPIPlatformDevice\n");
        return false;
    }
    fProvider->retain();
    
	DebugLog("starting ACPIACAdapter.\n");
    
    // get tracker for notifications
    fTracker = OSDynamicCast(BatteryTracker, waitForMatchingService(serviceMatching(kBatteryTrackerService)));
    
    return true;
}

void ACPIACAdapter::stop(IOService* provider)
{
    OSSafeReleaseNULL(fProvider);
    OSSafeReleaseNULL(fTracker);
    
    IOService::stop(provider);
}

IOReturn ACPIACAdapter::setPowerState(unsigned long state, IOService* device)
{
    DebugLog("ACPIACAdapter::setPowerState: state: %u, device: %s\n", (unsigned int) state, device->getName());
    
    return kIOPMAckImplied;
}

IOReturn ACPIACAdapter::message(UInt32 type, IOService* provider, void* argument)
{
    DebugLog("ACPIACAdapter::message: type: %08X provider: %s\n", (unsigned int)type, provider->getName());
    
    if (type == kIOACPIMessageDeviceNotification && fProvider)
    {
        UInt32 acpi = 0;
		fProvider->evaluateInteger("_PSR", &acpi);
        if (kIOReturnSuccess == fProvider->evaluateInteger("_PSR", &acpi))
        {
            DebugLog("ACPIACAdapter::message setting AC %s\n", (acpi ? "connected" : "disconnected"));
            
            // notify system of change in AC state
            if (IOPMrootDomain* root = getPMRootDomain())
                root->receivePowerNotification(kIOPMSetACAdaptorConnected | acpi ? kIOPMSetValue : 0);
            else
                DebugLog("ACPIACAdapter::message could not notify OS about AC status\n");
            
            // notify battery managers of change in AC state
            if (NULL != fTracker)
                fTracker->notifyBatteryManagers(acpi);
        }
        else
        {
            AlwaysLog("ACPIACAdapter: ACPI method _PSR failed\n");
        }
    }
    return kIOReturnSuccess;
}

