#ifndef CONTROLLER_H_INCLUDED
#define CONTROLLER_H_INCLUDED

#ifdef __MINGW32__
  #include "openvr_mingw.hpp"
#else
  #include "openvr.h"
#endif // __MINGW32__
#include "vectorstorm/matrix/matrix4.h"

namespace vrstorm {

#ifndef VRSTORM_DISABLED
  struct controller {
    unsigned int id = 0;
    matrix4f position;
    vr::RenderModel_t* model = nullptr;
  };
#endif // VRSTORM_DISABLED

}

#endif // CONTROLLER_H_INCLUDED
