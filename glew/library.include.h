#pragma once

#if ENABLE_OPENGL
#define GLEW_STATIC
#include "glew.h"
#include "wglew.h"

#ifdef DEFINE_SDK_LIB
#pragma DEFINE_SDK_LIB(glew)
#endif
#endif

#if ENABLE_OPENGL_ES
#define GL_GLEXT_PROTOTYPES
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif


