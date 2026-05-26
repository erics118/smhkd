#pragma once

#include <Carbon/Carbon.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include <cstdlib>
#include <string_view>

pid_t read_pid_file();
void create_pid_file();
bool check_privileges();

constexpr std::string_view PIDFILE_FMT = "/tmp/smhkd_{}.pid";
