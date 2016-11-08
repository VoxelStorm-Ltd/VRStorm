#ifndef VRSTORM_MANAGER_H_INCLUDED
#define VRSTORM_MANAGER_H_INCLUDED

#include <string>
#include <vector>
#include <unordered_map>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#ifdef __MINGW32__
  #include <openvr_mingw.hpp>
#else
  #include <openvr.h>
#endif // __MINGW32__
#include "platform_defines.h"
#include "vectorstorm/vector/vector2.h"
#include "vectorstorm/matrix/matrix4.h"
#include "controller.h"

#ifdef VRSTORM_DISABLED
  #define VRSTORM_CONST_IF_DISABLED __attribute__((__const__));
#else
  #define VRSTORM_CONST_IF_DISABLED
#endif // VRSTORM_DISABLED

namespace vrstorm {

class manager {
  friend class input::controller;

  void *lib = nullptr;

  #ifndef VRSTORM_DISABLED
    std::unordered_map<std::string, vr::RenderModel_t*> models;

    vr::IVRSystem     *hmd_handle = nullptr;
    vr::IVRCompositor *compositor = nullptr;

    std::array<matrix4f, 2> eye_to_head_transform;

    float ipd = 0.0f;
  #endif // VRSTORM_DISABLED

  vector2<GLsizei> render_target_size;

public:
  #ifndef VRSTORM_DISABLED
    std::vector<controller> controllers;
    input::controller input_controller;
  #endif // VRSTORM_DISABLED
  matrix4f hmd_position;

  float nearplane = 0.2f;                                                       // camera near and far planes
  float farplane = 1000.0f;

  float head_height = 1.5f;
  bool enabled = false;

  manager();
  ~manager();

  void init();
  void shutdown();

  void update() VRSTORM_CONST_IF_DISABLED;

  vector2<GLsizei> const &get_render_target_size() const __attribute__((__const__));

  #ifndef VRSTORM_DISABLED
    std::string get_tracked_device_string(vr::TrackedDeviceIndex_t device_index,
                                          vr::TrackedDeviceProperty prop,
                                          vr::TrackedPropertyError *vr_error = nullptr) const;
    void setup_render_perspective_one_eye(vr::EVREye eye);
  #endif // VRSTORM_DISABLED

  void setup_render_perspective_left()  VRSTORM_CONST_IF_DISABLED;
  void setup_render_perspective_right() VRSTORM_CONST_IF_DISABLED;

  void submit_frame_left( GLuint buffer) VRSTORM_CONST_IF_DISABLED;
  void submit_frame_right(GLuint buffer) VRSTORM_CONST_IF_DISABLED;
};

}

#endif // VRSTORM_MANAGER_H_INCLUDED
