/******************************************************************************
 * @file OD_kelvin
 * @brief object dictionnary managing kelvin color convesion over the standard Illuminance OD
 * @author Nicolas Rabault
 * @version 0.0.0
 ******************************************************************************/
#ifndef OD_OD_KELVIN_H_
#define OD_OD_KELVIN_H_

#include <math.h>
#include "od_illuminance.h"

static inline color_t IlluminanceOD_ColorFrom_Kelvin(float kelvin)
{
    color_t color;
    float temp = kelvin / 100;
    float red, green, blue;

    if (temp <= 66)
    {
        red   = 255;
        green = temp;
        green = 99.4708025861 * log(green) - 161.1195681661;
        if (temp <= 19)
        {
            blue = 0;
        }
        else
        {
            blue = temp - 10;
            blue = 138.5177312231 * log(blue) - 305.0447927307;
        }
    }
    else
    {
        red   = temp - 60;
        red   = 329.698727446 * pow(red, -0.1332047592);
        green = temp - 60;
        green = 288.1221695283 * pow(green, -0.0755148492);
        blue  = 255;
    }

    color.r = (uint8_t)red;
    color.g = (uint8_t)green;
    color.b = (uint8_t)blue;

    return color;
}

#endif /* OD_OD_KELVIN_H_ */
