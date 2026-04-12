#include "led.h"

#include <Arduino.h>
#include "wiring.h"

int8_t BLINK_TYPE = 0; // 0: off, 1: on, 2: slow blink, 3: fast blink, 4: config mode

void led_init()
{
    pinMode(PIN_LED_R, OUTPUT);
}

void ledTask(void *param)
{
    while(1)
    {
        switch(BLINK_TYPE)
        {
            case 0:
                digitalWrite(PIN_LED_R, LOW); // Off
                vTaskDelay(pdMS_TO_TICKS(1000));
            break;
            case 1:
                digitalWrite(PIN_LED_R, HIGH); // On
                vTaskDelay(pdMS_TO_TICKS(1000));
            break;
            case 2:
                digitalWrite(PIN_LED_R, HIGH); // On
                vTaskDelay(pdMS_TO_TICKS(1000));
                digitalWrite(PIN_LED_R, LOW); // Off
                vTaskDelay(pdMS_TO_TICKS(1000));
            break;
            case 3:
                digitalWrite(PIN_LED_R, HIGH); // On
                vTaskDelay(pdMS_TO_TICKS(200));
                digitalWrite(PIN_LED_R, LOW); // Off
                vTaskDelay(pdMS_TO_TICKS(200));
            break;
            case 4:
                vTaskDelay(pdMS_TO_TICKS(200));
                digitalWrite(PIN_LED_R, HIGH); // On
                vTaskDelay(pdMS_TO_TICKS(200));
                digitalWrite(PIN_LED_R, LOW); // Off
                vTaskDelay(pdMS_TO_TICKS(200));
                digitalWrite(PIN_LED_R, HIGH); // On
                vTaskDelay(pdMS_TO_TICKS(200));
                digitalWrite(PIN_LED_R, LOW); // Off
                vTaskDelay(pdMS_TO_TICKS(200));
                digitalWrite(PIN_LED_R, HIGH); // On
                vTaskDelay(pdMS_TO_TICKS(200));
                digitalWrite(PIN_LED_R, LOW); // Off
                vTaskDelay(pdMS_TO_TICKS(1000));
            break;
            default:
                digitalWrite(PIN_LED_R, LOW); // Off
                vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

void led_fast()
{
    BLINK_TYPE = 3;
}

void led_slow()
{
    BLINK_TYPE = 2;
}

void led_config()
{
    BLINK_TYPE = 4;
}

void led_on()
{
    BLINK_TYPE = 1;
}

void led_off()
{
    BLINK_TYPE = 0;
}

