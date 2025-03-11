#!/bin/bash

echo "changing word dir"
cd /home/inubashiri/GM/nuc/GMaster_project/src/

echo "sourcing dependencies"
source gary_msgs/install/setup.bash

echo "colcon building"
cd hero_contact
colcon build
