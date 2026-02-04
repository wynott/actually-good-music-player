#pragma once

constexpr int input_key_up = 1001;
constexpr int input_key_down = 1002;
constexpr int input_key_left = 1003;
constexpr int input_key_right = 1004;

void input_init();
void input_shutdown();
int input_poll_key();
