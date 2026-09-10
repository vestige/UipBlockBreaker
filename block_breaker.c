#include "ch32fun.h"
#include <stdint.h>

#define W 128u
#define H 64u
#define SDA 1u
#define SCL 2u
#define LED PC0
#define S1 PC5
#define S2 PD0
#define KEY PC3
#define ENCODER_DIRECTION 1
#define MOTOR 4u
#define BALL_SIZE 3u
#define BALL_INTERVAL_MS 35u
#define PADDLE_STEP 4u
#define PADDLE_MOTION_MS 120u
#define FIREWORK_INTERVAL_MS 180u
#define PADDLE_W 22u
#define PADDLE_Y 57u
#define BRICK_COLS 16u
#define BRICK_ROWS 3u
#define BRICK_W 7u
#define BRICK_H 5u
#define BRICK_STEP_X 8u
#define BRICK_STEP_Y 7u

typedef enum { STATE_READY, STATE_PLAYING, STATE_GAME_OVER, STATE_GAME_CLEAR } state_t;

static uint8_t fb[1024], dirty, dirty_first[8], dirty_last[8];
static uint8_t bricks[BRICK_ROWS][BRICK_COLS];
static uint8_t paddle_x, bricks_left;
static uint8_t paddle_sweep_left, paddle_sweep_right;
static int8_t ball_x, ball_y, ball_dx, ball_dy;
static uint8_t ball_x_accumulator, ball_x_rate;
static int8_t paddle_motion;
static uint32_t paddle_motion_deadline, firework_deadline;
static uint8_t firework_frame;
static state_t state;
static uint32_t motor_deadline;
static uint8_t motor_running;
static const int8_t transitions[16] = {
    0,-1,1,0, 1,0,0,-1, -1,0,0,1, 0,1,-1,0
};

static void bus_delay(void) { Delay_Us(10); }
static void sda_hi(void) { GPIOC->BSHR = 1u << SDA; }
static void sda_lo(void) { GPIOC->BCR = 1u << SDA; }
static void scl_hi(void) { GPIOC->BSHR = 1u << SCL; }
static void scl_lo(void) { GPIOC->BCR = 1u << SCL; }

static void bus_init(void)
{
    uint32_t mask = (0xfu << (SDA * 4u)) | (0xfu << (SCL * 4u));
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOC;
    GPIOC->CFGLR &= ~mask;
    GPIOC->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_OD) << (SDA * 4u);
    GPIOC->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_OD) << (SCL * 4u);
    sda_hi(); scl_hi();
}

static void bus_start(void)
{
    sda_hi(); scl_hi(); bus_delay(); sda_lo(); bus_delay(); scl_lo();
}

static void bus_stop(void)
{
    sda_lo(); bus_delay(); scl_hi(); bus_delay(); sda_hi(); bus_delay();
}

static void bus_recover(void)
{
    uint8_t i;
    sda_hi();
    for (i = 0; i < 9u; ++i) { scl_lo(); bus_delay(); scl_hi(); bus_delay(); }
    bus_stop();
}

static uint8_t bus_write(uint8_t value)
{
    uint8_t mask, ack;
    for (mask = 0x80u; mask; mask >>= 1u) {
        if (value & mask) sda_hi(); else sda_lo();
        bus_delay(); scl_hi(); bus_delay(); scl_lo();
    }
    sda_hi(); bus_delay(); scl_hi(); bus_delay();
    ack = (GPIOC->INDR & (1u << SDA)) ? 0u : 1u;
    scl_lo();
    return ack;
}

static uint8_t oled_begin(uint8_t control)
{
    bus_start();
    if (!bus_write(0x78u) || !bus_write(control)) { bus_stop(); return 0u; }
    return 1u;
}

static uint8_t oled_command(uint8_t value)
{
    if (!oled_begin(0u)) return 0u;
    if (!bus_write(value)) { bus_stop(); return 0u; }
    bus_stop();
    return 1u;
}

static uint8_t oled_init(void)
{
    static const uint8_t commands[] = {
        0xae,0x20,0x00,0xb0,0xc8,0x00,0x10,0x40,
        0x81,0x7f,0xa1,0xa6,0xa8,0x3f,0xa4,0xd3,
        0x00,0xd5,0x80,0xd9,0xf1,0xda,0x12,0xdb,
        0x40,0x8d,0x14,0xaf
    };
    uint8_t i;
    for (i = 0; i < sizeof(commands); ++i)
        if (!oled_command(commands[i])) return 0u;
    return 1u;
}

static uint8_t oled_flush(void)
{
    uint8_t page, x, first, last;
    for (page = 0; page < 8u; ++page) {
        if (!(dirty & (1u << page))) continue;
        first = dirty_first[page];
        last = dirty_last[page];
        if (!oled_command(0x21u) || !oled_command(first) ||
            !oled_command(last) || !oled_command(0x22u) ||
            !oled_command(page) || !oled_command(page) || !oled_begin(0x40u))
            return 0u;
        for (x = first; x <= last; ++x) {
            if (!bus_write(fb[(uint16_t)page * W + x])) {
                bus_stop(); return 0u;
            }
        }
        bus_stop();
        dirty &= (uint8_t)~(1u << page);
        dirty_first[page] = W - 1u;
        dirty_last[page] = 0u;
    }
    return 1u;
}

static void pixel(uint8_t x, uint8_t y, uint8_t on)
{
    uint16_t i;
    uint8_t mask, page;
    if (x >= W || y >= H) return;
    i = (uint16_t)(y >> 3) * W + x;
    mask = (uint8_t)(1u << (y & 7u));
    if (on) fb[i] |= mask; else fb[i] &= (uint8_t)~mask;
    page = y >> 3;
    if (!(dirty & (1u << page))) {
        dirty_first[page] = x;
        dirty_last[page] = x;
    } else {
        if (x < dirty_first[page]) dirty_first[page] = x;
        if (x > dirty_last[page]) dirty_last[page] = x;
    }
    dirty |= (uint8_t)(1u << page);
}

static void rect(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t on)
{
    uint8_t px, py;
    for (py = y; py < (uint8_t)(y + height); ++py)
        for (px = x; px < (uint8_t)(x + width); ++px) pixel(px, py, on);
}

static void clear_screen(void)
{
    uint16_t i;
    uint8_t page;
    for (i = 0; i < sizeof(fb); ++i) fb[i] = 0u;
    for (page = 0; page < 8u; ++page) {
        dirty_first[page] = 0u;
        dirty_last[page] = W - 1u;
    }
    dirty = 0xffu;
}

static const uint8_t *glyph(char c)
{
    static const uint8_t z[5]={0,0,0,0,0};
    static const uint8_t a[5]={0x7e,9,9,9,0x7e};
    static const uint8_t cc[5]={0x3e,0x41,0x41,0x41,0x22};
    static const uint8_t e[5]={0x7f,0x49,0x49,0x49,0x41};
    static const uint8_t g[5]={0x3e,0x41,0x49,0x49,0x7a};
    static const uint8_t l[5]={0x7f,0x40,0x40,0x40,0x40};
    static const uint8_t m[5]={0x7f,2,0x0c,2,0x7f};
    static const uint8_t o[5]={0x3e,0x41,0x41,0x41,0x3e};
    static const uint8_t r[5]={0x7f,9,0x19,0x29,0x46};
    static const uint8_t v[5]={0x1f,0x20,0x40,0x20,0x1f};
    switch (c) {
    case 'A': return a; case 'C': return cc; case 'E': return e;
    case 'G': return g; case 'L': return l; case 'M': return m;
    case 'O': return o; case 'R': return r; case 'V': return v;
    default: return z;
    }
}

static void text(const char *s, uint8_t x, uint8_t y)
{
    uint8_t col, row;
    while (*s) {
        const uint8_t *data = glyph(*s++);
        for (col = 0; col < 5u; ++col)
            for (row = 0; row < 7u; ++row)
                if (data[col] & (1u << row)) pixel(x + col, y + row, 1u);
        x = (uint8_t)(x + 6u);
    }
}

static void motor_set(uint8_t level)
{
    if (!level) {
        TIM1->CCER &= ~(1u << 12); TIM1->CH4CVR = 0u;
        GPIOC->BCR = 1u << MOTOR;
        GPIOC->CFGLR &= ~(0xfu << (MOTOR * 4u));
        GPIOC->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_PP) << (MOTOR * 4u);
        return;
    }
    TIM1->CH4CVR = (uint16_t)(((uint32_t)level * 256u + 50u) / 100u);
    TIM1->SWEVGR = TIM_UG;
    GPIOC->CFGLR &= ~(0xfu << (MOTOR * 4u));
    GPIOC->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_PP_AF) << (MOTOR * 4u);
    TIM1->CCER = (TIM1->CCER & ~(1u << 13)) | (1u << 12);
}

static void motor_init(void)
{
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_TIM1;
    GPIOC->CFGLR &= ~(0xfu << (MOTOR * 4u));
    GPIOC->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_PP) << (MOTOR * 4u);
    GPIOC->BCR = 1u << MOTOR;
    RCC->APB2PRSTR |= RCC_APB2Periph_TIM1;
    RCC->APB2PRSTR &= ~RCC_APB2Periph_TIM1;
    TIM1->PSC = 374u; TIM1->ATRLR = 255u; TIM1->CH4CVR = 0u;
    TIM1->CHCTLR2 = (TIM1->CHCTLR2 & ~0xff00u) | 0x6800u;
    TIM1->CCER &= ~0x3000u; TIM1->BDTR |= TIM_MOE;
    TIM1->SWEVGR = TIM_UG; TIM1->CTLR1 |= TIM_CEN;
    motor_running = 0u; motor_set(0u);
}

static void vibrate(uint8_t level, uint16_t ms)
{
    motor_set(level);
    motor_deadline = (uint32_t)SysTick->CNT + (uint32_t)ms * DELAY_MS_TIME;
    motor_running = 1u;
}

static void motor_tick(void)
{
    if (motor_running &&
        (int32_t)((uint32_t)SysTick->CNT - motor_deadline) >= 0) {
        motor_running = 0u;
        motor_set(0u);
    }
}

static void paddle(uint8_t on) { rect(paddle_x, PADDLE_Y, PADDLE_W, 2u, on); }
static void ball(uint8_t on)
{
    if (ball_x >= 0 && ball_y >= 0)
        rect((uint8_t)ball_x, (uint8_t)ball_y, BALL_SIZE, BALL_SIZE, on);
}

static void reset_game(void)
{
    uint8_t row, col;
    clear_screen();
    bricks_left = BRICK_ROWS * BRICK_COLS;
    for (row = 0; row < BRICK_ROWS; ++row)
        for (col = 0; col < BRICK_COLS; ++col) {
            bricks[row][col] = 1u;
            rect((uint8_t)(col * BRICK_STEP_X),
                 (uint8_t)(row * BRICK_STEP_Y + 1u), BRICK_W, BRICK_H, 1u);
        }
    paddle_x = (W - PADDLE_W) / 2u;
    paddle_sweep_left = paddle_x;
    paddle_sweep_right = (uint8_t)(paddle_x + PADDLE_W);
    ball_x = (int8_t)(paddle_x + (PADDLE_W - BALL_SIZE) / 2u);
    ball_y = PADDLE_Y - BALL_SIZE;
    ball_dx = 0; ball_dy = 0; ball_x_accumulator = 0u; ball_x_rate = 3u;
    paddle_motion = 0;
    paddle(1u); ball(1u);
    motor_running = 0u; motor_set(0u); state = STATE_READY;
}

static void move_paddle(int8_t direction)
{
    uint8_t next = paddle_x;
    uint8_t previous = paddle_x;
    if (direction > 0 && paddle_x < W - PADDLE_W)
        next = paddle_x > W - PADDLE_W - PADDLE_STEP ?
               W - PADDLE_W : paddle_x + PADDLE_STEP;
    else if (direction < 0 && paddle_x)
        next = paddle_x < PADDLE_STEP ? 0u : paddle_x - PADDLE_STEP;
    if (next == paddle_x) return;
    paddle(0u); if (state == STATE_READY) ball(0u);
    paddle_x = next;
    if (previous < paddle_sweep_left) paddle_sweep_left = previous;
    if (paddle_x < paddle_sweep_left) paddle_sweep_left = paddle_x;
    if ((uint8_t)(previous + PADDLE_W) > paddle_sweep_right)
        paddle_sweep_right = (uint8_t)(previous + PADDLE_W);
    if ((uint8_t)(paddle_x + PADDLE_W) > paddle_sweep_right)
        paddle_sweep_right = (uint8_t)(paddle_x + PADDLE_W);
    paddle_motion = direction;
    paddle_motion_deadline = (uint32_t)SysTick->CNT +
                             PADDLE_MOTION_MS * DELAY_MS_TIME;
    if (state == STATE_READY)
        ball_x = (int8_t)(paddle_x + (PADDLE_W - BALL_SIZE) / 2u);
    paddle(1u); if (state == STATE_READY) ball(1u);
}

static uint8_t find_brick(int8_t x, int8_t y, uint8_t *row, uint8_t *col)
{
    uint8_t r, c, bx, by;
    for (r = 0; r < BRICK_ROWS; ++r) {
        by = (uint8_t)(r * BRICK_STEP_Y + 1u);
        if (y + (int8_t)BALL_SIZE <= (int8_t)by ||
            y >= (int8_t)(by + BRICK_H)) continue;
        for (c = 0; c < BRICK_COLS; ++c) {
            bx = (uint8_t)(c * BRICK_STEP_X);
            if (bricks[r][c] && x + (int8_t)BALL_SIZE > (int8_t)bx &&
                x < (int8_t)(bx + BRICK_W)) {
                *row = r; *col = c; return 1u;
            }
        }
    }
    return 0u;
}

static void end_screen(const char *message, uint8_t x, state_t next)
{
    clear_screen(); text(message, x, 28u); state = next;
}

static void draw_fireworks(uint8_t frame, uint8_t on)
{
    static const uint8_t centers[4] = {20u, 15u, 107u, 47u};
    uint8_t burst;
    uint8_t radius = (uint8_t)(frame * 2u);
    for (burst = 0; burst < 2u; ++burst) {
        uint8_t cx = centers[burst * 2u];
        uint8_t cy = centers[burst * 2u + 1u];
        pixel(cx, (uint8_t)(cy - radius), on);
        pixel(cx, (uint8_t)(cy + radius), on);
        pixel((uint8_t)(cx - radius), cy, on);
        pixel((uint8_t)(cx + radius), cy, on);
        pixel((uint8_t)(cx - frame), (uint8_t)(cy - frame), on);
        pixel((uint8_t)(cx + frame), (uint8_t)(cy - frame), on);
        pixel((uint8_t)(cx - frame), (uint8_t)(cy + frame), on);
        pixel((uint8_t)(cx + frame), (uint8_t)(cy + frame), on);
    }
}

static void start_game_clear(void)
{
    clear_screen();
    text("GAME CLEAR", 34u, 28u);
    firework_frame = 1u;
    draw_fireworks(firework_frame, 1u);
    firework_deadline = (uint32_t)SysTick->CNT +
                        FIREWORK_INTERVAL_MS * DELAY_MS_TIME;
    state = STATE_GAME_CLEAR;
}

static void update_fireworks(void)
{
    draw_fireworks(firework_frame, 0u);
    firework_frame = firework_frame >= 4u ? 1u : (uint8_t)(firework_frame + 1u);
    draw_fireworks(firework_frame, 1u);
    firework_deadline = (uint32_t)SysTick->CNT +
                        FIREWORK_INTERVAL_MS * DELAY_MS_TIME;
}

static void update_ball(void)
{
    int8_t nx = ball_x;
    int8_t ny = (int8_t)(ball_y + ball_dy);
    uint8_t row, col;

    /* Three horizontal pixels per five vertical pixels is about 59 degrees. */
    ball_x_accumulator = (uint8_t)(ball_x_accumulator + ball_x_rate);
    if (ball_x_accumulator >= 5u) {
        ball_x_accumulator = (uint8_t)(ball_x_accumulator - 5u);
        nx = (int8_t)(ball_x + ball_dx);
    }
    ball(0u);
    if (nx < 0 || nx > (int8_t)(W - BALL_SIZE)) {
        ball_dx = (int8_t)-ball_dx; nx = (int8_t)(ball_x + ball_dx);
    }
    if (ny < 0) { ball_dy = 1; ny = (int8_t)(ball_y + 1); }
    if (ball_dy > 0 && ny + (int8_t)BALL_SIZE > (int8_t)PADDLE_Y &&
        ball_y + (int8_t)BALL_SIZE <= (int8_t)PADDLE_Y &&
        (uint8_t)(nx + (int8_t)BALL_SIZE) > paddle_sweep_left &&
        (uint8_t)nx < paddle_sweep_right) {
        ball_dy = -1;
        if (paddle_motion) {
            ball_dx = paddle_motion;
            ball_x_rate = 4u;
        } else {
            if (nx + (int8_t)(BALL_SIZE / 2u) <
                (int8_t)(paddle_x + PADDLE_W / 2u)) ball_dx = -1;
            else ball_dx = 1;
            ball_x_rate = 3u;
        }
        ball_x_accumulator = 0u;
        ny = (int8_t)(PADDLE_Y - BALL_SIZE);
    }
    if (find_brick(nx, ny, &row, &col)) {
        bricks[row][col] = 0u;
        rect((uint8_t)(col * BRICK_STEP_X),
             (uint8_t)(row * BRICK_STEP_Y + 1u), BRICK_W, BRICK_H, 0u);
        --bricks_left; ball_dy = (int8_t)-ball_dy;
        ny = (int8_t)(ball_y + ball_dy); vibrate(50u, 30u);
        if (!bricks_left) { start_game_clear(); return; }
    }
    ball_x = nx; ball_y = ny;
    paddle_sweep_left = paddle_x;
    paddle_sweep_right = (uint8_t)(paddle_x + PADDLE_W);
    if (ball_y >= (int8_t)H) {
        end_screen("GAME OVER", 37u, STATE_GAME_OVER); vibrate(80u, 1500u); return;
    }
    ball(1u);
}

static void fail(void)
{
    motor_set(0u);
    for (;;) {
        funDigitalWrite(LED, FUN_HIGH); Delay_Ms(150);
        funDigitalWrite(LED, FUN_LOW); Delay_Ms(850);
    }
}

int main(void)
{
    uint8_t encoder_old, key_old = 1u, key_count = 0u;
    uint32_t ball_deadline = 0u;
    int8_t quarter = 0;
    SystemInit(); funGpioInitAll();
    funPinMode(LED, GPIO_CFGLR_OUT_10Mhz_PP); funDigitalWrite(LED, FUN_LOW);
    funPinMode(S1, GPIO_CFGLR_IN_PUPD); funDigitalWrite(S1, FUN_HIGH);
    funPinMode(S2, GPIO_CFGLR_IN_PUPD); funDigitalWrite(S2, FUN_HIGH);
    funPinMode(KEY, GPIO_CFGLR_IN_PUPD); funDigitalWrite(KEY, FUN_HIGH);
    motor_init(); bus_init(); Delay_Ms(1000); bus_recover();
    if (!oled_init()) fail();
    reset_game(); if (!oled_flush()) fail(); funDigitalWrite(LED, FUN_HIGH);
    encoder_old = (uint8_t)((funDigitalRead(S1) << 1) | funDigitalRead(S2));
    for (;;) {
        uint8_t encoder = (uint8_t)((funDigitalRead(S1) << 1) | funDigitalRead(S2));
        uint8_t key = (uint8_t)funDigitalRead(KEY), pressed = 0u;
        if (encoder != encoder_old) {
            quarter = (int8_t)(quarter + transitions[(encoder_old << 2) | encoder]);
            encoder_old = encoder;
            if (quarter >= 4 || quarter <= -4) {
                int8_t direction = quarter >= 4 ? ENCODER_DIRECTION : -ENCODER_DIRECTION;
                quarter = 0;
                if (state == STATE_READY || state == STATE_PLAYING) move_paddle(direction);
            }
        }
        if (key == key_old) { if (key_count < 20u) ++key_count; }
        else { key_old = key; key_count = 0u; }
        if (key_count == 20u && !key) { pressed = 1u; key_count = 21u; }
        if (pressed && state == STATE_READY) {
            ball_dx = paddle_x < (W - PADDLE_W) / 2u ? 1 : -1;
            ball_dy = -1; ball_x_accumulator = 0u; ball_x_rate = 3u;
            state = STATE_PLAYING;
            ball_deadline = (uint32_t)SysTick->CNT + BALL_INTERVAL_MS * DELAY_MS_TIME;
        } else if (pressed && (state == STATE_GAME_OVER || state == STATE_GAME_CLEAR)) {
            reset_game();
        }
        if (state == STATE_PLAYING &&
            (int32_t)((uint32_t)SysTick->CNT - ball_deadline) >= 0) {
            ball_deadline = (uint32_t)SysTick->CNT + BALL_INTERVAL_MS * DELAY_MS_TIME;
            update_ball();
        }
        if (paddle_motion &&
            (int32_t)((uint32_t)SysTick->CNT - paddle_motion_deadline) >= 0)
            paddle_motion = 0;
        if (state == STATE_GAME_CLEAR &&
            (int32_t)((uint32_t)SysTick->CNT - firework_deadline) >= 0)
            update_fireworks();
        motor_tick();
        if (dirty && !oled_flush()) fail();
        Delay_Ms(1);
    }
}
