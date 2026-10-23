"""Link/state tracking for the VCU. No ROS imports so it can be unit tested anywhere."""

from collections import deque

# Mirrors corb::State in core/include/corb/state_machine.hpp.
STATE_NAMES = (
    "Boot",
    "SelfCheck",
    "Disarmed",
    "PreArm",
    "Armed",
    "Killed",
    "Recovering",
    "RecoverableFault",
    "Fault",
)

OK, WARN, ERROR = 0, 1, 2

STATE_LEVEL = {
    "Boot": OK,
    "SelfCheck": OK,
    "Disarmed": OK,
    "PreArm": OK,
    "Armed": OK,
    "Killed": WARN,
    "Recovering": WARN,
    "RecoverableFault": WARN,
    "Fault": ERROR,
}

# (r, g, b) per state, for markers.
STATE_COLOR = {
    "Boot": (0.6, 0.6, 0.6),
    "SelfCheck": (0.6, 0.6, 0.9),
    "Disarmed": (0.3, 0.5, 1.0),
    "PreArm": (1.0, 0.85, 0.2),
    "Armed": (0.1, 0.9, 0.2),
    "Killed": (1.0, 0.5, 0.0),
    "Recovering": (1.0, 0.7, 0.3),
    "RecoverableFault": (1.0, 0.3, 0.1),
    "Fault": (0.9, 0.0, 0.0),
}

_U32 = 1 << 32


def state_name(code):
    if 0 <= code < len(STATE_NAMES):
        return STATE_NAMES[code]
    return "Unknown(%d)" % code


class LinkMonitor:
    """Feed it every /vcu/state sample; ask it for link health at any time.

    All times are seconds on one monotonic clock chosen by the caller.
    """

    def __init__(self, stale_after_s=0.5, rate_window_s=2.0, log_len=8):
        self.stale_after_s = stale_after_s
        self.rate_window_s = rate_window_s
        self.events = deque(maxlen=log_len)  # (t, text)
        self._times = deque()
        self._last_rx = None
        self._was_stale = False

        self.state = None  # name
        self.state_code = None
        self.command_fresh = False
        self.uptime_ms = 0
        self.reboots = 0
        self.max_gap_s = 0.0

    def _log(self, now, text):
        self.events.append((now, text))

    def on_state(self, now, code, command_fresh, uptime_ms):
        if self._last_rx is not None:
            gap = now - self._last_rx
            self.max_gap_s = max(self.max_gap_s, gap)
            if gap > self.stale_after_s:
                self._log(now, "link recovered after %.2fs" % gap)
        self._was_stale = False

        # A drop in uptime means the MCU rebooted, unless it is the u32 ms wrap (~49.7 days).
        if self._last_rx is not None and (self.uptime_ms - uptime_ms) % _U32 < _U32 // 2 \
                and uptime_ms != self.uptime_ms:
            self.reboots += 1
            self._log(now, "MCU reboot (uptime %d -> %d ms)" % (self.uptime_ms, uptime_ms))

        name = state_name(code)
        if name != self.state:
            prev = self.state or "-"
            self._log(now, "state %s -> %s" % (prev, name))
        if self.state is not None and command_fresh != self.command_fresh:
            self._log(now, "command %s" % ("fresh" if command_fresh else "stale"))

        self.state = name
        self.state_code = code
        self.command_fresh = command_fresh
        self.uptime_ms = uptime_ms

        self._last_rx = now
        self._times.append(now)
        self._trim(now)

    def _trim(self, now):
        while self._times and now - self._times[0] > self.rate_window_s:
            self._times.popleft()

    def age(self, now):
        """Seconds since the last state sample, or None if none was ever received."""
        return None if self._last_rx is None else now - self._last_rx

    def is_stale(self, now):
        a = self.age(now)
        return a is None or a > self.stale_after_s

    def poll(self, now):
        """Call periodically; logs the moment the link is lost."""
        if self._last_rx is not None and self.is_stale(now) and not self._was_stale:
            self._was_stale = True
            self._log(now, "link lost (no /vcu/state for %.2fs)" % self.age(now))

    def rate_hz(self, now):
        self._trim(now)
        if len(self._times) < 2:
            return 0.0
        span = self._times[-1] - self._times[0]
        return (len(self._times) - 1) / span if span > 0 else 0.0

    def level(self, now):
        if self.state is None or self.is_stale(now):
            return ERROR
        return STATE_LEVEL.get(self.state, WARN)
