#ifndef API_H
#define API_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libproc.h>

int get_pid_by_name(const char* name);
void print_number(int number);

#endif // API_H