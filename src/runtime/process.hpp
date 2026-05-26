#pragma once

#include "../input/log.hpp"
#include <Carbon/Carbon.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <print>
#include <string>


pid_t read_pid_file();
void create_pid_file();
bool check_privileges();

constexpr std::string_view PIDFILE_FMT = "/tmp/smhkd_{}.pid";
