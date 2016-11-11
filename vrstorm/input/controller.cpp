#include <iostream>
#include "vrstorm/manager.h"

namespace vrstorm {
namespace input {

controller::controller(manager &this_parent)
  : parent(this_parent) {
  /// Default constructor
}
controller::~controller() {
  /// Default denstructor
}

void controller::init() {
  /// Assign a safe default function to all controller arrays
  for(unsigned int hand_id = 0; hand_id != max; ++hand_id) {
    for(unsigned int axis = 0; axis != max_axis; ++axis) {
      for(unsigned int axis_direction_id = 0; axis_direction_id != max_axis_direction; ++axis_direction_id) {
        #ifdef DEBUG_INPUTSTORM
          std::stringstream ss;
          ss << "VRStorm: DEBUG: unbound controller function called on axis " << axis;
          if(hand_id != 0) {
            ss << " on controller hand " << hand_id;
          }
          ss << " value ";
          bind_axis(static_cast<hand_type>(hand_id), axis, static_cast<axis_direction_type>(axis_direction_id), [s = ss.str()](float value){
            std::cout << s << std::fixed << value <<  std::endl;
          });
        #else
          bind_axis(static_cast<hand_type>(hand_id), axis, static_cast<axis_direction_type>(axis_direction_id), [](float value __attribute__((__unused__))){}); // default to noop
          axis_bindings[hand_id][axis][axis_direction_id].enabled = false;
        #endif // DEBUG_INPUTSTORM
      }
    }
  }
  /// assign a safe default function to all controller buttons
  for(actiontype action : actiontype()) {
    for(unsigned int hand_id = 0; hand_id != max; ++hand_id) {
      for(unsigned int button = 0; button != max_button; ++button) {
        #ifdef DEBUG_INPUTSTORM
          std::stringstream ss;
          ss << "VRStorm: DEBUG: unbound controller function called on button " << button;
          if(hand_id != 0) {
            ss << " on controller hand " << hand_id;
          }
          ss << " action " << get_actiontype_name(action);
          if(action == actiontype::PRESS) {
            bind_button(static_cast<hand_type>(hand_id), button, action, [s = ss.str()]{
              std::cout << s << std::endl;
            });
          } else {
            bind_button(static_cast<hand_type>(hand_id), button, action, []{});
          }
        #else
          bind_button(static_cast<hand_type>(hand_id), button, action, []{});   // default to noop
        #endif // DEBUG_INPUTSTORM
      }
    }
  }

  // enable all the joysticks by default
  enabled.fill(true);
  update_hands();
  update_names();

  // report status
  std::cout << "VRStorm: Controller axis bindings:   " << sizeof(axis_bindings) / 1024 << "KB" << std::endl;
  std::cout << "VRStorm: Controller button bindings: " << sizeof(button_bindings) / 1024 << "KB" << std::endl;
}

inputstorm::input::joystick_axis_bindingtype const &controller::axis_binding_at(hand_type hand,
                                                                                unsigned int axis,
                                                                                axis_direction_type axis_direction) const {
  /// Accessor for the controlle axis structure sparse arrays
  #ifndef NDEBUG
    // boundary safety check
    if(static_cast<unsigned int>(hand) >= max) {
      std::cout << "VRStorm: ERROR: attempting to address axis of controller hand " << static_cast<unsigned int>(hand) << " when max is " << max - 1 << std::endl;
      return axis_bindings[0][0][0];
    }
    if(axis >= max_axis) {
      std::cout << "VRStorm: ERROR: attempting to address controller axis number " << axis << " when max is " << max_axis - 1 << std::endl;
      return axis_bindings[0][0][0];
    }
    if(static_cast<unsigned int>(axis_direction) >= max_axis_direction) {
      std::cout << "VRStorm: ERROR: attempting to address controller axis direction " << static_cast<unsigned int>(axis_direction) << " when max is " << max_axis_direction - 1 << std::endl;
      return axis_bindings[0][0][0];
    }
  #endif // NDEBUG
  return axis_bindings[static_cast<unsigned int>(hand)][axis][static_cast<unsigned int>(axis_direction)];
}
std::function<void()> const &controller::button_binding_at(hand_type hand,
                                                           unsigned int button,
                                                           actiontype action) const {
  /// Accessor for the controlle button function sparse arrays
  #ifndef NDEBUG
    // boundary safety check
    if(static_cast<unsigned int>(hand) >= max) {
      std::cout << "VRStorm: ERROR: attempting to address button of controller hand " << static_cast<unsigned int>(hand) << " when max is " << max - 1 << std::endl;
      return button_bindings[0][0][0];
    }
    if(button >= max_button) {
      std::cout << "VRStorm: ERROR: attempting to address button number " << button << " when max is " << max_button - 1 << std::endl;
      return button_bindings[0][0][0];
    }
    if(static_cast<unsigned int>(action) > static_cast<unsigned int>(actiontype::LAST)) {
      std::cout << "VRStorm: ERROR: attempting to address button action " << static_cast<unsigned int>(action) << " when max is " << static_cast<unsigned int>(actiontype::LAST) << std::endl;
      return button_bindings[0][0][0];
    }
  #endif // NDEBUG
  return button_bindings[static_cast<unsigned int>(action)][static_cast<unsigned int>(hand)][button];
}

bool controller::get_enabled(hand_type hand) const {
  /// Return whether a specific controller hand is enabled
  return enabled[static_cast<unsigned int>(hand)];
}
std::string controller::get_name(hand_type hand) const {
  /// Return the name of a specific controller hand
  return names[static_cast<unsigned int>(hand)];
}
std::string controller::get_name_button(unsigned int button) const {
  /// Return the name of a controller button
  switch(button) {
  case vr::k_EButton_System:
    return "SYSTEM";
  case vr::k_EButton_ApplicationMenu:
    return "MENU";
  case vr::k_EButton_Grip:                                                      // duplicated as back
    return "GRIP";
  case vr::k_EButton_DPad_Left:
    return "DPAD LEFT";
  case vr::k_EButton_DPad_Up:
    return "DPAD UP";
  case vr::k_EButton_DPad_Right:
    return "DPAD RIGHT";
  case vr::k_EButton_DPad_Down:
    return "DPAD DOWN";
  case vr::k_EButton_A:
    return "A";
  //case vr::k_EButton_Axis0:                                                     // duplicated as touchpad
  //  return "AXIS 0";
  //case vr::k_EButton_Axis1:                                                     // duplicated as trigger
  //  return "AXIS 1";
  case vr::k_EButton_Axis2:
    return "AXIS 2";
  case vr::k_EButton_Axis3:
    return "AXIS 3";
  case vr::k_EButton_Axis4:
    return "AXIS 4";
  case vr::k_EButton_SteamVR_Touchpad:                                          // duplicate of k_EButton_Axis0
    return "TOUCHPAD";
  case vr::k_EButton_SteamVR_Trigger:                                           // duplicate of k_EButton_Axis1
    return "TRIGGER";
  //case vr::k_EButton_Dashboard_Back:                                            // duplicate of k_EButton_Grip
  //  return "BACK";
  default:
    std::stringstream ss;
    ss << "BUTTON " << button;
    return ss.str();
  }
  // TODO: compare with vr::GetButtonIdNameFromEnum(button);
}
std::string controller::get_name_axis(hand_type hand, unsigned int axis) const {
  /// Return the name of a controller axis
  #ifndef NDEBUG
    // boundary safety check
    if(axis >= vr::Prop_Axis4Type_Int32 - vr::Prop_Axis0Type_Int32) {
      std::cout << "VRStorm: ERROR: attempting to address controller axis number " << axis << " when max is " << vr::Prop_Axis4Type_Int32 - vr::Prop_Axis0Type_Int32 - 1 << std::endl;
      return "UNKNOWN";
    }
  #endif // NDEBUG
  // TODO: cache this with for(unsigned int axis = 0; axis != vr::Prop_Axis4Type_Int32 - vr::Prop_Axis0Type_Int32; ++axis)
  std::stringstream ss;
  unsigned int const controller_id = get_id(hand);
  switch(parent.hmd_handle->GetInt32TrackedDeviceProperty(controller_id, static_cast<vr::ETrackedDeviceProperty>(vr::Prop_Axis0Type_Int32 + axis))) {
  case vr::k_eControllerAxis_None:
    std::cout << "VRStorm: WARNING: OpenVR claims axis " << axis << " on controller " << controller_id << " is of type \"none\"" << std::endl;
    ss << "UNKNOWN(NONE)";
    break;
  case vr::k_eControllerAxis_TrackPad:
    ss << "TRACKPAD";
    break;
  case vr::k_eControllerAxis_Joystick:
    ss << "JOYSTICK";
    break;
  case vr::k_eControllerAxis_Trigger:
    ss << "TRIGGER";
    break;
  default:
    ss << "UNKNOWN";
    break;
  }
  ss << " #" << axis;
  return ss.str();
  // TODO: compare with vr::GetControllerAxisTypeNameFromEnum(axis);
}
unsigned int controller::get_id(hand_type hand) const {
  /// Get controller id by hand
  return controller_ids[static_cast<unsigned int>(hand)];
}
std::string controller::get_handtype_name(hand_type hand) {
  /// Return a human-readable name for this controller hand
  switch(hand) {
  case hand_type::LEFT:
    return "LEFT";
  case hand_type::RIGHT:
    return "RIGHT";
  default:
    return "UNKNOWN";
  }
}
std::string controller::get_actiontype_name(actiontype action) {
  /// Return a human-readable name for this controller button actiontype
  switch(action) {
  case actiontype::RELEASE:
    return "RELEASE";
  case actiontype::PRESS:
    return "PRESS";
  case actiontype::TOUCH:
    return "TOUCH";
  case actiontype::UNTOUCH:
    return "UNTOUCH";
  default:
    return "";
  }
}

void controller::bind_axis(hand_type hand,
                           unsigned int axis,
                           axis_direction_type axis_direction,
                           std::function<void(float)> func,
                           bool flip,
                           float deadzone_min,
                           float deadzone_max,
                           float saturation_min,
                           float saturation_max,
                           float centre) {
  /// Bind a function to a controlle axis, with the specified parameters
  #ifndef NDEBUG
    if(!func) {
      std::cout << "VRStorm: WARNING: Binding a null function to axis " << axis << " on controller hand " << static_cast<unsigned int>(hand) << ", this will throw an exception if called!" << std::endl;
    }
  #endif // NDEBUG
  auto &this_binding = axis_bindings[static_cast<unsigned int>(hand)][axis][static_cast<unsigned int>(axis_direction)]; // NOTE: this will contain the already bound axis configuration, which will remember anything not explicitly set here
  this_binding.deadzone_min = deadzone_min;
  this_binding.deadzone_max = deadzone_max;
  this_binding.saturation_min = saturation_min;
  this_binding.saturation_max = saturation_max;
  this_binding.centre = centre;

  if(flip) {                                                                    // invert the axis if requested
    if(this_binding.premultiply > 0) {
      this_binding.premultiply *= -1.0f;                                        // keep the premultiply, and invert it only if it's not already facing the right way
    }
  } else {
    if(this_binding.premultiply < 0) {
      this_binding.premultiply *= -1.0f;                                        // keep the premultiply, and invert it only if it's not already facing the right way
    }
  }
  this_binding.update_scales();
  this_binding.func = func;
  this_binding.enabled = true;
}
void controller::bind_axis_half(hand_type hand,
                                unsigned int axis,
                                axis_direction_type axis_direction,
                                std::function<void(float)> func,
                                bool flip) {
  /// Helper function for binding to output on a half-axis, for instance with a throttle control that ranges -1 to 1
  bind_axis(hand, axis, axis_direction, func, flip, 0.0f, 0.0f, -1.0f, 2.0f, -1.0f);
}
void controller::bind_axis(controller::binding_axis const &this_binding, std::function<void(float)> func) {
  /// Helper function to load binding settings from a binding object
  bind_axis(this_binding.hand,
            this_binding.axis,
            this_binding.direction,
            func,
            this_binding.flip,
            this_binding.deadzone_min,
            this_binding.deadzone_max,
            this_binding.saturation_min,
            this_binding.saturation_max,
            this_binding.centre);
}
void controller::bind_button(hand_type hand,
                             unsigned int button,
                             actiontype action,
                             std::function<void()> func) {
  /// Bind a function to a controlle button
  #ifndef NDEBUG
    if(!func) {
      std::cout << "VRStorm: WARNING: Binding a null function to button " << button << " on controller hand " << static_cast<unsigned int>(hand) << ", this will throw an exception if called!" << std::endl;
    }
  #endif // NDEBUG
  button_bindings[static_cast<unsigned int>(action)][static_cast<unsigned int>(hand)][button] = func;
}
void controller::bind_button_any(hand_type hand, std::function<void()> func) {
  /// Helper function to bind a callback to all controlle buttons, press event only
  for(unsigned int button = 0; button != max_button; ++button) {
    bind_button(hand, button, actiontype::PRESS, func);
  }
}
void controller::bind_button_any_all(std::function<void()> func) {
  /// Helper function to bind a callback to all controlle buttons on all joysticks, press event only
  bind_button_any(hand_type::LEFT,  func);
  bind_button_any(hand_type::RIGHT, func);
}
void controller::bind_button(binding_button const &this_binding,
                             std::function<void()> func_press,
                             std::function<void()> func_release,
                             std::function<void()> func_touch,
                             std::function<void()> func_untouch) {
  /// Helper function to load binding settings from a binding object
  switch(this_binding.type) {
  case binding_button::bindtype::SPECIFIC:
    if(func_press) {
      bind_button(this_binding.hand, this_binding.button, actiontype::PRESS, func_press);
    }
    if(func_release) {
      bind_button(this_binding.hand, this_binding.button, actiontype::RELEASE, func_release);
    }
    if(func_touch) {
      bind_button(this_binding.hand, this_binding.button, actiontype::TOUCH, func_touch);
    }
    if(func_untouch) {
      bind_button(this_binding.hand, this_binding.button, actiontype::UNTOUCH, func_untouch);
    }
    break;
  case binding_button::bindtype::ANY:
    if(func_press) {
      bind_button_any(this_binding.hand, func_press);
    } else {
      std::cout << "VRStorm: Joystick: WARNING - requested to bind to any button with a function other than PRESS on controller hand " << static_cast<unsigned int>(this_binding.hand) << ", this is not currently supported - create a set of specific bindings instead." << std::endl;
    }
    #ifndef NDEBUG
      if(func_release) {
        std::cout << "VRStorm: WARNING: Requested to bind a function to any button release on controller hand " << static_cast<unsigned int>(this_binding.hand) << ", which is not possible - create a set of specific bindings instead." << std::endl;
      }
    #endif // NDEBUG
    break;
  case binding_button::bindtype::ANY_ALL:
    if(func_press) {
      bind_button_any_all(func_press);
    } else {
      std::cout << "VRStorm: Joystick: WARNING - requested to bind to any button on all controllers with a function other than PRESS, this is not currently supported - create a set of specific bindings instead." << std::endl;
    }
    #ifndef NDEBUG
      if(func_release) {
        std::cout << "VRStorm: WARNING: Requested to bind a function to any button release on all controllers, which is not possible - create a set of specific bindings instead." << std::endl;
      }
    #endif // NDEBUG
    break;
  }
}

void controller::unbind_axis(hand_type hand,
                             unsigned int axis,
                             axis_direction_type axis_direction) {
  /// Unbind a callback on a controlle axis
  auto &this_binding = axis_bindings[static_cast<unsigned int>(hand)][axis][static_cast<unsigned int>(axis_direction)];
  this_binding.func = [](float value __attribute__((unused))){};                // noop
  this_binding.enabled = false;
}
void controller::unbind_axis_any(hand_type hand) {
  /// Helper function to unbind all axes on a controlle
  for(unsigned int axis = 0; axis != max_axis; ++axis) {
    unbind_axis(hand, axis, axis_direction_type::X);
    unbind_axis(hand, axis, axis_direction_type::Y);
  }
}
void controller::unbind_axis_any_all() {
  /// Helper function to unbind all axes on all controlles
  unbind_axis_any(hand_type::LEFT);
  unbind_axis_any(hand_type::RIGHT);
}
void controller::unbind_axis(binding_axis const &this_binding) {
  /// Helper function to unbind using a binding object
  unbind_axis(this_binding.hand, this_binding.axis, this_binding.direction);
}

void controller::unbind_button(hand_type hand, unsigned int button, actiontype action) {
  /// Unbind a callback on a controlle button with a specific action
  button_bindings[static_cast<unsigned int>(action)][static_cast<unsigned int>(hand)][button] = []{}; // noop
}
void controller::unbind_button_any(hand_type hand) {
  /// Helper function to unbind all buttons on a controlle, all actions
  for(unsigned int button = 0; button != max_button; ++button) {
    unbind_button(hand, button, actiontype::PRESS);
    unbind_button(hand, button, actiontype::RELEASE);
    unbind_button(hand, button, actiontype::TOUCH);
    unbind_button(hand, button, actiontype::UNTOUCH);
  }
}
void controller::unbind_button_any_all() {
  /// Helper function to unbind all buttons with all actions on all controlles
  unbind_button_any(hand_type::LEFT);
  unbind_button_any(hand_type::RIGHT);
}
void controller::unbind_button(binding_button const &this_binding) {
  /// Helper function to unbind using a binding object
  switch(this_binding.type) {
  case binding_button::bindtype::SPECIFIC:
    unbind_button(this_binding.hand, this_binding.button, actiontype::PRESS);
    unbind_button(this_binding.hand, this_binding.button, actiontype::RELEASE);
    unbind_button(this_binding.hand, this_binding.button, actiontype::TOUCH);
    unbind_button(this_binding.hand, this_binding.button, actiontype::UNTOUCH);
    break;
  case binding_button::bindtype::ANY:
    unbind_button_any(this_binding.hand);
    break;
  case binding_button::bindtype::ANY_ALL:
    unbind_button_any_all();
    break;
  }
}

void controller::execute_axis(hand_type hand,
                              unsigned int axis,
                              axis_direction_type axis_direction,
                              float value) const {
  /// Call the function associated with a controlle axis, having transformed the value appropriately
  auto const &this_binding = axis_binding_at(hand, axis, axis_direction);
  if(!this_binding.enabled) {
    return;                                                                     // early exit in case this binding isn't in use
  }
  this_binding.execute(value);
}
void controller::execute_axis(hand_type hand,
                              unsigned int axis,
                              vector2f const &values) const {
  /// Wrapper to call two directional functions simultaneously
  execute_axis(hand, axis, axis_direction_type::X, values.x);
  execute_axis(hand, axis, axis_direction_type::Y, values.y);
}
void controller::execute_button(hand_type hand, unsigned int button, actiontype action) {
  /// Call the function associated with a controlle button
  button_binding_at(hand, button, action)();
}

// TODO:
void controller::capture_axis(std::function<void(hand_type,
                                                 unsigned int,
                                                 axis_direction_type,
                                                 bool)> callback,
                              bool calibrate) {
  /// Capture an axis movement and return it to the given callback
  static bool calibrated = false;
  static std::array<std::array<std::array<float, max_axis_direction>, max_axis>, max> initial_values;
  if(calibrate || !calibrated) {
    // read the initial values of all axes, and store them for later comparison - some axes may not default to zero, so we need to catch the biggest change
    std::cout << "VRStorm: Calibrating controller for capture" << std::endl;
    for(unsigned int hand_id = 0; hand_id != max; ++hand_id) {
      if(!enabled[hand_id]) {
        continue;
      }
      vr::VRControllerState_t controller_state;
      parent.hmd_handle->GetControllerState(controller_ids[hand_id], &controller_state);
      for(unsigned int axis = 0; axis != max_axis; ++axis) {
        initial_values[hand_id][axis][static_cast<unsigned int>(axis_direction_type::X)] = controller_state.rAxis[axis].x;
        initial_values[hand_id][axis][static_cast<unsigned int>(axis_direction_type::Y)] = controller_state.rAxis[axis].y;
        #ifdef DEBUG_VRSTORM
          std::cout << "VRStorm: DEBUG: Calibrated controller hand " << hand_id
                    << " axis " << axis
                    << ": " << initial_values[hand_id][axis][static_cast<unsigned int>(axis_direction_type::X)]
                    << ", " << initial_values[hand_id][axis][static_cast<unsigned int>(axis_direction_type::Y)] << std::endl;
        #endif // DEBUG_VRSTORM
      }
    }
    calibrated = true;
    if(calibrate) {
      return;
    }
  }

  float constexpr const deadzone = 0.5f;
  for(unsigned int hand_id = 0; hand_id != max; ++hand_id) {
    auto const hand = static_cast<hand_type>(hand_id);
    if(!get_enabled(hand)) {
      continue;
    }
    for(unsigned int axis = 0; axis != max_axis; ++axis) {
      for(unsigned int axis_direction_id = 0; axis_direction_id != max_axis_direction; ++axis_direction_id) {
        #ifdef DEBUG_VRSTORM
          std::stringstream ss;
          ss << "VRStorm: DEBUG: controller hand " << hand_id << " axis " << axis << ": ";
        #endif // DEBUG_VRSTORM
        float const initial_value = initial_values[hand_id][axis][axis_direction_id];
        auto const axis_direction = static_cast<axis_direction_type>(axis_direction_id);
        bind_axis(hand,
                  axis,
                  axis_direction,
                  [callback,
                   #ifdef DEBUG_VRSTORM
                     s = ss.str(),
                   #endif // DEBUG_VRSTORM
                   hand,
                   axis,
                   axis_direction,
                   initial_value
                  ](float value){
          float const offset = value - initial_value;
          if(offset > deadzone) {
            #ifdef DEBUG_VRSTORM
              std::cout << s << std::fixed << value << "(offset: pos " << offset << ")" <<  std::endl;
            #endif // DEBUG_VRSTORM
            callback(hand, axis, axis_direction, false);
          } else if(offset < -deadzone) {
            #ifdef DEBUG_VRSTORM
              std::cout << s << std::fixed << value << "(offset: neg " << offset << ")" <<  std::endl;
            #endif // DEBUG_VRSTORM
            callback(hand, axis, axis_direction, true);
          }
        });
      }
    }
  }
}
void controller::capture_axis(std::function<void(binding_axis const&)> callback,
                              bool calibrate) {
  /// Capture an axis movement and return it to the given callback as a binding object
  static bool calibrated = false;
  static std::array<std::array<std::array<float, max_axis_direction>, max_axis>, max> initial_values;
  if(calibrate || !calibrated) {
    // read the initial values of all axes, and store them for later comparison - some axes may not default to zero, so we need to catch the biggest change
    std::cout << "VRStorm: Calibrating controller for capture" << std::endl;
    for(unsigned int hand_id = 0; hand_id != max; ++hand_id) {
      if(!enabled[hand_id]) {
        continue;
      }
      vr::VRControllerState_t controller_state;
      parent.hmd_handle->GetControllerState(controller_ids[hand_id], &controller_state);
      for(unsigned int axis = 0; axis != max_axis; ++axis) {
        initial_values[hand_id][axis][static_cast<unsigned int>(axis_direction_type::X)] = controller_state.rAxis[axis].x;
        initial_values[hand_id][axis][static_cast<unsigned int>(axis_direction_type::Y)] = controller_state.rAxis[axis].y;
        #ifdef DEBUG_VRSTORM
          std::cout << "VRStorm: DEBUG: Calibrated controller hand " << hand_id
                    << " axis " << axis
                    << ": " << initial_values[hand_id][axis][static_cast<unsigned int>(axis_direction_type::X)]
                    << ", " << initial_values[hand_id][axis][static_cast<unsigned int>(axis_direction_type::Y)] << std::endl;
        #endif // DEBUG_VRSTORM
      }
    }
    calibrated = true;
    if(calibrate) {
      return;
    }
  }

  float constexpr const deadzone = 0.5f;
  for(unsigned int hand_id = 0; hand_id != max; ++hand_id) {
    auto const hand = static_cast<hand_type>(hand_id);
    if(!get_enabled(hand)) {
      continue;
    }
    for(unsigned int axis = 0; axis != max_axis; ++axis) {
      for(unsigned int axis_direction_id = 0; axis_direction_id != max_axis_direction; ++axis_direction_id) {
        #ifdef DEBUG_VRSTORM
          std::stringstream ss;
          ss << "VRStorm: DEBUG: controller hand " << hand_id << " axis " << axis << ": ";
        #endif // DEBUG_VRSTORM
        float const initial_value = initial_values[hand_id][axis][axis_direction_id];
        auto const axis_direction = static_cast<axis_direction_type>(axis_direction_id);
        bind_axis(hand,
                  axis,
                  axis_direction,
                  [callback,
                   #ifdef DEBUG_VRSTORM
                     s = ss.str(),
                   #endif // DEBUG_VRSTORM
                   hand,
                   axis,
                   axis_direction,
                   initial_value
                  ](float value){
          float const offset = value - initial_value;
          if(offset > deadzone) {
            #ifdef DEBUG_VRSTORM
              std::cout << s << std::fixed << value << "(offset: pos " << offset << ")" <<  std::endl;
            #endif // DEBUG_VRSTORM
            callback(binding_axis{hand, axis, axis_direction, false, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f});
          } else if(offset < -deadzone) {
            #ifdef DEBUG_VRSTORM
              std::cout << s << std::fixed << value << "(offset: neg " << offset << ")" <<  std::endl;
            #endif // DEBUG_VRSTORM
            callback(binding_axis{hand, axis, axis_direction, true, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f});
          }
        });
      }
    }
  }
}
void controller::capture_button(std::function<void(hand_type, unsigned int)> callback) {
  /// Capture a button press and return it to the given callback
  for(unsigned int hand_id = 0; hand_id != max; ++hand_id) {
    auto const hand = static_cast<hand_type>(hand_id);
    for(unsigned int button = 0; button != max_button; ++button) {
      bind_button(hand, button, actiontype::RELEASE, []{});                     // unbind release actions
      bind_button(hand, button, actiontype::PRESS, [callback,
                                                    hand,
                                                    button]{                    // bind on the press action
        callback(hand, button);
      });
    }
  }
}
void controller::capture_button(std::function<void(binding_button const&)> callback) {
  /// Capture a button press and return it to the given callback as a binding object
  for(unsigned int hand_id = 0; hand_id != max; ++hand_id) {
    auto const hand = static_cast<hand_type>(hand_id);
    for(unsigned int button = 0; button != max_button; ++button) {
      bind_button(hand, button, actiontype::RELEASE, []{});                     // unbind release actions
      bind_button(hand, button, actiontype::PRESS, [callback,
                                                           hand,
                                                           button]{             // bind on the press action
        callback(binding_button{hand, binding_button::bindtype::SPECIFIC, button});
      });
    }
  }
}

void controller::update_hands() {
  /// Update the controller ids for the left and right hand
  bool found_left  = false;
  bool found_right = false;
  unsigned int id = 0;                                                          // reused for future loops
  for(; id != vr::k_unMaxTrackedDeviceCount; ++id) {
    if(parent.hmd_handle->GetTrackedDeviceClass(id) != vr::TrackedDeviceClass_Controller) {
      continue;                                                                 // skip non-controllers
    }
    switch(parent.hmd_handle->GetControllerRoleForTrackedDeviceIndex(id)) {
    case vr::TrackedControllerRole_LeftHand:
      #ifdef DEBUG_VRSTORM
        std::cout << "VRStorm: DEBUG: controller id " << id << " is now the left hand (first)." << std::endl;
      #endif // DEBUG_VRSTORM
      controller_ids[static_cast<unsigned int>(hand_type::LEFT)] = id;
      enabled[       static_cast<unsigned int>(hand_type::LEFT)] = true;
      found_left = true;
      break;
    case vr::TrackedControllerRole_RightHand:
      #ifdef DEBUG_VRSTORM
        std::cout << "VRStorm: DEBUG: controller id " << id << " is now the right hand (first)." << std::endl;
      #endif // DEBUG_VRSTORM
      controller_ids[static_cast<unsigned int>(hand_type::RIGHT)] = id;
      enabled[       static_cast<unsigned int>(hand_type::RIGHT)] = true;
      found_right = true;
      break;
    default:
      break;
    }
  }
  // this is an optimisation to reduce the number of comparisons and avoid searching further than we have to
  if(found_left) {
    for(; id != vr::k_unMaxTrackedDeviceCount; ++id) {                          // resume the loop
      if(parent.hmd_handle->GetTrackedDeviceClass(id) != vr::TrackedDeviceClass_Controller) {
        continue;                                                               // skip non-controllers
      }
      if(parent.hmd_handle->GetControllerRoleForTrackedDeviceIndex(id) == vr::TrackedControllerRole_RightHand) {
        #ifdef DEBUG_VRSTORM
          std::cout << "VRStorm: DEBUG: controller id " << id << " is now the right hand (second)." << std::endl;
        #endif // DEBUG_VRSTORM
        controller_ids[static_cast<unsigned int>(hand_type::RIGHT)] = id;
        enabled[       static_cast<unsigned int>(hand_type::RIGHT)] = true;
        return;
      }
    }
    #ifdef DEBUG_VRSTORM
      std::cout << "VRStorm: DEBUG: could not find a controller for the right hand." << std::endl;
    #endif // DEBUG_VRSTORM
    controller_ids[static_cast<unsigned int>(hand_type::RIGHT)] = 0;
    enabled[       static_cast<unsigned int>(hand_type::RIGHT)] = false;
  } else if(found_right) {
    for(; id != vr::k_unMaxTrackedDeviceCount; ++id) {                          // resume the loop
      if(parent.hmd_handle->GetTrackedDeviceClass(id) != vr::TrackedDeviceClass_Controller) {
        continue;                                                               // skip non-controllers
      }
      if(parent.hmd_handle->GetControllerRoleForTrackedDeviceIndex(id) == vr::TrackedControllerRole_LeftHand) {
        #ifdef DEBUG_VRSTORM
          std::cout << "VRStorm: DEBUG: controller id " << id << " is now the left hand (second)." << std::endl;
        #endif // DEBUG_VRSTORM
        controller_ids[static_cast<unsigned int>(hand_type::LEFT)] = id;
        enabled[       static_cast<unsigned int>(hand_type::LEFT)] = true;
        return;
      }
    }
    #ifdef DEBUG_VRSTORM
      std::cout << "VRStorm: DEBUG: could not find a controller for the left hand." << std::endl;
    #endif // DEBUG_VRSTORM
    controller_ids[static_cast<unsigned int>(hand_type::LEFT)] = 0;
    enabled[       static_cast<unsigned int>(hand_type::LEFT)] = false;
  } else {
    #ifdef DEBUG_VRSTORM
      std::cout << "VRStorm: DEBUG: could not find any controllers for either hand." << std::endl;
    #endif // DEBUG_VRSTORM
    controller_ids[static_cast<unsigned int>(hand_type::LEFT )] = 0;
    controller_ids[static_cast<unsigned int>(hand_type::RIGHT)] = 0;
    enabled[       static_cast<unsigned int>(hand_type::LEFT )] = false;
    enabled[       static_cast<unsigned int>(hand_type::RIGHT)] = false;
    return;
  }
}

void controller::update_names() {
  /// Update the list of controller names
  names[static_cast<unsigned int>(hand_type::LEFT )] = parent.get_tracked_device_string(get_id(hand_type::LEFT),  vr::Prop_ModelNumber_String);
  names[static_cast<unsigned int>(hand_type::RIGHT)] = parent.get_tracked_device_string(get_id(hand_type::RIGHT), vr::Prop_ModelNumber_String);
  #ifdef DEBUG_VRSTORM
    std::cout << "VRStorm: DEBUG: controller: left is  \"" << get_name(hand_type::LEFT) << "\"" << std::endl;
    std::cout << "VRStorm: DEBUG: controller: right is \"" << get_name(hand_type::RIGHT) << "\"" << std::endl;
  #endif // DEBUG_VRSTORM
}

void controller::poll() {
  /// Poll and update the analogue controller axes for the known hands
  for(hand_type hand = hand_type::LEFT; hand != hand_type::RIGHT; hand = hand_type::RIGHT) { // two-step loop
    if(get_enabled(hand)) {
      vr::VRControllerState_t controller_state;
      unsigned int controller_id = get_id(hand);
      parent.hmd_handle->GetControllerState(controller_id, &controller_state);
      #ifdef DEBUG_VRSTORM
        std::cout << "VRStorm: DEBUG: controller id " << controller_id << " unPacketNum " << controller_state.unPacketNum << " ulButtonPressed " << controller_state.ulButtonPressed << " ulButtonTouched " << controller_state.ulButtonTouched << std::endl;
      #endif // DEBUG_VRSTORM
      for(unsigned int axis = 0; axis != vr::k_unControllerStateAxisCount; ++axis) {
        #ifdef DEBUG_VRSTORM
          std::cout << "VRStorm: DEBUG: axis " << axis << " value " << controller_state.rAxis[axis].x << ", " << controller_state.rAxis[axis].y << std::endl;
        #endif // DEBUG_VRSTORM
        execute_axis(hand, axis, {controller_state.rAxis[axis].x, controller_state.rAxis[axis].y});
      }
    }
  }
}
void controller::poll(unsigned int controller_id) {
  /// Poll and update the analogue controller axes for a given controller id
  vr::VRControllerState_t controller_state;
  parent.hmd_handle->GetControllerState(controller_id, &controller_state);
  switch(parent.hmd_handle->GetControllerRoleForTrackedDeviceIndex(controller_id)) {
  case vr::TrackedControllerRole_LeftHand:
    if(get_enabled(hand_type::LEFT)) {
      #ifdef DEBUG_VRSTORM
        std::cout << "VRStorm: DEBUG: controller id " << controller_id << " unPacketNum " << controller_state.unPacketNum << " ulButtonPressed " << controller_state.ulButtonPressed << " ulButtonTouched " << controller_state.ulButtonTouched << std::endl;
      #endif // DEBUG_VRSTORM
      controller_ids[static_cast<unsigned int>(input::controller::hand_type::LEFT)] = controller_id; // opportunity to update the controller ids here for free
      for(unsigned int axis = 0; axis != vr::k_unControllerStateAxisCount; ++axis) {
        #ifdef DEBUG_VRSTORM
          std::cout << "VRStorm: DEBUG: axis " << axis << " value " << controller_state.rAxis[axis].x << ", " << controller_state.rAxis[axis].y << std::endl;
        #endif // DEBUG_VRSTORM
        execute_axis(input::controller::hand_type::LEFT, axis, {controller_state.rAxis[axis].x, controller_state.rAxis[axis].y});
      }
    }
    break;
  case vr::TrackedControllerRole_RightHand:
    if(get_enabled(hand_type::LEFT)) {
      #ifdef DEBUG_VRSTORM
        std::cout << "VRStorm: DEBUG: controller id " << controller_id << " unPacketNum " << controller_state.unPacketNum << " ulButtonPressed " << controller_state.ulButtonPressed << " ulButtonTouched " << controller_state.ulButtonTouched << std::endl;
      #endif // DEBUG_VRSTORM
      controller_ids[static_cast<unsigned int>(input::controller::hand_type::RIGHT)] = controller_id; // opportunity to update the controller ids here for free
      for(unsigned int axis = 0; axis != vr::k_unControllerStateAxisCount; ++axis) {
        #ifdef DEBUG_VRSTORM
          std::cout << "VRStorm: DEBUG: axis " << axis << " value " << controller_state.rAxis[axis].x << ", " << controller_state.rAxis[axis].y << std::endl;
        #endif // DEBUG_VRSTORM
        execute_axis(input::controller::hand_type::RIGHT, axis, {controller_state.rAxis[axis].x, controller_state.rAxis[axis].y});
      }
    }
    break;
  default:
    std::cout << "VRStorm: WARNING: axis failed to poll on unknown hand " << static_cast<int>(parent.hmd_handle->GetControllerRoleForTrackedDeviceIndex(controller_id)) << " for controller id " << controller_id << std::endl;
    break;
  }
}

void controller::draw_binding_graphs() const {
  for(unsigned int hand_id = 0; hand_id != max; ++hand_id) {
    for(unsigned int axis = 0; axis != max_axis; ++axis) {
      for(unsigned int direction_id = 0; direction_id != max_axis_direction; ++direction_id) {
        auto const &this_binding(axis_binding_at(static_cast<hand_type>(hand_id), axis, static_cast<axis_direction_type>(direction_id)));
        if(!this_binding.enabled) {
          continue;
        }
        std::cout << "VRStorm: Transform function on controller hand " << hand_id << " axis " << axis << " direction " << direction_id << ":" << std::endl;
        this_binding.draw_graph_console();
      }
    }
  }
}

}
}
