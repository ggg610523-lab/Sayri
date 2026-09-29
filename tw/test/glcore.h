#ifndef GLCORE_H
#define GLCORE_H

#include <SDL.h>

#ifdef _WIN32
#define MYGLAPI __stdcall
#else
#define MYGLAPI
#endif

#define glClear GLH_glClear
#define glClearColor GLH_glClearColor
#define glViewport GLH_glViewport
#define glGetString GLH_glGetString
#define glGetError GLH_glGetError
#define glDrawArrays GLH_glDrawArrays
#define glEnable GLH_glEnable
#define glBlendFunc GLH_glBlendFunc
#define glActiveTexture GLH_glActiveTexture
#define glGenTextures GLH_glGenTextures
#define glBindTexture GLH_glBindTexture
#define glDeleteTextures GLH_glDeleteTextures
#define glPixelStorei GLH_glPixelStorei
#define glTexImage2D GLH_glTexImage2D
#define glTexSubImage2D GLH_glTexSubImage2D
#define glTexParameteri GLH_glTexParameteri
#define glReadPixels GLH_glReadPixels
#include <SDL_opengl.h>
#undef glClear
#undef glClearColor
#undef glViewport
#undef glGetString
#undef glGetError
#undef glDrawArrays
#undef glEnable
#undef glBlendFunc
#undef glActiveTexture
#undef glGenTextures
#undef glBindTexture
#undef glDeleteTextures
#undef glPixelStorei
#undef glTexImage2D
#undef glTexSubImage2D
#undef glTexParameteri
#undef glReadPixels

#define GLDECL(ret, name, args) typedef ret(MYGLAPI *PFN_##name) args; \
  extern PFN_##name name;

GLDECL(void, glClear, (GLbitfield))
GLDECL(void, glClearColor, (GLfloat, GLfloat, GLfloat, GLfloat))
GLDECL(void, glViewport, (GLint, GLint, GLsizei, GLsizei))
GLDECL(const GLubyte *, glGetString, (GLenum))
GLDECL(GLenum, glGetError, (void))
GLDECL(void, glDrawArrays, (GLenum, GLint, GLsizei))
GLDECL(void, glEnable, (GLenum))
GLDECL(void, glBlendFunc, (GLenum, GLenum))
GLDECL(void, glActiveTexture, (GLenum))
GLDECL(void, glGenTextures, (GLsizei, GLuint *))
GLDECL(void, glBindTexture, (GLenum, GLuint))
GLDECL(void, glDeleteTextures, (GLsizei, const GLuint *))
GLDECL(void, glPixelStorei, (GLenum, GLint))
GLDECL(void, glTexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint,
                            GLenum, GLenum, const void *))
GLDECL(void, glTexSubImage2D, (GLenum, GLint, GLint, GLint, GLsizei, GLsizei,
                               GLenum, GLenum, const void *))
GLDECL(void, glTexParameteri, (GLenum, GLenum, GLint))
GLDECL(void, glReadPixels, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum,
                            void *))
GLDECL(GLuint, glCreateShader, (GLenum))
GLDECL(void, glShaderSource, (GLuint, GLsizei, const GLchar *const *,
                              const GLint *))
GLDECL(void, glCompileShader, (GLuint))
GLDECL(void, glGetShaderiv, (GLuint, GLenum, GLint *))
GLDECL(void, glGetShaderInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *))
GLDECL(void, glDeleteShader, (GLuint))
GLDECL(GLuint, glCreateProgram, (void))
GLDECL(void, glAttachShader, (GLuint, GLuint))
GLDECL(void, glLinkProgram, (GLuint))
GLDECL(void, glGetProgramiv, (GLuint, GLenum, GLint *))
GLDECL(void, glGetProgramInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *))
GLDECL(void, glDeleteProgram, (GLuint))
GLDECL(void, glUseProgram, (GLuint))
GLDECL(GLint, glGetUniformLocation, (GLuint, const GLchar *))
GLDECL(void, glUniform1f, (GLint, GLfloat))
GLDECL(void, glUniform2f, (GLint, GLfloat, GLfloat))
GLDECL(void, glUniform3f, (GLint, GLfloat, GLfloat, GLfloat))
GLDECL(void, glUniform4f, (GLint, GLfloat, GLfloat, GLfloat, GLfloat))
GLDECL(void, glUniform1i, (GLint, GLint))
GLDECL(void, glGenVertexArrays, (GLsizei, GLuint *))
GLDECL(void, glBindVertexArray, (GLuint))
GLDECL(void, glDeleteVertexArrays, (GLsizei, const GLuint *))

int glcore_load(void);
const char *glcore_fs_tri(void);
GLuint glcore_make_shader(GLenum type, const char *src);
GLuint glcore_make_program(const char *vs, const char *fs);

#endif