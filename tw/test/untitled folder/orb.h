#ifndef ORB_H
#define ORB_H

#include "util.h"

#define ORB_X 75
#define ORB_Y 210
#define ORB_SIZE 300

typedef struct OrbProg {
  GLuint prog;
  GLint loc_viewport, loc_resolution, loc_time, loc_primary, loc_secondary;
  GLint loc_noiseIntensity, loc_glowIntensity, loc_saturation, loc_brightness;
  GLint loc_rotationSpeed, loc_noiseScale, loc_coreIntensity, loc_edgeSoftness;
} OrbProg;

typedef struct OrbitState {
  float primary[3];
  float secondary[3];
} OrbitState;

Box orb_box(void);
OrbProg build_orb_prog(void);
void orb_draw(const OrbProg *p, const OrbitState *st, float t, int winH);
void roll_orb_colors(OrbitState *o);

#endif