// Copyright David Sanchez 2025. All Rights Reserved.
// DGSInputDevice.cpp
#include "DGSInputDevice.h"

#include "DGSGamepadSupportLog.h"
#include "Interfaces/IPluginManager.h"

#include "HAL/PlatformProcess.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Features/IModularFeatures.h"
#include "IInputDeviceModule.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GenericPlatform/IInputInterface.h"    // FForceFeedbackChannelType full enum definition
#include "GameFramework/ForceFeedbackEffect.h" // FForceFeedbackValues full struct definition

#if WITH_SDL3
#include "SDL3/SDL.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include <type_traits>

// ================================================================
// ASYNC REBUILD WORKER
//
// Handles SDL_OpenGamepad and SDL_CloseGamepad off the game thread.
// These calls can block 50-400ms on Windows due to USB/HID/BT driver
// initialization. Running them here keeps the game thread hitch-free.
//
// Threading model:
//   Game thread -> TriggerScan() / EnqueueClose() -> wakes worker
//   Worker      -> DoScan() -> DoRebuild() / DrainCloseQueue() -> posts FResult
//   Game thread -> TryDequeueResult()             -> applies result
//
// SDL3 thread safety: SDL_OpenGamepad, SDL_CloseGamepad,
// SDL_GetGamepads, SDL_GetGamepadName/Type/Serial all acquire
// SDL's internal joystick lock, so calling them from a worker
// thread is safe alongside game-thread SDL_PollEvent calls.
// ================================================================
class FDGSRebuildRunnable : public FRunnable
{
public:
	// Settings snapshot passed from game thread
	struct FSettings
	{
		int32 MaxTrackedControllers = 3;
		// True only when the engine's native XInput device was kept from being
		// created (see DGSNativeXInput). Otherwise Xbox pads stay on the native
		// path and are never opened here, so each pad has exactly one input path.
		bool  bAllowXbox = false;
	};

	// Newly opened pad returned to game thread
	struct FOpenedPad
	{
		int32        InstanceId = -1;
		SDL_Gamepad* Pad = nullptr;
	};

	// Result posted back to game thread
	struct FResult
	{
		TArray<FOpenedPad> NewPads;    // opened -- game thread adopts these handles
		TArray<int32>      RemovedIds; // no longer in SDL list -- game thread removes from Pads
		bool               bValid = false;
	};

	FDGSRebuildRunnable()
	{
		WakeEvent = FPlatformProcess::GetSynchEventFromPool(false); // auto-reset
	}

	virtual ~FDGSRebuildRunnable() override
	{
		if (WakeEvent)
		{
			FPlatformProcess::ReturnSynchEventToPool(WakeEvent);
			WakeEvent = nullptr;
		}
	}

	// FRunnable
	virtual bool   Init()  override { return true; }
	virtual void   Stop()  override { bStop = true; if (WakeEvent) WakeEvent->Trigger(); }
	virtual void   Exit()  override {}
	virtual uint32 Run()   override;

	// -- Read by game thread to skip ALL SDL calls during rebuild --
	// Set true at the start of DoScan and cleared only when fully done.
	// Covers: SDL_UpdateGamepads (10-50ms Xbox), SDL_GetGamepads (~1ms),
	//         SDL_OpenGamepad (300-400ms), and DrainCloseQueue/SDL_CloseGamepad.
	// While true, SendControllerEvents returns before STEP 3 (SDL_PollEvent)
	// and STEP 6 (SDL_GetGamepadButton/Axis), so the game thread never
	// blocks on SDL_LockJoysticks during the full hotplug window.
	FThreadSafeBool bRebuildInProgress{ false };

	// -- Called from game thread -----------------------------------
	// bForce=true: rebuild even if the SDL gamepad ID list hasn't changed
	// (use when filter settings change and previously rejected pads may now be allowed).
	void TriggerScan(const TArray<int32>& CurrentOpenIds,
		const FSettings& Settings, bool bForce = false);

	void EnqueueClose(SDL_Gamepad* Pad);

	bool TryDequeueResult(FResult& OutResult);

private:
	bool IsTypeAllowed(SDL_GamepadType Type, const FSettings& Settings) const;
	bool IsTypeAllowed(SDL_Gamepad*    Pad,  const FSettings& Settings) const;
	void DoScan(const TArray<int32>& CurrentOpenIds, const FSettings& Settings, bool bForce);
	void DoRebuild(const TArray<int32>& CurrentOpenIds,
		const TArray<int32>& NewSDLIds,
		const FSettings& Settings);
	void DrainCloseQueue();

	FEvent* WakeEvent = nullptr;
	FThreadSafeBool  bStop{ false };

	// Request (game -> worker)
	FCriticalSection  RequestMutex;
	bool              bHasPendingRequest = false;
	TArray<int32>     PendingCurrentOpenIds;
	FSettings         PendingSettings;
	bool              bPendingForce = false;

	// Worker-private: SDL gamepad ID dedup -- no mutex needed (worker thread only).
	TArray<int32>     LastKnownSDLIds;

	// Close queue (game -> worker, or internal)
	FCriticalSection  CloseMutex;
	TArray<SDL_Gamepad*> PendingCloses;

	// Result (worker -> game)
	FCriticalSection  ResultMutex;
	FResult           PendingResult;
};

// ----------------------------------------------------------------
uint32 FDGSRebuildRunnable::Run()
{
	while (!(bool)bStop)
	{
		WakeEvent->Wait(); // block until TriggerScan or EnqueueClose or Stop
		if ((bool)bStop) break;

		// Dequeue scan request (if any)
		TArray<int32> CurIds;
		FSettings     Settings;
		bool          bForce   = false;
		bool          bHasReq  = false;
		{
			FScopeLock Lock(&RequestMutex);
			if (bHasPendingRequest)
			{
				CurIds   = MoveTemp(PendingCurrentOpenIds);
				Settings = PendingSettings;
				bForce   = bPendingForce;
				bPendingForce     = false;
				bHasPendingRequest = false;
				bHasReq = true;
			}
		}

		if (bHasReq)
		{
			// DoScan calls SDL_UpdateGamepads, SDL_GetGamepads, and DoRebuild.
			// bRebuildInProgress is managed inside DoScan.
			DoScan(CurIds, Settings, bForce);
		}
		else
		{
			// Close-only wake: drain without a full scan.
			// Guard bRebuildInProgress so game thread skips SDL polling
			// while SDL_CloseGamepad holds the joystick lock.
			bRebuildInProgress = true;
			DrainCloseQueue();
			bRebuildInProgress = false;
		}
	}

	// Shutdown: drain everything cleanly
	bRebuildInProgress = true;
	DrainCloseQueue();
	{
		FScopeLock Lock(&ResultMutex);
		// Close any handles in an unconsumed result
		for (FOpenedPad& P : PendingResult.NewPads)
		{
			if (P.Pad) { SDL_CloseGamepad(P.Pad); P.Pad = nullptr; }
		}
		PendingResult = FResult{};
	}
	bRebuildInProgress = false;
	return 0;
}

void FDGSRebuildRunnable::TriggerScan(const TArray<int32>& CurrentOpenIds,
	const FSettings& Settings, bool bForce)
{
	{
		FScopeLock Lock(&RequestMutex);
		PendingCurrentOpenIds = CurrentOpenIds;
		PendingSettings       = Settings;
		bPendingForce         = bPendingForce || bForce; // once forced, stay forced until consumed
		bHasPendingRequest    = true;
	}
	WakeEvent->Trigger();
}

void FDGSRebuildRunnable::EnqueueClose(SDL_Gamepad* Pad)
{
	if (!Pad) return;
	{
		FScopeLock Lock(&CloseMutex);
		PendingCloses.Add(Pad);
	}
	WakeEvent->Trigger();
}

bool FDGSRebuildRunnable::TryDequeueResult(FResult& OutResult)
{
	FScopeLock Lock(&ResultMutex);
	if (!PendingResult.bValid) return false;
	OutResult = MoveTemp(PendingResult);
	PendingResult = FResult{};
	return true;
}

// ----------------------------------------------------------------
// DoScan: runs entirely on the worker thread.
//
// bRebuildInProgress is set true at the very start and cleared only when
// fully done (including DoRebuild). This covers:
//   - SDL_UpdateGamepads:  10-50 ms for Xbox (XInput enumeration)
//   - SDL_GetGamepads:     ~1 ms
//   - SDL_OpenGamepad:     300-400 ms for any controller type
//
// While bRebuildInProgress is true the game thread returns early from
// SendControllerEvents before calling SDL_GamepadConnected/GetGamepadButton/
// GetGamepadAxis, so it never blocks on SDL_LockJoysticks.
// ----------------------------------------------------------------
void FDGSRebuildRunnable::DoScan(const TArray<int32>& CurrentOpenIds,
	const FSettings& Settings, bool bForce)
{
	// Guard at the very start: SDL_UpdateGamepads acquires SDL_LockJoysticks and
	// can take 10-50 ms for Xbox controllers (XInput driver enumeration), not just
	// the 300-400 ms of SDL_OpenGamepad. Without this guard, the game thread can
	// still block inside SDL_GamepadConnected/GetGamepadButton/Axis during that
	// window -- which is why Xbox caused a larger FPS drop than PS4/PS5.
	bRebuildInProgress = true;

	// Close any pending handles (SDL_CloseGamepad acquires joystick lock,
	// but typically <1 ms for already-disconnected devices).
	DrainCloseQueue();

	// SDL_UpdateGamepads + SDL_GetGamepads both acquire SDL_LockJoysticks.
	// Running on the worker thread (with the game thread guarded above) ensures
	// the game thread never blocks on any SDL joystick lock during this entire
	// scan+rebuild window.
	SDL_UpdateGamepads();

	int Count = 0;
	SDL_JoystickID* Ids = SDL_GetGamepads(&Count);
	TArray<int32> NewSDLIds;
	if (Ids && Count > 0)
	{
		NewSDLIds.Reserve(Count);
		for (int32 i = 0; i < Count; ++i)
			NewSDLIds.Add((int32)Ids[i]);
		NewSDLIds.Sort();
	}
	if (Ids) SDL_free(Ids);

	if (!bForce && NewSDLIds == LastKnownSDLIds)
	{
		UE_LOG(LogDGSGamepadSupport, Log,
			TEXT("[DGS][Worker] Scan: SDL list unchanged, skipping rebuild."));
		bRebuildInProgress = false;
		return;
	}

	LastKnownSDLIds = NewSDLIds;

	// SDL_OpenGamepad can block 300-400 ms on USB/HID/BT driver init.
	// bRebuildInProgress was already set at function entry; keep it true
	// through the entire rebuild and clear only when fully done.
	DoRebuild(CurrentOpenIds, NewSDLIds, Settings);
	bRebuildInProgress = false;
}

bool FDGSRebuildRunnable::IsTypeAllowed(SDL_GamepadType Type, const FSettings& Settings) const
{
	// Every physical pad must reach UE through exactly ONE input path. If the
	// engine's XInput device and this plugin both read the same Xbox pad, Slate
	// can receive a button's press from one path and its release from the
	// other -- different FInputDeviceId each -- so CommonUI clicks never
	// complete. Xbox pads are therefore only opened here when the native XInput
	// device was kept from being created (Settings.bAllowXbox); otherwise they
	// stay on the native path. PlayStation pads have no native path on Windows.
	switch (Type)
	{
	case SDL_GAMEPAD_TYPE_PS3:
	case SDL_GAMEPAD_TYPE_PS4:
	case SDL_GAMEPAD_TYPE_PS5:      return true;
	case SDL_GAMEPAD_TYPE_XBOX360:
	case SDL_GAMEPAD_TYPE_XBOXONE:  return Settings.bAllowXbox;
	default:                        return false;
	}
}

bool FDGSRebuildRunnable::IsTypeAllowed(SDL_Gamepad* Pad, const FSettings& Settings) const
{
	if (!Pad) return false;
	return IsTypeAllowed(SDL_GetGamepadType(Pad), Settings);
}

void FDGSRebuildRunnable::DoRebuild(const TArray<int32>& CurrentOpenIds,
	const TArray<int32>& NewSDLIds,
	const FSettings& Settings)
{
	FResult Result;
	Result.bValid = true;

	// IDs removed: in current but NOT in new
	for (int32 Id : CurrentOpenIds)
	{
		bool bFound = false;
		for (int32 NewId : NewSDLIds) { if (NewId == Id) { bFound = true; break; } }
		if (!bFound) Result.RemovedIds.Add(Id);
	}

	// Re-evaluate already-open pads against the type filter and MaxTrackedControllers.
	// The standard removal pass above only catches pads that physically disconnected
	// (no longer in NewSDLIds). SDL_GetGamepadTypeForID() lets us check the type via
	// instance ID alone -- no open SDL_Gamepad* handle required.
	for (int32 Id : CurrentOpenIds)
	{
		bool bAlreadyRemoved = false;
		for (int32 RId : Result.RemovedIds) { if (RId == Id) { bAlreadyRemoved = true; break; } }
		if (bAlreadyRemoved) continue;

		const SDL_GamepadType Type = SDL_GetGamepadTypeForID((SDL_JoystickID)Id);
		if (!IsTypeAllowed(Type, Settings))
		{
			UE_LOG(LogDGSGamepadSupport, Log,
				TEXT("[DGS][Worker] Removing pad ID=%d (type not allowed)"), Id);
			Result.RemovedIds.Add(Id);
		}
	}

	// Enforce MaxTrackedControllers reduction: if the limit was lowered while pads
	// are already open, remove the most recently added ones (highest index in
	// CurrentOpenIds) so the longest-active controllers are preserved.
	{
		int32 KeptCount = CurrentOpenIds.Num() - Result.RemovedIds.Num();
		for (int32 i = CurrentOpenIds.Num() - 1; i >= 0 && KeptCount > Settings.MaxTrackedControllers; --i)
		{
			const int32 Id = CurrentOpenIds[i];
			bool bAlreadyRemoved = false;
			for (int32 RId : Result.RemovedIds) { if (RId == Id) { bAlreadyRemoved = true; break; } }
			if (bAlreadyRemoved) continue;

			UE_LOG(LogDGSGamepadSupport, Log,
				TEXT("[DGS][Worker] Removing pad ID=%d (MaxTrackedControllers limit=%d)"),
				Id, Settings.MaxTrackedControllers);
			Result.RemovedIds.Add(Id);
			--KeptCount;
		}
	}

	// IDs to open: in new but NOT in current
	TArray<int32> IdsToOpen;
	IdsToOpen.Reserve(NewSDLIds.Num());
	for (int32 NewId : NewSDLIds)
	{
		bool bFound = false;
		for (int32 CurId : CurrentOpenIds) { if (CurId == NewId) { bFound = true; break; } }
		if (!bFound) IdsToOpen.Add(NewId);
	}

	// Available slots = max - (currently kept after ALL removals above)
	const int32 KeptCount = CurrentOpenIds.Num() - Result.RemovedIds.Num();
	int32       AvailSlots = Settings.MaxTrackedControllers - KeptCount;
	bool        bOpenFailed = false;

	for (int32 Id : IdsToOpen)
	{
		if (AvailSlots <= 0) break;

		// Skip pads whose type is already known to be filtered out (e.g. Xbox
		// while the engine's native XInput device owns them). Filtered pads never
		// become "open", so without this they would be opened and closed again on
		// every rescan, stalling input for the whole SDL_OpenGamepad duration.
		// UNKNOWN still goes through the open + post-open type check below.
		const SDL_GamepadType PreOpenType = SDL_GetGamepadTypeForID((SDL_JoystickID)Id);
		if (PreOpenType != SDL_GAMEPAD_TYPE_UNKNOWN && !IsTypeAllowed(PreOpenType, Settings))
		{
			continue;
		}

		SDL_Gamepad* NewPad = SDL_OpenGamepad((SDL_JoystickID)Id);
		if (!NewPad)
		{
			UE_LOG(LogDGSGamepadSupport, Warning,
				TEXT("[DGS][Worker] SDL_OpenGamepad failed for ID=%d: %s"),
				Id, UTF8_TO_TCHAR(SDL_GetError()));
			bOpenFailed = true;
			continue;
		}

		if (!IsTypeAllowed(NewPad, Settings))
		{
			SDL_CloseGamepad(NewPad);
			continue;
		}

		UE_LOG(LogDGSGamepadSupport, Log,
			TEXT("[DGS][Worker] Opened: %s (ID=%d)"),
			UTF8_TO_TCHAR(SDL_GetGamepadName(NewPad)), Id);

		Result.NewPads.Add({ Id, NewPad });
		--AvailSlots;
	}

	// A failed open is often transient (device still initialising, or briefly
	// held by another app). Forget the dedup snapshot so the next scan retries
	// it instead of skipping the SDL list as "unchanged".
	if (bOpenFailed)
	{
		LastKnownSDLIds.Reset();
	}

	// Post result
	{
		FScopeLock Lock(&ResultMutex);
		// Discard any unconsumed previous result (game thread was too slow)
		for (FOpenedPad& P : PendingResult.NewPads)
		{
			if (P.Pad) { SDL_CloseGamepad(P.Pad); P.Pad = nullptr; }
		}
		PendingResult = MoveTemp(Result);
	}
}

void FDGSRebuildRunnable::DrainCloseQueue()
{
	TArray<SDL_Gamepad*> ToClose;
	{
		FScopeLock Lock(&CloseMutex);
		ToClose = MoveTemp(PendingCloses);
	}
	for (SDL_Gamepad* Pad : ToClose)
	{
		if (Pad) SDL_CloseGamepad(Pad);
	}
}

// ================================================================
// UE INPUT DEVICE MAPPER SHIMS
// ================================================================
// The plugin's device belongs to the player's platform user ONLY while a
// PlayStation pad is actually active. The rest of the time it is parked on
// the "unpaired" user as Disconnected -- the same call the engine's own XInput
// device makes for disconnected pads in UE 5.6+.
//
// Why: from UE 5.6 the XInput device assigns a newly connected pad through
// GetPlatformUserForNewlyConnectedDevice(). With the default Windows policy
// (PrimaryUserSharesKeyboardAndFirstGamepad) it counts EVERY device mapped to
// the primary user, connected or not. If this device stayed mapped to user 0
// while idle, user 0 already had "keyboard + gamepad", so a native Xbox pad
// was given a brand-new platform user and stopped controlling the game.
//
// Slate resolves the user from the device mapping (GetUserIndexForInputDevice),
// so input must only be sent while the device is mapped to the player.
static void DGS_SetTargetDeviceActive(IPlatformInputDeviceMapper& Mapper, FInputDeviceId DeviceId, FPlatformUserId PlayerUserId, bool bActive)
{
	if (bActive)
	{
		Mapper.Internal_MapInputDeviceToUser(DeviceId, PlayerUserId, EInputDeviceConnectionState::Connected);
	}
	else
	{
		Mapper.Internal_MapInputDeviceToUser(DeviceId, Mapper.GetUserForUnpairedInputDevices(), EInputDeviceConnectionState::Disconnected);
	}
}
#endif // WITH_SDL3

// ================================================================
// HELPERS
// ================================================================
FORCEINLINE static FString DGS_Utf8(const char* S)
{
	return S ? FString(UTF8_TO_TCHAR(S)) : FString();
}

#if WITH_SDL3
FORCEINLINE static float DGS_NormalizeStickAxis(const Sint16 Raw)
{
	const float Den = (Raw < 0) ? 32768.0f : 32767.0f;
	return FMath::Clamp((float)Raw / Den, -1.0f, 1.0f);
}
FORCEINLINE static float DGS_NormalizeTriggerAxis(const Sint16 Raw)
{
	const int32 R = FMath::Clamp((int32)Raw, 0, 32767);
	return FMath::Clamp((float)R * (1.0f / 32767.0f), 0.0f, 1.0f);
}

// ================================================================
// PRE-BAKED BUTTON/AXIS TABLES
// ================================================================
static constexpr SDL_GamepadButton GDGSButtons[FDGSInputDevice::NumButtons] = {
	SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH,
	SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_START,
	SDL_GAMEPAD_BUTTON_LEFT_STICK, SDL_GAMEPAD_BUTTON_RIGHT_STICK,
	SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
	SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN,
	SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT
};
static constexpr SDL_GamepadAxis GDGSAxes[FDGSInputDevice::NumAxes] = {
	SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY,
	SDL_GAMEPAD_AXIS_RIGHTX, SDL_GAMEPAD_AXIS_RIGHTY,
	SDL_GAMEPAD_AXIS_LEFT_TRIGGER, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER
};
static_assert(UE_ARRAY_COUNT(GDGSButtons) == FDGSInputDevice::NumButtons, "GDGSButtons size mismatch");
static_assert(UE_ARRAY_COUNT(GDGSAxes) == FDGSInputDevice::NumAxes, "GDGSAxes size mismatch");

// ================================================================
// PRE-BAKED FName CACHE
// ================================================================
namespace DGSKeyCache
{
	static FName ButtonNames[FDGSInputDevice::NumButtons];
	static FName AxisNames[FDGSInputDevice::NumAxes];
	static FName TriggerKeyLeft, TriggerKeyRight;
	static FName LeftStickLeft, LeftStickRight, LeftStickUp, LeftStickDown;
	static FName RightStickLeft, RightStickRight, RightStickUp, RightStickDown;

	static void EnsureInit()
	{
		static const struct FInit
		{
			FInit()
			{
				ButtonNames[0] = EKeys::Gamepad_FaceButton_Bottom.GetFName();
				ButtonNames[1] = EKeys::Gamepad_FaceButton_Right.GetFName();
				ButtonNames[2] = EKeys::Gamepad_FaceButton_Left.GetFName();
				ButtonNames[3] = EKeys::Gamepad_FaceButton_Top.GetFName();
				ButtonNames[4] = EKeys::Gamepad_Special_Left.GetFName();
				ButtonNames[5] = EKeys::Gamepad_Special_Right.GetFName();
				ButtonNames[6] = EKeys::Gamepad_LeftThumbstick.GetFName();
				ButtonNames[7] = EKeys::Gamepad_RightThumbstick.GetFName();
				ButtonNames[8] = EKeys::Gamepad_LeftShoulder.GetFName();
				ButtonNames[9] = EKeys::Gamepad_RightShoulder.GetFName();
				ButtonNames[10] = EKeys::Gamepad_DPad_Up.GetFName();
				ButtonNames[11] = EKeys::Gamepad_DPad_Down.GetFName();
				ButtonNames[12] = EKeys::Gamepad_DPad_Left.GetFName();
				ButtonNames[13] = EKeys::Gamepad_DPad_Right.GetFName();

				AxisNames[0] = EKeys::Gamepad_LeftX.GetFName();
				AxisNames[1] = EKeys::Gamepad_LeftY.GetFName();
				AxisNames[2] = EKeys::Gamepad_RightX.GetFName();
				AxisNames[3] = EKeys::Gamepad_RightY.GetFName();
				AxisNames[4] = EKeys::Gamepad_LeftTriggerAxis.GetFName();
				AxisNames[5] = EKeys::Gamepad_RightTriggerAxis.GetFName();

				TriggerKeyLeft = EKeys::Gamepad_LeftTrigger.GetFName();
				TriggerKeyRight = EKeys::Gamepad_RightTrigger.GetFName();

				LeftStickLeft = EKeys::Gamepad_LeftStick_Left.GetFName();
				LeftStickRight = EKeys::Gamepad_LeftStick_Right.GetFName();
				LeftStickUp = EKeys::Gamepad_LeftStick_Up.GetFName();
				LeftStickDown = EKeys::Gamepad_LeftStick_Down.GetFName();
				RightStickLeft = EKeys::Gamepad_RightStick_Left.GetFName();
				RightStickRight = EKeys::Gamepad_RightStick_Right.GetFName();
				RightStickUp = EKeys::Gamepad_RightStick_Up.GetFName();
				RightStickDown = EKeys::Gamepad_RightStick_Down.GetFName();
			}
		} Init;
		(void)Init;
	}

	static constexpr uint8 TriggerAxisMask = (1 << 4) | (1 << 5);
	static constexpr uint8 InvertYAxisMask = (1 << 1) | (1 << 3);

	// Threshold (digital) keys in held-mask bit order, right after the
	// NumButtons buttons. Must match the key table in PollActiveControllerAndEmit.
	static const FName* const DigitalKeys[] = {
		&TriggerKeyLeft, &TriggerKeyRight,
		&LeftStickLeft, &LeftStickRight, &LeftStickUp, &LeftStickDown,
		&RightStickLeft, &RightStickRight, &RightStickUp, &RightStickDown,
	};
	static constexpr int32 NumDigitalKeys = UE_ARRAY_COUNT(DigitalKeys);
	static_assert(FDGSInputDevice::NumButtons + NumDigitalKeys <= 32, "held-key mask is 32 bits");
}

// ================================================================
// SDL DLL RESOLUTION
// ================================================================
static FString DGS_GetPreferredSDL3DllPath()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("DualSenseGamepadSupport"));
	if (Plugin.IsValid())
	{
		const FString& BaseDir = Plugin->GetBaseDir();

		// Check Source/ThirdParty first -- this is the canonical, always-present copy.
		// Binaries/Win64 is intentionally checked last: UBT cleans that folder before
		// restaging, so it may be absent mid-compile even though the source copy is fine.
		const FString SrcTpDll = FPaths::Combine(BaseDir, TEXT("Source/ThirdParty/SDL3/lib/Win64/SDL3.dll"));
		if (FPaths::FileExists(SrcTpDll)) return SrcTpDll;

		const FString TpDll = FPaths::Combine(BaseDir, TEXT("ThirdParty/SDL3/lib/Win64/SDL3.dll"));
		if (FPaths::FileExists(TpDll)) return TpDll;

		const FString BinDll = FPaths::Combine(BaseDir, TEXT("Binaries/Win64/SDL3.dll"));
		if (FPaths::FileExists(BinDll)) return BinDll;
	}

	return TEXT("SDL3.dll");
}

// ================================================================
// SDL RUNTIME -- global, ref-counted
// ================================================================
namespace DGSSDLRuntime
{
	static FCriticalSection  GMutex;
	static int32             GRefCount = 0;
	static bool              bInitOk = false;
	static void* GDllHandle = nullptr;
	static FThreadSafeBool   GbInitReady{ false };

	static bool LoadDll_Locked()
	{
		if (GDllHandle) return true;

		const FString Preferred = DGS_GetPreferredSDL3DllPath();
		FString Loaded;

		GDllHandle = FPlatformProcess::GetDllHandle(*Preferred);
		if (GDllHandle) Loaded = Preferred;

		if (!GDllHandle)
		{
			GDllHandle = FPlatformProcess::GetDllHandle(TEXT("SDL3.dll"));
			if (GDllHandle) Loaded = TEXT("SDL3.dll (system)");
		}

		if (!GDllHandle)
		{
			const FString ExeDll = FPaths::Combine(
				FPaths::ConvertRelativePathToFull(FPlatformProcess::BaseDir()), TEXT("SDL3.dll"));
			if (FPaths::FileExists(ExeDll))
			{
				GDllHandle = FPlatformProcess::GetDllHandle(*ExeDll);
				if (GDllHandle) Loaded = ExeDll;
			}
		}

		if (GDllHandle)
		{
			UE_LOG(LogDGSGamepadSupport, Log, TEXT("[DGS] SDL3.dll loaded: %s"), *Loaded);
		}
		else
		{
			UE_LOG(LogDGSGamepadSupport, Error, TEXT("[DGS] SDL3.dll NOT found."));
		}

		return GDllHandle != nullptr;
	}

	bool Acquire()
	{
		FScopeLock Lock(&GMutex);
		if (GRefCount == 0)
		{
			if (!LoadDll_Locked()) return false;

			SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
			SDL_SetHint("SDL_JOYSTICK_HIDAPI_PS4", "1");
			SDL_SetHint("SDL_JOYSTICK_HIDAPI_PS5", "1");
			SDL_SetHint("SDL_JOYSTICK_HIDAPI_PS5_RUMBLE", "1");
			SDL_SetHint("SDL_JOYSTICK_HIDAPI_PS5_PLAYER_LED", "1");

			static_assert(std::is_same_v<decltype(SDL_Init(0)), bool>,
				"SDL_Init signature changed from bool -- update init check");

			const bool bSDLInitOk = SDL_Init(SDL_INIT_EVENTS | SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC);
			bInitOk = bSDLInitOk;
			GbInitReady = bSDLInitOk;

			if (!bInitOk)
			{
				UE_LOG(LogDGSGamepadSupport, Error, TEXT("[DGS] SDL_Init failed: %s"), *DGS_Utf8(SDL_GetError()));
				return false;
			}
			SDL_UpdateGamepads();
		}
		++GRefCount;
		UE_LOG(LogDGSGamepadSupport, Verbose, TEXT("[DGS] SDLRuntime Acquire -- refcount=%d"), GRefCount);
		return bInitOk;
	}

	void Release()
	{
		FScopeLock Lock(&GMutex);
		if (GRefCount <= 0)
		{
			UE_LOG(LogDGSGamepadSupport, Error, TEXT("[DGS] SDLRuntime Release() with refcount=%d"), GRefCount);
			return;
		}
		--GRefCount;
		UE_LOG(LogDGSGamepadSupport, Verbose, TEXT("[DGS] SDLRuntime Release -- refcount=%d"), GRefCount);
		if (GRefCount == 0)
		{
			if (bInitOk)
			{
				UE_LOG(LogDGSGamepadSupport, Log, TEXT("[DGS] SDLRuntime last Release -- SDL_Quit()"));
				SDL_Quit();
			}
			bInitOk = false;
			GbInitReady = false;
			// DLL handle intentionally NOT freed -- OS reclaims on exit
		}
	}

	FORCEINLINE bool IsReady() { return (bool)GbInitReady; }
}

// ================================================================
// NATIVE XINPUT TAKEOVER
//
// Goal: every controller (PlayStation AND Xbox) reaches UE through this
// plugin's single device, with zero project configuration.
//
// Why not leave Xbox to the engine's XInput device:
//   - Reading one Xbox pad through both paths breaks CommonUI clicks
//     (press and release arrive with different FInputDeviceIds).
//   - From UE 5.6, the XInput device assigns newly connected pads through
//     the device-mapping policy, which can create a second platform user
//     ("player 2") for the Xbox pad while this plugin's device is mapped to
//     player 1 -- requiring a project settings change to undo.
//
// How: the engine creates input devices lazily, on the first poll after
// GIsRunning, from the registered IInputDeviceModule modular features. This
// module loads earlier (PreDefault) than XInputDevice (Default), so it can
// unregister the XInputDevice feature before any XInput device exists.
//
// Safety: this only happens if SDL3 loaded and initialised successfully at
// module startup. Otherwise nothing is touched and Xbox pads keep working
// through the engine's native path (bAllowXbox stays false).
// ================================================================
namespace DGSNativeXInput
{
	static const FName        XInputModuleName(TEXT("XInputDevice"));
	static bool               bSDLHeldByModule   = false;
	static bool               bXboxOwnedByPlugin = false;
	static FDelegateHandle    ModulesChangedHandle;
	static FDelegateHandle    PostEngineInitHandle;

	static void TryUnregisterXInput()
	{
		if (!bSDLHeldByModule) return;

		IInputDeviceModule* XInputModule = FModuleManager::GetModulePtr<IInputDeviceModule>(XInputModuleName);
		if (!XInputModule) return;

		const FName FeatureName = IInputDeviceModule::GetModularFeatureName();
		TArray<IInputDeviceModule*> Registered =
			IModularFeatures::Get().GetModularFeatureImplementations<IInputDeviceModule>(FeatureName);
		if (Registered.Contains(XInputModule))
		{
			IModularFeatures::Get().UnregisterModularFeature(FeatureName, XInputModule);
			UE_LOG(LogDGSGamepadSupport, Log,
				TEXT("[DGS] Engine XInput device disabled -- Xbox controllers are handled by this plugin (single input path, no extra platform users)."));
		}
		bXboxOwnedByPlugin = true;
	}

	// Called once every plugin is loaded (PostEngineInit, still before the
	// engine creates its input devices). If the XInputDevice module does not
	// exist at all (e.g. disabled in the project), there is no native Xbox
	// path to compete with, so this plugin must read Xbox pads itself.
	static void FinalizeOwnership()
	{
		TryUnregisterXInput();
		if (bSDLHeldByModule && !FModuleManager::Get().IsModuleLoaded(XInputModuleName))
		{
			bXboxOwnedByPlugin = true;
		}
	}

	static bool OwnsXbox() { return bXboxOwnedByPlugin; }

	// FCoreDelegates::OnPostEngineInit became a getter in UE 5.8 (the static
	// member is deprecated there); pick the right API per engine version.
	static FSimpleMulticastDelegate& OnPostEngineInit()
	{
#if UE_VERSION_OLDER_THAN(5, 8, 0)
		return FCoreDelegates::OnPostEngineInit;
#else
		return FCoreDelegates::GetOnPostEngineInit();
#endif
	}
}

#endif // WITH_SDL3

void FDGSInputDevice::StartupModuleHooks()
{
#if WITH_SDL3
	if (IsRunningCommandlet() || IsRunningDedicatedServer()) return;

	if (!DGSSDLRuntime::Acquire())
	{
		UE_LOG(LogDGSGamepadSupport, Warning,
			TEXT("[DGS] SDL3 unavailable at startup -- engine XInput left untouched (Xbox via native path)."));
		return;
	}
	DGSNativeXInput::bSDLHeldByModule = true;

	// XInputDevice may already be loaded, will load later (normal case), or be
	// reloaded; cover all three. PostEngineInit fires before GIsRunning, i.e.
	// before the engine creates its input devices.
	DGSNativeXInput::TryUnregisterXInput();
	DGSNativeXInput::ModulesChangedHandle = FModuleManager::Get().OnModulesChanged().AddLambda(
		[](FName ModuleName, EModuleChangeReason Reason)
		{
			if (Reason == EModuleChangeReason::ModuleLoaded && ModuleName == DGSNativeXInput::XInputModuleName)
			{
				DGSNativeXInput::TryUnregisterXInput();
			}
		});
	DGSNativeXInput::PostEngineInitHandle = DGSNativeXInput::OnPostEngineInit().AddLambda(
		[]()
		{
			DGSNativeXInput::FinalizeOwnership();
		});
#endif
}

void FDGSInputDevice::ShutdownModuleHooks()
{
#if WITH_SDL3
	if (DGSNativeXInput::ModulesChangedHandle.IsValid())
	{
		FModuleManager::Get().OnModulesChanged().Remove(DGSNativeXInput::ModulesChangedHandle);
		DGSNativeXInput::ModulesChangedHandle.Reset();
	}
	if (DGSNativeXInput::PostEngineInitHandle.IsValid())
	{
		DGSNativeXInput::OnPostEngineInit().Remove(DGSNativeXInput::PostEngineInitHandle);
		DGSNativeXInput::PostEngineInitHandle.Reset();
	}
	if (DGSNativeXInput::bSDLHeldByModule)
	{
		DGSSDLRuntime::Release();
		DGSNativeXInput::bSDLHeldByModule = false;
	}
#endif
}

// ================================================================
// SINGLETON
// ================================================================
FDGSInputDevice* FDGSInputDevice::Instance = nullptr;

// ================================================================
// CONSTRUCTOR / DESTRUCTOR
// ================================================================
FDGSInputDevice::FDGSInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
{
	UE_LOG(LogDGSGamepadSupport, Log, TEXT("[DGS] InputDevice initializing..."));
	MessageHandler = InMessageHandler;
	Instance = this;

#if WITH_SDL3
	DGSKeyCache::EnsureInit();

	InitSDL();
#endif

	UE_LOG(LogDGSGamepadSupport, Log, TEXT("[DGS] InputDevice ready."));
}

FDGSInputDevice::~FDGSInputDevice()
{
	if (Instance == this) Instance = nullptr;

#if WITH_SDL3
	bIsShuttingDown = true;

	{
		FScopeLock Lock(&SDLCriticalSection);
		bSDLReady = false;
	}
	ShutdownSDL();
#endif
}

// ================================================================
// IInputDevice
// ================================================================
void FDGSInputDevice::SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
{
	MessageHandler = InMessageHandler;
}

void FDGSInputDevice::Tick(float) {}
bool FDGSInputDevice::Exec(UWorld*, const TCHAR*, FOutputDevice&) { return false; }

bool FDGSInputDevice::IsGamepadAttached() const
{
#if WITH_SDL3
	if (!DGSSDLRuntime::IsReady() || (bool)bIsShuttingDown) return false;
	FScopeLock Lock(&SDLCriticalSection);
	if (!bSDLReady) return false;
	for (const FPadState& P : Pads)
		if (P.Pad && P.bConnected) return true;
#endif
	return false;
}

// ================================================================
// SEND CONTROLLER EVENTS  (game thread, called every frame)
//
// Flow:
//   1. Apply any pending worker rebuild result (fast -- array swap)
//   2. Initial subsystem sync (once)
//   [Guard: return early if bRebuildInProgress -- worker holds SDL_LockJoysticks]
//   3. Pump SDL events (detect hotplug) -- under SDLCriticalSection
//   4. If hotplug: TriggerRebuildIfChanged (worker handles SDL_UpdateGamepads +
//      SDL_GetGamepads + SDL_OpenGamepad on its own thread)
//   5. World check (1 Hz)
//   6. Rate-limited input poll (guarded again by bRebuildInProgress)
// ================================================================
void FDGSInputDevice::SendControllerEvents()
{
#if WITH_SDL3
	if (UNLIKELY((bool)bIsShuttingDown)) return;

	// -- STEP 1: Apply async rebuild result (microseconds) ----------
	ApplyPendingRebuildResult();

	// Skip SDL_PollEvent (STEP 2) entirely while the worker holds SDL_LockJoysticks.
	// SDL_PollEvent -> SDL_PumpEvents -> SDL_UpdateJoysticks acquires that same mutex,
	// so if the worker is inside DoScan/DoRebuild (SDL_UpdateGamepads or SDL_OpenGamepad),
	// the game thread would block for the full driver-init duration (10-450 ms depending
	// on controller type: Xbox ~10-50 ms for UpdateGamepads + 300-400 ms for OpenGamepad).
	// SDL buffers events internally -- they will be processed next frame without loss.
	if (RebuildRunnable && (bool)RebuildRunnable->bRebuildInProgress) return;

	// -- STEP 2: Pump SDL events (detect hotplug) -- under lock -----
	bool bHotplug = false;
	{
		FScopeLock Lock(&SDLCriticalSection);
		if (UNLIKELY(!bSDLReady || !DGSSDLRuntime::IsReady())) return;
		PumpSDLEvents();       // may set bPendingHotplug
		bHotplug = bPendingHotplug;
		bPendingHotplug = false;
	}

	// -- STEP 3: Kick async scan (worker handles SDL_UpdateGamepads + SDL_GetGamepads) --
	// SDL_UpdateGamepads, SDL_GetGamepads, and SDL_OpenGamepad are now called
	// exclusively on the worker thread, so they never stall the game thread.
	if (bHotplug)
	{
		TriggerRebuildIfChanged();
	}

	// -- STEP 4: World check (1 Hz) ---------------------------------
	{
		const double NowW = FPlatformTime::Seconds();
		if (bHotplug || NowW >= NextWorldCheckTime)
		{
			const bool bNowInGame = ShouldInjectToGame();

			// Game/PIE world ended: zero out everything we injected into UE. A
			// stick or trigger held during the game->editor transition would
			// otherwise leave its last value latched in the editor's input state
			// (camera drifting on its own after ESC). Compare against
			// bPrevWorldCheckInGame (owned by this check), not bInGameWorld,
			// which ShouldInjectToGame() has just overwritten.
			if (bPrevWorldCheckInGame && !bNowInGame)
				ResetAllInputStates(ActiveInstanceId);
			bPrevWorldCheckInGame = bNowInGame;

			NextWorldCheckTime = NowW + 1.0;
		}
	}

	// -- STEP 5: Rate-limited input poll ----------------------------
	const double Now = FPlatformTime::Seconds();
	if (Now < NextPollTimeSeconds) return;
	NextPollTimeSeconds = Now + kPollInterval;

	// Skip SDL_GetGamepadButton/Axis calls while the worker holds SDL_LockJoysticks
	// during SDL_OpenGamepad (300-400 ms on USB/HID/BT driver init).
	// Without this guard, every per-frame SDL input poll call blocks for the full
	// driver-init duration, causing the visible FPS drop on 3rd-controller connect.
	if (RebuildRunnable && (bool)RebuildRunnable->bRebuildInProgress) return;

	int32 PrevActive, NewActive;
	{
		FScopeLock Lock(&SDLCriticalSection);
		PrevActive = ActiveInstanceId;
		if (bSDLReady && DGSSDLRuntime::IsReady())
			UpdateActiveControllerHeuristic();
		NewActive = ActiveInstanceId;
	}
	if (NewActive != PrevActive)
	{
		// The pad that just lost "active" status may still be mid-rumble, and may
		// still have buttons/sticks/triggers held down from the engine's point of
		// view -- only the active pad is ever polled (see PollActiveControllerAndEmit),
		// so neither its rumble nor its held input would otherwise ever be released
		// once a different pad becomes active.
		if (PrevActive >= 0)
		{
			{
				FScopeLock Lock(&SDLCriticalSection);
				for (FPadState& P : Pads)
				{
					if (P.InstanceId == PrevActive && P.Pad)
					{
						SDL_RumbleGamepad(P.Pad, 0, 0, 0);
						break;
					}
				}
			}
			ResetAllInputStates(PrevActive);
		}
		LastRumbleLow = 0xFFFF; LastRumbleHigh = 0xFFFF;
	}

	PollActiveControllerAndEmit(bInGameWorld);
#endif
}

// ================================================================
// FORCE FEEDBACK
// ================================================================
#if WITH_SDL3
FORCEINLINE static void DGS_ApplyRumble_Locked(SDL_Gamepad* ActivePad, float LeftMotor, float RightMotor,
	uint16& LastLow, uint16& LastHigh)
{
	const Uint16 Low = static_cast<Uint16>(FMath::Clamp(LeftMotor, 0.f, 1.f) * 65535.f);
	const Uint16 High = static_cast<Uint16>(FMath::Clamp(RightMotor, 0.f, 1.f) * 65535.f);
	if (Low == LastLow && High == LastHigh) return;
	LastLow = Low; LastHigh = High;
	SDL_RumbleGamepad(ActivePad, Low, High, 0);
}
#endif

void FDGSInputDevice::SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value)
{
#if WITH_SDL3
	if (UNLIKELY((bool)bIsShuttingDown || ControllerId != 0)) return;
	FScopeLock Lock(&SDLCriticalSection);
	if (!bSDLReady || !DGSSDLRuntime::IsReady()) return;

	if (ChannelType == FForceFeedbackChannelType::LEFT_LARGE)       CachedLeftLarge = Value;
	else if (ChannelType == FForceFeedbackChannelType::RIGHT_LARGE) CachedRightLarge = Value;
	else if (ChannelType == FForceFeedbackChannelType::LEFT_SMALL)  CachedLeftSmall = Value;
	else if (ChannelType == FForceFeedbackChannelType::RIGHT_SMALL) CachedRightSmall = Value;
	else return;

	// LEFT_LARGE + LEFT_SMALL -> low-freq (left) motor
	// RIGHT_LARGE + RIGHT_SMALL -> high-freq (right) motor
	CachedLeftMotor  = FMath::Max(CachedLeftLarge,  CachedLeftSmall);
	CachedRightMotor = FMath::Max(CachedRightLarge, CachedRightSmall);

	if (SDL_Gamepad* Pad = FindActivePad_Locked())
		DGS_ApplyRumble_Locked(Pad, CachedLeftMotor, CachedRightMotor, LastRumbleLow, LastRumbleHigh);
#endif
}

void FDGSInputDevice::SetChannelValues(int32 ControllerId, const FForceFeedbackValues& Values)
{
#if WITH_SDL3
	if (UNLIKELY((bool)bIsShuttingDown || ControllerId != 0)) return;
	FScopeLock Lock(&SDLCriticalSection);
	if (!bSDLReady || !DGSSDLRuntime::IsReady()) return;

	CachedLeftLarge = Values.LeftLarge;
	CachedRightLarge = Values.RightLarge;
	CachedLeftSmall = Values.LeftSmall;
	CachedRightSmall = Values.RightSmall;
	// LEFT_LARGE + LEFT_SMALL -> low-freq (left) motor
	// RIGHT_LARGE + RIGHT_SMALL -> high-freq (right) motor
	CachedLeftMotor  = FMath::Max(CachedLeftLarge,  CachedLeftSmall);
	CachedRightMotor = FMath::Max(CachedRightLarge, CachedRightSmall);

	if (SDL_Gamepad* Pad = FindActivePad_Locked())
		DGS_ApplyRumble_Locked(Pad, CachedLeftMotor, CachedRightMotor, LastRumbleLow, LastRumbleHigh);
#endif
}

// ================================================================
// SDL LIFETIME
// ================================================================
#if WITH_SDL3

bool FDGSInputDevice::InitSDL()
{
	if (!DGSSDLRuntime::Acquire())
	{
		UE_LOG(LogDGSGamepadSupport, Error, TEXT("[DGS] SDL runtime acquire failed."));
		bSDLReady = false;
		return false;
	}

	IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();
	TargetUserId = Mapper.GetPlatformUserForUserIndex(0);
	if (!TargetUserId.IsValid())
		UE_LOG(LogDGSGamepadSupport, Warning, TEXT("[DGS] TargetUserId invalid."));

	// Allocated now but NOT mapped to the player yet: it is mapped the first time
	// a PlayStation pad becomes active (ApplyPendingRebuildResult). Mapping it here
	// would make UE 5.6+ push a native Xbox pad to a new platform user -- see
	// DGS_SetTargetDeviceActive.
	TargetDeviceId = Mapper.AllocateNewInputDeviceId();

	// Create the rebuild worker thread BEFORE setting bSDLReady
	RebuildRunnable = new FDGSRebuildRunnable();
	RebuildThread = FRunnableThread::Create(RebuildRunnable, TEXT("DGS_RebuildWorker"),
		0, TPri_BelowNormal);

	const double Now = FPlatformTime::Seconds();
	NextPollTimeSeconds = Now;
	HotplugCooldownUntil = 0.0;
	bSDLReady = true;

	// Trigger initial device discovery asynchronously.
	// The worker calls SDL_UpdateGamepads + SDL_GetGamepads internally (DoScan).
	TriggerRebuildIfChanged();

	UE_LOG(LogDGSGamepadSupport, Log, TEXT("[DGS] SDL ready -- worker thread created, initial scan queued. Controllers handled: PlayStation%s"),
		DGSNativeXInput::OwnsXbox() ? TEXT(" + Xbox") : TEXT(" (Xbox via engine XInput)"));
	return true;
}

void FDGSInputDevice::ShutdownSDL()
{
	// 1. Stop the worker thread first (it might be mid-SDL_OpenGamepad)
	if (RebuildThread)
	{
		if (RebuildRunnable) RebuildRunnable->Stop();
		RebuildThread->WaitForCompletion();
		delete RebuildThread;
		RebuildThread = nullptr;
	}
	if (RebuildRunnable)
	{
		delete RebuildRunnable;
		RebuildRunnable = nullptr;
	}

	// 2. Close all open pad handles and clean up UE state
	{
		FScopeLock Lock(&SDLCriticalSection);

		for (FPadState& P : Pads)
		{
			if (P.Pad) { SDL_CloseGamepad(P.Pad); P.Pad = nullptr; }
		}
		Pads.Reset();
		ActiveInstanceId = -1;

		if (TargetDeviceId.IsValid())
		{
			if (bTargetDeviceConnected)
			{
				DGS_SetTargetDeviceActive(IPlatformInputDeviceMapper::Get(), TargetDeviceId, TargetUserId, false);
				bTargetDeviceConnected = false;
			}
			TargetDeviceId = INPUTDEVICEID_NONE;
		}
	}

	// 3. Release SDL runtime
	DGSSDLRuntime::Release();
}


// ================================================================
// HOTPLUG DETECTION (game thread, called under SDLCriticalSection)
//
// Only drains the SDL event queue and sets bPendingHotplug.
// SDL_UpdateGamepads is intentionally NOT called here -- the worker
// thread calls it (DoScan) so the game thread never stalls.
// ================================================================
void FDGSInputDevice::PumpSDLEvents()
{
	SDL_Event Ev;
	bool bHotplug = false;

	while (SDL_PollEvent(&Ev))
	{
		switch (Ev.type)
		{
		case SDL_EVENT_GAMEPAD_ADDED:
		case SDL_EVENT_GAMEPAD_REMOVED:
		case SDL_EVENT_JOYSTICK_ADDED:
		case SDL_EVENT_JOYSTICK_REMOVED:
			bHotplug = true;
			break;
		default:
			break;
		}
	}

	// Debounce without losing events: a hotplug seen during the cooldown
	// window is remembered and fires as soon as the window ends, instead of
	// being dropped (which could leave a quickly reconnected pad undetected
	// until the next plug/unplug).
	if (bHotplug) bHotplugDeferred = true;

	if (bHotplugDeferred)
	{
		const double Now = FPlatformTime::Seconds();
		if (Now >= HotplugCooldownUntil)
		{
			HotplugCooldownUntil = Now + FMath::Max(kHotplugCooldownSeconds, 0.1f);
			bPendingHotplug = true;
			bHotplugDeferred = false;
		}
	}
}

// ================================================================
// TRIGGER REBUILD IF CHANGED (game thread)
//
// Snapshots the current open-pad IDs and settings, then wakes the
// worker thread. The worker calls SDL_UpdateGamepads + SDL_GetGamepads
// internally (DoScan) and skips DoRebuild if the SDL list is unchanged.
// This removes SDL_GetGamepads from the game thread, preventing its
// SDL_LockJoysticks acquisition from stalling per-frame input polling.
//
// bForce=true: worker rebuilds regardless of SDL list dedup
// (used when filter settings change -- previously rejected pads may
//  now be allowed even if the SDL ID list is identical).
// ================================================================
void FDGSInputDevice::TriggerRebuildIfChanged(bool bForce)
{
	if (!RebuildRunnable) return;

	// Snapshot current open IDs and settings under lock (microseconds).
	TArray<int32>                  CurrentOpenIds;
	FDGSRebuildRunnable::FSettings Settings;
	{
		FScopeLock Lock(&SDLCriticalSection);
		CurrentOpenIds.Reserve(Pads.Num());
		for (const FPadState& P : Pads)
			CurrentOpenIds.Add(P.InstanceId);

		Settings.MaxTrackedControllers = kMaxTrackedControllers;
		Settings.bAllowXbox            = DGSNativeXInput::OwnsXbox();
	}

	UE_LOG(LogDGSGamepadSupport, Log,
		TEXT("[DGS] Async scan triggered (open=%d%s)"),
		CurrentOpenIds.Num(), bForce ? TEXT(", forced") : TEXT(""));

	RebuildRunnable->TriggerScan(CurrentOpenIds, Settings, bForce);
}

// ================================================================
// APPLY PENDING REBUILD RESULT (game thread, called every frame)
//
// Called at the TOP of SendControllerEvents -- before any SDL work.
// Extremely fast: just an array swap under lock + a few log lines.
// The expensive SDL calls already happened on the worker thread.
// ================================================================
void FDGSInputDevice::ApplyPendingRebuildResult()
{
	if (!RebuildRunnable) return;

	FDGSRebuildRunnable::FResult Result;
	if (!RebuildRunnable->TryDequeueResult(Result)) return;

	TArray<int32>        ResetIds;
	TArray<SDL_Gamepad*> HandlesToClose; // collected under lock, closed async after
	int32 ActiveAfterRebuild = -1;

	{
		FScopeLock Lock(&SDLCriticalSection);

		if (!bSDLReady)
		{
			// SDL was shut down while worker was running -- discard result
			for (auto& P : Result.NewPads)
				if (P.Pad) RebuildRunnable->EnqueueClose(P.Pad);
			return;
		}

		const double Now = FPlatformTime::Seconds();

		// Remove pads that are no longer in SDL's list
		for (int32 RemovedId : Result.RemovedIds)
		{
			for (int32 i = Pads.Num() - 1; i >= 0; --i)
			{
				if (Pads[i].InstanceId != RemovedId) continue;

				// Only the active pad ever emits input to UE, so only its removal
				// needs a reset. Resetting for an idle pad would release buttons
				// the player is still holding on the active pad.
				if (Pads[i].InstanceId == ActiveInstanceId)
				{
					ResetIds.Add(RemovedId);
					ActiveInstanceId = -1;
				}
				if (Pads[i].Pad) HandlesToClose.Add(Pads[i].Pad);
				Pads.RemoveAtSwap(i, 1, EAllowShrinking::No);
				break;
			}
		}

		// Add newly opened pads
		for (auto& NewPad : Result.NewPads)
		{
			// Already tracked (a scan snapshot raced with an unapplied result).
			// SDL ref-counts opens of the same ID, so closing this extra handle
			// leaves the tracked one valid and avoids a duplicate pad entry.
			bool bAlreadyTracked = false;
			for (const FPadState& P : Pads)
				if (P.InstanceId == NewPad.InstanceId) { bAlreadyTracked = true; break; }
			if (bAlreadyTracked)
			{
				if (NewPad.Pad) HandlesToClose.Add(NewPad.Pad);
				continue;
			}

			if (Pads.Num() >= kMaxTrackedControllers)
			{
				// Over limit -- send back to worker for async close
				if (NewPad.Pad) HandlesToClose.Add(NewPad.Pad);
				continue;
			}
			FPadState& State = Pads.AddDefaulted_GetRef();
			State.InstanceId = NewPad.InstanceId;
			State.Pad = NewPad.Pad;
			State.bConnected = true;
			State.LastActivityTime = Now;

			UE_LOG(LogDGSGamepadSupport, Log, TEXT("[DGS] Adopted pad ID=%d"), NewPad.InstanceId);
		}

		// Validate active instance is still present
		if (ActiveInstanceId >= 0)
		{
			bool bValid = false;
			for (const FPadState& P : Pads)
				if (P.InstanceId == ActiveInstanceId && P.bConnected) { bValid = true; break; }
			if (!bValid) ActiveInstanceId = -1;
		}

		// Auto-activate first available if none set
		if (ActiveInstanceId < 0)
		{
			for (FPadState& P : Pads)
			{
				if (P.bConnected)
				{
					ActiveInstanceId = P.InstanceId;
					P.LastActivityTime = Now;
					UE_LOG(LogDGSGamepadSupport, Log,
						TEXT("[DGS] Auto-activated ID=%d after rebuild"), P.InstanceId);
					break;
				}
			}
		}

		ActiveAfterRebuild = ActiveInstanceId;
	}
	// Lock released

	// Reset input states for removed pads. Must happen BEFORE the device can be
	// parked on the unpaired user below: Slate resolves the user from the device
	// mapping, so releases sent after that would never reach the player.
	for (int32 Id : ResetIds) ResetAllInputStates(Id);

	// Map TargetDeviceId to the player only while a PlayStation pad is active,
	// and park it on the unpaired user otherwise -- see DGS_SetTargetDeviceActive.
	const bool bHaveActivePad = (ActiveAfterRebuild >= 0);
	if (bHaveActivePad != bTargetDeviceConnected && TargetDeviceId.IsValid() && TargetUserId.IsValid())
	{
		DGS_SetTargetDeviceActive(IPlatformInputDeviceMapper::Get(), TargetDeviceId, TargetUserId, bHaveActivePad);
		bTargetDeviceConnected = bHaveActivePad;
	}

	// Close removed handles asynchronously (worker thread)
	for (SDL_Gamepad* Handle : HandlesToClose)
		if (Handle) RebuildRunnable->EnqueueClose(Handle);

	// Reset rumble cache
	LastRumbleLow = 0xFFFF;
	LastRumbleHigh = 0xFFFF;

	UE_LOG(LogDGSGamepadSupport, Log,
		TEXT("[DGS] Rebuild result applied -- Pads=%d Active=%d"),
		Pads.Num(), ActiveAfterRebuild);
}

// ================================================================
// INPUT STATE RESET
// ================================================================
void FDGSInputDevice::ResetAllInputStates(int32 DisconnectedInstanceId)
{
	if (!TargetUserId.IsValid() || !TargetDeviceId.IsValid()) return;
	DGSKeyCache::EnsureInit();
	UE_LOG(LogDGSGamepadSupport, Log, TEXT("[DGS] Resetting input states for ID=%d"), DisconnectedInstanceId);

	// Only keys actually held (see HeldKeys). Cleared before emitting, since
	// the handler may run game code.
	const uint32 Held = HeldKeys;
	HeldKeys = 0;

	for (int32 i = 0; i < NumButtons; ++i)
		if (Held & (1u << i))
			MessageHandler->OnControllerButtonReleased(DGSKeyCache::ButtonNames[i], TargetUserId, TargetDeviceId, false);
	for (int32 i = 0; i < NumAxes; ++i)
		MessageHandler->OnControllerAnalog(DGSKeyCache::AxisNames[i], TargetUserId, TargetDeviceId, 0.0f);
	for (int32 j = 0; j < DGSKeyCache::NumDigitalKeys; ++j)
		if (Held & (1u << (NumButtons + j)))
			MessageHandler->OnControllerButtonReleased(*DGSKeyCache::DigitalKeys[j], TargetUserId, TargetDeviceId, false);
}

void FDGSInputDevice::EmitDigitalKey(const FName& Key, int32 KeyBit, bool bPressed)
{
	const uint32 Bit = 1u << KeyBit;
	if (bPressed)
	{
		HeldKeys |= Bit;
		MessageHandler->OnControllerButtonPressed(Key, TargetUserId, TargetDeviceId, false);
	}
	else if (HeldKeys & Bit) // never send a release for a key that was not pressed
	{
		HeldKeys &= ~Bit;
		MessageHandler->OnControllerButtonReleased(Key, TargetUserId, TargetDeviceId, false);
	}
}

// ================================================================
// HOT-SWAP HEURISTIC (called UNDER SDLCriticalSection, per poll)
//
// SDL_GamepadConnected() removed from hot path -- bConnected is the
// source of truth, updated by ApplyPendingRebuildResult.
// ================================================================
void FDGSInputDevice::UpdateActiveControllerHeuristic()
{
	const int32 NumPads = Pads.Num();
	if (NumPads == 0)
	{
		if (ActiveInstanceId >= 0) ActiveInstanceId = -1;
		return;
	}

	// Single pad -- always active
	if (NumPads == 1)
	{
		FPadState& Only = Pads[0];
		if (Only.Pad && Only.bConnected && ActiveInstanceId != Only.InstanceId)
			ActiveInstanceId = Only.InstanceId;
		return;
	}

	const double Now = FPlatformTime::Seconds();
	const float ActiveThreshold   = kControllerSwitchThresh * 2.0f;
	const float InactiveThreshold = kControllerSwitchThresh;

	FPadState* CachedActivePad = nullptr;
	for (FPadState& P : Pads)
		if (P.InstanceId == ActiveInstanceId) { CachedActivePad = &P; break; }

	for (FPadState& P : Pads)
	{
		if (!P.Pad || !P.bConnected) continue;

		bool bHasInput = false;
		const float Threshold = (P.InstanceId == ActiveInstanceId) ? ActiveThreshold : InactiveThreshold;

		for (int32 i = 0; i < NumButtons && !bHasInput; ++i)
			bHasInput = (SDL_GetGamepadButton(P.Pad, GDGSButtons[i]) != 0);

		if (!bHasInput)
		{
			for (int32 i = 0; i < NumAxes && !bHasInput; ++i)
			{
				const Sint16 Raw = SDL_GetGamepadAxis(P.Pad, GDGSAxes[i]);
				const bool   bTrigger = ((DGSKeyCache::TriggerAxisMask >> i) & 1) != 0;
				const float  V = bTrigger ? DGS_NormalizeTriggerAxis(Raw) : FMath::Abs(DGS_NormalizeStickAxis(Raw));
				bHasInput = (V > Threshold);
			}
		}

		if (!bHasInput) continue;

		if (P.InstanceId != ActiveInstanceId)
		{
			if (ActiveInstanceId >= 0 && CachedActivePad && CachedActivePad->bConnected)
			{
				const double Elapsed = Now - CachedActivePad->LastActivityTime;
				if (Elapsed < kActiveHoldSeconds)
				{
					UE_LOG(LogDGSGamepadSupport, Log,
						TEXT("[DGS] Hot-swap blocked: hold %.2fs remaining"), kActiveHoldSeconds - Elapsed);
					return;
				}
			}
			UE_LOG(LogDGSGamepadSupport, Log,
				TEXT("[DGS] Hot-swap -> ID=%d"), P.InstanceId);
			ActiveInstanceId = P.InstanceId;
		}
		P.LastActivityTime = Now;
		return;
	}

	if (ActiveInstanceId < 0)
	{
		for (FPadState& P : Pads)
		{
			if (P.Pad && P.bConnected) { ActiveInstanceId = P.InstanceId; return; }
		}
	}
}

// ================================================================
// INPUT POLLING & EMISSION (hot path, PollRateHz)
// ================================================================
void FDGSInputDevice::PollActiveControllerAndEmit(bool bGameWorld)
{
	if (!TargetUserId.IsValid() || !TargetDeviceId.IsValid()) return;
	// Belt-and-suspenders: also guard here in case this is called directly.
	if (RebuildRunnable && (bool)RebuildRunnable->bRebuildInProgress) return;

	struct FInputSnapshot
	{
		bool  Buttons[NumButtons] = {};
		bool  PrevButtons[NumButtons] = {};
		float Axes[NumAxes] = {};
		float PrevAxes[NumAxes] = {};
		bool  TriggerLeft = false; bool PrevTriggerLeft = false;
		bool  TriggerRight = false; bool PrevTriggerRight = false;
		bool  LeftStickLeft = false; bool PrevLeftStickLeft = false;
		bool  LeftStickRight = false; bool PrevLeftStickRight = false;
		bool  LeftStickUp = false; bool PrevLeftStickUp = false;
		bool  LeftStickDown = false; bool PrevLeftStickDown = false;
		bool  RightStickLeft = false; bool PrevRightStickLeft = false;
		bool  RightStickRight = false; bool PrevRightStickRight = false;
		bool  RightStickUp = false; bool PrevRightStickUp = false;
		bool  RightStickDown = false; bool PrevRightStickDown = false;
		bool  bValid = false;
	};

	FInputSnapshot S;
	int32 MidPollDisconnectId = -1;

	{
		FScopeLock Lock(&SDLCriticalSection);
		if (!bSDLReady || !DGSSDLRuntime::IsReady()) return;

		// Find active pad (trusts bConnected -- no SDL_GamepadConnected per frame)
		FPadState* ActivePad = FindActivePadState_Locked();
		if (!ActivePad)
		{
			// Fallback: first connected pad
			for (FPadState& P : Pads)
			{
				if (P.Pad && P.bConnected) { ActivePad = &P; ActiveInstanceId = P.InstanceId; break; }
			}
		}
		if (!ActivePad) return;

		// Single SDL_GamepadConnected call as safety net for missed disconnect events
		// (e.g. Bluetooth drop without generating SDL_EVENT_GAMEPAD_REMOVED)
		if (SDL_GamepadConnected(ActivePad->Pad) == 0)
		{
			ActivePad->bConnected = false;
			MidPollDisconnectId = ActivePad->InstanceId;
			bPendingHotplug = true; // trigger rebuild check next frame
			// lock releases -- reset called below
		}
		else
		{
			const double Now = FPlatformTime::Seconds();

			for (int32 i = 0; i < NumButtons; ++i)
			{
				const bool bDown = (SDL_GetGamepadButton(ActivePad->Pad, GDGSButtons[i]) != 0);
				S.Buttons[i] = bDown;
				S.PrevButtons[i] = ActivePad->PrevButtons[i];
				if (bDown) ActivePad->LastActivityTime = Now;
			}

			for (int32 i = 0; i < NumAxes; ++i)
			{
				const Sint16 Raw = SDL_GetGamepadAxis(ActivePad->Pad, GDGSAxes[i]);
				const bool bTrigger = ((DGSKeyCache::TriggerAxisMask >> i) & 1) != 0;
				const bool bInvert = ((DGSKeyCache::InvertYAxisMask >> i) & 1) != 0;
				float V = bTrigger ? DGS_NormalizeTriggerAxis(Raw) : DGS_NormalizeStickAxis(Raw);
				if (bInvert) V = -V;
				S.Axes[i] = V;
				S.PrevAxes[i] = ActivePad->PrevAxes[i];
				if (FMath::Abs(V) > kControllerSwitchThresh) ActivePad->LastActivityTime = Now;
			}

			S.TriggerLeft = (S.Axes[4] > 0.5f); S.PrevTriggerLeft = ActivePad->PrevTriggerLeft;
			S.TriggerRight = (S.Axes[5] > 0.5f); S.PrevTriggerRight = ActivePad->PrevTriggerRight;
			S.LeftStickLeft = (S.Axes[0] < -0.5f); S.PrevLeftStickLeft = ActivePad->PrevLeftStickLeft;
			S.LeftStickRight = (S.Axes[0] > 0.5f); S.PrevLeftStickRight = ActivePad->PrevLeftStickRight;
			S.LeftStickUp = (S.Axes[1] > 0.5f); S.PrevLeftStickUp = ActivePad->PrevLeftStickUp;
			S.LeftStickDown = (S.Axes[1] < -0.5f); S.PrevLeftStickDown = ActivePad->PrevLeftStickDown;
			S.RightStickLeft = (S.Axes[2] < -0.5f); S.PrevRightStickLeft = ActivePad->PrevRightStickLeft;
			S.RightStickRight = (S.Axes[2] > 0.5f); S.PrevRightStickRight = ActivePad->PrevRightStickRight;
			S.RightStickUp = (S.Axes[3] > 0.5f); S.PrevRightStickUp = ActivePad->PrevRightStickUp;
			S.RightStickDown = (S.Axes[3] < -0.5f); S.PrevRightStickDown = ActivePad->PrevRightStickDown;

			FMemory::Memcpy(ActivePad->PrevButtons, S.Buttons, sizeof(S.Buttons));
			FMemory::Memcpy(ActivePad->PrevAxes, S.Axes, sizeof(S.Axes));
			ActivePad->PrevTriggerLeft = S.TriggerLeft;    ActivePad->PrevTriggerRight = S.TriggerRight;
			ActivePad->PrevLeftStickLeft = S.LeftStickLeft;  ActivePad->PrevLeftStickRight = S.LeftStickRight;
			ActivePad->PrevLeftStickUp = S.LeftStickUp;    ActivePad->PrevLeftStickDown = S.LeftStickDown;
			ActivePad->PrevRightStickLeft = S.RightStickLeft; ActivePad->PrevRightStickRight = S.RightStickRight;
			ActivePad->PrevRightStickUp = S.RightStickUp;   ActivePad->PrevRightStickDown = S.RightStickDown;
			S.bValid = true;
		}
	}
	// Lock released

	if (MidPollDisconnectId >= 0)
	{
		ResetAllInputStates(MidPollDisconnectId);
		return;
	}
	if (!S.bValid) return;

	for (int32 i = 0; i < NumButtons; ++i)
	{
		if (S.Buttons[i] == S.PrevButtons[i]) continue;
		EmitDigitalKey(DGSKeyCache::ButtonNames[i], i, S.Buttons[i]);
	}

	{
		// Same order as DGSKeyCache::DigitalKeys (the held-mask bit order).
		struct FDE { bool Cur; bool Prev; };
		const FDE D[] = {
			{ S.TriggerLeft,     S.PrevTriggerLeft     },
			{ S.TriggerRight,    S.PrevTriggerRight    },
			{ S.LeftStickLeft,   S.PrevLeftStickLeft   },
			{ S.LeftStickRight,  S.PrevLeftStickRight  },
			{ S.LeftStickUp,     S.PrevLeftStickUp     },
			{ S.LeftStickDown,   S.PrevLeftStickDown   },
			{ S.RightStickLeft,  S.PrevRightStickLeft  },
			{ S.RightStickRight, S.PrevRightStickRight },
			{ S.RightStickUp,    S.PrevRightStickUp    },
			{ S.RightStickDown,  S.PrevRightStickDown  },
		};
		static_assert(UE_ARRAY_COUNT(D) == DGSKeyCache::NumDigitalKeys, "digital key table mismatch");
		for (int32 j = 0; j < DGSKeyCache::NumDigitalKeys; ++j)
		{
			if (D[j].Cur == D[j].Prev) continue;
			EmitDigitalKey(*DGSKeyCache::DigitalKeys[j], NumButtons + j, D[j].Cur);
		}
	}

	for (int32 i = 0; i < NumAxes; ++i)
	{
		float Safe = S.Axes[i];
		const float Prev = S.PrevAxes[i];

		if (bGameWorld)
		{
			if (FMath::IsNearlyEqual(Safe, Prev, kAxisNoiseThreshold)) continue;
		}
		else
		{
			const bool bNowSig = FMath::Abs(Safe) > 0.5f;
			const bool bWasSig = FMath::Abs(Prev) > 0.5f;
			if (!bNowSig && !bWasSig) continue;
			if (bNowSig == bWasSig && FMath::IsNearlyEqual(Safe, Prev, 0.05f)) continue;

			// Crossing below the significance threshold: emit a hard 0.
			// The raw value here (e.g. 0.4) would be the LAST one ever sent --
			// every later sample is filtered by the !bNowSig && !bWasSig check
			// above -- leaving the axis latched non-zero in the editor.
			if (!bNowSig) Safe = 0.f;
		}

		MessageHandler->OnControllerAnalog(DGSKeyCache::AxisNames[i], TargetUserId, TargetDeviceId, Safe);
	}

}

// ================================================================
// CONTEXT CHECK
// ================================================================
bool FDGSInputDevice::ShouldInjectToGame()
{
	if (FApp::IsGame()) { bInGameWorld = true; return true; }
	if (!GEngine) { bInGameWorld = false; return false; }
	for (const FWorldContext& C : GEngine->GetWorldContexts())
	{
		const UWorld* W = C.World();
		if (W && (W->WorldType == EWorldType::PIE || W->WorldType == EWorldType::Game))
		{
			bInGameWorld = true; return true;
		}
	}
	bInGameWorld = false; return false;
}


SDL_Gamepad* FDGSInputDevice::FindActivePad_Locked() const
{
	for (const FPadState& P : Pads)
		if (P.InstanceId == ActiveInstanceId && P.Pad && P.bConnected)
			return P.Pad;
	return nullptr;
}

FDGSInputDevice::FPadState* FDGSInputDevice::FindActivePadState_Locked()
{
	for (FPadState& P : Pads)
		if (P.InstanceId == ActiveInstanceId && P.Pad && P.bConnected)
			return &P;
	return nullptr;
}

#endif // WITH_SDL3