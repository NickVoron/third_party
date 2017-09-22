#pragma once

#if ENABLE_OPENGL
#define GLEW_STATIC
#include "glew.h"
#include "wglew.h"

#pragma comment(lib, "glew32s.lib")
#endif

#if ENABLE_OPENGL_ES
#define GL_GLEXT_PROTOTYPES
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif


