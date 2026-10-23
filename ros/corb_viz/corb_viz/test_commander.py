"""Bench-test command source: publishes /vcu/thruster_cmd so there is something to look at.

Not part of the control stack. Patterns: sine (default), ramp, zero.
"""

import math

import rclpy
from corb_msgs.msg import ThrusterCommand
from rclpy.node import Node


class TestCommander(Node):
    def __init__(self):
        super().__init__("corb_test_commander")
        self.declare_parameter("pattern", "sine")
        self.declare_parameter("rate_hz", 20.0)
        self.declare_parameter("amplitude", 0.3)  # keep it small on a real vehicle
        self.declare_parameter("period_s", 6.0)
        self.declare_parameter("thruster_count", 4)
        self.pub = self.create_publisher(ThrusterCommand, "/vcu/thruster_cmd", 10)
        self.seq = 0
        self.t0 = self.get_clock().now()
        self.create_timer(1.0 / float(self.get_parameter("rate_hz").value), self._tick)

    def _tick(self):
        p = self.get_parameter
        t = (self.get_clock().now() - self.t0).nanoseconds * 1e-9
        amp = max(0.0, min(1.0, float(p("amplitude").value)))
        period = float(p("period_s").value)
        n = max(1, min(8, int(p("thruster_count").value)))
        pattern = p("pattern").value

        m = ThrusterCommand()
        m.seq = self.seq
        m.count = n
        vals = [0.0] * 8
        for i in range(n):
            if pattern == "sine":  # phase-shifted per thruster so each bar is distinguishable
                vals[i] = amp * math.sin(2 * math.pi * (t / period + i / n))
            elif pattern == "ramp":
                vals[i] = amp * (2.0 * ((t / period + i / n) % 1.0) - 1.0)
        m.values = vals
        self.pub.publish(m)
        self.seq = (self.seq + 1) & 0xFFFFFFFF


def main():
    rclpy.init()
    node = TestCommander()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.destroy_node()
    rclpy.try_shutdown()
