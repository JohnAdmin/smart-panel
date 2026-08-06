#pragma once
#include <Arduino.h>

#define MAX_SCHEDULES 16

// Days-of-week bitmask: bit 0 = Sunday, bit 1 = Monday, ... bit 6 = Saturday
#define DAY_SUN  (1 << 0)
#define DAY_MON  (1 << 1)
#define DAY_TUE  (1 << 2)
#define DAY_WED  (1 << 3)
#define DAY_THU  (1 << 4)
#define DAY_FRI  (1 << 5)
#define DAY_SAT  (1 << 6)
#define DAY_ALL  0x7F
#define DAY_WEEKDAYS (DAY_MON | DAY_TUE | DAY_WED | DAY_THU | DAY_FRI)
#define DAY_WEEKEND  (DAY_SAT | DAY_SUN)

struct Schedule {
  int scene_index;   // which scene to execute (0..MAX_SCENES-1)
  uint8_t hour;      // 0-23
  uint8_t minute;    // 0-59
  uint8_t days;      // bitmask of days (DAY_SUN..DAY_SAT)
  bool enabled;

  Schedule() : scene_index(0), hour(7), minute(0), days(DAY_ALL), enabled(true) {}
};

extern Schedule schedules[MAX_SCHEDULES];
extern int scheduleCount;

void loadSchedules();
bool saveSchedules();
bool addSchedule(int sceneIdx, uint8_t h, uint8_t m, uint8_t days);
void deleteSchedule(int index);
void checkSchedules();  // call every ~1 second from main loop
