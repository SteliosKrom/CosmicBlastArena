// Copyright David Sanchez 2025. All Rights Reserved.
// DGSInputDevice.h
#pragma once

#include "CoreMinimal.h"
#include "IInputDevice.h"
#include "InputCoreTypes.h"
#include "HAL/ThreadSafeBool.h"

// Forward declarations (keep SDL out of header)
struct SDL_Gamepad;
class FDGSRebuildRunnable;  // Worker thread -- defined in .cpp
class FRunnableThread;

/**
 * FDGSInputDevice
 *
 * SDL3-based IInputDevice that adds native PlayStation controller support
 * (PS3 / PS4 DualShock 4 / PS5 DualSense) to Unreal Engine on Windows.
 * Xbox controllers go through this same device: the engine's own XInput
 * device is kept from being created (only when SDL3 is available), so every
 * pad has exactly one input path and always drives player 1 -- no project
 * settings required.
 *
 * - Reactive hotplug via SDL events
 * - Rate-limited active-controller polling (120 Hz)
 * - Hot-swap: activity-based with debounce
 * - RAW axis values -- deadzones delegated to Enhanced Input
 * - EXPENSIVE SDL ops (Open/Close) run on a worker thread to avoid hitches
 */
class FDGSInputDevice final : public IInputDevice
{
public:
	explicit FDGSInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler);
	virtual ~FDGSInputDevice() override;

	// -- IInputDevice ----------------------------------------------
	virtual void SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) override;
	virtual void Tick(float DeltaTime) override;
	virtual void SendControllerEvents() override;
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	virtual void SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override;
	virtual void SetChannelValues(int32 ControllerId, const FForceFeedbackValues& Values) override;
	virtual bool IsGamepadAttached() const override;

	// -- Module hooks ----------------------------------------------
	// Called from the module's StartupModule/ShutdownModule. Initialises SDL3
	// early and, if that succeeds, takes over Xbox pads from the engine's
	// native XInput device before the engine creates its input devices.
	static void StartupModuleHooks();
	static void ShutdownModuleHooks();

	// -- Singleton -------------------------------------------------
	static FDGSInputDevice* Get()
	{
#if WITH_SDL3
		return Instance;
#else
		return nullptr;
#endif
	}

	static constexpr int32 NumButtons = 14;
	static constexpr int32 NumAxes    = 6;

private:
#if WITH_SDL3
	// -- SDL lifetime ----------------------------------------------
	bool InitSDL();
	void ShutdownSDL();

	// -- Hotplug (game thread) -------------------------------------
	void PumpSDLEvents();
	void TriggerRebuildIfChanged(bool bForce = false);
	void ApplyPendingRebuildResult();

	// -- Hot-swap heuristic ----------------------------------------
	void UpdateActiveControllerHeuristic();

	// -- Input polling & emission ----------------------------------
	void PollActiveControllerAndEmit(bool bGameWorld);
	void ResetAllInputStates(int32 DisconnectedInstanceId);

	// Emits one digital key change and keeps HeldKeys in sync. A release for a
	// key that is not held is dropped.
	void EmitDigitalKey(const FName& Key, int32 KeyBit, bool bPressed);

	// -- Context ---------------------------------------------------
	bool ShouldInjectToGame();

	SDL_Gamepad* FindActivePad_Locked() const;

	struct FPadState
	{
		int32        InstanceId = -1;
		SDL_Gamepad* Pad = nullptr;
		bool         bConnected = false;
		bool         PrevButtons[NumButtons] = { false };
		float        PrevAxes[NumAxes] = { 0.f };
		bool         PrevTriggerLeft = false;
		bool         PrevTriggerRight = false;
		bool         PrevLeftStickLeft = false;
		bool         PrevLeftStickRight = false;
		bool         PrevLeftStickUp = false;
		bool         PrevLeftStickDown = false;
		bool         PrevRightStickLeft = false;
		bool         PrevRightStickRight = false;
		bool         PrevRightStickUp = false;
		bool         PrevRightStickDown = false;
		double       LastActivityTime = 0.0;
	};

	FPadState* FindActivePadState_Locked();

	// -- Thread safety ---------------------------------------------
	mutable FCriticalSection SDLCriticalSection;
	FThreadSafeBool          bIsShuttingDown{ false };

	// -- SDL state -------------------------------------------------
	bool bSDLReady = false;

	// -- UE input routing ------------------------------------------
	FPlatformUserId TargetUserId  = PLATFORMUSERID_NONE;
	FInputDeviceId  TargetDeviceId = INPUTDEVICEID_NONE;
	bool            bTargetDeviceConnected = false;

	// Digital keys UE currently sees as held on this device (bit = button
	// index, then the threshold keys). Only held keys are ever released: a
	// release for a key that was never pressed is not harmless -- CommonUI's
	// analog cursor toggles a debug "cursor mode" on stray shoulder/trigger
	// releases, which breaks menu navigation and focus. Game thread only.
	uint32          HeldKeys = 0;

	// -- Device tracking -------------------------------------------
	TArray<FPadState> Pads;

	// -- Async rebuild worker --------------------------------------
	FDGSRebuildRunnable* RebuildRunnable = nullptr;
	FRunnableThread*     RebuildThread   = nullptr;

	// -- Timing ----------------------------------------------------
	double NextPollTimeSeconds  = 0.0;
	double HotplugCooldownUntil = 0.0;
	double NextWorldCheckTime   = 0.0;
	bool   bPendingHotplug      = false;
	bool   bHotplugDeferred     = false; // hotplug seen during cooldown -- fires when it ends
	bool   bInGameWorld         = false;
	bool   bPrevWorldCheckInGame = false; // last world-check result (detects game->editor)

	// -- Hardcoded settings ----------------------------------------
	static constexpr float  kPollRateHz              = 120.0f;
	static constexpr double kPollInterval            = 1.0 / kPollRateHz;
	static constexpr float  kAxisNoiseThreshold      = 0.01f;
	static constexpr float  kControllerSwitchThresh  = 0.15f;
	static constexpr float  kActiveHoldSeconds       = 0.35f;
	static constexpr float  kHotplugCooldownSeconds  = 0.20f;
	static constexpr int32  kMaxTrackedControllers   = 4;

	// -- Force Feedback cache --------------------------------------
	float  CachedLeftLarge  = 0.0f;
	float  CachedRightLarge = 0.0f;
	float  CachedLeftSmall  = 0.0f;
	float  CachedRightSmall = 0.0f;
	float  CachedLeftMotor  = 0.0f;
	float  CachedRightMotor = 0.0f;
	uint16 LastRumbleLow    = 0xFFFF;
	uint16 LastRumbleHigh   = 0xFFFF;

#endif // WITH_SDL3

	static FDGSInputDevice* Instance;

	TSharedPtr<FGenericApplicationMessageHandler> MessageHandler;

	int32 ActiveInstanceId = -1;
};
