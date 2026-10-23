"""Visualizes a real VCU: /vcu/state and /vcu/thruster_cmd -> RViz markers, diagnostics, plot topics.

Everything shown comes from what the firmware publishes. The VCU does not report the
thruster values it actually applied, so the bars show the *commanded* values; if the
state says Armed/Guided/fresh is false they are not reaching the thrusters.
"""

import math

import rclpy
from corb_msgs.msg import ThrusterCommand, VcuState
from diagnostic_msgs.msg import DiagnosticArray, DiagnosticStatus, KeyValue
from geometry_msgs.msg import Point
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from std_msgs.msg import ColorRGBA, Float32, UInt8
from visualization_msgs.msg import Marker, MarkerArray

from corb_viz.link_monitor import ERROR, OK, STATE_COLOR, WARN, LinkMonitor

_LEVELS = {OK: DiagnosticStatus.OK, WARN: DiagnosticStatus.WARN, ERROR: DiagnosticStatus.ERROR}


def _color(r, g, b, a=1.0):
    return ColorRGBA(r=float(r), g=float(g), b=float(b), a=float(a))


class VizNode(Node):
    def __init__(self):
        super().__init__("corb_viz")
        self.declare_parameter("frame_id", "corb_base")
        self.declare_parameter("thruster_count", 4)
        self.declare_parameter("ring_radius", 0.6)  # m, thrusters are drawn on a ring
        self.declare_parameter("bar_height", 0.5)  # m of bar per 1.0 of command
        self.declare_parameter("stale_after_s", 0.5)
        self.declare_parameter("cmd_stale_after_s", 0.5)
        self.declare_parameter("publish_hz", 10.0)

        p = self.get_parameter
        self.frame = p("frame_id").value
        self.n = max(1, min(8, int(p("thruster_count").value)))
        self.radius = float(p("ring_radius").value)
        self.bar_h = float(p("bar_height").value)
        self.cmd_stale_s = float(p("cmd_stale_after_s").value)

        self.mon = LinkMonitor(stale_after_s=float(p("stale_after_s").value))
        self.cmd = [0.0] * 8
        self.cmd_count = 0
        self.cmd_seq = None
        self.cmd_rx = None
        self.cmd_drops = 0  # seq jumps seen on our side (agent/network loss)
        self.cmd_rate_times = []

        # The firmware side is best effort; match it.
        self.create_subscription(VcuState, "/vcu/state", self._on_state, qos_profile_sensor_data)
        self.create_subscription(
            ThrusterCommand, "/vcu/thruster_cmd", self._on_cmd, qos_profile_sensor_data
        )

        self.markers = self.create_publisher(MarkerArray, "~/markers", 1)
        self.diag = self.create_publisher(DiagnosticArray, "/diagnostics", 1)
        # Flat topics for PlotJuggler / rqt_plot.
        self.pub_state = self.create_publisher(UInt8, "~/state_code", 1)
        self.pub_rate = self.create_publisher(Float32, "~/state_rate_hz", 1)
        self.pub_age = self.create_publisher(Float32, "~/state_age_ms", 1)
        self.pub_uptime = self.create_publisher(Float32, "~/uptime_s", 1)
        self.pub_thr = [self.create_publisher(Float32, "~/thruster/%d" % i, 1) for i in range(self.n)]

        self.create_timer(1.0 / float(p("publish_hz").value), self._tick)
        self.get_logger().info("waiting for /vcu/state (is the micro-ROS agent up?)")

    def _now(self):
        return self.get_clock().now().nanoseconds * 1e-9

    def _on_state(self, m):
        first = self.mon.state is None
        self.mon.on_state(self._now(), m.state, m.command_fresh, m.uptime_ms)
        if first:
            self.get_logger().info("VCU connected, state %s" % self.mon.state)

    def _on_cmd(self, m):
        now = self._now()
        if self.cmd_seq is not None:
            jump = (m.seq - self.cmd_seq) & 0xFFFFFFFF
            # jump == 0 or >= 2^31 is a duplicate/stale seq, which the VCU rejects too
            if 1 < jump < 0x80000000:
                self.cmd_drops += jump - 1
        self.cmd_seq = m.seq
        self.cmd_rx = now
        self.cmd_count = m.count
        self.cmd = list(m.values)
        self.cmd_rate_times = [t for t in self.cmd_rate_times if now - t < 2.0] + [now]

    def _cmd_rate(self, now):
        ts = [t for t in self.cmd_rate_times if now - t < 2.0]
        if len(ts) < 2 or ts[-1] == ts[0]:
            return 0.0
        return (len(ts) - 1) / (ts[-1] - ts[0])

    def _tick(self):
        now = self._now()
        self.mon.poll(now)
        stamp = self.get_clock().now().to_msg()
        self._publish_markers(now, stamp)
        self._publish_diag(now, stamp)
        self._publish_plots(now)

    # -- outputs ---------------------------------------------------------------------------

    def _marker(self, mid, mtype, stamp, ns="corb"):
        m = Marker()
        m.header.frame_id = self.frame
        m.header.stamp = stamp
        m.ns = ns
        m.id = mid
        m.type = mtype
        m.action = Marker.ADD
        m.pose.orientation.w = 1.0
        return m

    def _thruster_xy(self, i):
        a = 2.0 * math.pi * i / self.n
        return self.radius * math.cos(a), self.radius * math.sin(a)

    def _publish_markers(self, now, stamp):
        out = MarkerArray()
        stale = self.mon.is_stale(now)
        name = "NO LINK" if self.mon.state is None else self.mon.state
        rgb = (0.5, 0.5, 0.5) if stale else STATE_COLOR.get(self.mon.state, (1.0, 0.0, 1.0))

        body = self._marker(0, Marker.CYLINDER, stamp)
        body.scale.x = body.scale.y = 2.0 * self.radius
        body.scale.z = 0.05
        body.color = _color(*rgb, a=0.5)
        out.markers.append(body)

        title = self._marker(1, Marker.TEXT_VIEW_FACING, stamp)
        title.pose.position.z = self.bar_h + 0.5
        title.scale.z = 0.18
        title.color = _color(*rgb)
        title.text = "%s%s\nup %.1fs  %.1f Hz  cmd %s" % (
            name,
            " (STALE)" if stale and self.mon.state is not None else "",
            self.mon.uptime_ms / 1000.0,
            self.mon.rate_hz(now),
            "fresh" if (self.mon.command_fresh and not stale) else "stale",
        )
        out.markers.append(title)

        log = self._marker(2, Marker.TEXT_VIEW_FACING, stamp)
        log.pose.position.z = -0.4
        log.scale.z = 0.07
        log.color = _color(0.9, 0.9, 0.9)
        log.text = "\n".join("-%5.1fs  %s" % (now - t, s) for t, s in list(self.mon.events)[::-1]) or "no events"
        out.markers.append(log)

        cmd_stale = self.cmd_rx is None or now - self.cmd_rx > self.cmd_stale_s
        for i in range(self.n):
            x, y = self._thruster_xy(i)
            v = 0.0 if cmd_stale else max(-1.0, min(1.0, self.cmd[i]))
            bar = self._marker(10 + i, Marker.CUBE, stamp)
            h = max(abs(v) * self.bar_h, 0.005)
            bar.pose.position.x, bar.pose.position.y = x, y
            bar.pose.position.z = math.copysign(h / 2.0, v) if v else h / 2.0
            bar.scale.x = bar.scale.y = 0.12
            bar.scale.z = h
            bar.color = _color(0.1, 0.8, 0.2) if v >= 0 else _color(0.9, 0.2, 0.2)
            if cmd_stale:
                bar.color = _color(0.5, 0.5, 0.5, 0.6)
            out.markers.append(bar)

            lbl = self._marker(30 + i, Marker.TEXT_VIEW_FACING, stamp)
            lbl.pose.position.x, lbl.pose.position.y = x, y
            lbl.pose.position.z = -0.12
            lbl.scale.z = 0.07
            lbl.color = _color(1.0, 1.0, 1.0)
            lbl.text = "T%d %+.2f" % (i, v)
            out.markers.append(lbl)

        self.markers.publish(out)

    def _publish_diag(self, now, stamp):
        arr = DiagnosticArray()
        arr.header.stamp = stamp

        s = DiagnosticStatus(name="corb/vcu", hardware_id="corb")
        s.level = bytes([_LEVELS[self.mon.level(now)]])
        if self.mon.state is None:
            s.message = "no /vcu/state received"
        elif self.mon.is_stale(now):
            s.message = "link lost (last state %s)" % self.mon.state
        else:
            s.message = self.mon.state
        age = self.mon.age(now)
        s.values = [
            KeyValue(key="state", value=str(self.mon.state)),
            KeyValue(key="command_fresh", value=str(self.mon.command_fresh)),
            KeyValue(key="uptime_s", value="%.1f" % (self.mon.uptime_ms / 1000.0)),
            KeyValue(key="state_rate_hz", value="%.2f" % self.mon.rate_hz(now)),
            KeyValue(key="state_age_ms", value="n/a" if age is None else "%.0f" % (age * 1000)),
            KeyValue(key="max_state_gap_s", value="%.2f" % self.mon.max_gap_s),
            KeyValue(key="mcu_reboots", value=str(self.mon.reboots)),
        ]
        arr.status.append(s)

        c = DiagnosticStatus(name="corb/thruster_cmd", hardware_id="corb")
        if self.cmd_rx is None:
            c.level = bytes([DiagnosticStatus.STALE])
            c.message = "no /vcu/thruster_cmd seen"
        elif now - self.cmd_rx > self.cmd_stale_s:
            c.level = bytes([DiagnosticStatus.WARN])
            c.message = "command stream stopped"
        else:
            c.level = bytes([DiagnosticStatus.OK])
            c.message = "streaming"
        c.values = [
            KeyValue(key="seq", value=str(self.cmd_seq)),
            KeyValue(key="count", value=str(self.cmd_count)),
            KeyValue(key="rate_hz", value="%.1f" % self._cmd_rate(now)),
            KeyValue(key="seq_gaps_seen", value=str(self.cmd_drops)),
        ]
        arr.status.append(c)
        self.diag.publish(arr)

    def _publish_plots(self, now):
        if self.mon.state_code is not None:
            self.pub_state.publish(UInt8(data=int(self.mon.state_code)))
            self.pub_uptime.publish(Float32(data=self.mon.uptime_ms / 1000.0))
        self.pub_rate.publish(Float32(data=float(self.mon.rate_hz(now))))
        age = self.mon.age(now)
        if age is not None:
            self.pub_age.publish(Float32(data=float(age * 1000.0)))
        for i, pub in enumerate(self.pub_thr):
            pub.publish(Float32(data=float(self.cmd[i])))


def main():
    rclpy.init()
    node = VizNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.destroy_node()
    rclpy.try_shutdown()
