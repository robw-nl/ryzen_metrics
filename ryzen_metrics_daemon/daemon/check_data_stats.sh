#!/bin/bash
echo -ne "\nStats of binary data file\n"
ls -l /home/rob/.config/system_metrics/stats.dat

echo -ne "\nStats of tooltip file\n"
ls -l /dev/shm/system_metrics_tooltip.txt

echo -ne "\nTooltip content\n"
cat /dev/shm/system_metrics_tooltip.txt
echo -ne "\n\n"
