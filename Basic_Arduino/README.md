# Core2 for AWS IoT EduKit Arduino Basic Blinking LED Example

This example provides a basic blinky example using the Arduino-style framework for Espressif MCU hardware on PlatformIO with the M5Stack Core2 ESP32 IoT Development Kit for AWS IoT EduKit (available on [Amazon.com](https://www.amazon.com/dp/B08VGRZYJR) or on the [M5Stack store](https://m5stack.com/products/m5stack-core2-esp32-iot-development-kit-for-aws-iot-edukit)). This example currently does not showcase AWS IoT connectivity or the use of the on-board secure element. It is meant to provide an example to get quickly get started.

PlatformIO will automatically pull the dependencies and toolchain. Use the following command from the PIO terminal window to compile and upload the code to the device:
```bash
pio run -e core2foraws -t upload
```