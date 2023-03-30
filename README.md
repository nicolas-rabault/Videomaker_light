# My DIY desk light perfectly suited for videoMaking

I create this light to get a precise conttrol over the light intensity, temperature, and position around my desk.
I can configure it depending on the ambient light to get the best lightning for my future videos.

This project contain everything allowing you to create your own led_strip based videomaking light.
This project use :

 - [Kicad](https://www.kicad.org/) for the hardware design
 - [Platformio](https://platformio.org/) as IDE
 - [Luos](https://www.luos.io/) as embedded software architecture
 - [CubeMX](https://www.st.com/en/development-tools/stm32cubemx.html) to generate MCU low level interfaces

## How to make the hardware :electric_plug:
Electronics have been based on [Luos example electronic design](https://github.com/Luos-io/Examples/tree/master/Hardware). You can open and modify all of them using Kicad. This design use Luos_components library for more information to install and use it read [Luos doc](https://docs.luos.io).
In this projet I use 2 [L0 boards](https://github.com/Luos-io/luos_engine/tree/main/examples/hardware/l0):
 - The first one have 2 shield stacked one on the other, the [led_strip shield](https://github.com/Luos-io/luos_engine/tree/main/examples/hardware/l0-shields/L0_led_strip) and the [button shield](https://github.com/Luos-io/luos_engine/tree/main/examples/hardware/l0-shields/l0_button)
 - The second one have a simple [potentiometer shield](https://github.com/Luos-io/luos_engine/tree/main/examples/hardware/l0-shields/l0_potentiometer)

I put all of these boards in a derivation box and power up the system with a 5V led power supply.
