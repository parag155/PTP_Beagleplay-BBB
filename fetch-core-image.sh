#!/bin/bash
echo "Downloading latest pre-built images..."
wget -q --show-progress https://github.com/parag155/PTP_Beagleplay-BBB/releases/latest/download/core-image
echo "^ Beagle Play (master)^"
echo " "
wget -q --show-progress https://github.com/parag155/PTP_Beagleplay-BBB/releases/latest/download/core-image_bbb
echo " ^ BeagleBoneBlack (slave)^"
echo " "
echo "Download complete!"
