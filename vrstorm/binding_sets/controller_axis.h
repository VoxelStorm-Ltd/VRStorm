#ifndef VRSTORM_BINDING_SETS_CONTROLLER_AXIS_H_INCLUDED
#define VRSTORM_BINDING_SETS_CONTROLLER_AXIS_H_INCLUDED

#include <unordered_map>
#include <boost/range/iterator_range.hpp>
#include "inputstorm/binding_sets/base.h"
#include "vrstorm/input/controller.h"

namespace vrstorm::binding_sets {

#define BINDING_SET_TYPE std::unordered_multimap<T, input::controller::binding_axis>
#define BASE_TYPE inputstorm::binding_sets::base_crtp_adapter<T, BINDING_SET_TYPE, controller_axis>

template<typename T>
class controller_axis : public BASE_TYPE {
  using controltype = T;

  vrstorm::input::controller &parent_controller;

public:
  controller_axis(inputstorm::binding_manager<controltype> &parent_binding_manager,
                  vrstorm::input::controller &this_parent_controller);
  virtual ~controller_axis();

  // bind and unbind controls to inputs
  virtual void unbind(std::string const &binding_name, controltype control) override final;
  using BASE_TYPE::unbind;                                                      // required to allow visibility of hidden overridden base type

  void bind(std::string const &binding_name,
            controltype control,
            input::controller::hand_type hand,
            unsigned int axis,
            input::controller::axis_direction_type direction,
            bool flip = false,
            float deadzone_min = 0.0f,
            float deadzone_max = 0.0f,
            float saturation_min = -1.0f,
            float saturation_max = 1.0f,
            float centre = 0.0f);
  void bind(controltype control,
            input::controller::hand_type hand,
            unsigned int axis,
            input::controller::axis_direction_type direction,
            bool flip = false,
            float deadzone_min = 0.0f,
            float deadzone_max = 0.0f,
            float saturation_min = -1.0f,
            float saturation_max = 1.0f,
            float centre = 0.0f);

  // update control-based bindings
  virtual void update_all(controltype control) override final;
};

template<typename T>
controller_axis<T>::controller_axis(inputstorm::binding_manager<controltype> &parent_binding_manager,
                                    vrstorm::input::controller &this_parent_controller)
  : BASE_TYPE(parent_binding_manager),
    parent_controller(this_parent_controller) {
  /// Default constructor
}

template<typename T>
controller_axis<T>::~controller_axis() {
  /// Default destructor
}

//////////////////// bind and unbind controls to inputs ////////////////////////

template<typename T>
void controller_axis<T>::unbind(std::string const &binding_name, controltype control) {
  /// Erase a control binding from an input controller axis relationship
  #if defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
    std::cout << "VRStorm: DEBUG: Unbinding controller axis for control " << static_cast<unsigned int>(control) << " on set " << binding_name << std::endl;
  #endif // defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
  auto &binding_set(this->binding_sets[binding_name]);
  binding_set.erase(control);                                                   // clear the current associations with that control
  update_all(control);                                                          // update all controller axes, because they're not separable into components
}

template<typename T>
void controller_axis<T>::bind(std::string const &binding_name,
                              controltype control,
                              input::controller::hand_type hand,
                              unsigned int axis,
                              input::controller::axis_direction_type direction,
                              bool flip,
                              float deadzone_min,
                              float deadzone_max,
                              float saturation_min,
                              float saturation_max,
                              float centre) {
  /// Apply a new control binding to an input key relationship
  #if defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
    std::cout << "VRStorm: DEBUG: Binding control " << static_cast<unsigned int>(control)
              << " in set " << binding_name
              << ", controller " << parent_controller.get_name(hand)
              << " axis " << axis
              << " direction " << static_cast<unsigned int>(direction);
    if(flip) {
      std::cout << " (inverted)";
    }
    std::cout << std::endl;
  #endif // defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
  auto &binding_set(this->binding_sets[binding_name]);
  binding_set.emplace(control, input::controller::binding_axis{
    hand,
    axis,
    direction,
    flip,
    deadzone_min,
    deadzone_max,
    saturation_min,
    saturation_max,
    centre
  });
  update_all(control);                                                          // update all controller axes, because they're not separable into components
}
template<typename T>
void controller_axis<T>::bind(controltype control,
                              input::controller::hand_type hand,
                              unsigned int axis,
                              input::controller::axis_direction_type direction,
                              bool flip,
                              float deadzone_min,
                              float deadzone_max,
                              float saturation_min,
                              float saturation_max,
                              float centre) {
  /// Wrapper to work on the selected control set
  #if defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
    std::cout << "VRStorm: DEBUG: Binding control " << static_cast<unsigned int>(control)
              << " in set " << this->binding_selected_name
              << ", controller hand " << static_cast<unsigned int>(hand)
              << " axis " << axis
              << " direction " << static_cast<unsigned int>(direction);
    if(flip) {
      std::cout << " (inverted)";
    }
    std::cout << std::endl;
  #endif // defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
  bind(this->binding_selected_name, control, hand, axis, direction, flip, deadzone_min, deadzone_max, saturation_min, saturation_max, centre);
}

///////////////////// update control-based bindings ////////////////////////////

template<typename T>
void controller_axis<T>::update_all(controltype control) {
  /// Update controller axis bindings for this control
  #if defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
    std::cout << "VRStorm: DEBUG: Updating all controller axis bindings for control " << static_cast<unsigned int>(control) << std::endl;
  #endif // defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
  auto const &binding_set(this->get_selected_binding_set());
  auto const &binding_range(binding_set.equal_range(control));
  for(auto const &it : boost::make_iterator_range(binding_range.first, binding_range.second)) {
    auto const &func(this->bindings.action_bindings_analogue[static_cast<unsigned int>(control)]);
    input::controller::binding_axis const &this_binding = it.second;
    if(func) {
      #if defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
        std::cout << "VRStorm: DEBUG: Updating binding in set " << this->binding_selected_name
                                                                << " for controller " << parent_controller.get_name(this_binding.hand)
                                                                << " axis " << this_binding.axis
                                                                << " direction " << static_cast<unsigned int>(this_binding.direction) << std::endl;
      #endif // defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
      parent_controller.bind_axis(this_binding, func);
    } else {
      parent_controller.unbind_axis(this_binding.hand, this_binding.axis, this_binding.direction);
    }
  }
}

#undef BINDING_SET_TYPE
#undef BASE_TYPE

}

#endif // VRSTORM_BINDING_SETS_CONTROLLER_AXIS_H_INCLUDED
