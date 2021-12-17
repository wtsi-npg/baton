/**
 * Copyright (C) 2021 Genome
 * Research Ltd. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @file signal_handler.c
 * @author Michael Kubiak <mk35@sanger.ac.uk>
 */

#include "signal_handler.h"
#include "baton.h"

#include <signal.h>

int signals[] = {SIGINT, SIGQUIT, SIGHUP, SIGTERM, SIGUSR1, SIGUSR2, SIGPIPE, 0};
int exit_flag;

void handle_signal(int signal){
  switch (signal) {
  case SIGINT:
    exit_flag = 2;
    break;
  case SIGQUIT:
    exit_flag = 3;
    break;
  case SIGHUP:
    exit_flag = 4;
    break;
  case SIGTERM:
  case SIGUSR1:
  case SIGUSR2:
    exit_flag = 5;
    break;
  case SIGPIPE:
    exit_flag = 6;
    break;
  default:
    exit_flag = 1;
  }
}

int apply_signal_handler() {
    exit_flag = 0;

    struct sigaction saction;
    saction.sa_handler = &handle_signal;
    saction.sa_flags = 0;
    sigemptyset(&saction.sa_mask);
    int sigstatus;

    // Exit gracefully on fatal signals
    for (int i = 0; signals[i] != 0; i++) {
        sigstatus = sigaction(signals[i], &saction, NULL);
        if (sigstatus != 0) {
            logmsg(FATAL, "Failed to set the iRODS client handler for signal %s", strsignal(signals[i]));
            return -1;
        }
    }

    return 0;
}
