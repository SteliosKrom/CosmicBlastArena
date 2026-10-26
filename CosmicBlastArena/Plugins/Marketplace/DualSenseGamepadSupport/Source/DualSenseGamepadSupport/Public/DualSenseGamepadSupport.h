// Copyright David Sanchez 2025. All Rights Reserved.
// DualSenseGamepadSupport.h
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "IInputDeviceModule.h"

class IInputDevice;
class FDGSInputDevice;

/**
 * FDualSenseGamepadSupportModule
 *
 * Entry point for the DualSense Gamepad Support plugin.
 * Registers itself as an IInputDeviceModule so UE creates our
 * custom FDGSInputDevice alongside the engine's own input devices.
 *
 * Lifecycle:
 *   StartupModule     -> RegisterModularFeature
 *   CreateInputDevice -> called by the engine once (PIE / game start)
 *   ShutdownModule    -> deterministic UnregisterModularFeature + release
 */
class DUALSENSEGAMEPADSUPPORT_API FDualSenseGamepadSupportModule final : public IInputDeviceModule
{
public:
	// -- Module interface ------------------------------------------
	virtual void StartupModule()  override;
	virtual void ShutdownModule() override;

	// -- IInputDeviceModule ----------------------------------------
	virtual TSharedPtr<IInputDevice> CreateInputDevice(
		const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) override;

	// -- Singleton accessors ---------------------------------------

	FORCEINLINE static FDualSenseGamepadSupportModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FDualSenseGamepadSupportModule>("DualSenseGamepadSupport");
	}

	FORCEINLINE static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("DualSenseGamepadSupport");
	}

	/**
	 * Zero-cost access to the input device.
	 * Returns nullptr before CreateInputDevice() is called or after ShutdownModule().
	 * Lifetime is owned by InputDevice (TSharedPtr); never store this raw pointer.
	 */
	FORCEINLINE FDGSInputDevice* GetInputDevice() const { return RawInputDevice; }

private:
	/** Shared ownership -- kept alive for the full module lifetime. */
	TSharedPtr<IInputDevice> InputDevice;

	/**
	 * Non-owning raw pointer for O(1) device access without TSharedPtr overhead.
	 * Valid only between CreateInputDevice() and ShutdownModule().
	 */
	FDGSInputDevice* RawInputDevice = nullptr;

	/**
	 * Tracks whether WE registered the modular feature.
	 * IsModularFeatureAvailable() only tells us "someone registered it",
	 * not "this instance registered it" -- so we own our own state.
	 */
	bool bRegisteredFeature = false;
};
