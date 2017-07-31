#!/bin/sh -eu

if test -f /proc/self/cgroup; then
	# If the init system is a cgroup controler, it will be position 1.
	# Secondary controllers, like cgmanager, do not work.
	with_cgroupctrl=$(grep "^1:name=" /proc/self/cgroup | \
			sed -n 's/.*=//p' | sed -e 's/:.*$//')
	if test -z "$with_cgroupctrl"; then
		# Try to be our own cgroup controller
		with_cgroupctrl="elogind"
	fi
else
	# 'auto' but no cgroup fs is a problem.
	with_cgroupctrl=""
fi

echo "$with_cgroupctrl"
exit 0
