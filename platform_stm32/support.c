/* Runtime glue: HAL time base, newlib syscalls, and micro-ROS time. */
#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

#include "stm32h7xx.h"
#include "stm32h7xx_hal.h"

#include "FreeRTOS.h"
#include "task.h"

extern void board_console_put(char c);

/* ---- HAL time base ----
 * HAL only needs a millisecond counter for RCC/PWR timeouts, and runs before the scheduler
 * (which owns SysTick). Derive it from the DWT cycle counter instead. */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
  (void)TickPriority;
  return HAL_OK;
}

uint32_t HAL_GetTick(void)
{
  static uint32_t last;
  static uint32_t ms;
  static uint32_t rem;
  const uint32_t per_ms = SystemCoreClock / 1000u;
  const uint32_t now = DWT->CYCCNT;
  rem += now - last;
  last = now;
  ms += rem / per_ms;
  rem %= per_ms;
  return ms;
}

/* ---- newlib ---- */
int _write(int fd, const char *buf, int len)
{
  (void)fd;
  for (int i = 0; i < len; i++)
  {
    board_console_put(buf[i]);
  }
  return len;
}

int _read(int fd, char *buf, int len)
{
  (void)fd; (void)buf; (void)len;
  errno = EBADF;
  return -1;
}

int _close(int fd) { (void)fd; errno = EBADF; return -1; }
int _fstat(int fd, struct stat *st) { (void)fd; st->st_mode = S_IFCHR; return 0; }
int _isatty(int fd) { (void)fd; return 1; }
int _lseek(int fd, int off, int whence) { (void)fd; (void)off; (void)whence; return 0; }
int _getpid(void) { return 1; }
int _kill(int pid, int sig) { (void)pid; (void)sig; errno = EINVAL; return -1; }
void _exit(int status) { (void)status; for (;;) {} }

/* ---- time (micro-ROS reads CLOCK_MONOTONIC / REALTIME) ---- */
int _gettimeofday(struct timeval *tv, void *tz)
{
  (void)tz;
  const TickType_t t = xTaskGetTickCount();
  tv->tv_sec = (time_t)(t / 1000u);
  tv->tv_usec = (suseconds_t)((t % 1000u) * 1000u);
  return 0;
}

int clock_gettime(clockid_t clk, struct timespec *ts)
{
  (void)clk;
  const TickType_t t = xTaskGetTickCount();
  ts->tv_sec = (time_t)(t / 1000u);
  ts->tv_nsec = (long)((t % 1000u) * 1000000u);
  return 0;
}
