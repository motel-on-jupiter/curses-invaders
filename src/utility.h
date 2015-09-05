/*
 * utility.h
 *
 *  Created on: 2015/09/04
 *      Author: minagawa-sho
 */

#ifndef UTILITY_H_
#define UTILITY_H_

#include <stdio.h>

/* Compile */
#ifndef UNUSED
#define UNUSED(_var) do { (void)_var; } while(0)
#endif /* UNUSED */

/* Array utility */
#ifndef N_ELEMENTS
#define N_ELEMENTS(_array) ((int) (sizeof(_array) / sizeof(_array[0])))
#endif /* N_ELEMENTS */

/* Logging */
struct logger {
  const char *logpath;
  FILE *logfile;
};
extern void reset_logger(struct logger *logger, const char *logpath);
extern void emit_log(struct logger *logger, const char *format, ...);
extern void close_logger(struct logger *logger);

/* Time */
struct timer {
  long counter;
  long alarm_interval;
};
extern void reset_timer(struct timer *timer, long alarm_interval);
extern bool count_timer(struct timer *timer, long elapsed_time);
extern void clear_timer(struct timer *timer);

/* Physics */
struct vector2 {
  int x;
  int y;
};

extern bool detect_collided(struct vector2 *one_position,
                            struct vector2 *one_size,
                            struct vector2 *theother_position,
                            struct vector2 *theother_size);

#endif /* UTILITY_H_ */
