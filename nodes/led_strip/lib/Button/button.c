/******************************************************************************
 * @file button
 * @brief driver example a simple button
 * @author Luos
 * @version 0.0.0
 ******************************************************************************/

#include "button.h"
#include "profile_state.h"
#include "stm32f0xx_hal.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define BTN_Pin       GPIO_PIN_0
#define BTN_GPIO_Port GPIOB
/*******************************************************************************
 * Variables
 ******************************************************************************/
profile_state_t button;

/*******************************************************************************
 * Function
 ******************************************************************************/

/******************************************************************************
 * @brief init must be call in project init
 * @param None
 * @return None
 ******************************************************************************/
void Button_Init(void)
{
    // low level initialization
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // configure gpio clock
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /*Configure GPIO pin : button pin */
    GPIO_InitStruct.Pin  = BTN_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(BTN_GPIO_Port, &GPIO_InitStruct);

    // service initialization
    revision_t revision = {.major = 1, .minor = 0, .build = 0};
    // Profile configuration
    button.access = READ_ONLY_ACCESS;
    // Service creation following state profile
    ProfileState_CreateService(&button, 0, "button", revision);
}

/******************************************************************************
 * @brief loop must be call in project loop
 * @param None
 * @return None
 ******************************************************************************/
void Button_Loop(void)
{
    button.state = (bool)HAL_GPIO_ReadPin(BTN_GPIO_Port, BTN_Pin);
}