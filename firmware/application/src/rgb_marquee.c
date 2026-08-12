#include "math.h"
#include "app_error.h"
#include "app_util_platform.h"
#include "nrf_atomic.h"
#include "nrf_gpio.h"
#include "hw_connect.h"
#include "bsp_delay.h"
#include "rgb_marquee.h"
#include "bsp_time.h"
#include "rgb/battery_indicator.h"
#include "rgb/rainbow_color.h"


#define NRF_LOG_MODULE_NAME rgb
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
NRF_LOG_MODULE_REGISTER();


#define PWM_MAX RAINBOW_PWM_COUNTER_TOP // PWM Maximum
#define LIGHT_LEVEL_MAX 99 // The maximum value of brightness level
#define BOOT_RAINBOW_FRAME_MS 40U
#define BOOT_RAINBOW_MAX_FRAMES 15U
#define FULL_SHUTDOWN_FRAME_MS 50U
#define CHARGING_RAINBOW_FRAME_COUNT 48U
#define CHARGING_RAINBOW_COLOR_FRAME_MS 33U
#define BATTERY_WAIT_FRAME_MS 100U
typedef enum {
    BATTERY_INDICATOR_IDLE,
    BATTERY_INDICATOR_WAIT_OFF,
    BATTERY_INDICATOR_WAIT_ON,
    BATTERY_INDICATOR_LEVEL,
} battery_indicator_state_t;
static nrf_drv_pwm_t pwm0_ins = NRF_DRV_PWM_INSTANCE(1);
static nrf_drv_pwm_t boot_rgb_pwm_ins = NRF_DRV_PWM_INSTANCE(2);
static bool position_pwm_initialized = false;
static bool boot_rgb_pwm_initialized = false;
nrf_pwm_values_individual_t pwm_sequ_val; // PWM control 4 channels in the independent mode
nrf_pwm_sequence_t const seq = { //Configure the structure of PWM output
    .values.p_individual = &pwm_sequ_val,
    .length          = 4,
    .repeats         = 0,
    .end_delay       = 0
};
nrf_drv_pwm_config_t pwm_config = {//PWM configuration structure
    .irq_priority = APP_IRQ_PRIORITY_LOWEST,
    .base_clock = NRF_PWM_CLK_1MHz,
    .count_mode = NRF_PWM_MODE_UP,
    .top_value = PWM_MAX,
    .load_mode = NRF_PWM_LOAD_INDIVIDUAL, // 4 channels for four values
    .step_mode = NRF_PWM_STEP_AUTO
};
static nrf_pwm_values_individual_t rgb_frames[CHARGING_RAINBOW_FRAME_COUNT];
static nrf_drv_pwm_config_t boot_rgb_pwm_config = {
    .output_pins = {
        NRF_DRV_PWM_PIN_NOT_USED,
        NRF_DRV_PWM_PIN_NOT_USED,
        NRF_DRV_PWM_PIN_NOT_USED,
        NRF_DRV_PWM_PIN_NOT_USED,
    },
    .irq_priority = APP_IRQ_PRIORITY_LOWEST,
    // A fast color carrier prevents phase beating with the independent 1 kHz
    // position PWM that draws the moving trail.
    .base_clock = NRF_PWM_CLK_16MHz,
    .count_mode = NRF_PWM_MODE_UP,
    .top_value = RAINBOW_PWM_COUNTER_TOP,
    .load_mode = NRF_PWM_LOAD_INDIVIDUAL,
    .step_mode = NRF_PWM_STEP_AUTO,
};
static autotimer *timer;
static autotimer *charging_timer;
static uint8_t rgb_marquee_usb_idle_step = 0;
static uint32_t charging_particle_frame = 0U;
static uint8_t charging_last_percentage = 0xFFU;
static uint8_t rgb_marquee_usb_open_step = 0;
static nrf_pwm_sequence_t motion_rgb_sequence;
static nrf_pwm_sequence_t charging_rgb_sequence;
static nrf_atomic_u32_t usb_animation_owners = 0U;
static battery_indicator_state_t battery_indicator_state = BATTERY_INDICATOR_IDLE;
static uint8_t battery_indicator_last_percentage = 0xFFU;
extern bool g_usb_led_marquee_enable;

static uint16_t get_pwmduty(uint8_t light_level);

static void position_pwm_start(nrfx_pwm_handler_t handler) {
    if (position_pwm_initialized) {
        nrfx_pwm_uninit(&pwm0_ins);
    }
    APP_ERROR_CHECK(nrf_drv_pwm_init(&pwm0_ins, &pwm_config, handler));
    position_pwm_initialized = true;
    nrf_drv_pwm_simple_playback(&pwm0_ins, &seq, 1, NRF_DRV_PWM_FLAG_LOOP);
}

static void position_pwm_stop(void) {
    if (!position_pwm_initialized) {
        return;
    }
    nrfx_pwm_stop(&pwm0_ins, true);
    nrfx_pwm_uninit(&pwm0_ins);
    position_pwm_initialized = false;
}

static void color_layer_start(nrf_pwm_sequence_t const *sequence, uint32_t playback_flags) {
    boot_rgb_pwm_config.output_pins[0] = LED_R | NRF_DRV_PWM_PIN_INVERTED;
    boot_rgb_pwm_config.output_pins[1] = LED_G | NRF_DRV_PWM_PIN_INVERTED;
    boot_rgb_pwm_config.output_pins[2] = LED_B | NRF_DRV_PWM_PIN_INVERTED;
    APP_ERROR_CHECK(nrf_drv_pwm_init(&boot_rgb_pwm_ins, &boot_rgb_pwm_config, NULL));
    boot_rgb_pwm_initialized = true;
    nrf_drv_pwm_simple_playback(
        &boot_rgb_pwm_ins,
        sequence,
        1,
        playback_flags
    );
}

void rgb_marquee_usb_suspend(rgb_led_owner_t owner) {
    nrf_atomic_u32_or(&usb_animation_owners, (uint32_t)owner);
    if (boot_rgb_pwm_initialized) {
        nrfx_pwm_stop(&boot_rgb_pwm_ins, false);
    }
    if (position_pwm_initialized &&
            (rgb_marquee_usb_idle_step != 0U || rgb_marquee_usb_open_step != 0U)) {
        position_pwm_stop();
    }
}

void rgb_marquee_usb_resume(rgb_led_owner_t owner) {
    uint32_t owners = nrf_atomic_u32_and(&usb_animation_owners, ~(uint32_t)owner);
    if (owners == 0U) {
        rgb_marquee_usb_idle_step = 0U;
    }
}

bool rgb_marquee_usb_is_suspended(void) {
    return usb_animation_owners != 0U;
}

bool rgb_marquee_usb_owner_is_active(rgb_led_owner_t owner) {
    return (usb_animation_owners & (uint32_t)owner) != 0U;
}


void rgb_marquee_init(void) {
    timer = bsp_obtain_timer(0);
    charging_timer = bsp_obtain_timer(0);
}

void rgb_marquee_stop(void) {
    if (boot_rgb_pwm_initialized) {
        nrfx_pwm_stop(&boot_rgb_pwm_ins, true);
        nrfx_pwm_uninit(&boot_rgb_pwm_ins);
        boot_rgb_pwm_initialized = false;
    }
    position_pwm_stop();
    rgb_marquee_usb_idle_step = 0;
    charging_particle_frame = 0U;
    charging_last_percentage = 0xFFU;
    rgb_marquee_usb_open_step = 0;
    battery_indicator_state = BATTERY_INDICATOR_IDLE;
    battery_indicator_last_percentage = 0xFFU;
}

static void rainbow_prepare_colors(uint8_t frame_count, uint8_t brightness) {
    for (uint8_t frame = 0U; frame < frame_count; frame++) {
        uint16_t phase = (uint16_t)(((uint32_t)frame * RAINBOW_PHASE_CYCLE) / frame_count);
        rainbow_rgb_t color = rainbow_color_at(phase, brightness);
        rgb_frames[frame].channel_0 = rainbow_pwm_compare(color.red, PWM_MAX);
        rgb_frames[frame].channel_1 = rainbow_pwm_compare(color.green, PWM_MAX);
        rgb_frames[frame].channel_2 = rainbow_pwm_compare(color.blue, PWM_MAX);
        rgb_frames[frame].channel_3 = rainbow_pwm_compare(0U, PWM_MAX);
    }
}

static bool rainbow_start_color_layer(uint8_t frame_count, uint8_t frame_ms, uint8_t brightness) {
    if (frame_count == 0U || frame_count > CHARGING_RAINBOW_FRAME_COUNT || frame_ms == 0U) {
        return false;
    }

    rgb_marquee_stop();
    rainbow_prepare_colors(frame_count, brightness);
    motion_rgb_sequence.values.p_individual = rgb_frames;
    motion_rgb_sequence.length = (uint16_t)(frame_count * 4U);
    motion_rgb_sequence.repeats = rainbow_pwm_repeats(frame_ms);
    motion_rgb_sequence.end_delay = 0U;
    color_layer_start(&motion_rgb_sequence, NRF_DRV_PWM_FLAG_STOP);
    return true;
}

static void battery_indicator_show_level(uint8_t battery_percentage) {
    battery_indicator_rgb_t color = battery_indicator_color(battery_percentage);
    uint8_t lit_count = battery_indicator_lit_count(battery_percentage, RGB_LIST_NUM);
    uint32_t *led_pins = hw_get_led_array();

    rgb_marquee_stop();
    for (uint8_t position = 0U; position < RGB_LIST_NUM; position++) {
        nrf_gpio_pin_clear(led_pins[position]);
    }
    rgb_frames[0].channel_0 = rainbow_pwm_compare(color.red, PWM_MAX);
    rgb_frames[0].channel_1 = rainbow_pwm_compare(color.green, PWM_MAX);
    rgb_frames[0].channel_2 = rainbow_pwm_compare(color.blue, PWM_MAX);
    rgb_frames[0].channel_3 = rainbow_pwm_compare(0U, PWM_MAX);
    motion_rgb_sequence.values.p_individual = rgb_frames;
    motion_rgb_sequence.length = 4U;
    motion_rgb_sequence.repeats = 0U;
    motion_rgb_sequence.end_delay = 0U;
    color_layer_start(&motion_rgb_sequence, NRF_DRV_PWM_FLAG_LOOP);
    for (uint8_t position = 0U; position < lit_count; position++) {
        nrf_gpio_pin_set(led_pins[position]);
        bsp_delay_ms(50);
    }
    battery_indicator_state = BATTERY_INDICATOR_LEVEL;
    battery_indicator_last_percentage = battery_percentage;
}

static void battery_indicator_show_stock(rgb_battery_sample_provider_t sample_provider) {
    uint32_t *led_pins = hw_get_led_array();
    uint8_t battery_percentage;

    rgb_marquee_stop();
    while (!sample_provider(&battery_percentage)) {
        set_slot_light_color(RGB_RED);
        for (uint8_t position = 0U; position < RGB_LIST_NUM; position++) {
            nrf_gpio_pin_clear(led_pins[position]);
        }
        bsp_delay_ms(BATTERY_WAIT_FRAME_MS);
        for (uint8_t position = 0U; position < RGB_LIST_NUM; position++) {
            nrf_gpio_pin_set(led_pins[position]);
        }
        bsp_delay_ms(BATTERY_WAIT_FRAME_MS);
    }

    uint8_t lit_count = battery_indicator_lit_count(battery_percentage, RGB_LIST_NUM);
    for (uint8_t position = 0U; position < RGB_LIST_NUM; position++) {
        nrf_gpio_pin_clear(led_pins[position]);
    }
    set_slot_light_color(RGB_CYAN);
    for (uint8_t position = 0U; position < lit_count; position++) {
        nrf_gpio_pin_set(led_pins[position]);
        bsp_delay_ms(50);
    }
}

static void battery_indicator_show_long_b(rgb_battery_sample_provider_t sample_provider) {
    uint32_t *led_pins = hw_get_led_array();
    uint8_t battery_percentage;

    if (sample_provider(&battery_percentage)) {
        if (battery_indicator_state != BATTERY_INDICATOR_LEVEL ||
                battery_percentage != battery_indicator_last_percentage) {
            battery_indicator_show_level(battery_percentage);
        }
        return;
    }
    if (battery_indicator_state == BATTERY_INDICATOR_IDLE ||
            battery_indicator_state == BATTERY_INDICATOR_LEVEL) {
        rgb_marquee_stop();
        set_slot_light_color(RGB_RED);
        for (uint8_t position = 0U; position < RGB_LIST_NUM; position++) {
            nrf_gpio_pin_clear(led_pins[position]);
        }
        battery_indicator_state = BATTERY_INDICATOR_WAIT_OFF;
        bsp_set_timer(charging_timer, 0U);
        return;
    }
    if (NO_TIMEOUT_1MS(charging_timer, BATTERY_WAIT_FRAME_MS)) {
        return;
    }

    bool turn_on = battery_indicator_state == BATTERY_INDICATOR_WAIT_OFF;
    for (uint8_t position = 0U; position < RGB_LIST_NUM; position++) {
        if (turn_on) {
            nrf_gpio_pin_set(led_pins[position]);
        } else {
            nrf_gpio_pin_clear(led_pins[position]);
        }
    }
    battery_indicator_state = turn_on
        ? BATTERY_INDICATOR_WAIT_ON
        : BATTERY_INDICATOR_WAIT_OFF;
    bsp_set_timer(charging_timer, 0U);
}

void rgb_marquee_show_battery(
    rgb_battery_trigger_t trigger,
    rgb_battery_sample_provider_t sample_provider
) {
    if (trigger == RGB_BATTERY_TRIGGER_LONG_B) {
        battery_indicator_show_long_b(sample_provider);
    } else {
        battery_indicator_show_stock(sample_provider);
    }
}

static void pwm_position_channel_set(uint8_t channel, uint16_t duty) {
    switch (channel) {
        case 0U:
            pwm_sequ_val.channel_0 = duty;
            break;
        case 1U:
            pwm_sequ_val.channel_1 = duty;
            break;
        case 2U:
            pwm_sequ_val.channel_2 = duty;
            break;
        default:
            pwm_sequ_val.channel_3 = duty;
            break;
    }
}

static void position_outputs_clear(void) {
    for (uint8_t channel = 0U; channel < BOOT_TRAIL_LENGTH; channel++) {
        pwm_config.output_pins[channel] = NRF_DRV_PWM_PIN_NOT_USED;
    }
}

static void position_frame_play(void) {
    position_pwm_start(NULL);
}

static void stock_rainbow_sweep_frame_show(uint8_t frame, uint8_t dir, uint8_t end) {
    uint32_t *led_pins = hw_get_led_array();

    position_outputs_clear();
    for (uint8_t channel = 0U; channel < BOOT_TRAIL_LENGTH; channel++) {
        int8_t position = stock_sweep_position(frame, channel, dir, end);
        pwm_position_channel_set(
            channel,
            stock_trail_pwm_compare((uint8_t)(BOOT_TRAIL_LENGTH - 1U - channel))
        );
        if (position >= 0) {
            pwm_config.output_pins[channel] = led_pins[(uint8_t)position];
        }
    }
    position_frame_play();
}

static void stock_rainbow_sweep(uint8_t dir, uint8_t end) {
    uint8_t frame_count = stock_sweep_frame_count(end);
    for (uint8_t frame = 0U; frame < frame_count; frame++) {
        stock_rainbow_sweep_frame_show(frame, dir, end);
        bsp_delay_ms(BOOT_RAINBOW_FRAME_MS);
    }
}

void rgb_marquee_full_startup_rainbow(uint8_t slot, uint8_t final_color) {
    uint32_t *led_pins = hw_get_led_array();
    uint8_t frame_count = stock_full_startup_frame_count(slot, RGB_LIST_NUM);
    uint8_t dir = slot > 3U ? 1U : 0U;
    uint8_t final_end = dir != 0U ? slot : (uint8_t)(7U - slot);

    if (!rainbow_start_color_layer(frame_count, BOOT_RAINBOW_FRAME_MS, RAINBOW_DISPLAY_BRIGHTNESS)) {
        return;
    }
    for (uint8_t position = 0U; position < RGB_LIST_NUM; position++) {
        nrf_gpio_pin_clear(led_pins[position]);
    }

    stock_rainbow_sweep((uint8_t)!dir, 11U);
    stock_rainbow_sweep(dir, 11U);
    stock_rainbow_sweep((uint8_t)!dir, final_end);

    rgb_marquee_stop();
    set_slot_light_color(final_color);
    for (uint8_t position = 0U; position < RGB_LIST_NUM; position++) {
        if (position == slot) {
            nrf_gpio_pin_set(led_pins[position]);
        } else {
            nrf_gpio_pin_clear(led_pins[position]);
        }
    }
}

static bool rf_field_owns_leds(void) {
    return rgb_marquee_usb_owner_is_active(RGB_LED_OWNER_HF) ||
           rgb_marquee_usb_owner_is_active(RGB_LED_OWNER_LF);
}

bool rgb_marquee_full_shutdown_begin(uint8_t slot) {
    uint8_t stop = slot > 3U ? 7U : 0U;
    uint8_t frame_count = (uint8_t)(
        stock_linear_frame_count(slot, stop) + stock_fade_frame_count(7U) * 4U
    );
    if (rf_field_owns_leds()) {
        return false;
    }
    bool started = rainbow_start_color_layer(
        frame_count,
        FULL_SHUTDOWN_FRAME_MS,
        RAINBOW_DISPLAY_BRIGHTNESS
    );
    if (rf_field_owns_leds()) {
        rgb_marquee_stop();
        return false;
    }
    return started;
}

bool rgb_marquee_full_shutdown_move_to_edge(uint8_t start, uint8_t stop) {
    uint32_t *led_pins = hw_get_led_array();
    uint8_t frame_count = stock_linear_frame_count(start, stop);

    pwm_sequ_val.channel_0 = get_pwmduty(99U);
    pwm_sequ_val.channel_1 = 0U;
    pwm_sequ_val.channel_2 = 0U;
    pwm_sequ_val.channel_3 = 0U;
    for (uint8_t frame = 0U; frame < frame_count; frame++) {
        if (rf_field_owns_leds()) {
            return false;
        }
        position_outputs_clear();
        pwm_config.output_pins[0] = led_pins[stock_linear_position(frame, start, stop)];
        position_frame_play();
        bsp_delay_ms(FULL_SHUTDOWN_FRAME_MS);
    }
    return !rf_field_owns_leds();
}

bool rgb_marquee_full_shutdown_fade(
    uint8_t dir,
    uint8_t end,
    uint8_t start_light,
    uint8_t stop_light
) {
    uint32_t *led_pins = hw_get_led_array();
    uint8_t frame_count = stock_fade_frame_count(end);

    for (uint8_t frame = 0U; frame < frame_count; frame++) {
        if (rf_field_owns_leds()) {
            return false;
        }
        position_outputs_clear();
        for (uint8_t channel = 0U; channel < BOOT_TRAIL_LENGTH; channel++) {
            int8_t position = stock_fade_position(frame, channel, dir, end);
            pwm_position_channel_set(
                channel,
                get_pwmduty(stock_fade_level(frame, channel, end, start_light, stop_light))
            );
            if (position >= 0) {
                pwm_config.output_pins[channel] = led_pins[(uint8_t)position];
            }
        }
        position_frame_play();
        bsp_delay_ms(FULL_SHUTDOWN_FRAME_MS);
    }
    return !rf_field_owns_leds();
}

static void boot_rainbow_show_trail(uint8_t frame, uint8_t slot) {
    uint32_t *led_pins = hw_get_led_array();
    uint8_t channel = 0U;

    for (uint8_t index = 0U; index < 4U; index++) {
        pwm_config.output_pins[index] = NRF_DRV_PWM_PIN_NOT_USED;
    }
    for (uint8_t position = 0U; position < RGB_LIST_NUM; position++) {
        int8_t age = boot_trail_age(frame, position, slot, RGB_LIST_NUM);
        if (age < 0) {
            continue;
        }
        pwm_config.output_pins[channel] = led_pins[position];
        pwm_position_channel_set(channel, stock_trail_pwm_compare((uint8_t)age));
        channel++;
        if (channel == BOOT_TRAIL_LENGTH) {
            break;
        }
    }

    position_pwm_start(NULL);
}

void rgb_marquee_boot_rainbow_trail(uint8_t slot, uint8_t final_color) {
    uint32_t *led_pins = hw_get_led_array();
    uint8_t frame_count = boot_trail_frame_count(slot, RGB_LIST_NUM);
    if (frame_count > BOOT_RAINBOW_MAX_FRAMES) {
        frame_count = BOOT_RAINBOW_MAX_FRAMES;
    }

    rgb_marquee_stop();
    for (uint8_t position = 0U; position < RGB_LIST_NUM; position++) {
        nrf_gpio_pin_clear(led_pins[position]);
    }

    rainbow_prepare_colors(frame_count, RAINBOW_DISPLAY_BRIGHTNESS);
    boot_rgb_pwm_config.output_pins[0] = LED_R | NRF_DRV_PWM_PIN_INVERTED;
    boot_rgb_pwm_config.output_pins[1] = LED_G | NRF_DRV_PWM_PIN_INVERTED;
    boot_rgb_pwm_config.output_pins[2] = LED_B | NRF_DRV_PWM_PIN_INVERTED;
    nrf_pwm_sequence_t boot_rgb_sequence = {
        .values.p_individual = rgb_frames,
        .length = (uint16_t)(frame_count * 4U),
        .repeats = rainbow_pwm_repeats(BOOT_RAINBOW_FRAME_MS),
        .end_delay = 0U,
    };
    APP_ERROR_CHECK(nrf_drv_pwm_init(&boot_rgb_pwm_ins, &boot_rgb_pwm_config, NULL));
    boot_rgb_pwm_initialized = true;
    nrf_drv_pwm_simple_playback(
        &boot_rgb_pwm_ins,
        &boot_rgb_sequence,
        1,
        NRF_DRV_PWM_FLAG_STOP
    );

    for (uint8_t frame = 0U; frame < frame_count; frame++) {
        boot_rainbow_show_trail(frame, slot);
        bsp_delay_ms(BOOT_RAINBOW_FRAME_MS);
    }

    rgb_marquee_stop();
    set_slot_light_color(final_color);
    for (uint8_t position = 0U; position < RGB_LIST_NUM; position++) {
        if (position == slot) {
            nrf_gpio_pin_set(led_pins[position]);
        } else {
            nrf_gpio_pin_clear(led_pins[position]);
        }
    }
}

// reset RGB state machines to force a refresh of the LED color
void rgb_marquee_reset(void) {
    rgb_marquee_usb_idle_step = 0;
    rgb_marquee_usb_open_step = 0;
}

// Brightness to PWM value
static uint16_t get_pwmduty(uint8_t light_level) {
    return PWM_MAX - (PWM_MAX * pow(((double)light_level / LIGHT_LEVEL_MAX), 2.2));
}

// 4 Lights and the level of brightness levels (no return)
//COLOR 0-R,1-G,2-B
void rgb_marquee_usb_open_sweep(uint8_t color, uint8_t dir) {
    static uint8_t startled = 0;
    static uint8_t setled = 0;
    uint32_t *led_pins_arr;

    if (!g_usb_led_marquee_enable && rgb_marquee_usb_open_step != 0) {
        startled = 0;
        setled = 0;
        rgb_marquee_stop();
        return;
    }

    //Processing direction
    if (dir == 0) {
        led_pins_arr = hw_get_led_array();
    } else {
        led_pins_arr = hw_get_led_reversal_array();
    }

    if (rgb_marquee_usb_open_step == 0) {
        //Adjust the color
        set_slot_light_color(color);
        pwm_sequ_val.channel_0 = 1;
        pwm_sequ_val.channel_1 = 1;
        pwm_sequ_val.channel_2 = 1;
        pwm_sequ_val.channel_3 = 1;
        bsp_set_timer(timer, 0);
        rgb_marquee_usb_open_step = 1;

        // Reset the state of the light when the USB is turned on to open the communication
        rgb_marquee_usb_idle_step = 0;
    }

    if (rgb_marquee_usb_open_step == 1) {
        setled = startled;
        for (uint8_t i = 0; i < 4; i++) {
            pwm_config.output_pins[i] = led_pins_arr[setled];
            setled++;
            if (setled > 7)setled = 0;
        }
        startled++;
        if (startled > 7)startled = 0;
        position_pwm_start(NULL);

        bsp_set_timer(timer, 0);
        rgb_marquee_usb_open_step = 2;
    }

    if (rgb_marquee_usb_open_step == 2) {
        if (!(NO_TIMEOUT_1MS(timer, 80))) {
            rgb_marquee_usb_open_step = 1;
        }
    }
}

void rgb_marquee_usb_open_symmetric(uint8_t color) {
    static uint8_t startled = 0;
    static uint8_t setled = 0;
    uint32_t *led_pins_arr = hw_get_led_array();

    if (!g_usb_led_marquee_enable && rgb_marquee_usb_open_step != 0) {
        startled = 0;
        setled = 0;
        rgb_marquee_stop();
        return;
    }

    if (rgb_marquee_usb_open_step == 0) {
        //Adjust the color
        set_slot_light_color(color);
        pwm_sequ_val.channel_0 = 1;
        pwm_sequ_val.channel_1 = 1;
        pwm_sequ_val.channel_2 = 1;
        pwm_sequ_val.channel_3 = 1;
        bsp_set_timer(timer, 0);
        rgb_marquee_usb_open_step = 1;

        // Reset the state of the light when the USB is turned on to open the communication
        rgb_marquee_usb_idle_step = 0;
    }

    if (rgb_marquee_usb_open_step == 1) {
        setled = startled < 4 ? startled : (4 - (startled - 3));
        pwm_config.output_pins[0] = led_pins_arr[setled];
        pwm_config.output_pins[1] = led_pins_arr[7 - setled];
        pwm_config.output_pins[2] = NRF_DRV_PWM_PIN_NOT_USED;
        pwm_config.output_pins[3] = NRF_DRV_PWM_PIN_NOT_USED;
        startled++;
        if (startled > 7)startled = 0;
        position_pwm_start(NULL);

        bsp_set_timer(timer, 0);
        rgb_marquee_usb_open_step = 2;
    }

    if (rgb_marquee_usb_open_step == 2) {
        if (!(NO_TIMEOUT_1MS(timer, 100))) {
            rgb_marquee_usb_open_step = 1;
        }
    }
}

// 4 Lights Dragon Tail horizontal movement cycle (not returning), including the disappearance of the tail and the head of the head slowly
//dir 0-from 1 card slot to 8 card slot, 1-from 8 card slot to 1 card slot (Direction, the end point is determined by the END parameter)
//end To scan the number of lamps, decide the final animation area with the direction
void rgb_marquee_sweep_to(uint8_t color, uint8_t dir, uint8_t end) {
    uint8_t startled = 0;
    uint8_t setled = 0;
    uint8_t leds2turnon = 0;
    uint8_t i = 0;
    uint32_t *led_pins_arr;
    //Processing direction
    if (dir == 0) {
        led_pins_arr = hw_get_led_array();
    } else {
        led_pins_arr = hw_get_led_reversal_array();
    }

    //Adjust the color
    set_slot_light_color(color);
    pwm_sequ_val.channel_3 = 1; //Brightest
    pwm_sequ_val.channel_2 = 600;
    pwm_sequ_val.channel_1 = 880;
    pwm_sequ_val.channel_0 = 980; // The darkest
    while (1) {
        //Close all channels
        pwm_config.output_pins[0] = NRF_DRV_PWM_PIN_NOT_USED;
        pwm_config.output_pins[1] = NRF_DRV_PWM_PIN_NOT_USED;
        pwm_config.output_pins[2] = NRF_DRV_PWM_PIN_NOT_USED;
        pwm_config.output_pins[3] = NRF_DRV_PWM_PIN_NOT_USED;

        setled = startled;
        if (setled < 3) { //During the positive period, only the first few LEDs can be on during 0, 1, 2
            //First determine that you can light a few lights
            leds2turnon = setled + 1; //1,2,3
            //Then set the PWM output channel
            for (i = 0; i < leds2turnon; i++) {
                pwm_config.output_pins[3 - i] = led_pins_arr[setled - i];
            }
        } else if (setled <= 7) { //During the positive period, it can light up 4 LEDs when it is greater than 4 less than 4
            // Set the PWM output channel
            for (i = 0; i < 4; i++) {
                pwm_config.output_pins[3 - i] = led_pins_arr[setled];
                setled--;
            }
        } else if (setled > 7 && setled <= 10) { // During the positive period, only a few LEDs can be lit at 8.9.10
            //First determine that you can light a few lights
            leds2turnon = 11 - setled;
            //Then set the PWM output channel
            for (i = 0; i < leds2turnon; i++) {
                pwm_config.output_pins[i] = led_pins_arr[setled - 3 + i];
            }

        } else { //During the positive period, reach 11
            //
        }
        //Process stop condition
        if (startled >= end) {
            //Calculation needs to hide a few lights
            leds2turnon = startled - end;
            //Hidden all those who go out
            for (i = 0; i < leds2turnon; i++) {
                pwm_config.output_pins[3 - i] = NRF_DRV_PWM_PIN_NOT_USED;
            }
            //Re -setting the specified position is the brightest
            if (end <= 7) {
                pwm_config.output_pins[3] = led_pins_arr[end];
            }

        }
        position_pwm_start(NULL);
        bsp_delay_ms(40);
        startled++;
        if (startled - end >= 4)break;
        if (startled > 11)break;
    }
}

//Switch card slot animation
//led_up The LED to be lit
//color_led_up The color of the lit LED 0-R,1-G,2-B
//led_down LED to be extinguished
//color_led_down The color of the LED to be extinguished 0-R,1-G,2-B
volatile bool callback_waiting = 0;
static void rgb_marquee_slot_switch_pwm_callback(nrfx_pwm_evt_type_t event_type) {
    if (event_type == NRF_DRV_PWM_EVT_FINISHED) {
        callback_waiting = 1;
    }
}
void rgb_marquee_slot_switch(uint8_t led_down, uint8_t color_led_down, uint8_t led_up, uint8_t color_led_up) {
    int16_t light_level = 99; //ledBrightnessValue
    uint32_t *led_pins = hw_get_led_array();
    if (led_down >= 0 && led_down <= 7) {
        //treatmentFirst
        pwm_config.output_pins[0] = led_pins[led_down];
        pwm_config.output_pins[1] = NRF_DRV_PWM_PIN_NOT_USED;
        pwm_config.output_pins[2] = NRF_DRV_PWM_PIN_NOT_USED;
        pwm_config.output_pins[3] = NRF_DRV_PWM_PIN_NOT_USED;
        while (light_level >= 0) {
            //processBrightness
            pwm_sequ_val.channel_0 = get_pwmduty(light_level);

            if (led_up >= 0 && led_up <= 7) {
                nrf_gpio_pin_clear(led_pins[led_up]);
            }

            set_slot_light_color(color_led_down);

            position_pwm_start(rgb_marquee_slot_switch_pwm_callback);

            while (callback_waiting == 0); //Waiting for the output of the PWM module to complete
            bsp_delay_us(1234);
            callback_waiting = 0;
            light_level --;
        }
    }
    for (uint8_t i = 0; i < RGB_LIST_NUM; i++) {
        nrf_gpio_pin_clear(led_pins[i]);
    }
    if (led_up >= 0 && led_up <= 7) {
        //Treatment
        pwm_config.output_pins[0] = led_pins[led_up];
        pwm_config.output_pins[1] = NRF_DRV_PWM_PIN_NOT_USED;
        pwm_config.output_pins[2] = NRF_DRV_PWM_PIN_NOT_USED;
        pwm_config.output_pins[3] = NRF_DRV_PWM_PIN_NOT_USED;
        light_level  = 0;
        while (light_level < 99) {
            //Process brightness
            pwm_sequ_val.channel_0 = get_pwmduty(light_level);

            if (led_down >= 0 && led_down <= 7) {
                nrf_gpio_pin_clear(led_pins[led_down]);
            }

            set_slot_light_color(color_led_up);

            position_pwm_start(rgb_marquee_slot_switch_pwm_callback);

            while (callback_waiting == 0); //Waiting for the output of the PWM module to complete
            bsp_delay_us(1234);
            callback_waiting = 0;
            light_level ++;
        }
    }
}

// 4 Light Tail horizontal movement cycle (not returning), does not include the disappearance of the tail, but includes the head of the head (for the type of playback type animation)
//dir 0-from 1 card slot to 8 card slot, 1-from 8 card slot to 1 card slot (Direction, the end point is determined by the END parameter)
//end To scan the number of lamps, decide the final animation area with the direction
//start_light stop_light 0-99 Indicate gradient brightness
void rgb_marquee_sweep_fade(uint8_t color, uint8_t dir, uint8_t end, uint8_t start_light, uint8_t stop_light) {
    uint8_t startled = 0;
    uint8_t setled = 0;
    uint8_t leds2turnon = 0;
    uint8_t i = 0;
    uint32_t *led_pins_arr;
    volatile double light_cnd;
    //Processing direction
    if (dir == 0) {
        led_pins_arr = hw_get_led_array();
    } else {
        led_pins_arr = hw_get_led_reversal_array();
    }

    //Adjust the color
    set_slot_light_color(color);
    while (1) {
        //Set the brightness
        // The current brightness coefficient
        // Start reaches STOP through END times
        light_cnd = (((double)stop_light - (double)start_light) / end) * startled + start_light;
        pwm_sequ_val.channel_3 = get_pwmduty((uint8_t)(0.99 * light_cnd)); //1; //Brightest
        pwm_sequ_val.channel_2 = get_pwmduty((uint8_t)(0.60 * light_cnd)); //600;
        pwm_sequ_val.channel_1 = get_pwmduty((uint8_t)(0.30 * light_cnd)); //880;
        pwm_sequ_val.channel_0 = get_pwmduty((uint8_t)(0.01 * light_cnd)); // 980; // The darkest
        //Close all channels
        pwm_config.output_pins[0] = NRF_DRV_PWM_PIN_NOT_USED;
        pwm_config.output_pins[1] = NRF_DRV_PWM_PIN_NOT_USED;
        pwm_config.output_pins[2] = NRF_DRV_PWM_PIN_NOT_USED;
        pwm_config.output_pins[3] = NRF_DRV_PWM_PIN_NOT_USED;

        setled = startled;
        if (setled < 3) { //During the positive period, only the first few LEDs can be on during 0, 1, 2
            //First determine that you can light a few lights
            leds2turnon = setled + 1; //1,2,3
            //Then set the PWM output channel
            for (i = 0; i < leds2turnon; i++) {
                pwm_config.output_pins[3 - i] = led_pins_arr[setled - i];
            }
        } else if (setled <= 7) { //During the positive period, it can light up 4 LEDs when it is greater than 4 less than 4
            //Set the PWM output channel
            for (i = 0; i < 4; i++) {
                pwm_config.output_pins[3 - i] = led_pins_arr[setled];
                setled--;
            }
        } else if (setled > 7 && setled <= 10) { // During the positive period, only a few LEDs can be lit at 8.9.10
            //First determine that you can light a few lights
            leds2turnon = 11 - setled;
            //Then set the PWM output channel
            for (i = 0; i < leds2turnon; i++) {
                pwm_config.output_pins[i] = led_pins_arr[setled - 3 + i];
            }

        } else { //During the positive period, reach 11
            //Nothing
        }
        //Process stop condition
        if (startled == end) {
            break;
        }
        position_pwm_start(NULL);
        bsp_delay_ms(50);
        startled++;
        if (startled - end >= 4)break;
        if (startled > 11)break;
    }
}

//Single light level movement
//color The color of the lit LED 0-R,1-G,2-B
//start Start the lamp position
//stop Stop lamp position
void rgb_marquee_sweep_from_to(uint8_t color, uint8_t start, uint8_t stop) {
    int8_t setled = start;
    uint32_t *led_pins = hw_get_led_array();
    //Set the brightness
    pwm_sequ_val.channel_3 = 0;
    pwm_sequ_val.channel_2 = 0;
    pwm_sequ_val.channel_1 = 0;
    pwm_sequ_val.channel_0 = get_pwmduty(99);
    //Adjust the color
    set_slot_light_color(color);
    while (start < stop ? (setled < stop + 1) : (setled > (int8_t)stop - 1)) {
        //Close all channels
        pwm_config.output_pins[0] = NRF_DRV_PWM_PIN_NOT_USED;
        pwm_config.output_pins[1] = NRF_DRV_PWM_PIN_NOT_USED;
        pwm_config.output_pins[2] = NRF_DRV_PWM_PIN_NOT_USED;
        pwm_config.output_pins[3] = NRF_DRV_PWM_PIN_NOT_USED;
        pwm_config.output_pins[0] = led_pins[setled];
        position_pwm_start(NULL);
        bsp_delay_ms(50);
        setled = start < stop ? setled + 1 : setled - 1;
    }
}


static bool charging_show_positions(uint8_t battery_percentage) {
    uint32_t *led_array = hw_get_led_array();
    uint8_t filled = charging_filled_count(battery_percentage, RGB_LIST_NUM);
    uint8_t channel = 0U;

    CRITICAL_REGION_ENTER();
    if (g_usb_led_marquee_enable && usb_animation_owners == 0U) {
        for (uint8_t position = 0U; position < RGB_LIST_NUM; position++) {
            if (position < filled) {
                nrf_gpio_pin_set(led_array[position]);
            } else {
                nrf_gpio_pin_clear(led_array[position]);
            }
        }
        for (uint8_t index = 0U; index < 4U; index++) {
            pwm_config.output_pins[index] = NRF_DRV_PWM_PIN_NOT_USED;
        }
        for (uint8_t position = 0U; position < RGB_LIST_NUM; position++) {
            int8_t age = charging_particle_age(
                charging_particle_frame,
                position,
                battery_percentage,
                RGB_LIST_NUM
            );
            if (age < 0) {
                continue;
            }
            pwm_config.output_pins[channel] = led_array[position];
            pwm_position_channel_set(channel, stock_trail_pwm_compare((uint8_t)age));
            channel++;
            if (channel == BOOT_TRAIL_LENGTH) {
                break;
            }
        }
        position_pwm_start(NULL);
    }
    CRITICAL_REGION_EXIT();
    return g_usb_led_marquee_enable && usb_animation_owners == 0U;
}

static bool charging_start_rainbow(void) {
    rainbow_prepare_colors(CHARGING_RAINBOW_FRAME_COUNT, RAINBOW_DISPLAY_BRIGHTNESS);
    boot_rgb_pwm_config.output_pins[0] = LED_R | NRF_DRV_PWM_PIN_INVERTED;
    boot_rgb_pwm_config.output_pins[1] = LED_G | NRF_DRV_PWM_PIN_INVERTED;
    boot_rgb_pwm_config.output_pins[2] = LED_B | NRF_DRV_PWM_PIN_INVERTED;
    charging_rgb_sequence = (nrf_pwm_sequence_t) {
        .values.p_individual = rgb_frames,
        .length = (uint16_t)(CHARGING_RAINBOW_FRAME_COUNT * 4U),
        .repeats = rainbow_pwm_repeats(CHARGING_RAINBOW_COLOR_FRAME_MS),
        .end_delay = 0U,
    };
    bool started = false;
    CRITICAL_REGION_ENTER();
    if (g_usb_led_marquee_enable && usb_animation_owners == 0U) {
        APP_ERROR_CHECK(nrf_drv_pwm_init(&boot_rgb_pwm_ins, &boot_rgb_pwm_config, NULL));
        boot_rgb_pwm_initialized = true;
        nrf_drv_pwm_simple_playback(
            &boot_rgb_pwm_ins,
            &charging_rgb_sequence,
            1,
            NRF_DRV_PWM_FLAG_LOOP
        );
        started = true;
    }
    CRITICAL_REGION_EXIT();
    return started;
}

void rgb_marquee_usb_idle(uint8_t battery_percentage) {
    if (!g_usb_led_marquee_enable) {
        if (rgb_marquee_usb_idle_step != 0U) {
            rgb_marquee_stop();
        }
        return;
    }

    if (usb_animation_owners != 0U) {
        return;
    }

    if (rgb_marquee_usb_idle_step == 0U) {
        rgb_marquee_stop();
        charging_last_percentage = battery_percentage;
        charging_particle_frame = 0U;
        if (!charging_start_rainbow()) {
            return;
        }
        bsp_set_timer(charging_timer, 0U);
        rgb_marquee_usb_open_step = 0U;
        if (!charging_show_positions(battery_percentage)) {
            rgb_marquee_stop();
            return;
        }
        rgb_marquee_usb_idle_step = 1U;
        return;
    }

    if (battery_percentage != charging_last_percentage) {
        // Let the normal 120 ms position tick apply ordinary percentage
        // changes. Reinitializing the position PWM here adds a second,
        // off-cadence flash. Full charge is applied immediately because the
        // particle stops and all positions become a static rainbow mask.
        charging_last_percentage = battery_percentage;
        if (battery_percentage >= 100U && !charging_show_positions(battery_percentage)) {
            rgb_marquee_stop();
        }
        return;
    }

    if (battery_percentage >= 100U) {
        return;
    }
    if (!NO_TIMEOUT_1MS(charging_timer, CHARGING_PARTICLE_FRAME_MS)) {
        charging_particle_frame++;
        if (!charging_show_positions(battery_percentage)) {
            rgb_marquee_stop();
            return;
        }
        bsp_set_timer(charging_timer, 0U);
    }
}

void rgb_marquee_symmetric_out(uint8_t color, uint8_t slot) {
    uint32_t *led_pins = hw_get_led_array();

    //Adjust the color
    set_slot_light_color(color);
    pwm_sequ_val.channel_3 = 950;
    pwm_sequ_val.channel_2 = 770;
    pwm_sequ_val.channel_1 = 770;
    pwm_sequ_val.channel_0 = 950;

    const uint8_t half_leds = 4;
    const uint8_t slide_leds = 2;
    const uint8_t solid_leds = 6;
    for (uint8_t step = 0; step < slide_leds + solid_leds + half_leds + slide_leds; step++) {
        //Close all channels
        pwm_config.output_pins[0] = NRF_DRV_PWM_PIN_NOT_USED;
        pwm_config.output_pins[1] = NRF_DRV_PWM_PIN_NOT_USED;
        pwm_config.output_pins[2] = NRF_DRV_PWM_PIN_NOT_USED;
        pwm_config.output_pins[3] = NRF_DRV_PWM_PIN_NOT_USED;
        for (uint8_t i = 0; i < RGB_LIST_NUM; i++) {
            nrf_gpio_pin_clear(led_pins[i]);
        }

        const uint8_t length = slide_leds + solid_leds + slide_leds;
        for (uint8_t offset = 0; offset < length; offset++) {
            if (step < offset || step >= (offset + half_leds)) continue;
            switch (offset) {
                case 0:
                case length - 1:
                    pwm_config.output_pins[0] = led_pins[3 - step + offset];
                    pwm_config.output_pins[3] = led_pins[4 + step - offset];
                    break;
                case 1:
                case length - 2:
                    pwm_config.output_pins[1] = led_pins[3 - step + offset];
                    pwm_config.output_pins[2] = led_pins[4 + step - offset];
                    break;
                default:
                    nrf_gpio_pin_set(led_pins[3 - step + offset]);
                    nrf_gpio_pin_set(led_pins[4 + step - offset]);
            }
        }

        if ((slot <= 3 && slot > (3 - step + slide_leds)) ||
                (slot >= 4 && slot < (4 + step - slide_leds))) {
            nrf_gpio_pin_set(led_pins[slot]);
            for (uint8_t j = 0; j < 4; j++) {
                if (pwm_config.output_pins[j] == led_pins[slot]) {
                    pwm_config.output_pins[j] = NRF_DRV_PWM_PIN_NOT_USED;
                }
            }
        }

        position_pwm_start(NULL);
        bsp_delay_ms(60);
    }
}

void rgb_marquee_symmetric_in(uint8_t color, uint8_t slot) {
    uint32_t *led_pins = hw_get_led_array();

    //Adjust the color
    set_slot_light_color(color);
    pwm_sequ_val.channel_3 = 950;
    pwm_sequ_val.channel_2 = 770;
    pwm_sequ_val.channel_1 = 770;
    pwm_sequ_val.channel_0 = 950;

    const uint8_t half_leds = 4;
    const uint8_t slide_leds = 2;
    const uint8_t solid_leds = 6;
    for (uint8_t step = 0; step < slide_leds + solid_leds + half_leds + slide_leds; step++) {
        //Close all channels
        pwm_config.output_pins[0] = NRF_DRV_PWM_PIN_NOT_USED;
        pwm_config.output_pins[1] = NRF_DRV_PWM_PIN_NOT_USED;
        pwm_config.output_pins[2] = NRF_DRV_PWM_PIN_NOT_USED;
        pwm_config.output_pins[3] = NRF_DRV_PWM_PIN_NOT_USED;
        for (uint8_t i = 0; i < RGB_LIST_NUM; i++) {
            nrf_gpio_pin_clear(led_pins[i]);
        }

        const uint8_t length = slide_leds + solid_leds + slide_leds;
        for (uint8_t offset = 0; offset < length; offset++) {
            if (step < offset || step >= (offset + half_leds)) continue;
            switch (offset) {
                case 0:
                case length - 1:
                    pwm_config.output_pins[0] = led_pins[0 + step - offset];
                    pwm_config.output_pins[3] = led_pins[7 - step + offset];
                    break;
                case 1:
                case length - 2:
                    pwm_config.output_pins[1] = led_pins[0 + step - offset];
                    pwm_config.output_pins[2] = led_pins[7 - step + offset];
                    break;
                default:
                    nrf_gpio_pin_set(led_pins[0 + step - offset]);
                    nrf_gpio_pin_set(led_pins[7 - step + offset]);
            }
        }

        if ((slot <= 3 && slot > (0 + step - slide_leds)) ||
                (slot >= 4 && slot < (7 - step + slide_leds))) {
            nrf_gpio_pin_set(led_pins[slot]);
            for (uint8_t j = 0; j < 4; j++) {
                if (pwm_config.output_pins[j] == led_pins[slot]) {
                    pwm_config.output_pins[j] = NRF_DRV_PWM_PIN_NOT_USED;
                }
            }
        }

        position_pwm_start(NULL);
        bsp_delay_ms(60);
    }
}

/**
 * @brief Whether the current lighting effect enables
 *
 * @return true Make the state, flickering in the lighting effect
 * @return false The state is prohibited, in the state of ordinary card slot indicator
 */
bool rgb_marquee_is_enabled(void) {
    return g_usb_led_marquee_enable;
}
