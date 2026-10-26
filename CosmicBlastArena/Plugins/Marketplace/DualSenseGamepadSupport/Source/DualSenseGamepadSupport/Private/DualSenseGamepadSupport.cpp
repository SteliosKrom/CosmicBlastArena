// Copyright David Sanchez 2025. All Rights Reserved.
// DualSenseGamepadSupport.cpp
#include "DualSenseGamepadSupport.h"
#include "DGSInputDevice.h"
#include "DGSGamepadSupportLog.h"

#include "IInputDevice.h"
#include "Features/IModularFeatures.h"

// Log category defined once in DGSGamepadSupportLog.cpp

// ================================================================
// DRY alias for the modular feature name.
// GetModularFeatureName() already returns a static FName so there
// is no runtime cost to calling it directly -- this is purely a
// readability / consistency win across the three call sites below.
// ================================================================
namespace
{
	static const FName GInputDeviceFeatureName = IInputDeviceModule::GetModularFeatureName();
}

// ================================================================
// MODULE LIFECYCLE
// ================================================================

void FDualSenseGamepadSupportModule::StartupModule()
{
	IModularFeatures::Get().RegisterModularFeature(GInputDeviceFeatureName, this);
	bRegisteredFeature = true;	// We own this flag -- don't rely on global state

	UE_LOG(LogDGSGamepadSupport, Verbose,
		TEXT("[DGS] Module startup -- registered as InputDeviceModule (%s)"),
		*GInputDeviceFeatureName.ToString());

	// Must run now (PreDefault), before XInputDevice loads and long before the
	// engine creates its input devices -- see FDGSInputDevice::StartupModuleHooks.
	FDGSInputDevice::StartupModuleHooks();
}

void FDualSenseGamepadSupportModule::ShutdownModule()
{
	// Use our own flag -- IsModularFeatureAvailable() only checks whether
	// *any* implementation is registered, not whether THIS instance is.
	// That makes it useless as a guard here.
	if (bRegisteredFeature)
	{
		IModularFeatures::Get().UnregisterModularFeature(GInputDeviceFeatureName, this);
		bRegisteredFeature = false;
	}

	// Clear raw pointer BEFORE releasing shared ownership so that any
	// external code using GetInputDevice() sees nullptr immediately,
	// not a pointer to an object in the middle of being destroyed.
	RawInputDevice = nullptr;
	InputDevice.Reset();

	FDGSInputDevice::ShutdownModuleHooks();

	UE_LOG(LogDGSGamepadSupport, Verbose, TEXT("[DGS] Module shutdown -- input device released"));
}

// ================================================================
// DEVICE CREATION
// Called once by the engine when it initialises input devices.
// UE holds the returned TSharedPtr; we keep our own copy so
// ShutdownModule can deterministically release it.
// ================================================================

TSharedPtr<IInputDevice> FDualSenseGamepadSupportModule::CreateInputDevice(
	const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
{
	// Guard against double-creation.
	// The engine should never call this twice, but PIE teardown/restart
	// edge cases in the editor can trigger unexpected module reload flows.
	if (InputDevice.IsValid())
	{
		UE_LOG(LogDGSGamepadSupport, Warning,
			TEXT("[DGS] CreateInputDevice called more than once -- returning existing device"));
		return InputDevice;
	}

	// TSharedRef (not TSharedPtr) -- MakeShared never returns null,
	// and TSharedRef makes that contract explicit at the type level.
	TSharedRef<FDGSInputDevice> NewDevice = MakeShared<FDGSInputDevice>(InMessageHandler);

	// Ownership first, raw access second -- makes the safety contract obvious.
	InputDevice = NewDevice;			// TSharedRef implicitly converts to TSharedPtr
	RawInputDevice = &NewDevice.Get();	// Lifetime guaranteed by InputDevice above

	UE_LOG(LogDGSGamepadSupport, Verbose, TEXT("[DGS] Input device created successfully"));

	return InputDevice;
}

IMPLEMENT_MODULE(FDualSenseGamepadSupportModule, DualSenseGamepadSupport)
