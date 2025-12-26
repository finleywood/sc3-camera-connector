#pragma once

#include <iostream>
#include <cstdio>
#include <string>
#include <thread>
#include <future>
#include <chrono>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

namespace Util {

inline static std::string getCmdOutput(std::string cmd, bool remove_whitespace, int* rt_code = nullptr, int timeout_sec = 0) {
    auto task = std::async(std::launch::async, [cmd, remove_whitespace, rt_code]() -> std::string {
        FILE* proc = popen(cmd.c_str(), "r");
        if (!proc) return "";

        char buffer[256];
        std::string result;

        while (fgets(buffer, sizeof(buffer), proc)) {
            result += buffer;
        }

        int status = pclose(proc);
        if (rt_code != nullptr) {
            if (status != -1) {
                if (WIFEXITED(status)) {
                    *rt_code = WEXITSTATUS(status);
                } else {
                    *rt_code = -1;
                }
            } else {
                *rt_code = -1;
            }
        }

        if (remove_whitespace) {
            result.erase(result.find_last_not_of(" \n\r\t") + 1);
        }

        return result;
    });

    if (timeout_sec > 0) {
        if (task.wait_for(std::chrono::seconds(timeout_sec)) == std::future_status::ready) {
            return task.get();
        } else {
            // Timeout reached
            if (rt_code) *rt_code = -1;
            return "";  // optionally indicate timeout
        }
    } else {
        return task.get();  // no timeout, just return normally
    }
}

}