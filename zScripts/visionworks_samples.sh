#!/bin/bash

# path variable

LOCATION = "/home/nvidia/VisionWorks-1.6-Samples/bin/aarch64/linux/release"

demo_name = "nvx_demo_feature_tracker"

if [$# == 1]; then
    :
else 
    echo "Usage: 
