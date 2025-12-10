#ifndef __WEATHER_H__
#define __WEATHER_H__

#include <API.hpp>

int8_t api_info_status();
Weather* weather_data();
std::vector<Word> daily_words();
void api_info_exec(int status = 0);
void api_info_stop();

#endif