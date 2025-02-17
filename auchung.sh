#!/bin/bash

echo 'Changing directory...'
cd /home/nuc1/test_contact_node/nuc/GMaster_project/src/

echo 'Sourcing ROS 2...'
source /opt/ros/humble/setup.bash

echo 'Sourcing sentry_msg...'
source sentry_msgs/install/setup.bash

echo 'Sourcing contact node...'
source contact/install/setup.bash

echo 'chmoding'
sudo chmod 777 /dev/ttyACM0

echo 'Running contact node...'
ros2 run contact contact
