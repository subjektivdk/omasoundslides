#pragma once

// True when the first argument is a command-line command (render, info, …)
// rather than a project to open in the window.
bool isCliCommand(const char *arg);

int runCli(int argc, char *argv[]);
