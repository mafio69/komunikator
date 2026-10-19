#include "proces.hpp"

#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>

extern char** environ;

std::string wyjscie_programu(std::vector<std::string> args, bool& ok) {
    ok = false;
    int rura[2];
    if (pipe2(rura, O_CLOEXEC) != 0) return "";
    posix_spawn_file_actions_t akcje;
    posix_spawn_file_actions_init(&akcje);
    posix_spawn_file_actions_adddup2(&akcje, rura[1], STDOUT_FILENO);
    posix_spawn_file_actions_addopen(&akcje, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    std::vector<char*> argv;
    for (auto& a : args) argv.push_back(a.data());
    argv.push_back(nullptr);
    pid_t pid;
    int r = posix_spawnp(&pid, argv[0], &akcje, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&akcje);
    close(rura[1]);
    std::string w;
    if (r == 0) {
        char bufor[65536];
        for (ssize_t n; (n = read(rura[0], bufor, sizeof bufor)) != 0;) {
            if (n < 0) {
                if (errno == EINTR) continue;
                break;
            }
            w.append(bufor, static_cast<size_t>(n));
        }
        int status = 0;
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
        }
        ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
    close(rura[0]);
    return w;
}
