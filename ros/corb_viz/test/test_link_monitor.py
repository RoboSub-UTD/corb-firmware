import unittest

from corb_viz.link_monitor import ERROR, OK, WARN, LinkMonitor, state_name


def feed(mon, t0, n, code=2, fresh=False, up0=0, dt=0.1):
    for i in range(n):
        mon.on_state(t0 + i * dt, code, fresh, up0 + int(i * dt * 1000))
    return t0 + (n - 1) * dt


class LinkMonitorTest(unittest.TestCase):
    def test_no_data_is_error(self):
        mon = LinkMonitor()
        self.assertEqual(mon.level(0.0), ERROR)
        self.assertIsNone(mon.age(0.0))

    def test_rate_and_ok(self):
        mon = LinkMonitor()
        end = feed(mon, 10.0, 20)
        self.assertAlmostEqual(mon.rate_hz(end), 10.0, places=3)
        self.assertEqual(mon.level(end), OK)
        self.assertEqual(mon.state, "Disarmed")

    def test_stale_and_recovery_are_logged(self):
        mon = LinkMonitor(stale_after_s=0.5)
        end = feed(mon, 0.0, 5)
        mon.poll(end + 1.0)
        self.assertEqual(mon.level(end + 1.0), ERROR)
        mon.poll(end + 1.1)  # must not log twice
        self.assertEqual(sum("link lost" in t for _, t in mon.events), 1)
        mon.on_state(end + 1.2, 2, False, 2000)
        self.assertTrue(any("recovered" in t for _, t in mon.events))
        self.assertEqual(mon.level(end + 1.2), OK)

    def test_state_change_and_levels(self):
        mon = LinkMonitor()
        mon.on_state(0.0, 2, False, 0)
        mon.on_state(0.1, 4, True, 100)
        self.assertTrue(any("Disarmed -> Armed" in t for _, t in mon.events))
        self.assertTrue(any("command fresh" in t for _, t in mon.events))
        mon.on_state(0.2, 5, True, 200)
        self.assertEqual(mon.level(0.2), WARN)
        mon.on_state(0.3, 8, True, 300)
        self.assertEqual(mon.level(0.3), ERROR)

    def test_reboot_detected_but_not_wrap(self):
        mon = LinkMonitor()
        mon.on_state(0.0, 2, False, 50_000)
        mon.on_state(0.1, 2, False, 120)
        self.assertEqual(mon.reboots, 1)

    def test_uptime_wrap_is_not_a_reboot(self):
        mon = LinkMonitor()
        mon.on_state(0.0, 2, False, 0xFFFFFFF0)
        mon.on_state(0.1, 2, False, 0x10)  # u32 ms counter wrapped
        self.assertEqual(mon.reboots, 0)

    def test_unknown_state(self):
        self.assertEqual(state_name(42), "Unknown(42)")


if __name__ == "__main__":
    unittest.main()
