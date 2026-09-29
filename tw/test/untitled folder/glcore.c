#include "glcore.h"

#include <string.h>

PFN_glClear glClear;
PFN_glClearColor glClearColor;
PFN_glViewport glViewport;
PFN_glGetString glGetString;
PFN_glGetError glGetError;
PFN_glDrawArrays glDrawArrays;
PFN_glEnable glEnable;
PFN_glBlendFunc glBlendFunc;
PFN_glActiveTexture glActiveTexture;
PFN_glGenTextures glGenTextures;
PFN_glBindTexture glBindTexture;
PFN_glDeleteTextures glDeleteTextures;
PFN_glPixelStorei glPixelStorei;
PFN_glTexImage2D glTexImage2D;
PFN_glTexSubImage2D glTexSubImage2D;
PFN_glTexParameteri glTexParameteri;
PFN_glReadPixels glReadPixels;
PFN_glCreateShader glCreateShader;
PFN_glShaderSource glShaderSource;
PFN_glCompileShader glCompileShader;
PFN_glGetShaderiv glGetShaderiv;
PFN_glGetShaderInfoLog glGetShaderInfoLog;
PFN_glDeleteShader glDeleteShader;
PFN_glCreateProgram glCreateProgram;
PFN_glAttachShader glAttachShader;
PFN_glLinkProgram glLinkProgram;
PFN_glGetProgramiv glGetProgramiv;
PFN_glGetProgramInfoLog glGetProgramInfoLog;
PFN_glDeleteProgram glDeleteProgram;
PFN_glUseProgram glUseProgram;
PFN_glGetUniformLocation glGetUniformLocation;
PFN_glUniform1f glUniform1f;
PFN_glUniform2f glUniform2f;
PFN_glUniform3f glUniform3f;
PFN_glUniform4f glUniform4f;
PFN_glUniform1i glUniform1i;
PFN_glGenVertexArrays glGenVertexArrays;
PFN_glBindVertexArray glBindVertexArray;
PFN_glDeleteVertexArrays glDeleteVertexArrays;

const char *glcore_fs_tri(void) {
  return "#version 150\n"
         "void main() {\n"
         "  vec2 p;\n"
         "  if (gl_VertexID == 0) p = vec2(-1.0, -1.0);\n"
         "  else if (gl_VertexID == 1) p = vec2(3.0, -1.0);\n"
         "  else p = vec2(-1.0, 3.0);\n"
         "  gl_Position = vec4(p, 0.0, 1.0);\n"
         "}\n";
}

int glcore_load(void) {
#define BIND(name)                                                        \
  do {                                                                    \
    name = (PFN_##name)SDL_GL_GetProcAddress(#name);                      \
    if (!name) {                                                          \
      SDL_Log("Missing GL proc %s", #name);                               \
      return 0;                                                           \
    }                                                                     \
  } while (0)
  BIND(glClear);
  BIND(glClearColor);
  BIND(glViewport);
  BIND(glGetString);
  BIND(glGetError);
  BIND(glDrawArrays);
  BIND(glEnable);
  BIND(glBlendFunc);
  BIND(glActiveTexture);
  BIND(glGenTextures);
  BIND(glBindTexture);
  BIND(glDeleteTextures);
  BIND(glPixelStorei);
  BIND(glTexImage2D);
  BIND(glTexSubImage2D);
  BIND(glTexParameteri);
  BIND(glReadPixels);
  BIND(glCreateShader);
  BIND(glShaderSource);
  BIND(glCompileShader);
  BIND(glGetShaderiv);
  BIND(glGetShaderInfoLog);
  BIND(glDeleteShader);
  BIND(glCreateProgram);
  BIND(glAttachShader);
  BIND(glLinkProgram);
  BIND(glGetProgramiv);
  BIND(glGetProgramInfoLog);
  BIND(glDeleteProgram);
  BIND(glUseProgram);
  BIND(glGetUniformLocation);
  BIND(glUniform1f);
  BIND(glUniform2f);
  BIND(glUniform3f);
  BIND(glUniform4f);
  BIND(glUniform1i);
  BIND(glGenVertexArrays);
  BIND(glBindVertexArray);
  BIND(glDeleteVertexArrays);
#undef BIND
  return 1;
}

GLuint glcore_make_shader(GLenum type, const char *src) {
  GLuint sh = glCreateShader(type);
  const GLchar *s[] = {src};
  glShaderSource(sh, 1, s, NULL);
  glCompileShader(sh);
  GLint ok = 0;
  glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[4096];
    GLsizei n = 0;
    glGetShaderInfoLog(sh, sizeof(log) - 1, &n, log);
    log[n] = 0;
    SDL_Log("Shader compile error: %s", log);
    glDeleteShader(sh);
    return 0;
  }
  return sh;
}

GLuint glcore_make_program(const char *vs, const char *fs) {
  GLuint v = glcore_make_shader(GL_VERTEX_SHADER, vs);
  GLuint f = glcore_make_shader(GL_FRAGMENT_SHADER, fs);
  if (!v || !f) return 0;
  GLuint p = glCreateProgram();
  glAttachShader(p, v);
  glAttachShader(p, f);
  glLinkProgram(p);
  GLint ok = 0;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[4096];
    GLsizei n = 0;
    glGetProgramInfoLog(p, sizeof(log) - 1, &n, log);
    log[n] = 0;
    SDL_Log("Program link error: %s", log);
  }
  glDeleteShader(v);
  glDeleteShader(f);
  return p;
}