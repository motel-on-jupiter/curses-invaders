/*
 * invaders.c
 *
 *  Created on: 2015/08/26
 *      Author: minagawa-sho
 */

#include <sys/time.h>
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <ncurses.h>

#include "invaders_config.h"
#include "utility.h"

enum scene {
  TITLE_SCENE = 0,
  INGAME_SCENE,
};

enum game_event {
  GAME_EVENT_NONE = 0,
  GAME_CLEAR_EVENT,
  GAME_OVER_EVENT,
};

enum color_pair {
  _PADDING = 0,

  /* in-game entities */
  PLAYER_JET_COLOR_PAIR,
  PLAYER_BULLET_COLOR_PAIR,
  TOCHCA_COLOR_PAIR,
  COMMANDER_INVADER_COLOR_PAIR,
  SENIOR_INVADER_COLOR_PAIR,
  YOUNG_INVADER_COLOR_PAIR,
  LOOKIE_INVADER_COLOR_PAIR,
  INVADER_BULLET_COLOR_PAIR,

  /* HUD objects */
  TITLE_COLOR_PAIR,
  EVENT_CAPTION_COLOR_PAIR,
  SCORE_COLOR_PAIR,
  CREDIT_COLOR_PAIR,
  CANVAS_FRAME_COLOR_PAIR,
};

enum invader_type {
  COMMANDER_INVADER,
  SENIOR_INVADER,
  YOUNG_INVADER,
  LOOKIE_INVADER,
};

enum bullet_type {
  PLAYER_BULLET,
  INVADER_BULLET,
};

struct event_caption {
  bool displaying;
  struct timer timer;
};

struct player_jet {
  struct vector2 position;
  struct vector2 size;
};

struct tochca {
  bool block_standings[N_TOCHCA_BLOCKS];
  struct vector2 position;
};

struct invader {
  enum invader_type type;
  bool alive;
  struct vector2 position;
  struct vector2 size;
  struct timer moving_timer;
  int moving_speed_y;
};

struct invader_team {
  struct invader members[N_INVADERS];
  struct invader commander;
  struct timer shooting_timer;
  struct timer commander_turn_timer;
};

struct bullet {
  enum bullet_type type;
  bool active;
  struct vector2 position;
  struct timer moving_timer;
};

struct invaders_game {
  enum game_event event;
  struct event_caption event_caption;
  long score;
  int credit;
  struct player_jet player_jet;
  struct bullet player_bullet;
  struct tochca tochcas[N_TOCHCAS];
  struct invader_team invader_team;
  struct bullet invader_bullets[N_INVADER_BULLETS];
};

static void update_game_on_title_scene(int *scene_change) {
  /* Interpret the key inputs */
  switch (getch()) {
    case 'a':
    case KEY_LEFT:
    case 'd':
    case KEY_RIGHT:
    case 'w':
    case KEY_UP:
      *scene_change = INGAME_SCENE;
      break;
  }
}

static void draw_title_scene() {
  attron(COLOR_PAIR(TITLE_COLOR_PAIR));
  move(TITLE_POSITION_X, TITLE_POSITION_Y - strlen(TITLE_TEXT) / 2);
  addstr(TITLE_TEXT);
}

/**
 * Reset all environments of game
 */
static void reset_game(struct invaders_game *game) {
  int i, j;

  game->event = GAME_EVENT_NONE;
  game->event_caption.displaying = false;
  reset_timer(&game->event_caption.timer, EVENT_CAPTION_DISPLAYING_TIME);
  game->score = SCORE_INITIAL_VALUE;
  game->credit = CREDIT_INITIAL_VALUE;
  game->player_jet.position.x = PLAYER_JET_POSITION_X;
  game->player_jet.position.y = PLAYER_JET_START_POSITION_Y;
  game->player_jet.size.x = PLAYER_JET_SIZE_X;
  game->player_jet.size.y = PLAYER_JET_SIZE_Y;
  game->player_bullet.type = PLAYER_BULLET;
  game->player_bullet.active = false;
  reset_timer(&game->player_bullet.moving_timer, PLAYER_BULLET_MOVING_INTERVAL);
  for (i = 0; i < N_ELEMENTS(game->tochcas); ++i) {
    for (j = 0; j < N_ELEMENTS(game->tochcas[i].block_standings); ++j) {
      game->tochcas[i].block_standings[j] = true;
    }
    game->tochcas[i].position.x = TOCHCA_POSITION_X;
    game->tochcas[i].position.y = TOCHCA_POSITION_Y
        + TOCHCA_LAYOUT_INTERVAL_Y * i;
  }
  for (i = 0; i < N_ELEMENTS(game->invader_team.members); ++i) {
    game->invader_team.members[i].type =
        (0 == i % N_INVADERS_LAYOUT_X) ? SENIOR_INVADER :
        (2 >= i % N_INVADERS_LAYOUT_X) ? YOUNG_INVADER : LOOKIE_INVADER;
    game->invader_team.members[i].alive = true;
    game->invader_team.members[i].position.x = INVADER_START_POSITION_X
        + INVADER_LAYOUT_INTERVAL_X * (i % 5);
    game->invader_team.members[i].position.y = INVADER_START_POSITION_Y
        + INVADER_LAYOUT_INTERVAL_Y * (i / 5);
    game->invader_team.members[i].size.x = INVADER_SIZE_X;
    game->invader_team.members[i].size.y = INVADER_SIZE_Y;
    reset_timer(&game->invader_team.members[i].moving_timer,
                INVADER_MOVING_INTERVAL);
    game->invader_team.members[i].moving_speed_y = 1;
  }
  game->invader_team.commander.type = COMMANDER_INVADER;
  game->invader_team.commander.alive = false;
  game->invader_team.commander.position.x = COMMANDER_INVADER_START_POSITION_X;
  game->invader_team.commander.position.y = COMMANDER_INVADER_START_POSITION_Y;
  reset_timer(&game->invader_team.commander.moving_timer,
              COMMANDER_INVADER_MOVING_INTERVAL);
  game->invader_team.commander.moving_speed_y = 0;
  reset_timer(&game->invader_team.shooting_timer, INVADER_SHOOTING_INTERVAL);
  reset_timer(&game->invader_team.commander_turn_timer, COMMANDER_INVADER_TURN_INTERVAL);
  for (i = 0; i < N_ELEMENTS(game->invader_bullets); ++i) {
    game->invader_bullets[i].type = INVADER_BULLET;
    game->invader_bullets[i].active = false;
    reset_timer(&game->invader_bullets[i].moving_timer, INVADER_BULLET_MOVING_INTERVAL);
  }
}

static void move_bullet(struct bullet *bullet, long elapsed_time) {
  if (bullet->active) {
    if (count_timer(&bullet->moving_timer, elapsed_time)) {
      if (2 >= bullet->position.x
          || (CANVAS_SIZE_X - 3) <= bullet->position.x) {
        bullet->active = false;
      } else {
        bullet->position.x += (PLAYER_BULLET == bullet->type) ? -1 : 1;
      }
    }
  }
}

static void get_tochca_block_position(struct tochca *tochca, int block,
                                      struct vector2 *block_position) {
   block_position->x = tochca->position.x + (block % N_TOCHCA_BLOCKS_LAYOUT_X);
   block_position->y = tochca->position.y + (block / N_TOCHCA_BLOCKS_LAYOUT_X);
}

static struct tochca *detect_collieded_with_tochcas(struct vector2 *point,
                                                    struct tochca *tochcas,
                                                    size_t n_tochcas,
                                                    int *block_hit_with) {
  int i;
  int j;
  struct vector2 block_position;

  for (i = 0; i < (int) n_tochcas; ++i) {
    for (j = 0; j < N_ELEMENTS(tochcas[i].block_standings); ++j) {
      if (tochcas[i].block_standings[j]) {
        get_tochca_block_position(&tochcas[i], j, &block_position);
        if (detect_collided(point, NULL, &block_position, NULL)) {
          *block_hit_with = j;
          return &tochcas[i];
        }
      }
    }
  }
  return NULL;
}

static bool detect_collieded_with_invader(struct vector2 *point,
                                          struct invader *invader) {
  return (invader->alive &&
          detect_collided(point, NULL, &invader->position, &invader->size));
}

static void invoke_event(struct invaders_game *game, enum game_event event) {
  game->event = event;
  game->event_caption.displaying = true;
  clear_timer(&game->event_caption.timer);
}

static void update_game_on_ingame_scene(struct invaders_game *game,
                                        long elapsed_time, int *scene_change) {
  int i, j, k, n_living_invaders, invader_move_speed, block_hit_with, n_living_lines;
  bool stepable, is_annihilation;
  struct invader *shooting_invader, *line_head_invader,
    *line_head_invaders[N_INVADERS_LAYOUT_Y], *invader_hit_with;
  struct tochca *tochca_hit_with;
  struct vector2 block_position;

  if (GAME_EVENT_NONE == game->event) {
    /* Interpret the key inputs */
    switch (getch()) {
      case 'a':
      case KEY_LEFT:
        --game->player_jet.position.y;
        break;
      case 'd':
      case KEY_RIGHT:
        ++game->player_jet.position.y;
        break;
      case 'w':
      case KEY_UP:
        if (!game->player_bullet.active) {
          game->player_bullet.active = true;
          memcpy(&game->player_bullet.position,
                 &game->player_jet.position,
                 sizeof(game->player_bullet.position));
          ++game->player_bullet.position.y;
          clear_timer(&game->player_bullet.moving_timer);
        }
        break;
      default:
        break;
    }


    /* Decide the current aggression level */
    n_living_invaders = 0;
    n_living_lines = 0;
    memset(line_head_invaders, 0, sizeof(line_head_invaders));
    for (i = 0; i < N_INVADERS_LAYOUT_Y; ++i) {
      line_head_invader = NULL;
      for (j = N_INVADERS_LAYOUT_X - 1; j >= 0; --j) {
        struct invader *invader = &game->invader_team.members[i
            * N_INVADERS_LAYOUT_X + j];
        if (invader->alive) {
          ++n_living_invaders;
          if (NULL == line_head_invader) {
            line_head_invader = invader;
          }
        }
      }
      if (NULL != line_head_invader) {
        line_head_invaders[n_living_lines] = line_head_invader;
        ++n_living_lines;
      }
    }
    if (LEVEL7_THRESHOLD >= n_living_invaders) {
      invader_move_speed = LEVEL7_MOVE_SPEED;
    } else if (LEVEL6_THRESHOLD >= n_living_invaders) {
      invader_move_speed = LEVEL6_MOVE_SPEED;
    } else if (LEVEL5_THRESHOLD >= n_living_invaders) {
      invader_move_speed = LEVEL5_MOVE_SPEED;
    } else if (LEVEL4_THRESHOLD >= n_living_invaders) {
      invader_move_speed = LEVEL4_MOVE_SPEED;
    } else if (LEVEL3_THRESHOLD >= n_living_invaders) {
      invader_move_speed = LEVEL3_MOVE_SPEED;
    } else if (LEVEL2_THRESHOLD >= n_living_invaders) {
      invader_move_speed = LEVEL2_MOVE_SPEED;
    } else if (LEVEL1_THRESHOLD >= n_living_invaders) {
      invader_move_speed = LEVEL1_MOVE_SPEED;
    } else {
      invader_move_speed = LEVEL0_MOVE_SPEED;
    }

    /* The commander invader appear on schedule */
    if (!game->invader_team.commander.alive &&
        count_timer(&game->invader_team.commander_turn_timer, elapsed_time)) {
      game->invader_team.commander.alive = true;
      game->invader_team.commander.position.x = COMMANDER_INVADER_START_POSITION_X;
      game->invader_team.commander.position.y = COMMANDER_INVADER_START_POSITION_Y;
      clear_timer(&game->invader_team.commander.moving_timer);
    }

    /* move the invaders */
    stepable = false;
    for (i = 0; i < N_ELEMENTS(game->invader_team.members); ++i) {
      if (game->invader_team.members[i].alive) {
        if (0 > game->invader_team.members[i].moving_speed_y
            &&
            INVADER_MOVING_RANGE_Y_MIN
                >= game->invader_team.members[i].position.y) {
          stepable = true;
          break;
        } else if (0 < game->invader_team.members[i].moving_speed_y
            &&
            INVADER_MOVING_RANGE_Y_MAX
                <= game->invader_team.members[i].position.y) {
          stepable = true;
          break;
        }
      }
    }
    for (i = 0; i < N_ELEMENTS(game->invader_team.members); ++i) {
      if (game->invader_team.members[i].alive) {
        if (count_timer(&game->invader_team.members[i].moving_timer,
                        elapsed_time * invader_move_speed / 100)) {
          if (stepable) {
            game->invader_team.members[i].position.x += INVADER_INVASION_STEP_X;
            game->invader_team.members[i].moving_speed_y *= -1;
          } else {
            game->invader_team.members[i].position.y += game->invader_team
                .members[i].moving_speed_y;
          }
        }
      }
    }
    if (game->invader_team.commander.alive) {
      if (INVADER_MOVING_RANGE_Y_MAX <= game->invader_team.commander.position.y) {
        game->invader_team.commander.alive = false;
        clear_timer(&game->invader_team.commander_turn_timer);
      } else if (count_timer(&game->invader_team.commander.moving_timer, elapsed_time)) {
        ++game->invader_team.commander.position.y;
      }
    }

    /* Make the invader to shoot his bullet */
    if (count_timer(&game->invader_team.shooting_timer, elapsed_time)) {
      shooting_invader = line_head_invaders[rand() % n_living_lines];
      assert(NULL != shooting_invader);
      for (i = 0; i < N_ELEMENTS(game->invader_bullets); ++i) {
        if (!game->invader_bullets[i].active) {
          game->invader_bullets[i].active = true;
          memcpy(&game->invader_bullets[i].position, &shooting_invader->position,
                 sizeof(game->invader_bullets[i].position));
          game->invader_bullets[i].position.x += 2;
          ++game->invader_bullets[i].position.y;
          clear_timer(&game->invader_bullets[i].moving_timer);
          break;
        }
      }
    }

    /* move bullets */
    move_bullet(&game->player_bullet, elapsed_time);
    for (i = 0; i < N_ELEMENTS(game->invader_bullets); ++i) {
      move_bullet(&game->invader_bullets[i], elapsed_time);
    }

    /* Detect player bullet hit */
    if (game->player_bullet.active) {
      tochca_hit_with = detect_collieded_with_tochcas(&game->player_bullet.position,
                                                      game->tochcas,
                                                      N_ELEMENTS(game->tochcas),
                                                      &block_hit_with);
      if (NULL != tochca_hit_with) {
        game->player_bullet.active = false;
        tochca_hit_with->block_standings[block_hit_with] = false;
      } else {
        invader_hit_with = NULL;
        for (j = 0; j < N_ELEMENTS(game->invader_team.members); ++j) {
          if (detect_collieded_with_invader(&game->player_bullet.position,
                                            &game->invader_team.members[j])) {
            invader_hit_with = &game->invader_team.members[j];
            break;
          }
        }
        if (NULL == invader_hit_with) {
          if (game->invader_team.commander.alive &&
              detect_collided(&game->player_bullet.position, NULL,
                              &game->invader_team.commander.position,
                              &game->invader_team.commander.size)) {
            invader_hit_with = &game->invader_team.commander;
          }
        }
        if (NULL != invader_hit_with) {
          game->player_bullet.active = false;
          invader_hit_with->alive = false;
          game->score +=
              (COMMANDER_INVADER == invader_hit_with->type) ?
              COMMANDER_INVADER_SCORE :
              (SENIOR_INVADER == invader_hit_with->type) ?
              SENIOR_INVADER_SCORE :
              (YOUNG_INVADER == invader_hit_with->type) ?
                  YOUNG_INVADER_SCORE : LOOKIE_INVADER_SCORE;
        }
      }
    }

    /* Detect invader bullet hit */
    for (i = 0; i < N_ELEMENTS(game->invader_bullets); ++i) {
      struct bullet *bullet = &game->invader_bullets[i];
      if (bullet->active) {
        if (detect_collided(&bullet->position, NULL, &game->player_jet.position,
                            &game->player_jet.size)) {
          bullet->active = false;
          if (0 < game->credit) {
            game->credit -= 1;
          } else {
            invoke_event(game, GAME_OVER_EVENT);
          }
        } else if (game->player_bullet.active &&
                   game->player_bullet.position.x <= bullet->position.x &&
                   game->player_bullet.position.y == bullet->position.y) {
          game->player_bullet.active = false;
          bullet->active = false;
        } else {
          tochca_hit_with = detect_collieded_with_tochcas(&bullet->position,
                                                          (struct tochca *) game->tochcas,
                                                          N_ELEMENTS(game->tochcas),
                                                          &block_hit_with);
          if (NULL != tochca_hit_with) {
            bullet->active = false;
            tochca_hit_with->block_standings[block_hit_with] = false;
          }
        }
      }
    }

    /* Detect invaders hit with tochcas */
    for (i = 0; i < N_ELEMENTS(game->tochcas); ++i) {
      for (j = 0; j < N_ELEMENTS(game->tochcas[i].block_standings); ++j) {
        if (game->tochcas[i].block_standings[j]) {
          for (k = 0; k < N_ELEMENTS(game->invader_team.members); ++k) {
            get_tochca_block_position(&game->tochcas[i], j, &block_position);
            if (detect_collieded_with_invader(&block_position,
                                              &game->invader_team.members[k])) {
              game->tochcas[i].block_standings[j] = false;
              break;
            }
          }
        }
      }
    }

    /* Check the annihilation */
    is_annihilation = true;
    for (i = 0; i < N_ELEMENTS(game->invader_team.members); ++i) {
      if (game->invader_team.members[i].alive) {
        is_annihilation = false;
        break;
      }
    }
    if (is_annihilation) {
      invoke_event(game, GAME_CLEAR_EVENT);
    }

    /* Detect player jet hit with the invaders */
    if (GAME_EVENT_NONE == game->event) {
      for (i = 0; i < N_ELEMENTS(game->invader_team.members); ++i) {
        if (game->invader_team.members[i].alive &&
            detect_collided(&game->player_jet.position,
                            &game->player_jet.size,
                            &game->invader_team.members[i].position,
                            &game->invader_team.members[i].size)) {
          invoke_event(game, GAME_OVER_EVENT);
          break;
        }
      }
    }

    /* Check the invasion */
    if (GAME_EVENT_NONE == game->event) {
      for (i = 0; i < n_living_lines; ++i) {
        if (line_head_invaders[i]->alive &&
            INVADER_INVASION_THRESHOLD_POSITION_X
            <= line_head_invaders[i]->position.x + 1) {
          invoke_event(game, GAME_OVER_EVENT);
          break;
        }
      }
    }
  } else {
    /* Clean the input buffer */
    getch();
  }

  /* Update the caption timer */
  if (game->event_caption.displaying
      && count_timer(&game->event_caption.timer, elapsed_time)) {
    game->event_caption.displaying = false;
    *scene_change = TITLE_SCENE;
  }
}

void draw_invader(struct invader *invader) {
  if (invader->alive) {
    switch (invader->type) {
      case COMMANDER_INVADER:
        attron(COLOR_PAIR(COMMANDER_INVADER_COLOR_PAIR));
        break;
      case SENIOR_INVADER:
        attron(COLOR_PAIR(SENIOR_INVADER_COLOR_PAIR));
        break;
      case YOUNG_INVADER:
        attron(COLOR_PAIR(YOUNG_INVADER_COLOR_PAIR));
        break;
      default:
        attron(COLOR_PAIR(LOOKIE_INVADER_COLOR_PAIR));
        break;
    }
    move(invader->position.x, invader->position.y);
    addch(INVADER_RENDERING_CHAR);
    addch(INVADER_RENDERING_CHAR);
    addch(INVADER_RENDERING_CHAR);
    move(invader->position.x + 1, invader->position.y);
    addch(INVADER_RENDERING_CHAR);
    addch(INVADER_RENDERING_CHAR);
    addch(INVADER_RENDERING_CHAR);
  }
}

static void draw_ingame_scene(struct invaders_game *game) {
  int i, j;

  /* Render the player jet */
  if (0 <= game->credit) {
    attron(COLOR_PAIR(PLAYER_JET_COLOR_PAIR));
    move(game->player_jet.position.x, game->player_jet.position.y + 1);
    addch(PLAYER_JET_RENDERING_CHAR);
    move(game->player_jet.position.x + 1, game->player_jet.position.y);
    addch(PLAYER_JET_RENDERING_CHAR);
    addch(PLAYER_JET_RENDERING_CHAR);
    addch(PLAYER_JET_RENDERING_CHAR);
  }

  /* Render the player bullet */
  if (game->player_bullet.active) {
    attron(COLOR_PAIR(PLAYER_BULLET_COLOR_PAIR));
    move(game->player_bullet.position.x, game->player_bullet.position.y);
    addch(PLAYER_BULLET_RENDERING_CHAR);
  }

  /* Render the tochcas */
  attron(COLOR_PAIR(TOCHCA_COLOR_PAIR));
  for (i = 0; i < N_ELEMENTS(game->tochcas); ++i) {
    for (j = 0; j < N_ELEMENTS(game->tochcas[i].block_standings); ++j) {
      if (game->tochcas[i].block_standings[j]) {
        move(game->tochcas[i].position.x + (j % N_TOCHCA_BLOCKS_LAYOUT_X),
             game->tochcas[i].position.y + (j / N_TOCHCA_BLOCKS_LAYOUT_X));
        addch(TOCHCA_RENDERING_CHAR);
      }
    }
  }

  /* Render the invaders */
  for (i = 0; i < N_ELEMENTS(game->invader_team.members); ++i) {
    draw_invader(&game->invader_team.members[i]);
  }
  draw_invader(&game->invader_team.commander);

  /* Render the invader bullets */
  for (i = 0; i < N_ELEMENTS(game->invader_bullets); ++i) {
    if (game->invader_bullets[i].active) {
      attron(COLOR_PAIR(INVADER_BULLET_COLOR_PAIR));
      move(game->invader_bullets[i].position.x,
           game->invader_bullets[i].position.y);
      addch(INVADER_BULLET_RENDERING_CHAR);
    }
  }

  /* Render score HUD */
  attron(COLOR_PAIR(SCORE_COLOR_PAIR));
  move(SCORE_POSITION_X,
       SCORE_POSITION_Y - 11/* the length of "SCORE: %04ld" */);
  printw("SCORE: %04ld", game->score);

  /* Render credit HUD */
  attron(COLOR_PAIR(CREDIT_COLOR_PAIR));
  move(CREDIT_POSITION_X, CREDIT_POSITION_Y);
  printw("CREDIT: %d", game->credit);

  /* Render caption HUD with blinking */
  if (game->event_caption.displaying
      && (EVENT_CAPTION_BLINKING_INTERVAL
          <= game->event_caption.timer.counter % 1000L)) {
    attron(COLOR_PAIR(EVENT_CAPTION_COLOR_PAIR));
    const char *caption_text =
        (GAME_CLEAR_EVENT == game->event) ?
        GAME_CLEAR_CAPTION_TEXT : GAME_OVER_CAPTION_TEXT;
    move(EVENT_CAPTION_POSITION_X,
         EVENT_CAPTION_POSITION_Y - strlen(caption_text) / 2);
    printw(caption_text);
  }
}

static void draw_canvas_frame() {
  int i;

  attron(COLOR_PAIR(CANVAS_FRAME_COLOR_PAIR));
  move(0, 0);
  for (i = 0; i < CANVAS_SIZE_Y; ++i) {
    addch(CANVAS_FRAME_RENDERING_CHAR);
  }
  for (i = 0; i < CANVAS_SIZE_X; ++i) {
    move(i, 0);
    addch(CANVAS_FRAME_RENDERING_CHAR);
    move(i, CANVAS_SIZE_Y - 1);
    addch(CANVAS_FRAME_RENDERING_CHAR);
  }
  move(CANVAS_SIZE_X - 1, 0);
  for (i = 0; i < CANVAS_SIZE_Y; ++i) {
    addch(CANVAS_FRAME_RENDERING_CHAR);
  }
}

int main(int argc, char **argv) {
  int status, scene, next_scene;
  char errmsg[128];
  long elapsed_msec, wait_msec;
  struct timespec wait_time, left_time;
  struct timeval frame_start_time, frame_end_time;
  WINDOW *window;
  struct invaders_game game;
  struct logger error_logger;

  UNUSED(argc);
  UNUSED(argv);

  /* Initialize for ncurses library */
  reset_logger(&error_logger, ERRORLOG_FILEPATH);
  status = 1;
  window = initscr();
  if (ERR == wresize(window, CANVAS_SIZE_X, CANVAS_SIZE_Y)) {
    emit_log(&error_logger,
             "Failed to change the ncurses setting for window size");
    goto cleanup;
  }
  if (ERR == keypad(window, true)) {
    emit_log(&error_logger,
             "Failed to change the ncurses setting for key input receiving");
    goto cleanup;
  }
  if (ERR == noecho()) {
    emit_log(&error_logger,
             "Failed to change the ncurses setting for echoing setting");
    goto cleanup;
  }
  curs_set(0);
  timeout(0);
  if (has_colors() && can_change_color()) {
    if (ERR == start_color()) {
      emit_log(&error_logger,
               "Failed to change the ncurses setting to set up coloring");
      goto cleanup;
    }
    if ((ERR == init_pair(PLAYER_JET_COLOR_PAIR, PLAYER_JET_COLOR, COLOR_BLACK))
        || (ERR == init_pair(PLAYER_BULLET_COLOR_PAIR, PLAYER_BULLET_COLOR,
        COLOR_BLACK))
        || (ERR == init_pair(TOCHCA_COLOR_PAIR, TOCHCA_COLOR, COLOR_BLACK))
        || (ERR == init_pair(COMMANDER_INVADER_COLOR_PAIR, COMMANDER_INVADER_COLOR,
            COLOR_BLACK))
        || (ERR == init_pair(SENIOR_INVADER_COLOR_PAIR, SENIOR_INVADER_COLOR,
        COLOR_BLACK))
        || (ERR == init_pair(YOUNG_INVADER_COLOR_PAIR, YOUNG_INVADER_COLOR,
        COLOR_BLACK))
        || (ERR == init_pair(LOOKIE_INVADER_COLOR_PAIR, LOOKIE_INVADER_COLOR,
        COLOR_BLACK))
        || (ERR == init_pair(INVADER_BULLET_COLOR_PAIR, INVADER_BULLET_COLOR,
        COLOR_BLACK))
        || (ERR == init_pair(TITLE_COLOR_PAIR, TITLE_COLOR, COLOR_BLACK))
        || (ERR == init_pair(EVENT_CAPTION_COLOR_PAIR, EVENT_CAPTION_COLOR,
        COLOR_BLACK))
        || (ERR == init_pair(SCORE_COLOR_PAIR, SCORE_COLOR, COLOR_BLACK))
        || (ERR == init_pair(CREDIT_COLOR_PAIR, CREDIT_COLOR, COLOR_BLACK))
        || (ERR
            == init_pair(CANVAS_FRAME_COLOR_PAIR, CANVAS_FRAME_COLOR, COLOR_BLACK))) {
      emit_log(&error_logger,
               "Failed to change the ncurses setting to define color pair");
      goto cleanup;
    }

  }

  /* Execute game loop */
  scene = -1;
  next_scene = TITLE_SCENE;
  while (1) {
    /* Record the frame starting time */
    if (0 != gettimeofday(&frame_start_time, NULL)) {
      emit_log(&error_logger, "Failed to get time of frame starting");
      goto cleanup;
    }

    /* Change the next scene if needed */
    if (scene != next_scene) {
      scene = next_scene;
      if (INGAME_SCENE == scene) {
        reset_game(&game);
      }
    }

    /* Update the objects */
    if (TITLE_SCENE == scene) {
      update_game_on_title_scene(&next_scene);
    } else if (INGAME_SCENE == scene) {
      update_game_on_ingame_scene(&game, IDEAL_FRAME_TIME, &next_scene);
    }

    /* Render the objects */
    erase();
    if (TITLE_SCENE == scene) {
      draw_title_scene();
    } else if (INGAME_SCENE == scene) {
      draw_ingame_scene(&game);
    }
    draw_canvas_frame();
    refresh();

    /* Adjust the frame interval */
    if (0 != gettimeofday(&frame_end_time, NULL)) {
      emit_log(&error_logger, "Failed to get time of frame finished");
      goto cleanup;
    }
    elapsed_msec = (frame_end_time.tv_sec - frame_start_time.tv_sec) * 1000L
        + (frame_end_time.tv_usec - frame_start_time.tv_usec) / 1000000L;
    wait_msec = IDEAL_FRAME_TIME - elapsed_msec;
    if (0L < wait_msec) {
      wait_time.tv_sec = 0;
      wait_time.tv_nsec = wait_msec * 1000000L;
      while (-1 == nanosleep(&wait_time, &left_time)) {
        if (EINTR == errno) {
          memcpy(&wait_time, &left_time, sizeof(struct timespec));
        } else {
          if (0 == strerror_r(errno, errmsg, sizeof(errmsg))) {
            emit_log(
                &error_logger,
                "Failed to sleep until the end of frame: errmsg=%s, sleeping_msec=%ld",
                errmsg, wait_msec);
          } else {
            emit_log(&error_logger, "Failed to get error message: errno=%d",
            errno);
          }
          goto cleanup;
        }
      }
    }
  }
  status = 0;

 cleanup:
  endwin();
  close_logger(&error_logger);
  return status;
}
