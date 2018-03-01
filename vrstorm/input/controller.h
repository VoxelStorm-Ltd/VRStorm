#ifndef VRSTORM_INPUT_CONTROLLER_H_INCLUDED
#define VRSTORM_INPUT_CONTROLLER_H_INCLUDED

#include <functional>
#ifdef __MINGW32__
  #include <openvr_mingw.hpp>
#else
  #include <openvr.h>
#endif // __MINGW32__
#include "vectorstorm/vector/vector2_forward.h"
#include "inputstorm/input/joystick_axis_bindingtype.h"

namespace vrstorm {
class manager;

namespace input {

class controller {
  manager &parent;                                                              // the parent manager

public:
  enum class axis_direction_type : bool {
    X,
    Y
  };
  enum class actiontype : int {
    RELEASE,
    PRESS,
    TOUCH,
    UNTOUCH,
    // array bounds
    BEGIN = RELEASE,
    LAST = UNTOUCH,
    END = LAST + 1
  };
  enum class hand_type {
    UNKNOWN,
    LEFT,
    RIGHT
  };

  struct binding_axis {
    /// Convenience struct for storing and passing all parameters that make up an axis binding
    hand_type hand;
    unsigned int axis;
    axis_direction_type direction;
    bool flip;
    float deadzone_min;
    float deadzone_max;
    float saturation_min;
    float saturation_max;
    float centre;
  };
  struct binding_button {
    /// Convenience struct for storing and passing all parameters that make up a button binding
    hand_type hand;
    enum class bindtype : char {
      SPECIFIC,                                                                 // button binding with an action (press or release)
      ANY,                                                                      // button binding with any action (press or release)
      ANY_ALL                                                                   // button binding on any joystick with any action
    } type;
    unsigned int button;                                                        // unused if bindtype is ANY or ANY_ALL

    bool operator==(const binding_button &rhs) const {
      /// Equality operator
      // two formulations with the same effect - benchmark to see which performs best in each specific use case
      #ifdef INPUTSTORM_EQUALITY_COMPARISON_SWITCH
        switch(type) {
        case bindtype::SPECIFIC:
          switch(rhs.type) {
          case bindtype::SPECIFIC:
            return (hand == rhs.hand) && (button == rhs.button);
          case bindtype::ANY:
            return hand == rhs.hand;
          case bindtype::ANY_ALL:
            return true;
          }
          break;
        case bindtype::ANY:
          switch(rhs.type) {
          case bindtype::SPECIFIC:
          case bindtype::ANY:
            return hand == rhs.hand;
          case bindtype::ANY_ALL:
            return true;
          }
          break;
        case bindtype::ANY_ALL:
          return true;
        default:
          return false;
        }
      #else
        if(type == bindtype::ANY_ALL || rhs.type == bindtype::ANY_ALL) {
          return true;
        } else if(type == bindtype::ANY || rhs.type == bindtype::ANY) {
          return hand == rhs.hand;
        } else {
          return (hand == rhs.hand) && (button == rhs.button);
        }
      #endif // INPUTSTORM_EQUALITY_COMPARISON_SWITCH
    }

    size_t hash_value() const {
      /// Hash function to return a unique hash for each binding
      return max_button * static_cast<unsigned int>(hand) +
             button;
    }
  };

  // limits
  static unsigned int constexpr const max = static_cast<unsigned int>(hand_type::RIGHT) + 1;
  static unsigned int constexpr const max_axis = vr::k_unControllerStateAxisCount;
  static unsigned int constexpr const max_axis_direction = static_cast<unsigned int>(axis_direction_type::Y) + 1;
  static unsigned int constexpr const max_button = vr::k_EButton_Max;

private:
  // data
  std::array<bool, max> enabled;                                                // whether each controller is enabled or not
  std::array<std::string, max> names;                                           // cached human-readable names of controllers
  std::array<unsigned int, max> controller_ids;                                 // cached openvr controller id for each hand
  std::array<std::array<std::array<inputstorm::input::joystick_axis_bindingtype, max_axis_direction>, max_axis>, max> axis_bindings; // callback functions for controller axes
  std::array<std::array<std::array<std::function<void()>, max_button>, max>, static_cast<int>(actiontype::END)> button_bindings; // callback functions for controller buttons

public:
  controller(manager &this_parent);
  ~controller();

  void init();

private:
  inputstorm::input::joystick_axis_bindingtype const &axis_binding_at(hand_type hand,
                                                                      unsigned int axis,
                                                                      axis_direction_type axis_direction) const __attribute__((__const__));
  std::function<void()> const &button_binding_at(hand_type hand,
                                                 unsigned int button,
                                                 actiontype action = actiontype::PRESS) const __attribute__((__const__));

public:
  bool get_enabled(hand_type hand) const __attribute__((__pure__));
  std::string get_name(hand_type hand) const;
  std::string get_name_button(unsigned int button) const;
  std::string get_name_axis(hand_type hand, unsigned int axis) const;
  unsigned int get_id(hand_type hand) const __attribute__((__pure__));
  static std::string get_handtype_name(hand_type hand);
  static std::string get_actiontype_name(actiontype action);

  void bind_axis(          hand_type hand,
                           unsigned int axis,
                           axis_direction_type axis_direction,
                           std::function<void(float)> func,
                           bool flip = false,
                           float deadzone_min = 0.0f,
                           float deadzone_max = 0.0f,
                           float saturation_min = -1.0f,
                           float saturation_max = 1.0f,
                           float centre = 0.0f);
  void bind_axis_half(     hand_type hand,
                           unsigned int axis,
                           axis_direction_type axis_direction,
                           std::function<void(float)> func,
                           bool flip = false);
  void bind_axis(          binding_axis const &this_binding,
                           std::function<void(float)> func);
  void bind_button(        hand_type hand,
                           unsigned int button,
                           actiontype action,
                           std::function<void()> func);
  void bind_button_any(    hand_type hand,
                           std::function<void()> func);
  void bind_button_any_all(std::function<void()> func);
  void bind_button(        binding_button const &this_binding,
                           std::function<void()> func_press,
                           std::function<void()> func_release = nullptr,
                           std::function<void()> func_touch   = nullptr,
                           std::function<void()> func_untouch = nullptr);

  void unbind_axis(hand_type hand,
                   unsigned int axis,
                   axis_direction_type axis_direction);
  void unbind_axis_any(hand_type hand);
  void unbind_axis_any_all();
  void unbind_axis(binding_axis const &this_binding);
  void unbind_button(hand_type hand,
                     unsigned int button,
                     actiontype action);
  void unbind_button_any(hand_type hand);
  void unbind_button_any_all();
  void unbind_button(binding_button const &this_binding);

  void execute_axis(  hand_type hand,
                      unsigned int axis,
                      axis_direction_type axis_direction,
                      float value = 0.0f) const;
  void execute_axis(  hand_type hand,
                      unsigned int axis,
                      vec2f const &values) const;
  void execute_button(hand_type hand,
                      unsigned int button,
                      actiontype action = actiontype::PRESS);

  void capture_axis(  std::function<void(hand_type, unsigned int, axis_direction_type, bool)> callback,
                      bool calibrate = false);
  void capture_axis(  std::function<void(binding_axis const&    )> callback,
                      bool calibrate = false);
  void capture_button(std::function<void(hand_type, unsigned int)> callback);
  void capture_button(std::function<void(binding_button const&  )> callback);

  void update_hands();
  void update_names();

  void poll();
  void poll(unsigned int controller_id);

  void draw_binding_graphs() const;
};

/// Helper functions to allow controller::actiontype to be iterated
inline controller::actiontype operator++(controller::actiontype &i) {
  return i = static_cast<controller::actiontype>(std::underlying_type<controller::actiontype>::type(i) + 1);
}
inline controller::actiontype operator*(controller::actiontype c) {
  return c;
}
inline controller::actiontype begin(controller::actiontype thistype [[maybe_unused]]) {
  return controller::actiontype::BEGIN;
}
inline controller::actiontype end(controller::actiontype thistype [[maybe_unused]]) {
  return controller::actiontype::END;
}

inline size_t hash_value(controller::binding_button const &this_binding) {
  /// Forward to the binding's hash function
  return this_binding.hash_value();
}

}
}

namespace std {

template<>
struct hash<vrstorm::input::controller::binding_button> {
  /// Forward the binding's hash function for std::hash specialisation
  size_t operator()(vrstorm::input::controller::binding_button const &this_binding) const {
    return this_binding.hash_value();
  }
};

}

#endif // VRSTORM_INPUT_CONTROLLER_H_INCLUDED
