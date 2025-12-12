
#ifndef __WIRING_H__
#define __WIRING_H__

#define LOLIN32_LITE


#ifdef LOLIN32_LITE
// Output PIN
//#define SPI_MISO GPIO_NUM_7 // Reserved
// #define SPI_MOSI GPIO_NUM_7 //DIO
// #define SPI_SCK GPIO_NUM_6
// #define SPI_CS GPIO_NUM_5
// #define SPI_DC GPIO_NUM_4
// #define SPI_RST GPIO_NUM_3
// #define SPI_BUSY GPIO_NUM_2
#define SPI_SCK   42
#define SPI_MOSI  41
#define SPI_CS    46
#define SPI_DC    47
#define SPI_RST   39
#define SPI_BUSY  40

// I2C
#define I2C_SDA GPIO_NUM_41
#define I2C_SCL GPIO_NUM_40
// Other PIN
#define KEY_M GPIO_NUM_13 // 注意：由于此按键负责唤醒，因此需要选择支持RTC唤醒的PIN脚。
#define BEEP_PIN GPIO_NUM_10
#define PIN_LED_R GPIO_NUM_48

#endif

#endif
