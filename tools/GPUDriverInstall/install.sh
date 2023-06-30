#!/bin/bash

PKG_DIR=$1

function helpdoc()
{
    cat <<- EOF
    Usage: ./install.sh <package directory>
	EOF
    exit 2
}


function install()
{
	if [ ! -d $PKG_DIR ]
	then
		echo "The $PKG_DIR does not exist, please check it!"
		exit 2
	fi

	cd $PKG_DIR

	sudo dpkg -i --force-all *.deb
	sudo -E apt --fix-broken -y install
	sudo dpkg -i intel-platform-vsec-dkms*.deb
	sudo dpkg -i intel-platform-cse-dkms_*.deb
	sudo dpkg -i intel-i915-dkms_*.deb
}

if [ ! $# -eq 1 ]
then
	helpdoc
else
	install
fi



