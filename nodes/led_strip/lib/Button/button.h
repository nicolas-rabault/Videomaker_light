/******************************************************************************
 * @file button
 * @brief driver example a simple button
 * @author Luos
 * @version 0.0.0
 ******************************************************************************/
#ifndef BUTTON_H
#define BUTTON_H
#define BTN_Pin       GPIO_PIN_0
#define BTN_GPIO_Port GPIOB
/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/

/*******************************************************************************
 * Function
 ******************************************************************************/
void Button_Init(void);
void Button_Loop(void);

#endif /* BUTTON_H */
