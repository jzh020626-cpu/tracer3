from setuptools import setup
import os
from glob import glob

package_name = 'ekf_localization'

setup(
    name=package_name,
    version='0.0.1',
    packages=[package_name],
    data_files=[
        # 注册到 ament 索引
        (os.path.join('share', 'ament_index', 'resource_index', 'packages'),
            ['resource/' + package_name]),

        # 安装 package.xml
        (os.path.join('share', package_name), ['package.xml']),

        # 安装 launch 与 config 目录内容
        (os.path.join('share', package_name, 'launch'), glob('launch/*.launch.py')),
        (os.path.join('share', package_name, 'config'), glob('config/*.yaml')),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='Your Name',
    maintainer_email='your.email@example.com',
    description='Python-based EKF localization and analysis tools',
    license='Apache-2.0',
    entry_points={
        'console_scripts': [
            'measure_delay = ekf_localization.measure_delay:main',
            'velocity_acceleration_monitor = ekf_localization.velocity_acceleration_monitor:main',
            'pd_straight_controler = ekf_localization.pd_straight_controler:main',
            'trajectory_plotter = ekf_localization.trajectory_plotter:main',
            'pose_monitor = ekf_localization.pose_monitor:main',
            'goto_pose_rigid15 = ekf_localization.goto_pose_rigid15:main',
        ],
    },
)
