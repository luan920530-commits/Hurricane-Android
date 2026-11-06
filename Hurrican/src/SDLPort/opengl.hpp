#ifndef _OPENGL_H_
#define _OPENGL_H_

/*
 * Inclusion de las cabeceras de OpenGL.
 *
 * Escritorio: libepoxy resuelve en tiempo de ejecucion todas las funciones GL/GLES, que es
 * lo que espera el resto del motor.
 *
 * Android: no hay libepoxy, pero tampoco hace falta. El NDK proporciona <GLES2/gl2.h> con
 * las funciones de OpenGL ES 2.0 ya declaradas y el cargador del sistema se encarga de
 * enlazarlas al cargar libGLESv2.so. Se mantiene la misma superficie de API para el codigo
 * del juego (mismas funciones, mismos enums).
 */
#if defined(ANDROID)
#  include <GLES2/gl2.h>
#  include <GLES2/gl2ext.h>

// GLES2 no incluye glClearDepth (solo glClearDepthf), igual que en el resto del port GLES.
#  ifndef GL_GLEXT_PROTOTYPES
#    define GL_GLEXT_PROTOTYPES 1
#  endif
#else
#  include <epoxy/gl.h>
#endif

// Do some sanity checks
#if !defined(USE_GL1) && !defined(USE_GL2) && !defined(USE_GL3)
#error "ERROR USE_GL1/2/3 are not defined. At least one must be defined."
#elif defined(USE_GL1) && (defined(USE_GL2) || defined(USE_GL3))
#error "ERROR USE_GL1 and USE_GL2/3 are defined. Only one must be defined."
#elif defined(USE_GL2) && defined(USE_GL3)
#error "ERROR USE_GL2 and USE_GL3 are defined. Only one must be defined."
#elif defined(USE_GL1) && (defined(USE_GLES2) || defined(USE_GLES3))
#error "ERROR USE_GL1 and USE_GLES2 are defined. Replace USE_GLES2 or 3 for USE_GLES1"
#elif defined(USE_GL2) && defined(USE_GLES1)
#error "ERROR USE_GL2 and USE_GLES1 are defined. Replace USE_GLES1 for USE_GLES2"
#endif

#endif /* _OPENGL_H_ */
