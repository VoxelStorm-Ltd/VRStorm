#ifndef VRSTORM_BINDING_SETS_CONTROLLER_BUTTON_H_INCLUDED
#define VRSTORM_BINDING_SETS_CONTROLLER_BUTTON_H_INCLUDED

#include <boost/bimap.hpp>
#include <boost/bimap/unordered_multiset_of.hpp>
#include "inputstorm/binding_sets/base.h"
#include "vrstorm/input/controller.h"

namespace vrstorm::binding_sets {

#define BINDING_SET_TYPE boost::bimap<boost::bimaps::unordered_multiset_of<T>, boost::bimaps::unordered_multiset_of<input::controller::binding_button>>
#define BASE_TYPE inputstorm::binding_sets::base_crtp_adapter<T, BINDING_SET_TYPE, controller_button>

template<typename T>
class controller_button : public BASE_TYPE {
  using controltype = T;

  vrstorm::input::controller &parent_controller;

public:
  controller_button(inputstorm::binding_manager<controltype> &parent_binding_manager,
                    vrstorm::input::controller &this_parent_controller);
  ~controller_button();

  // bind and unbind controls to inputs
  virtual void unbind(std::string const &binding_name, controltype control) override final;
  using BASE_TYPE::unbind;                                                      // required to allow visibility of hidden overridden base type

  void bind(std::string const &binding_name,
            controltype control,
            input::controller::hand_type hand,
            unsigned int button);
  void bind(controltype control,
            input::controller::hand_type hand,
            unsigned int button);

  // update control-based bindings
  void update(std::string const &binding_name,
              input::controller::binding_button const &binding);
  void update(input::controller::binding_button const &binding);

  virtual void update_all(controltype control) override final;
};

template<typename T>
controller_button<T>::controller_button(inputstorm::binding_manager<controltype> &parent_binding_manager,
                                        vrstorm::input::controller &this_parent_controller)
  : BASE_TYPE(parent_binding_manager),
    parent_controller(this_parent_controller) {
  /// Default constructor
}

template<typename T>
controller_button<T>::~controller_button() {
  /// Default destructor
}

//////////////////// bind and unbind controls to inputs ////////////////////////

template<typename T>
void controller_button<T>::unbind(std::string const &binding_name,
                                  controltype control) {
  /// Erase a control binding from an input joystick button relationship
  #if defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
    std::cout << "VRStorm: DEBUG: Unbinding controller button for control " << static_cast<unsigned int>(control) << " on set " << binding_name << std::endl;
  #endif // defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
  auto &binding_set(this->binding_sets[binding_name]);
  auto const &binding_range(binding_set.left.equal_range(control));
  std::unordered_set<input::controller::binding_button> bindings_to_update;
  for(auto const &it : boost::make_iterator_range(binding_range.first, binding_range.second)) {
    bindings_to_update.emplace(it.second);                                      // queue each button that was affected by the change to update after
  }

  binding_set.left.erase(control);                                              // clear the current associations with that control

  for(auto const &it : bindings_to_update) {
    update(it);                                                                 // update each button that was affected by the change
  }
}

template<typename T>
void controller_button<T>::bind(std::string const &binding_name,
                                controltype control,
                                input::controller::hand_type hand,
                                unsigned int button) {
  /// Apply a new control binding to an input key relationship
  #if defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
    std::cout << "VRStorm: DEBUG: Binding control " << static_cast<int>(control)
              << " in set " << this->binding_selected_name
              << ", controller " << parent_controller.get_name(hand)
              << " button " << button << std::endl;
  #endif // defined(DEBUG_VRSTORM) || defined(DEBUG_INPUTSTORM)
  auto &binding_set(this->binding_sets[binding_name]);
  binding_set.insert(typename BASE_TYPE::binding_set_value_type(control, input::controller::binding_button{
    hand,
    input::controller::binding_button::bindtype::SPECIFIC,
    button
  }));
}
template<typename T>
void controller_button<T>::bind(controltype control,
                                input::controller::hand_type hand,
                                unsigned int button) {
  /// Wrapper to work on the selected control set
  bind(this->binding_selected_name, control, hand, button);
}

///////////////////// update control-based bindings ////////////////////////////

template<typename T>
void controller_button<T>::update(std::string const &binding_name,
                                  input::controller::binding_button const &binding) {
  /// Update all digital bindings for a specific controller button
  auto const &binding_set(this->binding_sets.at(binding_name));
  auto const &control_range(binding_set.right.equal_range(binding));            // find all controls (and hence functions) that apply to this key
  std::vector<controltype> conts;                                               // generate a vector of controls
  for(auto const &this_control : boost::make_iterator_range(control_range.first, control_range.second)) {
    conts.emplace_back(this_control.second);                                    // store a list of all controls that use this key
  }
  std::sort(conts.begin(), conts.end());                                        // sort and unique the controls
  auto const &conts_new_end(std::unique(conts.begin(), conts.end()));           // this should be faster to do once than to use a set to insert
  conts.resize(std::distance(conts.begin(), conts_new_end));
  std::vector<std::function<void()>> funcs_press;                               // generate a vector of functions for when it's pressed
  std::vector<std::function<void()>> funcs_release;                             // generate a vector of functions for when it's released
  for(auto const &this_control : conts) {
    auto const &this_func(this->bindings.action_bindings_digital[static_cast<unsigned int>(this_control)]);
    if(this_func.press) {
      funcs_press.emplace_back(this_func.press);
    }
    if(this_func.release) {
      funcs_release.emplace_back(this_func.release);
    }
  }
  std::function<void()> func_press_combined;
  if(funcs_press.empty()) {                                                     // don't bind if there's nothing to bind
    func_press_combined = nullptr;
  } else {
    if(funcs_press.size() == 1) {                                               // there's only one function, so bind it directly
      func_press_combined = funcs_press[0];
    } else {
      func_press_combined = [funcs_press]{                                      // there are multiple functions, so create a function to iterate and call them all
        for(auto const &this_func : funcs_press) {
          this_func();
        }
      };
    }
  }
  std::function<void()> func_release_combined;
  if(funcs_release.empty()) {                                                   // don't bind if there's nothing to bind
    func_release_combined = nullptr;
  } else {
    if(funcs_release.size() == 1) {                                             // there's only one function, so bind it directly
      func_release_combined = funcs_release[0];
    } else {
      func_release_combined = [funcs_release]{                                  // there are multiple functions, so create a function to iterate and call them all
        for(auto const &this_func : funcs_release) {
          this_func();
        }
      };
    }
  }
  if(func_press_combined || func_release_combined) {
    parent_controller.bind_button(binding, func_press_combined, func_release_combined);
  } else {
    parent_controller.unbind_button(binding);
  }
}
template<typename T>
void controller_button<T>::update(input::controller::binding_button const &binding) {
  /// Update all digital bindings for a specific controller button in the default control set
  update(this->binding_selected_name, binding);
}

template<typename T>
void controller_button<T>::update_all(controltype control) {
  /// Update all button bindings for this control
  auto const &binding_set(this->get_selected_binding_set());
  auto const &binding_range(binding_set.left.equal_range(control));
  for(auto const &it : boost::make_iterator_range(binding_range.first, binding_range.second)) {
    update(it.second);
  }
}

#undef BINDING_SET_TYPE
#undef BASE_TYPE

}

#endif // VRSTORM_BINDING_SETS_CONTROLLER_BUTTON_H_INCLUDED
