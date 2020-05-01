#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <math.h>

#include "lcd.h"
#include "graphics.h"
#include "sprite.h"
#include "cpu_speed.h"

#include "usb_serial.h"

//Function declarations
void init_hardware(void);
void init_adc(void);

void intro_screen(void);
void countdown(void);
void border(void);
void status_display(void);

void ship_setup(void);
void alien_setup(void);
void missile_setup(void);
void mothership_setup(void);
void msMissile_setup(void);
void turret_setup(void);
void turret_calc(void);

void process(void);
void alien_attack(Sprite* alien);
void alien_spawn(Sprite* alien);
void mothership_attack(Sprite* mothership);
void msMissile_attack(Sprite* msMissile);

void collision_checker(void);
bool collision_SnA(Sprite* alien);
bool collision_MSnS(void);
bool collision_MSnM(Sprite* missile);
bool collision_MnA(Sprite* missile, Sprite* alien);
bool collision_MsMnS(void);

void game_end(void);

void sprite_turn_to(Sprite* sprite, double dx, double dy);
bool sprite_step(Sprite* sprite);
bool sprite_show(Sprite* sprite);
bool sprite_hide(Sprite* sprite);
bool sprite_move_to(Sprite* sprite, double x, double y);
void sprite_set_image(Sprite* sprite, char image[]);
void sprite_turn(Sprite* sprite, double degrees);

int get_game_time(void);
double get_system_time(void);

int rand(void);
void srand(unsigned int seed);

void check_debugger(void);
void ship_info(int x_pos, int y_pos, char* direction);

void draw_centred(unsigned char y, char* string);
void send_line(char* string);
void send_debug_string(char string[]);


//Sprites
#define SHIP_WIDTH 3
#define SHIP_HEIGHT 3

unsigned char ship_image[3] = {
0b01000000,
0b10100000,
0b11100000,
};

unsigned char ship_left[3] = {
0b01100000,
0b10100000,
0b01100000,
};

unsigned char ship_right[3] = {
0b11000000,
0b10100000,
0b11000000,
};

unsigned char ship_down[3] = {
0b11100000,
0b10100000,
0b01000000,
};

Sprite ship;


#define ALIEN_WIDTH 3
#define ALIEN_HEIGHT 3
#define MAX_A 5

unsigned char alien_image[3] = {
0b11100000,
0b11100000,
0b11100000,
};

Sprite alien[MAX_A];


#define MISSILE_WIDTH 2
#define MISSILE_HEIGHT 2
#define MAX_M 5

unsigned char missile_image[2] = {
0b11000000,
0b11000000,
};

Sprite missile[MAX_M];


#define MOTHERSHIP_WIDTH 8
#define MOTHERSHIP_HEIGHT 8

unsigned char mothership_full[8] = {
0b11111111,
0b11000011,
0b10100101,
0b10011001,
0b10011001,
0b10100101,
0b11000011,
0b11111111,
};

unsigned char mothership_34[8] = {
0b11111111,
0b10000001,
0b10100101,
0b10011001,
0b10011001,
0b10100101,
0b10000001,
0b11111111,
};

unsigned char mothership_half[8] = {
0b11111111,
0b10000001,
0b10000001,
0b10011001,
0b10011001,
0b10000001,
0b10000001,
0b11111111,
};

unsigned char mothership_14[8] = {
0b11111111,
0b10000001,
0b10000001,
0b10000001,
0b10000001,
0b10000001,
0b10000001,
0b11111111,
};

unsigned char mothership_dead[8] = {
0b11101101,
0b00000001,
0b10000000,
0b10000001,
0b00000001,
0b10000000,
0b10000001,
0b01101101,
};

Sprite mothership;


#define msMISSILE_WIDTH 3
#define msMISSILE_HEIGHT 1

unsigned char msMissile_image[1] = {
0b10100000
};

Sprite msMissile;


#define TURRET_WIDTH 4
#define TURRET_HEIGHT 1

unsigned char turret_image[1] = {
0b11110000
};

Sprite turret;


//Variables
volatile int score = 0;
volatile int lives = 6;
volatile int timer = 0;
volatile int min;
volatile int alienCount;
volatile int bossHealth;
volatile int debugCounter = 0;
int angle;

int16_t adc_value = 0;

bool game_over = false;
bool attack[MAX_A] = {false};
bool updateDirection[MAX_M] = {false};
bool usb_connected = false;
bool mothershipActive = false;
bool gametime = false;
bool mothershipAttack;

char * direction = "Up";

  //Overflow count
volatile unsigned long ovf_count;
volatile unsigned long overflow;

  //Attack information
double dx;
double dy;
double dist;

//Timer information
#define FREQUENCY 8000000.0
#define PRESCALER 1024.0

//Main
int main(void){

  //Setting the clock speed
  set_clock_speed(CPU_8MHz);

  init_hardware();

  if (!usb_connected){
    check_debugger();
  }

  intro_screen();
  countdown();

  srand(TCNT1);
  adc_value = 0;

  ship_setup();
  alien_setup();
  missile_setup();

  ovf_count = 0;
  min = 0;
  mothershipActive = false;

  while (!game_over){
    process();
    _delay_ms(50);
  }
}

void init_hardware(void) {
  //Initialising the LCD Screen
  lcd_init(LCD_HIGH_CONTRAST);

  //Initialising the buttons as inputs
  DDRF &= ~(0b1<<6); //SW1
  DDRF &= ~(0b1<<5); //SW2

  DDRD |= ~(1<<1); //UP
  DDRD |= ~(1<<0); //RIGHT
  DDRB |= ~(1<<7); //DOWN
  DDRB |= ~(1<<1); //LEFT

  //Setting up the timer
  TCCR1B &= ~((1<<WGM12)); //Timer 1
  TCCR0B &= ~((1<<WGM02)); //Timer 0

    //Set prescaler (T1) = 1024
  TCCR1B |= (1<<CS12);
  TCCR1B |= (1<<CS10);
  TCCR1B &= ~((1<<CS11));

    //Set prescaler (T2) = 1024
  TCCR0B |= (1<<CS02);
  TCCR0B |= (1<<CS00);
  TCCR0B &= ~((1<<CS01));

    //Enabling overflow interrupts
  TIMSK1 |= (1 << TOIE1); // Timer 1
  TIMSK0 |= (1 << TOIE1); // Timer 0
  sei();

  //Initialise the USB Serial connection
  usb_init();

  //Initialise ADC
  init_adc();

}

void init_adc(void){
  ADMUX |= 0b1 << 6;
  ADMUX |= 0b1 << 0;

  //Set prescaler to 128
  ADCSRA |= 0b1 << 0;
  ADCSRA |= 0b1 << 1;
  ADCSRA |= 0b1 << 2;
  ADCSRA |= 0b1 << 7;
}

void intro_screen(void){
  clear_screen();

  //Screen height and width
  int h = LCD_Y;

  draw_string(10, h / 2 - 24, "Alien Advance");
  draw_string(14, h / 2 - 12, "Mary Millar");
  draw_string(20, h / 2 - 4, "n9698337");
  draw_string(7, h / 2 + 8, "Press a button");
  draw_string(7, h / 2 + 16, "to continue...");

  show_screen();

  while (! (((PINF>>5) & 0b1) | ((PINF>>6) & 0b1)));

  if ((((PINF>>5) & 0b1) | ((PINF>>6) & 0b1))){
    while ((PINF>>5) & 0b1);
    while ((PINF>>6) & 0b1);
  }

  clear_screen();
}

void countdown(void){
  int w = LCD_X, h = LCD_Y;
  draw_string(w / 2 - 5, h / 2 - 5, "3");
  show_screen();

  _delay_ms(300);

  clear_screen();
  draw_string(w / 2 - 5, h / 2 - 5, "2");
  show_screen();

  _delay_ms(300);

  clear_screen();
  draw_string(w / 2 - 5, h / 2 - 5, "1");
  show_screen();

  _delay_ms(300);

  clear_screen();
}

void border(void){
  int w = LCD_X, h = LCD_Y;

  //Top, bottom, left, right
  draw_line(1, 10, w - 1 , 10);
  draw_line(1, h - 1, w - 1 , h - 1);
  draw_line(1, 10, 1, h - 1);
  draw_line(w - 1, 10, w - 1, h - 1);

  show_screen();
}

void status_display(void){
  char life_count[5];
  char timer_count[20];
  char score_count[5];
  char minutes[5];

  draw_string(1, 1, "L:");
  sprintf(life_count, "%d", lives);
  draw_string(10, 1, life_count);

  draw_string(20, 1, "S:");
  sprintf(score_count, "%d", score);
  draw_string(29, 1, score_count);

  draw_string(42, 1, "T:");
  sprintf(timer_count, "%d", get_game_time());
  draw_string(54, 1 , "0");
  sprintf(minutes, "%d", min);
  draw_string(59, 1, minutes);
  draw_string(64, 1, ":0");

  if (get_game_time() < 10){
    draw_string(75, 1, timer_count);
  }


  if (get_game_time() > 9){
    draw_string(65, 1, ":");
    draw_string(70, 1, timer_count);
  }

  if (get_game_time() > 59){
    ovf_count = 0;
    min++;
  }

  show_screen();
}

void ship_setup(void){
  int w = LCD_X - 3, h = LCD_Y - 3;

  int ship_xcor = rand() % w;
  int ship_ycor = rand() % h;

  bool setShip = false;

  for (int i = 0; i < MAX_A; i++){
    if (!setShip){
      if (ship_ycor < 15){
        ship_ycor = ship_ycor + 10;
      }

      else if (ship_xcor < 15){
        ship_xcor = ship_xcor + 10;
      }

      else if (ship_xcor == alien[i].x){
        ship_xcor = rand() % w;
      }

      else if (ship_ycor == alien[i].y){
        ship_ycor = rand() % h;
      }
    }
  }

  init_sprite(&ship, ship_xcor, ship_ycor, SHIP_WIDTH, SHIP_HEIGHT, ship_image);
  draw_sprite(&ship);

  //turret_setup();
}

void alien_setup(void){
  int w = LCD_X - 15, h = LCD_Y - 15;

  alienCount = MAX_A;

  for (int i = 0; i < MAX_A; i++){
    int alien_xcor = rand() % w;
    int alien_ycor = rand() % h;

    bool setAlien = false;

    while (!setAlien){
      if (alien_xcor == ship.x){
        alien_xcor = rand() % w;
        if (alien_ycor == ship.y){
          alien_ycor = rand() % h;
          if (alien_ycor < 15){
            alien_ycor = 20;
            if (alien_xcor < 15){
              alien_xcor = 20;
            }
          }
        }
      }
      setAlien = true;
      attack[i] = false;
    }

    init_sprite(&alien[i], alien_xcor + 12, alien_ycor + 12, ALIEN_WIDTH, ALIEN_HEIGHT, alien_image);
    draw_sprite(&alien[i]);
  }
}

void missile_setup(void){
  int mis_xcor = ship.x + SHIP_WIDTH / 2;
  int mis_ycor = ship.y - SHIP_HEIGHT / 2;

  for (int i = 0; i < MAX_M; i++){

    init_sprite(&missile[i], mis_xcor, mis_ycor, MISSILE_WIDTH, MISSILE_HEIGHT, missile_image);

    draw_sprite(&missile[i]);
    sprite_hide(&missile[i]);
  }

}

void mothership_setup(void){
  mothershipActive = true;
  bossHealth = 10;
  mothershipAttack = false;
  int w = LCD_X - 20, h = LCD_Y - 20;

  int ms_xcor = rand() % w;
  int ms_ycor = rand() % h;

  if (ms_xcor == ship.x){
    ms_xcor = rand() % w;
    if (ms_ycor == ship.y){
      ms_ycor = rand() % h;
      if (ms_ycor < 15){
        ms_ycor = 20;
        if (ms_xcor < 15){
          ms_xcor = 20;
        }
      }
    }
  }

  init_sprite(&mothership, ms_xcor + 12, ms_ycor + 12, MOTHERSHIP_WIDTH, MOTHERSHIP_HEIGHT, mothership_full);
  draw_sprite(&mothership);
  sprite_hide(&mothership);

  msMissile_setup();
}

void msMissile_setup(void){
  init_sprite(&msMissile, 30, 30, msMISSILE_WIDTH, msMISSILE_HEIGHT, msMissile_image);
  draw_sprite(&msMissile);
  sprite_hide(&msMissile);
}

void turret_setup(void){
  init_sprite(&turret, ship.x, ship.y, TURRET_WIDTH, TURRET_HEIGHT, turret_image);
}

void turret_calc(void){
  turret.dx = angle;
  turret.dy = angle;
}

void process(void){
  gametime = true;

  char key = usb_serial_getchar();

//Ship-related code
  int topWall = 11;
  int rightWall = LCD_X - 4;
  int bottomWall = LCD_Y - 4;
  int leftWall = 2;

  int sx = ship.x;
  int sy = ship.y;

  //Move up
  if ((((PIND>>1) & 0b1) || key == 'w') && sy > topWall){
    init_sprite(&ship, sx, sy - 1, SHIP_WIDTH, SHIP_HEIGHT, ship_image);
    draw_sprite(&ship);
    direction = "Up";
  }

  //Move right
  if ((((PIND>>0) & 0b1) || key == 'd') && sx < rightWall){
    init_sprite(&ship, sx + 1, sy, SHIP_WIDTH, SHIP_HEIGHT, ship_right);
    draw_sprite(&ship);
    direction = "Right";
  }

  //Move down
  if ((((PINB>>7) & 0b1) || key == 's') && sy < bottomWall){
    init_sprite(&ship, sx, sy + 1, SHIP_WIDTH, SHIP_HEIGHT, ship_down);
    draw_sprite(&ship);
    direction = "Down";
  }

  //Move left
  if ((((PINB>>1) & 0b1) || key == 'a') && sx > leftWall){
    init_sprite(&ship, sx - 1, sy, SHIP_WIDTH, SHIP_HEIGHT, ship_left);
    draw_sprite(&ship);
    direction = "Left";
  }

//Alien-related code
  for (int i = 0; i < MAX_A; i++){
    int ax = alien[i].x;
    int ay = alien[i].y;

    //Move towards player
    if (attack[i] == true){
      sprite_turn_to(&alien[i], alien[i].dx, alien[i].dy);
      sprite_step(&alien[i]);
      draw_sprite(&alien[i]);

      //Testing if the aliens hit the walls
      if (ay < topWall + 1 || ay >= bottomWall - 1 || ax >= rightWall - 1 || ax <= leftWall + 1){
        attack[i] = false;
      }
    }
  }

  for (int i = 0; i < MAX_A; i++){
    if (&alien[i].is_visible){
      if (!attack[i]){
        if (rand() % 30 == rand() % 30){
          sprite_step(&alien[i]);
          attack[i] = true;
          alien_attack(&alien[i]);
        }
      }
    }
  }

//Mothership-related code
  int msx = mothership.x;
  int msy = mothership.y;

  //Move towards player
  if (mothershipAttack == true){
    sprite_turn_to(&mothership, mothership.dx, mothership.dy);
    sprite_step(&mothership);
    draw_sprite(&mothership);

    //Testing if the mothership hit the walls
    if (msy == topWall + 1|| msy == bottomWall - 5 || msx == rightWall - 5 || msx == leftWall + 1){
      mothershipAttack = false;
    }

  }

  if (!mothershipAttack){
    if (rand() % 50 == rand() % 50){
      mothershipAttack = true;
      mothership_attack(&mothership);
    }
  }


  sprite_turn_to(&msMissile, msMissile.dx, msMissile.dy);
  sprite_step(&msMissile);

  if (mothershipActive == true){
    if (!msMissile.is_visible){
      if (rand() % 10 == rand() % 10){
        int mxcor = mothership.x + MOTHERSHIP_WIDTH / 2;
        int mycor = mothership.y + MOTHERSHIP_HEIGHT / 2;
        sprite_move_to(&msMissile, mxcor, mycor);
        msMissile_attack(&msMissile);
        sprite_show(&msMissile);
      }
    }
  }

  if (msMissile.y < 10 || msMissile.y > LCD_Y || msMissile.x < 1 || msMissile.x > LCD_X) {
    sprite_hide(&msMissile);
  }


//Missile-related code
  //Fire missile
  for (int i = 0; i < MAX_M; i++){
    if (((PINF>>5) & 0b1) || key == 'k'){
      _delay_ms(10);
      if (!(missile[i].is_visible)){
        sprite_show(&missile[i]);
        int xcor = ship.x + SHIP_WIDTH / 2;
        int ycor = ship.y + SHIP_HEIGHT / 2;
        sprite_move_to(&missile[i], xcor, ycor);
        break;
      }
    }
  }

  //Missile Direction
  for (int i = 0; i < MAX_M; i++){
    if (updateDirection[i] == true){
      if (direction == "Up"){
        sprite_turn_to(&missile[i], 0, -1.5);
        updateDirection[i] = false;
        }
      else if (direction == "Down"){
        sprite_turn_to(&missile[i], 0, +1.5);
        updateDirection[i] = false;
      }
      else if (direction == "Right"){
        sprite_turn_to(&missile[i], +1.5, 0);
        updateDirection[i] = false;
      }
      else if (direction == "Left"){
        sprite_turn_to(&missile[i], -1.5, 0);
        updateDirection[i] = false;
      }
    }
  }

  for (int i = 0; i < MAX_M; i++){
    sprite_step(&missile[i]);
    if (missile[i].y < 10 || missile[i].y > LCD_Y || missile[i].x < 1 || missile[i].x > LCD_X) {
      sprite_hide(&missile[i]);
    }

    if (missile[i].is_visible){
      updateDirection[i] = false;
    }
    else if (!(missile[i].is_visible)){
      updateDirection[i] = true;
    }
  }

  //Potentiometer
  char my_buffer[80];
    //Start conversion
  ADCSRA |= 0b1 << 6;
  while (ADCSRA & (0b1 << 6)); //Wait until it's finished
  adc_value = ADC; //Grab the value
  int angle = ADC / 1.42; //Convert to degrees

    //Convert value to string
  sprintf(my_buffer, "%d", angle);

  //Turn turret
  //turret_calc();
  //sprite_move_to(&turret, ship.x + 1, ship.y + 1);



  collision_checker();

  clear_screen();

  draw_sprite(&ship);

  // sprite_turn(&turret, 1);
  // sprite_step(&turret);
  // draw_sprite(&turret);

  for (int i = 0; i < MAX_A; i++){
    draw_sprite(&alien[i]);
  }

  for (int i = 0; i < MAX_M; i++){
    draw_sprite(&missile[i]);
  }

  if (mothershipActive == true){
    draw_sprite(&mothership);
    draw_sprite(&msMissile);
    sprite_show(&mothership);
  }

  //Display angle value
  draw_string(20, 20, my_buffer);

  border();
  status_display();
}

void alien_attack(Sprite* alien){
  alien->dx = ship.x - alien->x;
  alien->dy = ship.y - alien->y;

  dist = sqrt((alien->dx * alien->dx) + (alien->dy * alien->dy));

  alien->dx = alien->dx * 1.5 / dist;
  alien->dy = alien->dy * 1.5 / dist;
}

void mothership_attack(Sprite* mothership){
  mothership->dx = ship.x - mothership->x;
  mothership->dy = ship.y - mothership->y;

  dist = sqrt((mothership->dx * mothership->dx) + (mothership->dy * mothership->dy));

  mothership->dx = mothership->dx * 0.75 / dist;
  mothership->dy = mothership->dy * 0.75 / dist;
}

void msMissile_attack(Sprite* msMissile){
  msMissile->dx = ship.x - msMissile->x;
  msMissile->dy = ship.y - msMissile->y;

  dist = sqrt((msMissile->dx * msMissile->dx) + (msMissile->dy * msMissile->dy));

  msMissile->dx = msMissile->dx / dist;
  msMissile->dy = msMissile->dy / dist;
}

void collision_checker(){
  //Check if the alien and ship collide
  for (int i = 0; i < MAX_A; i++){
    if (alien[i].is_visible){
      if (collision_SnA(&alien[i])){
        for (int j = 0; j < MAX_M; j++){
          sprite_hide(&missile[j]);
        }
        if (lives > 1){
          ship_setup();
          _delay_ms(10);
          lives -= 1;
          char message[] = "An Alien has killed the Player.";
          send_debug_string(message);
        }
        else if (lives == 1){
          game_end();
        }
      }
    }
  }


  //Check if missile and alien collide
  for (int i = 0; i < MAX_A; i++){
    if (alien[i].is_visible){
      for (int j = 0; j < MAX_M; j++){
        if (missile[j].is_visible){
          if (collision_MnA(&missile[j], &alien[i])){
            char message[] = "The Player has killed an Alien.";
            send_debug_string(message);
            sprite_move_to(&missile[j], -100 * 10, -100 * 10);
            _delay_ms(10);
            score += 1;
            alienCount -= 1;
            sprite_hide(&missile[j]);
            sprite_hide(&alien[i]);
            if (alienCount == 0){
              mothership_setup();
            }
          }
        }
      }
    }
  }

  //Check if ship and mothership collide
  if (mothershipActive == true){

      if (collision_MSnS()){
        if (&mothership.is_visible){
          if (lives > 1){
            ship_setup();
            _delay_ms(10);
            lives -= 1;
            char message[] = "The Mothership has killed the Player.";
            send_debug_string(message);
          }
        }
        else if (lives == 1){
          game_end();
        }
      }


    //Check if missile and mothership collide
    for (int i = 0; i < MAX_M; i++){
      if (&mothership.is_visible){
        if (missile[i].is_visible){
          if (collision_MSnM(&missile[i])){
            sprite_move_to(&missile[i], -100 * 10, -100 * 10);
            sprite_hide(&missile[i]);
            _delay_ms(10);
            bossHealth -= 1;
            if (bossHealth == 8){
              sprite_set_image(&mothership, mothership_34);
              mothershipAttack == true;
            }
            if (bossHealth == 6){
              sprite_set_image(&mothership, mothership_half);
              mothershipAttack == true;
            }
            if (bossHealth == 4){
              sprite_set_image(&mothership, mothership_14);
              mothershipAttack == true;
            }
            if (bossHealth == 2){
              sprite_set_image(&mothership, mothership_dead);
              mothershipAttack == true;
            }
            if (bossHealth == 1){
              sprite_hide(&mothership);
              mothershipActive = false;
              score += 10;
              alien_setup();
            }
          }
        }
      }
    }

    //Check if ship and mothership's missle collide
    if (msMissile.is_visible){
      if (collision_MsMnS()){
        if (lives > 1){
          sprite_move_to(&msMissile, -100 * 10, -100 * 10);
          sprite_hide(&msMissile);
          ship_setup();
          _delay_ms(10);
          lives -= 1;
          char message[] = "The Mothership has killed the Player.";
          send_debug_string(message);
        }
      }
      else if (lives == 1){
        game_end();
      }
    }

  }

}

void game_end(){
  gametime = false;
  clear_screen();

  int h = LCD_Y;

  draw_string(17, h / 2 - 20, "The game");
  draw_string(14, h / 2 - 12, "has ended.");
  draw_string(7, h / 2 + 8, "Press a button");
  draw_string(7, h / 2 + 16, "to play again...");

  show_screen();

  while (! (((PINF>>5) & 0b1) | ((PINF>>6) & 0b1)));

  if ((((PINF>>5) & 0b1) | ((PINF>>6) & 0b1))){
    while ((PINF>>5) & 0b1);
    while ((PINF>>6) & 0b1);
  }
  lives = 6;
  score = 0;

  clear_screen();

  main();

}

bool collision_SnA(Sprite* alien){
  bool collided = true;

  int ship_top = round(ship.y);
  int ship_bottom = ship_top + SHIP_HEIGHT - 1;
  int ship_left = round(ship.x);
  int ship_right = ship_left + SHIP_WIDTH - 1;

  int alien_top = round(alien->y);
  int alien_bottom = alien_top + ALIEN_HEIGHT - 1;
  int alien_left = round(alien->x);
  int alien_right = alien_left + ALIEN_WIDTH - 1;

  if (alien_bottom < ship_top) collided = false;
  else if (alien_top > ship_bottom) collided = false;
  else if (alien_right < ship_left) collided = false;
  else if (alien_left > ship_right) collided = false;

  return collided;

}

bool collision_MnA(Sprite* missile, Sprite* alien){
  bool collided = true;

  int missile_top = round(missile->y);
  int missile_bottom = missile_top + MISSILE_HEIGHT;
  int missile_left = round(missile->x);
  int missile_right = missile_left + MISSILE_WIDTH;

  int alien_top = round(alien->y);
  int alien_bottom = alien_top + ALIEN_HEIGHT - 1;
  int alien_left = round(alien->x);
  int alien_right = alien_left + ALIEN_WIDTH - 1;

  if (alien_bottom < missile_top) collided = false;
  else if (alien_top > missile_bottom) collided = false;
  else if (alien_right < missile_left) collided = false;
  else if (alien_left > missile_right) collided = false;

  return collided;
}

bool collision_MSnS(void){
  bool collided = true;

  int ship_top = round(ship.y);
  int ship_bottom = ship_top + SHIP_HEIGHT - 1;
  int ship_left = round(ship.x);
  int ship_right = ship_left + SHIP_WIDTH - 1;

  int mothership_top = round(mothership.y);
  int mothership_bottom = mothership_top + MOTHERSHIP_HEIGHT - 1;
  int mothership_left = round(mothership.x);
  int mothership_right = mothership_left + MOTHERSHIP_WIDTH - 1;

  if (mothership_bottom < ship_top) collided = false;
  else if (mothership_top > ship_bottom) collided = false;
  else if (mothership_right < ship_left) collided = false;
  else if (mothership_left > ship_right) collided = false;

  return collided;
}

bool collision_MSnM(Sprite* missile){
  bool collided = true;

  int missile_top = round(missile->y);
  int missile_bottom = missile_top + MISSILE_HEIGHT;
  int missile_left = round(missile->x);
  int missile_right = missile_left + MISSILE_WIDTH;

  int mothership_top = round(mothership.y);
  int mothership_bottom = mothership_top + MOTHERSHIP_HEIGHT - 1;
  int mothership_left = round(mothership.x);
  int mothership_right = mothership_left + MOTHERSHIP_WIDTH - 1;

  if (mothership_bottom < missile_top) collided = false;
  else if (mothership_top > missile_bottom) collided = false;
  else if (mothership_right < missile_left) collided = false;
  else if (mothership_left > missile_right) collided = false;

  return collided;
}

bool collision_MsMnS(void){
  bool collided = true;

  int ship_top = round(ship.y);
  int ship_bottom = ship_top + SHIP_HEIGHT - 1;
  int ship_left = round(ship.x);
  int ship_right = ship_left + SHIP_WIDTH - 1;

  int msMissile_top = round(msMissile.y);
  int msMissile_bottom = msMissile_top + msMISSILE_HEIGHT - 1;
  int msMissile_left = round(msMissile.x);
  int msMissile_right = msMissile_left + msMISSILE_WIDTH - 1;

  if (msMissile_bottom < ship_top) collided = false;
  else if (msMissile_top > ship_bottom) collided = false;
  else if (msMissile_right < ship_left) collided = false;
  else if (msMissile_left > ship_right) collided = false;

  return collided;
}

void alien_spawn(Sprite* alien){
  int w = LCD_X - 5, h = LCD_Y - 5;

  int alien_xcor = rand() % w;
  int alien_ycor = rand() % h;

  bool setAlien = false;

  while (!setAlien){
    if (alien_xcor == ship.x){
      alien_xcor = rand() % w;
    }

    else if (alien_ycor == ship.y){
      alien_ycor = rand() % h;
    }

    else if (alien_ycor < 15){
      alien_ycor = 20;
    }

    else if (alien_xcor < 15){
      alien_xcor = 20;
    }

    setAlien = true;
  }
  sprite_move_to(alien, alien_xcor, alien_ycor);
}

void ship_info(int x_pos, int y_pos, char* direction){
  char output[100];
  sprintf(output, "The ship is now at (%d, %d), and aiming %s.", x_pos, y_pos, direction);
  send_debug_string(output);
}


//SPRITE FUNCTIONS
  //Note: These function were retrieved from the cab202_sprites.c file
  //      and were written by Lawrence Buckingham and Ben Talbot.

void sprite_turn_to(Sprite* sprite, double dx, double dy) {
	sprite->dx = dx;
	sprite->dy = dy;
}

bool sprite_step(Sprite* sprite) {
	int x0 = round( sprite->x );
	int y0 = round( sprite->y );
	sprite->x += sprite->dx;
	sprite->y += sprite->dy;
	int x1 = round( sprite->x );
	int y1 = round( sprite->y );
	return ( x1 != x0 ) || ( y1 != y0 );
}

bool sprite_show(Sprite* sprite) {
	bool old_val = sprite->is_visible;
	sprite->is_visible = true;
	return ! old_val;
}

bool sprite_hide(Sprite* sprite) {
	bool old_val = sprite->is_visible;
	sprite->is_visible = false;
	return old_val;
}

bool sprite_move_to(Sprite* sprite, double x, double y ) {
	int x0 = round( sprite->x );
	int y0 = round( sprite->y );
	sprite->x = x;
	sprite->y = y;
	int x1 = round( sprite->x );
	int y1 = round( sprite->y );
	return ( x1 != x0 ) || ( y1 != y0 );
}

void sprite_set_image(Sprite* sprite, char image[]) {
	sprite->bitmap = image;
}

void sprite_turn(Sprite* sprite, double degrees) {
	double radians = degrees * M_PI / 180;
	double s = sin( radians );
	double c = cos( radians );
	double dx = c * sprite->dx + s * sprite->dy;
	double dy = -s * sprite->dx + c * sprite->dy;
	sprite->dx = dx;
	sprite->dy = dy;
}

//TIMER FUNCTIONS
int get_game_time(void) {
    return (ovf_count * 65536 + TCNT1) * PRESCALER / FREQUENCY;
}

double get_system_time(void) {
    return (overflow * 65536 + TCNT1) * PRESCALER / FREQUENCY;
}

//DEBUGGER FUNCTIONS
void check_debugger(){
  draw_centred(17, "Waiting for");
  draw_centred(24, "debugger...");
  show_screen();
  send_line("Waiting for usb connection...");
  while(!usb_configured() || !usb_serial_get_control());
  clear_screen();

  //Teensy is successfully connected
  draw_centred(17, "USB connected.");
  send_line("The usb has been connected.");
  send_line("Use W to move up.");
  send_line("Use A to move left.");
  send_line("Use S to move down.");
  send_line("Use D to move right.");
  send_line("Use K to shoot.");
  show_screen();
  _delay_ms(3000);
  clear_screen();

  usb_connected = true;
}

void send_debug_string(char string[]) {
    char debugger[50];
    sprintf(debugger, "[DBG @ %3.3f] ", get_system_time());

     // Send the debug preamble...
     usb_serial_write(debugger, 15);

     // Send all of the characters in the string
     unsigned char char_count = 0;
     while (*string != '\0') {
         usb_serial_putchar(*string);
         string++;
         char_count++;
     }

     // Go to a new line (force this to be the start of the line)
     usb_serial_putchar('\r');
     usb_serial_putchar('\n');
 }

 void send_line(char* string) {
     // Send all of the characters in the string
     unsigned char char_count = 0;
     while (*string != '\0') {
         usb_serial_putchar(*string);
         string++;
         char_count++;
     }

     // Go to a new line (force this to be the start of the line)
     usb_serial_putchar('\r');
     usb_serial_putchar('\n');
 }



//HELPER FUNCTIONS
void draw_centred(unsigned char y, char* string) {
    // Draw a string centred in the LCD when you don't know the string length
    unsigned char l = 0, i = 0;
    while (string[i] != '\0') {
        l++;
        i++;
    }
    char x = 42-(l*5/2);
    draw_string((x > 0) ? x : 0, y, string);
}

/*
* Interrupt service routines
*/
ISR(TIMER1_OVF_vect) {
    ovf_count++;
    overflow++;
}

ISR(TIMER0_OVF_vect) {
  if (gametime == true){
    debugCounter++;
    if (debugCounter == 15){
      ship_info(ship.x, ship.y, direction);
      debugCounter = 0;
    }
  }
}
