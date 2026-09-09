#include "ch32fun.h"

#include <stdint.h>

/* OLED-only reference test: SDA=D3/PC1, SCL=D4/PC2, VDD=3.3V. */
#define SDA_PIN 1u
#define SCL_PIN 2u
#define LED_PIN PC0
#define I2C_DELAY_US 10u
#define OLED_ADDRESS 0x3cu

#ifdef OLED_ROTARY_TEST
#define ENCODER_S1_PIN PC5
#define ENCODER_S2_PIN PD0
#define ENCODER_KEY_PIN PC3
#define ENCODER_DIRECTION 1
#define HELLO_WIDTH 30u
#define HELLO_MAX_X (128u - HELLO_WIDTH)
#define HELLO_START_X 20u
#define MOTOR_PWM_PERIOD 256u
#define MOTOR_PWM_PRESCALER 374u
#define EDGE_HAPTIC_LEVEL 50u
#define EDGE_HAPTIC_MS 80u

static const int8_t transition_table[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
};

static uint16_t motor_ms_remaining;
#endif

static void delay_i2c(void) { Delay_Us(I2C_DELAY_US); }
static void sda_high(void) { GPIOC->BSHR = 1u << SDA_PIN; }
static void sda_low(void) { GPIOC->BCR = 1u << SDA_PIN; }
static void scl_high(void) { GPIOC->BSHR = 1u << SCL_PIN; }
static void scl_low(void) { GPIOC->BCR = 1u << SCL_PIN; }

static void i2c_gpio_init(void)
{
    uint32_t mask = (0x0fu << (SDA_PIN * 4u)) | (0x0fu << (SCL_PIN * 4u));

    RCC->APB2PCENR |= RCC_APB2Periph_GPIOC;
    GPIOC->CFGLR &= ~mask;
    GPIOC->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_OD) << (SDA_PIN * 4u);
    GPIOC->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_OD) << (SCL_PIN * 4u);
    sda_high();
    scl_high();
}

static void i2c_start(void)
{
    sda_high();
    scl_high();
    delay_i2c();
    sda_low();
    delay_i2c();
    scl_low();
}

static void i2c_stop(void)
{
    sda_low();
    delay_i2c();
    scl_high();
    delay_i2c();
    sda_high();
    delay_i2c();
}

static void i2c_recover(void)
{
    uint8_t i;

    sda_high();
    for (i = 0u; i < 9u; ++i) {
        scl_low();
        delay_i2c();
        scl_high();
        delay_i2c();
    }
    i2c_stop();
}

static uint8_t i2c_write(uint8_t value)
{
    uint8_t mask;
    uint8_t acknowledged;

    for (mask = 0x80u; mask != 0u; mask >>= 1u) {
        if (value & mask) sda_high(); else sda_low();
        delay_i2c();
        scl_high();
        delay_i2c();
        scl_low();
    }
    sda_high();
    delay_i2c();
    scl_high();
    delay_i2c();
    acknowledged = (GPIOC->INDR & (1u << SDA_PIN)) ? 0u : 1u;
    scl_low();
    return acknowledged;
}

static uint8_t begin_write(uint8_t control)
{
    i2c_start();
    if (!i2c_write((uint8_t)(OLED_ADDRESS << 1)) || !i2c_write(control)) {
        i2c_stop();
        return 0u;
    }
    return 1u;
}

static uint8_t command(uint8_t value)
{
    if (!begin_write(0x00u)) return 0u;
    if (!i2c_write(value)) {
        i2c_stop();
        return 0u;
    }
    i2c_stop();
    return 1u;
}

static uint8_t initialize_display(void)
{
    static const uint8_t commands[] = {
        0xae, 0x20, 0x00, 0xb0, 0xc8, 0x00, 0x10, 0x40,
        0x81, 0x7f, 0xa1, 0xa6, 0xa8, 0x3f, 0xa4, 0xd3,
        0x00, 0xd5, 0x80, 0xd9, 0xf1, 0xda, 0x12, 0xdb,
        0x40, 0x8d, 0x14, 0xaf
    };
    uint8_t i;

    for (i = 0u; i < sizeof(commands); ++i) {
        if (!command(commands[i])) return 0u;
    }
    return 1u;
}

static uint8_t set_window(uint8_t first_column, uint8_t last_column,
                          uint8_t first_page, uint8_t last_page)
{
    return command(0x21u) && command(first_column) && command(last_column) &&
           command(0x22u) && command(first_page) && command(last_page);
}

static uint8_t clear_display(void)
{
    uint8_t block;
    uint8_t i;

    if (!set_window(0u, 127u, 0u, 7u)) return 0u;
    for (block = 0u; block < 16u; ++block) {
        if (!begin_write(0x40u)) return 0u;
        for (i = 0u; i < 64u; ++i) {
            if (!i2c_write(0u)) {
                i2c_stop();
                return 0u;
            }
        }
        i2c_stop();
    }
    return 1u;
}

static uint8_t write_data(const uint8_t *data, uint8_t length)
{
    uint8_t i;

    if (!begin_write(0x40u)) return 0u;
    for (i = 0u; i < length; ++i) {
        if (!i2c_write(data[i])) {
            i2c_stop();
            return 0u;
        }
    }
    i2c_stop();
    return 1u;
}

static uint8_t draw_hello_at(uint8_t x)
{
    static const uint8_t glyphs[][6] = {
        {0x7f, 0x08, 0x08, 0x08, 0x7f, 0x00},
        {0x38, 0x54, 0x54, 0x54, 0x18, 0x00},
        {0x00, 0x41, 0x7f, 0x40, 0x00, 0x00},
        {0x00, 0x41, 0x7f, 0x40, 0x00, 0x00},
        {0x38, 0x44, 0x44, 0x44, 0x38, 0x00}
    };
    uint8_t i;

    /* Match the working Pico W script: selected x, page=3, last page=7. */
    if (!set_window(x, 127u, 3u, 7u)) return 0u;
    for (i = 0u; i < 5u; ++i) {
        if (!write_data(glyphs[i], 6u)) return 0u;
    }
    return 1u;
}

#ifdef OLED_ROTARY_TEST
static void motor_set_level(uint8_t level)
{
    uint16_t pulse;

    if (level == 0u) {
        TIM1->CCER &= ~(1u << 12);
        TIM1->CH4CVR = 0u;
        GPIOC->BCR = 1u << 4;
        GPIOC->CFGLR &= ~(0x0fu << (4u * 4u));
        GPIOC->CFGLR |=
            (GPIO_Speed_10MHz | GPIO_CNF_OUT_PP) << (4u * 4u);
        return;
    }

    pulse = (uint16_t)(((uint32_t)level * MOTOR_PWM_PERIOD + 50u) / 100u);
    TIM1->CH4CVR = pulse;
    TIM1->SWEVGR = TIM_UG;
    GPIOC->CFGLR &= ~(0x0fu << (4u * 4u));
    GPIOC->CFGLR |=
        (GPIO_Speed_10MHz | GPIO_CNF_OUT_PP_AF) << (4u * 4u);
    TIM1->CCER &= ~(1u << 13);
    TIM1->CCER |= 1u << 12;
}

static void motor_init(void)
{
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_TIM1;

    /* Fail-safe: D6/PC4 is Low before PWM is configured. */
    GPIOC->CFGLR &= ~(0x0fu << (4u * 4u));
    GPIOC->CFGLR |=
        (GPIO_Speed_10MHz | GPIO_CNF_OUT_PP) << (4u * 4u);
    GPIOC->BCR = 1u << 4;

    RCC->APB2PRSTR |= RCC_APB2Periph_TIM1;
    RCC->APB2PRSTR &= ~RCC_APB2Periph_TIM1;
    TIM1->PSC = MOTOR_PWM_PRESCALER;
    TIM1->ATRLR = MOTOR_PWM_PERIOD - 1u;
    TIM1->CNT = 0u;
    TIM1->CH4CVR = 0u;
    TIM1->CHCTLR2 &= ~0xff00u;
    TIM1->CHCTLR2 |= 0x6800u;
    TIM1->CCER &= ~0x3000u;
    TIM1->BDTR |= TIM_MOE;
    TIM1->SWEVGR = TIM_UG;
    TIM1->CTLR1 |= TIM_CEN;
    motor_ms_remaining = 0u;
    motor_set_level(0u);
}

static void motor_start_edge_pulse(void)
{
    motor_set_level(EDGE_HAPTIC_LEVEL);
    motor_ms_remaining = EDGE_HAPTIC_MS;
}

static void motor_tick_1ms(void)
{
    if (motor_ms_remaining == 0u) return;
    --motor_ms_remaining;
    if (motor_ms_remaining == 0u) motor_set_level(0u);
}

static uint8_t erase_hello_at(uint8_t x)
{
    static const uint8_t blank[HELLO_WIDTH] = {0};

    if (!set_window(x, (uint8_t)(x + HELLO_WIDTH - 1u), 3u, 3u)) return 0u;
    return write_data(blank, HELLO_WIDTH);
}

static uint8_t refresh_hello(uint8_t x)
{
    return clear_display() && draw_hello_at(x);
}
#endif

static void blink_error_forever(void)
{
    for (;;) {
        funDigitalWrite(LED_PIN, FUN_HIGH);
        Delay_Ms(150);
        funDigitalWrite(LED_PIN, FUN_LOW);
        Delay_Ms(850);
    }
}

#ifndef OLED_ROTARY_TEST
int main(void)
{
    SystemInit();
    funGpioInitAll();
    funPinMode(LED_PIN, GPIO_CFGLR_OUT_10Mhz_PP);
    funDigitalWrite(LED_PIN, FUN_LOW);
    i2c_gpio_init();
    Delay_Ms(1000);
    i2c_recover();

    if (!initialize_display()) {
        blink_error_forever();
    }

    /* Visible panel check: every pixel must light for two seconds. */
    if (!command(0xa5u) || !command(0xafu)) blink_error_forever();
    Delay_Ms(2000);
    if (!command(0xa4u) || !clear_display() || !draw_hello_at(20u)) {
        blink_error_forever();
    }

    /* Solid LED means every OLED transfer was acknowledged. */
    funDigitalWrite(LED_PIN, FUN_HIGH);
    for (;;) Delay_Ms(1000);
}
#else
int main(void)
{
    uint8_t previous;
    uint8_t hello_x = HELLO_START_X;
    uint8_t key_previous = 1u;
    uint8_t key_stable = 0u;
    int8_t quarter_steps = 0;

    SystemInit();
    funGpioInitAll();
    funPinMode(LED_PIN, GPIO_CFGLR_OUT_10Mhz_PP);
    funDigitalWrite(LED_PIN, FUN_LOW);
    funPinMode(ENCODER_S1_PIN, GPIO_CFGLR_IN_PUPD);
    funPinMode(ENCODER_S2_PIN, GPIO_CFGLR_IN_PUPD);
    funPinMode(ENCODER_KEY_PIN, GPIO_CFGLR_IN_PUPD);
    funDigitalWrite(ENCODER_S1_PIN, FUN_HIGH);
    funDigitalWrite(ENCODER_S2_PIN, FUN_HIGH);
    funDigitalWrite(ENCODER_KEY_PIN, FUN_HIGH);
    motor_init();
    i2c_gpio_init();
    Delay_Ms(1000);
    i2c_recover();

    if (!initialize_display() || !refresh_hello(hello_x)) {
        blink_error_forever();
    }
    funDigitalWrite(LED_PIN, FUN_HIGH);
    previous = (uint8_t)((funDigitalRead(ENCODER_S1_PIN) << 1) |
                         funDigitalRead(ENCODER_S2_PIN));

    for (;;) {
        uint8_t current =
            (uint8_t)((funDigitalRead(ENCODER_S1_PIN) << 1) |
                      funDigitalRead(ENCODER_S2_PIN));
        uint8_t key_current = (uint8_t)funDigitalRead(ENCODER_KEY_PIN);

        if (current != previous) {
            int8_t movement = transition_table[(previous << 2) | current];
            previous = current;
            quarter_steps = (int8_t)(quarter_steps + movement);

            if (quarter_steps >= 4 || quarter_steps <= -4) {
                int8_t direction = (quarter_steps >= 4) ?
                    ENCODER_DIRECTION : -ENCODER_DIRECTION;
                uint8_t next_x = hello_x;

                quarter_steps = 0;
                if (direction > 0 && hello_x < HELLO_MAX_X) {
                    next_x = (hello_x > HELLO_MAX_X - 4u) ?
                        HELLO_MAX_X : (uint8_t)(hello_x + 4u);
                } else if (direction < 0 && hello_x > 0u) {
                    next_x = (hello_x < 4u) ? 0u : (uint8_t)(hello_x - 4u);
                }
                if (next_x != hello_x) {
                    if (!erase_hello_at(hello_x) || !draw_hello_at(next_x)) {
                        blink_error_forever();
                    }
                    hello_x = next_x;
                    if (hello_x == 0u || hello_x == HELLO_MAX_X) {
                        motor_start_edge_pulse();
                    }
                }
            }
        }

        if (key_current == key_previous) {
            if (key_stable < 20u) ++key_stable;
        } else {
            key_previous = key_current;
            key_stable = 0u;
        }
        if (key_stable == 20u && key_current == 0u) {
            hello_x = HELLO_START_X;
            if (!refresh_hello(hello_x)) blink_error_forever();
            key_stable = 21u;
        }
        motor_tick_1ms();
        Delay_Ms(1);
    }
}
#endif
