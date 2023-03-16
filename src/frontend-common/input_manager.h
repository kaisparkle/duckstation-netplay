// SPDX-FileCopyrightText: 2019-2022 Connor McLaughlin <stenzek@gmail.com>
// SPDX-License-Identifier: (GPL-3.0 OR CC-BY-NC-ND-4.0)

#pragma once

#include <functional>
#include <mutex>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>

#include "common/settings_interface.h"
#include "common/types.h"
#include "common/window_info.h"

#include "core/input_types.h"

/// Class, or source of an input event.
enum class InputSourceType : u32
{
  Keyboard,
  Pointer,
  Sensor,
#ifdef _WIN32
  DInput,
  XInput,
  RawInput,
#endif
#ifdef WITH_SDL2
  SDL,
#endif
#ifdef WITH_EVDEV
  Evdev,
#endif
#ifdef __ANDROID__
  Android,
#endif
  Count,
};

/// Subtype of a key for an input source.
enum class InputSubclass : u32
{
  None = 0,

  PointerButton = 0,
  PointerAxis = 1,

  ControllerButton = 0,
  ControllerAxis = 1,
  ControllerHat = 2,
  ControllerMotor = 3,
  ControllerHaptic = 4,

  SensorAccelerometer = 0,
};

enum class InputModifier : u32
{
  None = 0,
  Negate,   ///< Input * -1, gets the negative side of the axis
  FullAxis, ///< (Input * 0.5) + 0.5, uses both the negative and positive side of the axis together
};

/// A composite type representing a full input key which is part of an event.
union InputBindingKey
{
  struct
  {
    InputSourceType source_type : 4;
    u32 source_index : 8;             ///< controller number
    InputSubclass source_subtype : 3; ///< if 1, binding is for an axis and not a button (used for controllers)
    InputModifier modifier : 2;
    u32 invert : 1; ///< if 1, value is inverted prior to being sent to the sink
    u32 unused : 14;
    u32 data;
  };

  u64 bits;

  bool operator==(const InputBindingKey& k) const { return bits == k.bits; }
  bool operator!=(const InputBindingKey& k) const { return bits != k.bits; }

  /// Removes the direction bit from the key, which is used to look up the bindings for it.
  /// This is because negative bindings should still fire when they reach zero again.
  InputBindingKey MaskDirection() const
  {
    InputBindingKey r;
    r.bits = bits;
    r.modifier = InputModifier::None;
    r.invert = 0;
    return r;
  }
};
static_assert(sizeof(InputBindingKey) == sizeof(u64), "Input binding key is 64 bits");

/// Hashability for InputBindingKey
struct InputBindingKeyHash
{
  std::size_t operator()(const InputBindingKey& k) const { return std::hash<u64>{}(k.bits); }
};

/// Callback type for a binary event. Usually used for hotkeys.
using InputButtonEventHandler = std::function<void(s32 value)>;

/// Callback types for a normalized event. Usually used for pads.
using InputAxisEventHandler = std::function<void(float value)>;

/// ------------------------------------------------------------------------
/// Event Handler Type
/// ------------------------------------------------------------------------
/// This class acts as an adapter to convert from normalized values to
/// binary values when the callback is a binary/button handler. That way
/// you don't need to convert float->bool in your callbacks.
using InputEventHandler = std::variant<InputAxisEventHandler, InputButtonEventHandler>;

/// Input monitoring for external access.
struct InputInterceptHook
{
  enum class CallbackResult
  {
    StopProcessingEvent,
    ContinueProcessingEvent,
    RemoveHookAndStopProcessingEvent,
    RemoveHookAndContinueProcessingEvent,
  };

  using Callback = std::function<CallbackResult(InputBindingKey key, float value)>;
};

/// Hotkeys are actions (e.g. toggle frame limit) which can be bound to keys or chords.
/// The handler is called with an integer representing the key state, where 0 means that
/// one or more keys were released, 1 means all the keys were pressed, and -1 means that
/// the hotkey was cancelled due to a chord with more keys being activated.
struct HotkeyInfo
{
  const char* name;
  const char* category;
  const char* display_name;
  void (*handler)(s32 pressed);
};
#define DECLARE_HOTKEY_LIST(name) extern const HotkeyInfo name[]
#define BEGIN_HOTKEY_LIST(name) const HotkeyInfo name[] = {
#define DEFINE_HOTKEY(name, category, display_name, handler) {(name), (category), (display_name), (handler)},
#define END_HOTKEY_LIST()                                                                                              \
  {                                                                                                                    \
    nullptr, nullptr, nullptr, nullptr                                                                                 \
  }                                                                                                                    \
  }                                                                                                                    \
  ;

DECLARE_HOTKEY_LIST(g_common_hotkeys);
DECLARE_HOTKEY_LIST(g_host_hotkeys);

/// Generic input bindings. These roughly match a DualShock 4 or XBox One controller.
/// They are used for automatic binding to PS2 controller types, and for big picture mode navigation.
enum class GenericInputBinding : u8;
using GenericInputBindingMapping = std::vector<std::pair<GenericInputBinding, std::string>>;

/// Host mouse relative axes are X, Y, wheel horizontal, wheel vertical.
enum class InputPointerAxis : u8
{
  X,
  Y,
  WheelX,
  WheelY,
  Count
};

/// External input source class.
class InputSource;

namespace InputManager {
/// Minimum interval between vibration updates when the effect is continuous.
static constexpr double VIBRATION_UPDATE_INTERVAL_SECONDS = 0.5; // 500ms

/// Maximum number of host mouse devices.
static constexpr u32 MAX_POINTER_DEVICES = 1;

/// Number of macro buttons per controller.
static constexpr u32 NUM_MACRO_BUTTONS_PER_CONTROLLER = 4;

/// Returns a pointer to the external input source class, if present.
InputSource* GetInputSourceInterface(InputSourceType type);

/// Converts an input class to a string.
const char* InputSourceToString(InputSourceType clazz);

/// Returns the default state for an input source.
bool GetInputSourceDefaultEnabled(InputSourceType type);

/// Parses an input class string.
std::optional<InputSourceType> ParseInputSourceString(const std::string_view& str);

/// Parses a pointer device string, i.e. tells you which pointer is specified.
std::optional<u32> GetIndexFromPointerBinding(const std::string_view& str);

/// Returns the device name for a pointer index (e.g. Pointer-0).
std::string GetPointerDeviceName(u32 pointer_index);

/// Converts a key code from a human-readable string to an identifier.
std::optional<u32> ConvertHostKeyboardStringToCode(const std::string_view& str);

/// Converts a key code from an identifier to a human-readable string.
std::optional<std::string> ConvertHostKeyboardCodeToString(u32 code);

/// Creates a key for a host-specific key code.
InputBindingKey MakeHostKeyboardKey(u32 key_code);

/// Creates a key for a host-specific button.
InputBindingKey MakePointerButtonKey(u32 index, u32 button_index);

/// Creates a key for a host-specific mouse relative event
/// (axis 0 = horizontal, 1 = vertical, 2 = wheel horizontal, 3 = wheel vertical).
InputBindingKey MakePointerAxisKey(u32 index, InputPointerAxis axis);

/// Creates a key for a host-specific sensor.
InputBindingKey MakeSensorAxisKey(InputSubclass sensor, u32 axis);

/// Parses an input binding key string.
std::optional<InputBindingKey> ParseInputBindingKey(const std::string_view& binding);

/// Converts a input key to a string.
std::string ConvertInputBindingKeyToString(InputBindingInfo::Type binding_type, InputBindingKey key);

/// Converts a chord of binding keys to a string.
std::string ConvertInputBindingKeysToString(InputBindingInfo::Type binding_type, const InputBindingKey* keys,
                                            size_t num_keys);

/// Returns a list of all hotkeys.
std::vector<const HotkeyInfo*> GetHotkeyList();

/// Enumerates available devices. Returns a pair of the prefix (e.g. SDL-0) and the device name.
std::vector<std::pair<std::string, std::string>> EnumerateDevices();

/// Enumerates available vibration motors at the time of call.
std::vector<InputBindingKey> EnumerateMotors();

/// Retrieves bindings that match the generic bindings for the specified device.
GenericInputBindingMapping GetGenericBindingMapping(const std::string_view& device);

/// Returns true if the specified input source is enabled.
bool IsInputSourceEnabled(SettingsInterface& si, InputSourceType type);

/// Re-parses the config and registers all hotkey and pad bindings.
void ReloadBindings(SettingsInterface& si, SettingsInterface& binding_si);

/// Migrates any bindings from the pre-InputManager configuration.
bool MigrateBindings(SettingsInterface& si);

/// Re-parses the sources part of the config and initializes any backends.
void ReloadSources(SettingsInterface& si, std::unique_lock<std::mutex>& settings_lock);

/// Called when a device change is triggered by the system (DBT_DEVNODES_CHANGED on Windows).
/// Returns true if any device changes are detected.
bool ReloadDevices();

/// Shuts down any enabled input sources.
void CloseSources();

/// Polls input sources for events (e.g. external controllers).
void PollSources();

/// Returns true if any bindings exist for the specified key.
/// Can be safely called on another thread.
bool HasAnyBindingsForKey(InputBindingKey key);

/// Returns true if any bindings exist for the specified source + index.
/// Can be safely called on another thread.
bool HasAnyBindingsForSource(InputBindingKey key);

/// Parses a string binding into its components. Use with external AddBinding().
bool ParseBindingAndGetSource(const std::string_view& binding, InputBindingKey* key, InputSource** source);

/// Externally adds a fixed binding. Be sure to call *after* ReloadBindings() otherwise it will be lost.
void AddBinding(const std::string_view& binding, const InputEventHandler& handler);

/// Adds an external vibration binding.
void AddVibrationBinding(u32 pad_index, const InputBindingKey* motor_0_binding, InputSource* motor_0_source,
                         const InputBindingKey* motor_1_binding, InputSource* motor_1_source);

/// Updates internal state for any binds for this key, and fires callbacks as needed.
/// Returns true if anything was bound to this key, otherwise false.
bool InvokeEvents(InputBindingKey key, float value, GenericInputBinding generic_key = GenericInputBinding::Unknown);

/// Clears internal state for any binds with a matching source/index.
void ClearBindStateFromSource(InputBindingKey key);

/// Sets a hook which can be used to intercept events before they're processed by the normal bindings.
/// This is typically used when binding new controls to detect what gets pressed.
void SetHook(InputInterceptHook::Callback callback);

/// Removes any currently-active interception hook.
void RemoveHook();

/// Returns true if there is an interception hook present.
bool HasHook();

/// Internal method used by pads to dispatch vibration updates to input sources.
/// Intensity is normalized from 0 to 1.
void SetPadVibrationIntensity(u32 pad_index, float large_or_single_motor_intensity, float small_motor_intensity);

/// Zeros all vibration intensities. Call when pausing.
/// The pad vibration state will internally remain, so that when emulation is unpaused, the effect resumes.
void PauseVibration();

/// Updates absolute pointer position. Can call from UI thread, use when the host only reports absolute coordinates.
void UpdatePointerAbsolutePosition(u32 index, float x, float y);

/// Updates relative pointer position. Can call from the UI thread, use when host supports relative coordinate
/// reporting.
void UpdatePointerRelativeDelta(u32 index, InputPointerAxis axis, float d, bool raw_input = false);

/// Sets the state of the specified macro button.
void SetMacroButtonState(u32 pad, u32 index, bool state);

/// Returns true if the raw input source is being used.
bool IsUsingRawInput();

/// Returns true if any bindings are present which require relative mouse movement.
bool HasPointerAxisBinds();

/// Restores default configuration.
void SetDefaultConfig(SettingsInterface& si);

/// Clears all bindings for a given port.
void ClearPortBindings(SettingsInterface& si, u32 port);

/// Copies pad configuration from one interface (ini) to another.
void CopyConfiguration(SettingsInterface* dest_si, const SettingsInterface& src_si, bool copy_pad_config = true,
                       bool copy_pad_bindings = true, bool copy_hotkey_bindings = true);

/// Performs automatic controller mapping with the provided list of generic mappings.
bool MapController(SettingsInterface& si, u32 controller,
                   const std::vector<std::pair<GenericInputBinding, std::string>>& mapping);

/// Returns a list of input profiles available.
std::vector<std::string> GetInputProfileNames();

/// Called when a new input device is connected.
void OnInputDeviceConnected(const std::string_view& identifier, const std::string_view& device_name);

/// Called when an input device is disconnected.
void OnInputDeviceDisconnected(const std::string_view& identifier);
} // namespace InputManager

namespace Host {
/// Return the current window handle. Needed for DInput.
std::optional<WindowInfo> GetTopLevelWindowInfo();

/// Called when a new input device is connected.
void OnInputDeviceConnected(const std::string_view& identifier, const std::string_view& device_name);

/// Called when an input device is disconnected.
void OnInputDeviceDisconnected(const std::string_view& identifier);
} // namespace Host
