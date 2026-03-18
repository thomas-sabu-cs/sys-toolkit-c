#define _XOPEN_SOURCE 700

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifndef MAP_ANONYMOUS
#ifdef MAP_ANON
#define MAP_ANONYMOUS MAP_ANON
#else
/* Fallback for platforms where MAP_ANONYMOUS is not exposed with current feature macros.
 * On Linux this value is 0x20; on other platforms, anonymous mappings may not be available. */
#define MAP_ANONYMOUS 0x20
#endif
#endif

typedef struct cpu_sample {
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
    unsigned long long iowait;
    unsigned long long irq;
    unsigned long long softirq;
    unsigned long long steal;
} cpu_sample;

typedef struct proc_info {
    pid_t pid;
    int threads;
    char name[64];
} proc_info;

static volatile sig_atomic_t g_stop = 0;

static void handle_signal(int signo)
{
    (void)signo;
    g_stop = 1;
}

static void install_signal_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    (void)sigaction(SIGINT, &sa, NULL);
    (void)sigaction(SIGTERM, &sa, NULL);
}

static void term_enter_alt_screen(void)
{
    (void)printf("\x1b[?1049h\x1b[2J\x1b[H\x1b[?25l");
    (void)fflush(stdout);
}

static void term_leave_alt_screen(void)
{
    (void)printf("\x1b[?1049l\x1b[?25h");
    (void)fflush(stdout);
}

static void sleep_millis(long millis)
{
    struct timespec ts;
    ts.tv_sec = millis / 1000;
    ts.tv_nsec = (millis % 1000) * 1000000L;
    (void)nanosleep(&ts, NULL);
}

static int read_cpu_samples(cpu_sample *samples, size_t max_cpus, size_t *out_count)
{
    FILE *f = fopen("/proc/stat", "r");
    if (f == NULL) {
        return -1;
    }

    char line[256];
    size_t count = 0U;

    while (fgets(line, sizeof(line), f) != NULL) {
        if (strncmp(line, "cpu", 3) != 0) {
            break;
        }

        if (line[3] == ' ' || line[3] == '\t') {
            continue;
        }

        if (count >= max_cpus) {
            break;
        }

        cpu_sample s;
        memset(&s, 0, sizeof(s));

        const int scanned = sscanf(line,
                                   "cpu%*u %llu %llu %llu %llu %llu %llu %llu %llu",
                                   &s.user, &s.nice, &s.system, &s.idle,
                                   &s.iowait, &s.irq, &s.softirq, &s.steal);
        if (scanned < 4) {
            continue;
        }

        samples[count] = s;
        count++;
    }

    (void)fclose(f);
    *out_count = count;
    return 0;
}

static double cpu_usage_between(const cpu_sample *prev, const cpu_sample *cur)
{
    const unsigned long long prev_idle = prev->idle + prev->iowait;
    const unsigned long long idle = cur->idle + cur->iowait;

    const unsigned long long prev_total = prev->user + prev->nice + prev->system +
                                          prev->idle + prev->iowait + prev->irq +
                                          prev->softirq + prev->steal;
    const unsigned long long total = cur->user + cur->nice + cur->system +
                                     cur->idle + cur->iowait + cur->irq +
                                     cur->softirq + cur->steal;

    const unsigned long long totald = total - prev_total;
    const unsigned long long idled = idle - prev_idle;

    if (totald == 0ULL) {
        return 0.0;
    }

    const double usage = (double)(totald - idled) / (double)totald * 100.0;
    if (usage < 0.0) {
        return 0.0;
    }
    if (usage > 100.0) {
        return 100.0;
    }
    return usage;
}

static int read_mem_pressure(double *out_pressure)
{
    FILE *f = fopen("/proc/meminfo", "r");
    if (f == NULL) {
        return -1;
    }

    char line[256];
    unsigned long long mem_total = 0ULL;
    unsigned long long mem_available = 0ULL;

    while (fgets(line, sizeof(line), f) != NULL) {
        if (sscanf(line, "MemTotal: %llu kB", &mem_total) == 1) {
            continue;
        }
        if (sscanf(line, "MemAvailable: %llu kB", &mem_available) == 1) {
            continue;
        }
    }

    (void)fclose(f);

    if (mem_total == 0ULL) {
        return -1;
    }

    const double used = (double)(mem_total - mem_available);
    const double pressure = used / (double)mem_total;
    *out_pressure = pressure;
    return 0;
}

static int read_process_threads(proc_info *buf, size_t buf_capacity, size_t *out_count)
{
    DIR *dir = opendir("/proc");
    if (dir == NULL) {
        return -1;
    }

    size_t count = 0U;
    struct dirent *ent;

    while ((ent = readdir(dir)) != NULL) {
        char *endptr = NULL;
        const long pid_long = strtol(ent->d_name, &endptr, 10);
        if (ent->d_name[0] == '\0' || *endptr != '\0') {
            continue;
        }
        if (pid_long <= 0L) {
            continue;
        }

        pid_t pid = (pid_t)pid_long;

        char path[256];
        (void)snprintf(path, sizeof(path), "/proc/%ld/status", pid_long);

        FILE *f = fopen(path, "r");
        if (f == NULL) {
            continue;
        }

        char line[256];
        int threads = 0;
        char name[64];
        name[0] = '\0';

        while (fgets(line, sizeof(line), f) != NULL) {
            if (strncmp(line, "Name:", 5) == 0) {
                (void)sscanf(line, "Name: %63s", name);
            } else if (strncmp(line, "Threads:", 8) == 0) {
                (void)sscanf(line, "Threads: %d", &threads);
            }
        }

        (void)fclose(f);

        if (threads <= 0) {
            continue;
        }

        if (count < buf_capacity) {
            buf[count].pid = pid;
            buf[count].threads = threads;
            if (name[0] != '\0') {
                (void)snprintf(buf[count].name, sizeof(buf[count].name), "%s", name);
            } else {
                (void)snprintf(buf[count].name, sizeof(buf[count].name), "pid-%ld", pid_long);
            }
            count++;
        }
    }

    (void)closedir(dir);
    *out_count = count;
    return 0;
}

static void select_top_by_threads(proc_info *buf, size_t count, size_t top_n)
{
    for (size_t i = 0U; i < count; ++i) {
        size_t max_idx = i;
        for (size_t j = i + 1U; j < count; ++j) {
            if (buf[j].threads > buf[max_idx].threads) {
                max_idx = j;
            }
        }
        if (max_idx != i) {
            proc_info tmp = buf[i];
            buf[i] = buf[max_idx];
            buf[max_idx] = tmp;
        }
        if (i + 1U >= top_n) {
            break;
        }
    }
}

static void draw_dashboard(const cpu_sample *prev, const cpu_sample *cur, size_t cpu_count,
                           double mem_pressure, const proc_info *procs, size_t proc_count)
{
    (void)printf("\x1b[2J\x1b[H");
    (void)printf("sys-monitor (sys-toolkit-c)\n");
    (void)printf("q = quit | Ctrl-C = quit\n\n");

    (void)printf("CPU usage per core:\n");
    for (size_t i = 0U; i < cpu_count; ++i) {
        const double usage = cpu_usage_between(&prev[i], &cur[i]);
        (void)printf("  cpu%zu: %6.2f%%\n", i, usage);
    }

    (void)printf("\nMemory pressure:\n");
    (void)printf("  %.2f%% used (pressure)\n", mem_pressure * 100.0);

    (void)printf("\nTop 5 processes by thread count:\n");
    (void)printf("  %-6s %-6s %-32s\n", "PID", "THR", "NAME");

    const size_t n = proc_count < 5U ? proc_count : 5U;
    for (size_t i = 0U; i < n; ++i) {
        (void)printf("  %-6d %-6d %-32s\n",
                     (int)procs[i].pid, procs[i].threads, procs[i].name);
    }

    (void)fflush(stdout);
}

int main(void)
{
    const size_t max_cpus = 128U;
    const size_t max_procs = 4096U;

    size_t cpu_bytes = max_cpus * sizeof(cpu_sample);
    cpu_sample *prev_samples = (cpu_sample *)mmap(NULL, cpu_bytes,
                                                  PROT_READ | PROT_WRITE,
                                                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    cpu_sample *cur_samples = (cpu_sample *)mmap(NULL, cpu_bytes,
                                                 PROT_READ | PROT_WRITE,
                                                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (prev_samples == MAP_FAILED || cur_samples == MAP_FAILED) {
        (void)fprintf(stderr, "sys-monitor: mmap failed for CPU samples\n");
        return 1;
    }

    size_t proc_bytes = max_procs * sizeof(proc_info);
    proc_info *proc_buf = (proc_info *)mmap(NULL, proc_bytes,
                                            PROT_READ | PROT_WRITE,
                                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (proc_buf == MAP_FAILED) {
        (void)fprintf(stderr, "sys-monitor: mmap failed for process buffer\n");
        (void)munmap(prev_samples, cpu_bytes);
        (void)munmap(cur_samples, cpu_bytes);
        return 1;
    }

    install_signal_handlers();
    term_enter_alt_screen();

    size_t cpu_count = 0U;
    if (read_cpu_samples(prev_samples, max_cpus, &cpu_count) != 0 || cpu_count == 0U) {
        term_leave_alt_screen();
        (void)fprintf(stderr, "sys-monitor: failed to read /proc/stat\n");
        (void)munmap(prev_samples, cpu_bytes);
        (void)munmap(cur_samples, cpu_bytes);
        (void)munmap(proc_buf, proc_bytes);
        return 1;
    }

    while (!g_stop) {
        sleep_millis(500);

        size_t cur_cpu_count = 0U;
        if (read_cpu_samples(cur_samples, max_cpus, &cur_cpu_count) != 0 || cur_cpu_count != cpu_count) {
            break;
        }

        double mem_pressure = 0.0;
        if (read_mem_pressure(&mem_pressure) != 0) {
            break;
        }

        size_t proc_count = 0U;
        if (read_process_threads(proc_buf, max_procs, &proc_count) != 0) {
            break;
        }

        select_top_by_threads(proc_buf, proc_count, 5U);
        draw_dashboard(prev_samples, cur_samples, cpu_count, mem_pressure, proc_buf, proc_count);

        cpu_sample *tmp = prev_samples;
        prev_samples = cur_samples;
        cur_samples = tmp;
    }

    term_leave_alt_screen();

    (void)munmap(prev_samples, cpu_bytes);
    (void)munmap(cur_samples, cpu_bytes);
    (void)munmap(proc_buf, proc_bytes);

    return 0;
}

