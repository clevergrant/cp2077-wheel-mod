#pragma once

#include "sources.h"

#include <array>
#include <cstdint>

namespace gwheel::input_bindings
{
    // Stable integer IDs for each physical control on the wheel. Order is
    // locked by the config.json schema and by the field order in
    // gwheel_settings.reds — do NOT renumber. New controls append at the
    // end. Count (kCount) is updated as we go.
    enum PhysicalInput : int32_t
    {
        PaddleLeft = 0,
        PaddleRight,
        DpadUp,
        DpadDown,
        DpadLeft,
        DpadRight,
        ButtonA,
        ButtonB,
        ButtonX,
        ButtonY,
        Start,
        Select,
        LSB,
        RSB,
        Plus,
        Minus,
        ScrollClick,
        ScrollCW,
        ScrollCCW,
        Xbox,
        kCount
    };

    // Curated action set. Every value here dispatches to a specific Windows
    // virtual-key or mouse event via SendInput. Keep in sync with the
    // `GWheelAction` enum in gwheel_settings.reds (same values, same order)
    // so Mod Settings dropdown indices round-trip correctly.
    //
    // CameraCycleBackward is actually "rear-view camera" (MMB tap, shows
    // what's behind you) despite the historical name. It's a tap action;
    // holding the physical button does not hold MMB, because the game
    // treats held MMB as tag / weapon wheel which is not what we want.
    enum Action : int32_t
    {
        None = 0,
        Horn,
        Headlights,
        Handbrake,
        Autodrive,
        ExitVehicle,
        CameraCycleForward,
        CameraCycleBackward,
        CameraReset,
        ZoomIn,
        ZoomOut,
        ShootPrimary,
        ShootSecondary,
        ShootTertiary,
        NextWeapon,
        PrevWeapon,
        WeaponSlot1,
        WeaponSlot2,
        SwitchWeapons,
        HolsterWeapon,
        OpenMap,
        OpenJournal,
        OpenInventory,
        OpenPhone,
        OpenPerks,
        OpenCrafting,
        QuickSave,
        RadioMenu,
        UseConsumable,
        IconicCyberware,
        Pause,
        Tag,
        CallVehicle,
        MenuConfirm,
        MenuCancel,
        MenuUp,
        MenuDown,
        MenuLeft,
        MenuRight,
        kActionCount
    };

    using BindingArray = std::array<int32_t, kCount>;

    // Replace the entire binding table at once (called by config on load /
    // per-change). Index = PhysicalInput, value = Action.
    void ReplaceAll(const BindingArray& bindings);

    // Set a single binding. Used by the per-input native. Out-of-range
    // inputId or action silently no-ops.
    void Set(int32_t inputId, int32_t action);

    // Read current binding for an input. Returns 0 (None) if unset.
    int32_t Get(int32_t inputId);

    // Pick the per-device button/POV mapping based on the wheel's friendly
    // product name (from LogiGetFriendlyProductName). Call once at bind
    // time. Logs which layout was matched and whether it is empirically
    // verified. Unknown wheels fall back to the G923 Xbox layout with a
    // warning.
    void SetDeviceLayout(const char* friendlyProductName);

    // Called once per pump tick with the latest input frame. Detects
    // rising/falling edges on all physical inputs, dispatches bound actions
    // via SendInput. Gated internally on config.input.enabled. Uses the
    // menu-active + in-vehicle context from sources to decide which
    // actions to dispatch.
    void OnTick(const sources::Frame& frame);

}
