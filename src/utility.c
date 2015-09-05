/*
 * timer.c
 *
 *  Created on: 2015/09/04
 *      Author: minagawa-sho
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#include "utility.h"

void reset_logger(struct logger *logger, const char *logpath) {
  logger->logpath = logpath;
  logger->logfile = NULL;
}

void emit_log(struct logger *logger, const char *format, ...) {
  va_list args;

  if (NULL == logger->logfile) {
    /* Open the log file */
    logger->logfile = fopen("error.log", "a");
    if (NULL == logger->logfile) {
      fprintf(stderr, "Failed to open logfile: path=%s", logger->logpath);
      return;
    }
  }
  va_start(args, format);
  vfprintf(logger->logfile, format, args);
  fprintf(logger->logfile, "\n");
  va_end(args);
}

void close_logger(struct logger *logger) {
  if (NULL != logger->logfile) {
    fclose(logger->logfile);
  }
  logger->logfile = NULL;
}

void reset_timer(struct timer *timer, long alarm_interval) {
  timer->counter = 0L;
  timer->alarm_interval = alarm_interval;
}

bool count_timer(struct timer *timer, long elapsed_time) {
  timer->counter += elapsed_time;
  if (timer->alarm_interval <= timer->counter) {
    timer->counter -= timer->alarm_interval;
    return true;
  }
  return false;
}

void clear_timer(struct timer *timer) {
  timer->counter = 0L;
}

static bool detect_collided_with_point(struct vector2 *position,
                                       struct vector2 *size,
                                       struct vector2 *point_position) {
  return (position->x <= point_position->x
      && position->x + size->x > point_position->x
      && position->y <= point_position->y
      && position->y + size->y > point_position->y);
}

extern bool detect_collided(struct vector2 *one_position,
                            struct vector2 *one_size,
                            struct vector2 *theother_position,
                            struct vector2 *theother_size) {
  struct vector2 default_size;
  default_size.x = 1;
  default_size.y = 1;
  if (NULL == one_size) {
    one_size = &default_size;
  }
  if (NULL == theother_size) {
    theother_size = &default_size;
  }
  return detect_collided_with_point(one_position, one_size, theother_position)
      || detect_collided_with_point(theother_position, theother_size, one_position);
}
