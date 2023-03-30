/******************************************************************************
 * @file potentiometer
 * @brief driver example a simple potentiometer
 * @author Luos
 * @version 0.0.0
 ******************************************************************************/
#include "light_controler.h"
#include "product_config.h"
#include "od_kelvin.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define BUTTON_STOP_PERIOS_MS   1000
#define BUTTON_UPDATE_PERIOD_MS 50
#define POT_UPDATE_PERIOD_MS    20
#define LED_STRIP_NB_LED        74
#define LED_STRIP_SIZE_M        2.45
#define FRAMERATE_MS            10
#define RED_DOT_DURATION_MS     6000

typedef enum
{
    STOP_MODE,
    START_MODE,
    ANGLE_MODE,
    RADIUS_MODE,
    INTENSITY_MODE,
    COLOR_MODE,
} desk_mode_t;

typedef struct
{
    angular_position_t angle; // angular position of the light between 0 and 180°
    linear_position_t radius; // radius of the light between 0 and LED_STRIP_SIZE_M
    ratio_t intensity;        // intensity of the light between 0 and 100%
    color_t color;            // color of the light
} light_param_t;

typedef struct
{
    bool red_dot;
    time_luos_t red_dot_date;
} red_dot_t;

typedef void (*DESK_LOOP)(void);

typedef struct
{
    desk_mode_t mode;
    DESK_LOOP desk_loop;
} desk_ctx_t;

typedef struct
{
    float prev_val;
    float inertial_force;
} filtering_ctx_t;

/*******************************************************************************
 * Variables
 ******************************************************************************/
// Parameters and variables
static service_t *light_service;
static light_param_t light_param;
static light_param_t light_param_bak;
static routing_table_t *potentiometer = NULL;
static routing_table_t *led_strip     = NULL;
static routing_table_t *button        = NULL;
static angular_position_t raw_angle;
static linear_position_t raw_radius;
static ratio_t raw_intensity;
static float raw_temperature;

// Modes
desk_ctx_t desk_ctx;
static red_dot_t red_dot_mode;

/*******************************************************************************
 * Function
 ******************************************************************************/
static void LightCtrl_MsgHandler(service_t *service, msg_t *msg);
static void LightCtrl_UpdateLight(void);
float LightCtrl_genericFiltering(filtering_ctx_t *f_ctx, float raw_val);

// Loop pointer functions
static void LightCtrl_angleFiltering(void);
static void LightCtrl_radiusFiltering(void);
static void LightCtrl_intensityFiltering(void);
// static void LightCtrl_colorFiltering(void);
static void LightCtrl_fadeLight(void);
static void LightCtrl_doNothing(void);

// Mode and state machine
static void LightCtrl_modeSwitch(void);

/******************************************************************************
 * @brief init must be call in project init
 * @param None
 * @return None
 ******************************************************************************/
void LightCtrl_Init(void)
{
    revision_t revision = {.major = 1, .minor = 0, .build = 0};
    // ******************* service creation *******************
    light_service = Luos_CreateService(LightCtrl_MsgHandler, LIGHT_CONTROLER_APP, "Controler", revision);

    // ******************* Light parameters initialization *******************
    light_param.angle     = AngularOD_PositionFrom_deg(90.0);
    light_param.radius    = LinearOD_PositionFrom_m(0.5);
    light_param.intensity = RatioOD_RatioFrom_Percent(25.0);
    // Start with a 4500K color temperature
    light_param.color.r = 255;
    light_param.color.g = 196;
    light_param.color.b = 137;

    light_param_bak = light_param;

    raw_angle       = light_param.angle;
    raw_radius      = light_param.radius;
    raw_intensity   = RatioOD_RatioFrom_Percent(0.0);
    raw_temperature = 3500.0f;

    red_dot_mode.red_dot      = true;
    red_dot_mode.red_dot_date = TimeOD_TimeFrom_ms(Luos_GetSystick());

    // ******************* context initialization *******************
    desk_ctx.mode      = STOP_MODE;
    desk_ctx.desk_loop = LightCtrl_fadeLight;

    // ******************* service detection *******************
    Luos_Detect(light_service);
}

/******************************************************************************
 * @brief loop must be call in project loop
 * @param None
 * @return None
 ******************************************************************************/
void LightCtrl_Loop(void)
{
    static uint32_t lastframe_time_ms = 0;
    if (Luos_GetSystick() - lastframe_time_ms >= FRAMERATE_MS)
    {
        lastframe_time_ms = Luos_GetSystick();
        desk_ctx.desk_loop();
    }
}
/******************************************************************************
 * @brief Msg Handler call back when a msg receive for this service
 *
 * @param Service destination
 * @param Msg receive
 * @return None
 ******************************************************************************/
static void LightCtrl_MsgHandler(service_t *service, msg_t *msg)
{
    if (msg->header.cmd == ANGULAR_POSITION)
    {
        float delta = 0.0;
        ratio_t temp;
        switch (desk_ctx.mode)
        {
            case ANGLE_MODE:
                // Save the new raw angular position
                AngularOD_PositionFromMsg(&raw_angle, msg);
                // We want the potentiometer to give a value between 0 and 180°
                // We need to convert the raw value to this range knowing that my potentiometer can go up to 300°
                raw_angle = AngularOD_PositionFrom_deg(AngularOD_PositionTo_deg(raw_angle) * 180.0 / 300.0);
                break;
            case RADIUS_MODE:
                // Save the new raw radius position
                LinearOD_PositionFromMsg(&raw_radius, msg);
                // We want the potentiometer to give a value between 0 and LED_STRIP_SIZE_M
                // We need to convert the raw value to this range knowing that my potentiometer can go up to 300°
                raw_radius = LinearOD_PositionFrom_m(LinearOD_PositionTo_m(raw_radius) * LED_STRIP_SIZE_M / 300.0);
                delta      = fabs(LinearOD_PositionTo_m(raw_radius) - LinearOD_PositionTo_m(light_param.radius));
                if (delta > 0.01)
                {
                    red_dot_mode.red_dot      = true;
                    red_dot_mode.red_dot_date = TimeOD_TimeFrom_ms(Luos_GetSystick());
                }
                break;
            case INTENSITY_MODE:
                // Save the new raw intensity position
                RatioOD_RatioFromMsg(&raw_intensity, msg);
                // We want the potentiometer to give a value between 0 and 100%
                // We need to convert the raw value to this range knowing that my potentiometer can go up to 300°
                raw_intensity = RatioOD_RatioFrom_Percent(RatioOD_RatioTo_Percent(raw_intensity) * 100.0 / 300.0);
                delta         = fabs(RatioOD_RatioTo_Percent(raw_intensity) - RatioOD_RatioTo_Percent(light_param.intensity));
                if (delta > 0.5)
                {
                    red_dot_mode.red_dot      = true;
                    red_dot_mode.red_dot_date = TimeOD_TimeFrom_ms(Luos_GetSystick());
                }
                break;
            case COLOR_MODE:
                // Save the new raw color
                // We will compute that from kelvin values between 1500 to 5500
                // Convert the kelvin value to
                RatioOD_RatioFromMsg(&temp, msg);
                if (fabs((RatioOD_RatioTo_Percent(temp) * 4000.0 / 100.0 + 1500.0) - raw_temperature) > 200.0)
                {
                    red_dot_mode.red_dot      = true;
                    red_dot_mode.red_dot_date = TimeOD_TimeFrom_ms(Luos_GetSystick());
                }
                raw_temperature = RatioOD_RatioTo_Percent(temp) * 4000.0 / 100.0 + 1500.0;
                // Directly apply the temperature to the light
                light_param.color = IlluminanceOD_ColorFrom_Kelvin(raw_temperature);
                break;

            default:
                break;
        }
        return;
    }
    if (msg->header.cmd == IO_STATE)
    {
        static bool last_state            = false;
        static time_luos_t last_push_date = {0.0};
        bool state                        = (bool)msg->data[0];
        //
        if (state != last_state)
        {
            if (state)
            {
                // Someone start pushing the button, start measuring time
                last_push_date = TimeOD_TimeFrom_ms(Luos_GetSystick());
            }
            else
            {
                // Someone stop pushing the button, check if it was short enough to not stop the light
                if (TimeOD_TimeTo_ms(last_push_date) > 0.01)
                {
                    // The button was released before the end of the BUTTON_STOP_PERIOS_MS, we have to use it as a mode switch
                    LightCtrl_modeSwitch();
                    red_dot_mode.red_dot      = true;
                    red_dot_mode.red_dot_date = TimeOD_TimeFrom_ms(Luos_GetSystick());
                    // Reset time to avoid stopping the light
                    last_push_date = TimeOD_TimeFrom_ms(0.0);
                }
            }
            last_state = state;
        }
        if ((TimeOD_TimeTo_ms(last_push_date) > 0.01)
            && (Luos_GetSystick() - TimeOD_TimeTo_ms(last_push_date) > BUTTON_STOP_PERIOS_MS)
            && (desk_ctx.mode != STOP_MODE))
        {
            // The button is still pushed after the BUTTON_STOP_PERIOS_MS. We have to stop the light
            light_param_bak    = light_param;
            desk_ctx.mode      = STOP_MODE;
            desk_ctx.desk_loop = LightCtrl_fadeLight;
            last_push_date     = TimeOD_TimeFrom_ms(0.0);
        }
        return;
    }
    if (msg->header.cmd == END_DETECTION)
    {
        search_result_t target_list;
        // Get the first potentiometer
        RTFilter_Reset(&target_list);
        RTFilter_Type(&target_list, ANGLE_TYPE);
        LUOS_ASSERT(target_list.result_nbr > 0);
        potentiometer = target_list.result_table[0];
        // Get the first led strip
        RTFilter_Reset(&target_list);
        RTFilter_Type(&target_list, COLOR_TYPE);
        LUOS_ASSERT(target_list.result_nbr > 0);
        led_strip = target_list.result_table[0];
        // Get the first button
        RTFilter_Reset(&target_list);
        RTFilter_Type(&target_list, STATE_TYPE);
        LUOS_ASSERT(target_list.result_nbr > 0);
        button = target_list.result_table[0];

        // Setup auto update each POT_UPDATE_PERIOD_MS on potentiometer
        // This value is resetted on all service at each detection
        // It's important to setting it each time.
        msg_t send_msg;
        send_msg.header.target      = potentiometer->id;
        send_msg.header.target_mode = IDACK;

        time_luos_t time = TimeOD_TimeFrom_ms(POT_UPDATE_PERIOD_MS);
        TimeOD_TimeToMsg(&time, &send_msg);
        send_msg.header.cmd = UPDATE_PUB;
        while (Luos_SendMsg(service, &send_msg) != SUCCEED)
            ;

        // Setup auto update each BUTTON_UPDATE_PERIOD_MS on the button
        // This value is resetted on all service at each detection
        // It's important to setting it each time.
        send_msg.header.target      = button->id;
        send_msg.header.target_mode = IDACK;

        time = TimeOD_TimeFrom_ms(BUTTON_UPDATE_PERIOD_MS);
        TimeOD_TimeToMsg(&time, &send_msg);
        send_msg.header.cmd = UPDATE_PUB;
        while (Luos_SendMsg(service, &send_msg) != SUCCEED)
            ;

        return;
    }
}

/******************************************************************************
 * @brief Update the light parameters
 *
 * @param None
 * @return None
 ******************************************************************************/
static void LightCtrl_UpdateLight(void)
{
    // Compute the picture depending on the light parameters
    color_t pic[LED_STRIP_NB_LED] = {0};
    volatile float center_led     = AngularOD_PositionTo_deg(light_param.angle) * LED_STRIP_NB_LED / 180.0;
    volatile float radius_led     = LinearOD_PositionTo_m(light_param.radius) * LED_STRIP_NB_LED / LED_STRIP_SIZE_M;
    volatile int first_led;
    volatile int second_led;
    for (uint8_t i = 0; i < LED_STRIP_NB_LED; i++)
    {
        // Compute the led number between the led and the center
        float led_diff = fabs(center_led - i);
        // Compute the light intentisty depending on the radius
        float intensity = (1.0f - (led_diff / radius_led))
                          * RatioOD_RatioTo_Percent(light_param.intensity) / 100.0f;
        if (intensity < 0.0)
        {
            intensity = 0.0;
        }
        pic[i].r = light_param.color.r * intensity;
        pic[i].g = light_param.color.g * intensity;
        pic[i].b = light_param.color.b * intensity;
    }

    // Red dot mode
    if (red_dot_mode.red_dot)
    {
        // Get dot elapsed time
        float dot_elapsed = (Luos_GetSystick() - TimeOD_TimeTo_ms(red_dot_mode.red_dot_date)) / RED_DOT_DURATION_MS;
        // Check if the red dot mode is over
        if (dot_elapsed > 1.0)
        {
            red_dot_mode.red_dot = false;
        }
        else
        {
            // Display the red dot depending on the current mode
            switch (desk_ctx.mode)
            {
                case ANGLE_MODE:
                    // Overlap the center led to be red and fade it depending on dot_elapsed
                    pic[(uint8_t)center_led].r = (255 * (1.0 - dot_elapsed)) + pic[(uint8_t)center_led].r * dot_elapsed;
                    pic[(uint8_t)center_led].g = pic[(uint8_t)center_led].g * dot_elapsed;
                    pic[(uint8_t)center_led].b = pic[(uint8_t)center_led].b * dot_elapsed;
                    break;
                case INTENSITY_MODE:
                    // Overlap the first led to be white and fade it depending on dot_elapsed
                    pic[0].r = (255 * (1.0 - dot_elapsed)) + pic[0].r * dot_elapsed;
                    pic[0].g = (255 * (1.0 - dot_elapsed)) + pic[0].g * dot_elapsed;
                    pic[0].b = (255 * (1.0 - dot_elapsed)) + pic[0].b * dot_elapsed;
                    break;
                case RADIUS_MODE:
                    // Overlap the 2 external leds to be red and fade it depending on dot_elapsed
                    first_led = center_led - radius_led;
                    if (first_led < 0)
                    {
                        first_led = 0;
                    }
                    second_led = center_led + radius_led;
                    if (second_led >= LED_STRIP_NB_LED)
                    {
                        second_led = LED_STRIP_NB_LED - 1;
                    }
                    pic[first_led].r = (255 * (1.0 - dot_elapsed)) + pic[first_led].r * dot_elapsed;
                    pic[first_led].g = pic[first_led].g * dot_elapsed;
                    pic[first_led].b = pic[first_led].b * dot_elapsed;

                    pic[second_led].r = (255 * (1.0 - dot_elapsed)) + pic[second_led].r * dot_elapsed;
                    pic[second_led].g = pic[second_led].g * dot_elapsed;
                    pic[second_led].b = pic[second_led].b * dot_elapsed;
                    break;
                case COLOR_MODE:
                    // Put the 2 first led into the lowest and highest temperature we manage (between 1500K to 5500K) and fade it depending on dot_elapsed
                    pic[0].r = (255 * (1.0 - dot_elapsed)) + pic[0].r * dot_elapsed;
                    pic[0].g = (109 * (1.0 - dot_elapsed)) + pic[0].g * dot_elapsed;
                    pic[0].b = (0 * (1.0 - dot_elapsed)) + pic[0].b * dot_elapsed;
                    pic[1].r = (255 * (1.0 - dot_elapsed)) + pic[0].r * dot_elapsed;
                    pic[1].g = (236 * (1.0 - dot_elapsed)) + pic[0].g * dot_elapsed;
                    pic[1].b = (224 * (1.0 - dot_elapsed)) + pic[0].b * dot_elapsed;
                    break;
                default:
                    break;
            }
        }
    }

    // Send the picture to the led strip
    msg_t msg;
    msg.header.target      = led_strip->id;
    msg.header.target_mode = IDACK;
    msg.header.cmd         = COLOR;
    Luos_SendData(light_service, &msg, pic, sizeof(pic));
}

/******************************************************************************
 * @brief Switch the light mode
 *
 * @param None
 * @return None
 ******************************************************************************/
static void LightCtrl_modeSwitch(void)
{
    switch (desk_ctx.mode)
    {
        case STOP_MODE:
            // Get back the light parameters
            light_param = light_param_bak;
            // Set the light intensity to 0 to get a smooth start.
            light_param.intensity = RatioOD_RatioFrom_Percent(0.0);
            desk_ctx.mode         = INTENSITY_MODE;
            desk_ctx.desk_loop    = LightCtrl_intensityFiltering;
            break;
        case START_MODE:
            desk_ctx.mode      = INTENSITY_MODE;
            desk_ctx.desk_loop = LightCtrl_intensityFiltering;
            break;
        case INTENSITY_MODE:
            desk_ctx.mode      = RADIUS_MODE;
            desk_ctx.desk_loop = LightCtrl_radiusFiltering;
            break;
        case RADIUS_MODE:
            desk_ctx.mode      = COLOR_MODE;
            desk_ctx.desk_loop = LightCtrl_doNothing;
            break;
        case COLOR_MODE:
            desk_ctx.mode      = ANGLE_MODE;
            desk_ctx.desk_loop = LightCtrl_angleFiltering;
            break;
        case ANGLE_MODE:
            desk_ctx.mode      = INTENSITY_MODE;
            desk_ctx.desk_loop = LightCtrl_intensityFiltering;
            break;
        default:
            break;
    }
}

/******************************************************************************
 * @brief Do nothing
 *
 * @param None
 * @return None
 ******************************************************************************/
static void LightCtrl_doNothing(void)
{
    // Go back to the start mode if noting moves.
    if (red_dot_mode.red_dot)
    {
        // Update the light
        LightCtrl_UpdateLight();
    }
}

/******************************************************************************
 * @brief Fade the light
 *
 * @param None
 * @return None
 ******************************************************************************/
static void LightCtrl_fadeLight(void)
{
    if (RatioOD_RatioTo_Percent(light_param.intensity) >= 0.01)
    {
        // Fade the light
        light_param.intensity = RatioOD_RatioFrom_Percent(RatioOD_RatioTo_Percent(light_param.intensity) - 1.0f);
        if (RatioOD_RatioTo_Percent(light_param.intensity) < 0.01)
        {
            light_param.intensity = RatioOD_RatioFrom_Percent(0.0);
        }
        // Update the light
        LightCtrl_UpdateLight();
    }
}

/******************************************************************************
 * @brief Filter the light angle parameters
 *
 * @param None
 * @return None
 ******************************************************************************/
void LightCtrl_angleFiltering(void)
{
    // Filtering variables
    static filtering_ctx_t angular_filtering = {0.0, 0.0};

    light_param.angle = AngularOD_PositionFrom_deg(LightCtrl_genericFiltering(&angular_filtering, AngularOD_PositionTo_deg(raw_angle)));
}

/******************************************************************************
 * @brief Filter the light intensity parameters
 *
 * @param None
 * @return None
 ******************************************************************************/
static void LightCtrl_intensityFiltering(void)
{
    // Filtering variables
    static filtering_ctx_t intensity_filtering = {0.0, 0.0};

    light_param.intensity = RatioOD_RatioFrom_Percent(LightCtrl_genericFiltering(&intensity_filtering, RatioOD_RatioTo_Percent(raw_intensity)));
}

/******************************************************************************
 * @brief Filter the light radius parameters
 *
 * @param None
 * @return None
 ******************************************************************************/
static void LightCtrl_radiusFiltering(void)
{
    // Filtering variables
    static filtering_ctx_t radius_filtering = {0.0, 0.0};

    light_param.radius = LinearOD_PositionFrom_cm(LightCtrl_genericFiltering(&radius_filtering, LinearOD_PositionTo_cm(raw_radius)));
}

/******************************************************************************
 * @brief Generic filtering function
 *
 * @param f_ctx: filtering context
 * @param raw_val: raw value to filter
 * @return filtered value
 ******************************************************************************/
float LightCtrl_genericFiltering(filtering_ctx_t *f_ctx, float raw_val)
{
    // Angular movement
    const float filtering_strength = 0.03; // 0.04
    const float inertia_strength   = 0.1;  // 0.03
    const float max_speed          = 0.3;  // 0.3

    // Compute the error between the light and the real hand position
    float err = raw_val - f_ctx->prev_val;
    // Now check if the value changed to enable the red dot mode
    if (fabs(err) > 2.0)
    {
        red_dot_mode.red_dot      = true;
        red_dot_mode.red_dot_date = TimeOD_TimeFrom_ms(Luos_GetSystick());
    }
    // Compute inertial delta force (integral)
    f_ctx->inertial_force += err;
    // Inertia clamping
    if (f_ctx->inertial_force < -max_speed)
        f_ctx->inertial_force = -max_speed;
    if (f_ctx->inertial_force > max_speed)
        f_ctx->inertial_force = max_speed;

    // Then filter the position to give an inertia effect
    float result = f_ctx->prev_val + (filtering_strength * err) + (inertia_strength * f_ctx->inertial_force);

    // Update the light
    LightCtrl_UpdateLight();

    // Go back to the start mode if noting moves.
    if (!red_dot_mode.red_dot)
    {

        desk_ctx.mode      = START_MODE;
        desk_ctx.desk_loop = LightCtrl_doNothing;
    }

    f_ctx->prev_val = result;
    return result;
}