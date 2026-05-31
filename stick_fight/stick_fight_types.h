#pragma once
#include <Arduino.h>

struct Pose {
  int8_t hx,hy;
  int8_t sx,sy;
  int8_t bx,by;
  int8_t lax,lay;
  int8_t lhx,lhy;
  int8_t rax,ray;
  int8_t rhx,rhy;
  int8_t lkx,lky;
  int8_t lfx,lfy;
  int8_t rkx,rky;
  int8_t rfx,rfy;
};

struct Scene {
  const Pose* f1a;
  const Pose* f1b;
  const Pose* f2a;
  const Pose* f2b;
  uint8_t  dur;
  int8_t   hitFrame;
  int8_t   hitX, hitY;
  int8_t   hitParts;
  int8_t   hp1dmg;
  int8_t   hp2dmg;
};
