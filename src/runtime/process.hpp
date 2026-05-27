#pragma once

#include <cstdlib>
#include <string_view>

pid_t readPidFile();
void createPidFile();
bool checkPrivileges();

constexpr std::string_view PIDFILE_FMT = "/tmp/smhkd_{}.pid";
