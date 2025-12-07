#ifndef __WEATHER_H__
#define __WEATHER_H__

#include <API.hpp>

int8_t weather_status();
Weather* weather_data_now();
void weather_exec(int status = 0);
void weather_stop();

#endif