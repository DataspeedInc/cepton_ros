# Cepton ROS

## Overview

This package provides ROS support for the Cepton LIDAR sensor.

Before using this ROS driver, we recommend that you download CeptonViewer.

## Compatibility

Currently, this driver only works on Ubuntu.

## Installation

If you have not done so already, install ROS, and [create a catkin workspace](http://wiki.ros.org/ROS/Tutorials/InstallingandConfiguringROSEnvironment).

Change to the catkin workspace directory.

Clone the repository.

```sh
$ git clone --recursive git@github.com:ceptontech/cepton_ros.git src/cepton_ros
```

Run catkin make.

```sh
$ catkin_make
```

Source the catkin setup script.

```sh
$ source devel/setup.bash
```

## Getting started

Connect the sensor's ethernet cable to the host computer (we recommend using a USB -> Ethernet adapter). The sensor IP address is of the form `192.168.*.*`, and it sends UDP broadcast packets on port 8808. The sensor will start sending packets as soon as the power is connected.

On Ubuntu, it is necessary to assign a static IP address to the host computer's Ethernet interface, e.g. IP=`19.168.0.1`, Netmask=`255.255.0.0`. This can be done through the Network Manager GUI.

First, try viewing the sensor in CeptonViewer, to ensure that it is connected properly. Then, launch the ROS demo (`roscore` must be running already).

    $ roslaunch cepton_ros demo.launch

A rviz window should popup showing a sample point cloud.

### Using multiple sensors

Multiple sensors can be viewed using the `driver_multi.launch` file. The driver will publish separate topics and transforms for each sensor.

    $ roslaunch cepton_ros driver_multi.launch transforms_path:=<transforms_file>

A sample transforms file can be found at `samples/cepton_transforms.json`. The rotation is in quaternion format `<x, y, z, w>`. The coordinate system is as follows: `+x` = right, `+y` = forward, `+z` = up.

## Troubleshooting

First, try viewing the sensor in CeptonViewer to determine if the issue is ROS or the sensor/network.

The most common issue is the host computer blocking the sensor packets. Using Wireshark, or another networking tool, check that you are receiving packets on port 8808. If you not, check your networking/firewall settings.

## Reference

### Driver nodelet

The driver nodelet is a thin wrapper around the Cepton SDK. It publishes sensor information and PointCloud2 topics for each sensor. The point type definitions can be found in `include/cepton_ros/point.hpp`.
