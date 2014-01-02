#!/bin/sh
#open -a Terminal.app ./mac_launch_qemu.sh
./mac_launch_qemu.sh &
i386-elf-gdb -quiet -command "mac_init.gdb" bootimg
