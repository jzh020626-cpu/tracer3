#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import math
import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist, PoseStamped
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt


def wrap_angle(a: float) -> float:
    while a > math.pi:
        a -= 2.0 * math.pi
    while a < -math.pi:
        a += 2.0 * math.pi
    return a


class GotoPoseRigid15(Node):
    """
    Goto Pose (point + heading) 控制器，适配 Rigid15/pose：
      - 位置：x–z 平面（单位 mm）
      - 航向：orientation.y（单位 °）
    工作流程：
      1) 锁定目标（relative 或 absolute）
      2) 阶段1：Pure Pursuit 到点
      3) 阶段2：P 控制对齐航向
      4) 完成后打印终点误差
    """

    def __init__(self):
        super().__init__('goto_pose_rigid15')

        # ===== 声明参数 =====
        # --- 话题相关 ---
        self.declare_parameter('cmd_topic', '/tracer2/cmd_vel')   # 控制输出话题
        self.declare_parameter('mocap_topic', 'Rigid14/pose')     # 动捕输入话题

        # --- 目标模式 ---
        self.declare_parameter('goal_mode', 'absolute')   # 'absolute' 或 'relative'

        # --- 相对目标模式参数（单位 m / °）---
        self.declare_parameter('rel_dx_m', 3.0)          # 相对 X 方向偏移（世界系）
        self.declare_parameter('rel_dy_m', 0.0)          # 相对 Y 方向偏移（世界系）
        self.declare_parameter('rel_dyaw_deg', 0.0)      # 相对航向变化（+左转）

        # --- 绝对目标模式参数（单位 mm / °）---
        self.declare_parameter('abs_goal_x_mm', 9660.0)       # 动捕坐标系目标 x (mm)
        self.declare_parameter('abs_goal_z_mm', 4863.0)       # 动捕坐标系目标 z (mm)
        self.declare_parameter('abs_goal_yaw_deg', -90.0)     # 目标航向 (deg)

        # --- 控制参数 ---
        self.declare_parameter('v_nominal', 0.05)        # 名义前进速度 (m/s)
        self.declare_parameter('Ld', 0.6)                # Pure Pursuit 前视距离 (m)
        self.declare_parameter('v_max', 0.7)             # 最大线速度 (m/s)
        self.declare_parameter('w_max', 2.0)             # 最大角速度 (rad/s)
        self.declare_parameter('pos_tol', 0.05)          # 位置收敛判定阈值 (m)
        self.declare_parameter('k_yaw', 1.2)             # 航向阶段角度控制增益
        self.declare_parameter('yaw_tol_deg', 1.0)       # 航向收敛阈值 (deg)
        self.declare_parameter('w_yaw_max', 1.2)         # 航向阶段最大角速度 (rad/s)
        self.declare_parameter('v_align', 0.0)           # 航向阶段线速度 (m/s)

        # ===== 读取参数 =====
        self.cmd_topic = self.get_parameter('cmd_topic').value
        self.mocap_topic = self.get_parameter('mocap_topic').value
        self.goal_mode = self.get_parameter('goal_mode').value

        self.rel_dx_m = self.get_parameter('rel_dx_m').value
        self.rel_dy_m = self.get_parameter('rel_dy_m').value
        self.rel_dyaw_deg = self.get_parameter('rel_dyaw_deg').value

        self.abs_goal_x_mm = self.get_parameter('abs_goal_x_mm').value
        self.abs_goal_z_mm = self.get_parameter('abs_goal_z_mm').value
        self.abs_goal_yaw_deg = self.get_parameter('abs_goal_yaw_deg').value

        self.v_nominal = self.get_parameter('v_nominal').value
        self.Ld = self.get_parameter('Ld').value
        self.v_max = self.get_parameter('v_max').value
        self.w_max = self.get_parameter('w_max').value
        self.pos_tol = self.get_parameter('pos_tol').value
        self.k_yaw = self.get_parameter('k_yaw').value
        self.yaw_tol_deg = self.get_parameter('yaw_tol_deg').value
        self.w_yaw_max = self.get_parameter('w_yaw_max').value
        self.v_align = self.get_parameter('v_align').value

        # ===== 其他固定参数（根据你现场情况）=====
        self.mm_to_m = 0.001
        self.swap_xz = False
        self.negate_x = False
        self.negate_z = True
        self.flip_heading_sign = False
        self.heading_deg_bias = 0.0

        # ===== 发布/订阅 =====
        self.cmd_pub = self.create_publisher(Twist, self.cmd_topic, 10)
        self.mocap_sub = self.create_subscription(
            PoseStamped, self.mocap_topic, self.mocap_cb, 30
        )

        # ===== 定时器 =====
        self.dt = 0.02
        self.timer = self.create_timer(self.dt, self.update)

        # ===== 状态变量 =====
        self.have_pose = False
        self.goal_locked = False
        self.phase = 1
        self.x = 0.0
        self.y = 0.0
        self.yaw = 0.0
        self.start_x = 0.0
        self.start_y = 0.0
        self.goal_x = 0.0
        self.goal_y = 0.0
        self.goal_yaw = 0.0

        # 轨迹记录
        self.actual_x, self.actual_y = [], []
        self.desired_x, self.desired_y = [], []
        self.plot_counter = 0
        self.plot_interval = 100

        self.get_logger().info('Goto Pose (Rigid15) with parameters initialized.')


    # ---------- 动捕回调 ----------
    def mocap_cb(self, msg: PoseStamped):
        # 位置（mm → m）
        mx = msg.pose.position.x * self.mm_to_m
        mz = msg.pose.position.z * self.mm_to_m
        if self.swap_xz:
            Xw, Yw = mz, mx
        else:
            Xw, Yw = mx, mz
        if self.negate_x:
            Xw = -Xw
        if self.negate_z:
            Yw = -Yw

        # 航向（度 → rad）
        yaw_deg = msg.pose.orientation.y
        if self.flip_heading_sign:
            yaw_deg = -yaw_deg
        yaw_deg += self.heading_deg_bias
        yaw = wrap_angle(math.radians(yaw_deg))

        self.x, self.y, self.yaw = Xw, Yw, yaw
        self.have_pose = True

        # 锁定目标（只做一次）
        if not self.goal_locked:
            self.lock_goal()

    # ---------- 锁定目标 ----------
    def lock_goal(self):
        self.start_x, self.start_y = self.x, self.y

        if self.goal_mode == 'relative':     
            # 相对当前位姿（直接按世界系 X/Y 偏移；如需沿车头方向，请改为基于 yaw 的旋转）
            self.goal_x = self.start_x + self.rel_dx_m
            self.goal_y = self.start_y + self.rel_dy_m
            self.goal_yaw = wrap_angle(self.yaw + math.radians(self.rel_dyaw_deg))
            mode_str = f'rel(dx={self.rel_dx_m:.2f}, dy={self.rel_dy_m:.2f}, dyaw={self.rel_dyaw_deg:.1f}°)'
        else:
            # 绝对目标（动捕坐标 → 控制器世界系）
            gx = self.abs_goal_x_mm * self.mm_to_m
            gz = self.abs_goal_z_mm * self.mm_to_m
            if self.swap_xz:
                Xg, Yg = gz, gx
            else:
                Xg, Yg = gx, gz
            if self.negate_x:
                Xg = -Xg
            if self.negate_z:
                Yg = -Yg
            self.goal_x, self.goal_y = Xg, Yg

            gyaw_deg = self.abs_goal_yaw_deg
            if self.flip_heading_sign:
                gyaw_deg = -gyaw_deg
            gyaw_deg += self.heading_deg_bias
            self.goal_yaw = wrap_angle(math.radians(gyaw_deg))
            mode_str = f'abs(X={self.goal_x:.2f}, Y={self.goal_y:.2f}, yaw={math.degrees(self.goal_yaw):.1f}°)'

        self.goal_locked = True
        self.phase = 1
        self.get_logger().info(
            f'[Goal-Locked] start=({self.start_x:.2f},{self.start_y:.2f}), '
            f'goal=({self.goal_x:.2f},{self.goal_y:.2f}), '
            f'goal_yaw={math.degrees(self.goal_yaw):.1f}°, mode={mode_str}'
        )

    # ---------- 主循环 ----------
    def update(self):
        if not (self.have_pose and self.goal_locked):
            return

        cmd = Twist()

        if self.phase == 1:
            # ---- 阶段1：到点（直线段 Pure Pursuit）----
            dxg = self.goal_x - self.start_x
            dyg = self.goal_y - self.start_y
            seg_len = math.hypot(dxg, dyg)

            # 最近点（投影到线段）
            rx = self.x - self.start_x
            ry = self.y - self.start_y
            L2 = seg_len * seg_len
            t = 0.0 if L2 < 1e-9 else (rx * dxg + ry * dyg) / L2
            t = max(0.0, min(1.0, t))
            qx = self.start_x + t * dxg
            qy = self.start_y + t * dyg

            # 前视点（封顶至终点）
            ahead = min(t * seg_len + self.Ld, seg_len)
            ratio = 1.0 if seg_len < 1e-6 else ahead / seg_len
            px = self.start_x + ratio * dxg
            py = self.start_y + ratio * dyg

            # 转到车体系
            dx = px - self.x
            dy = py - self.y
            c, s = math.cos(self.yaw), math.sin(self.yaw)
            x_b = c * dx + s * dy
            y_b = -s * dx + c * dy
            kappa = 0.0 if self.Ld < 1e-6 else 2.0 * y_b / (self.Ld * self.Ld)

            # 速度与限幅
            v = self.v_nominal
            w = v * kappa
            v = max(min(v, self.v_max), -self.v_max)
            w = max(min(w, self.w_max), -self.w_max)
            ang_ratio = max(0.0, 1.0 - abs(w) / (self.w_max + 1e-6))
            v *= (0.6 + 0.4 * ang_ratio)

            # 到点判定
            dist_to_goal = math.hypot(self.x - self.goal_x, self.y - self.goal_y)
            if dist_to_goal <= self.pos_tol:
                self.phase = 2
                v, w = 0.0, 0.0
                self.get_logger().info(f'Position reached (≤{self.pos_tol:.2f} m). Aligning heading...')
            cmd.linear.x = v
            cmd.angular.z = w


            # 记录绘图（期望点用投影 Q）
            self.desired_x.append(qx)
            self.desired_y.append(qy)

        elif self.phase == 2:
            # ---- 阶段2：姿态对齐（原地或低速转向）----
            yaw_err = wrap_angle(self.goal_yaw - self.yaw)
            w = self.k_yaw * yaw_err
            w = max(min(w, self.w_yaw_max), -self.w_yaw_max)
            v = self.v_align  # 一般为 0，必要时可给很小值便于底盘转向

            if abs(yaw_err) <= math.radians(self.yaw_tol_deg):
                self.phase = 3
                v, w = 0.0, 0.0
                # 计算误差
                pos_err = math.hypot(self.x - self.goal_x, self.y - self.goal_y)
                yaw_err_final = wrap_angle(self.yaw - self.goal_yaw)
                yaw_err_deg = math.degrees(yaw_err_final)
                self.get_logger().info(
                    f'Done. Final position error = {pos_err:.3f} m, '
                    f'yaw error = {yaw_err_deg:.2f}°'
                )


            cmd.linear.x = v
            cmd.angular.z = w

            # 记录绘图（阶段2 期望点就固定为目标点）
            self.desired_x.append(self.goal_x)
            self.desired_y.append(self.goal_y)

        else:
            # ---- 阶段3：完成，停车保持 ----
            cmd.linear.x = 0.0
            cmd.angular.z = 0.0

        # 发布控制
        self.cmd_pub.publish(cmd)

        # 记录与实时图
        self.actual_x.append(self.x)
        self.actual_y.append(self.y)
        self.plot_counter += 1
        if self.plot_counter % self.plot_interval == 0:
            self.save_plot(live=True)

    # ---------- 绘图 ----------
    def save_plot(self, live=False):
        plt.figure()
        # 画直线段
        plt.plot([self.start_x, self.goal_x], [self.start_y, self.goal_y], 'r--', label='Desired Line')
        # 画实际轨迹
        plt.plot(self.actual_x, self.actual_y, 'b-', label='Actual')
        # 画目标点
        plt.plot(self.goal_x, self.goal_y, 'go', label='Goal')
        plt.xlabel('X [m]')
        plt.ylabel('Y [m]')
        plt.axis('equal')
        plt.legend()
        plt.title('Goto Pose (Rigid15)' + (' (Live)' if live else ' (Final)'))
        plt.savefig('trajectory.png', dpi=200)
        plt.close()
        if live:
            self.get_logger().info('Updated trajectory.png')

    # ---------- 收尾 ----------
    def on_shutdown(self):
        self.cmd_pub.publish(Twist())
        self.save_plot(live=False)


def main(args=None):
    rclpy.init(args=args)
    node = GotoPoseRigid15()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.on_shutdown()
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
