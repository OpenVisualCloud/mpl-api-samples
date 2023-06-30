#!/bin/bash

if [ $UID -eq 0 ]
then
	echo "please use a normal user instead of the superuser"
	exit 1
fi

PKG_LSIT=$1
CUR_DIR=$(pwd)
SCRIPT_DIR=$(dirname $(readlink -f "$0"))
PKG_DIR_NAME=${PKG_LSIT%.*}_deb_packages
PKG_DIR=${SCRIPT_DIR}/${PKG_DIR_NAME}

function help(){
	cat <<- EOF
Usage: ./download_package.sh <package list file>
EOF
    	exit 2

}

#For Install the Intel GPU driver repository
function install_repository(){
	
	sudo -E apt-get update
	sudo -E apt-get install -y gpg-agent wget

	wget -qO - https://repositories.intel.com/graphics/intel-graphics.key | sudo gpg --dearmor --output /usr/share/keyrings/intel-graphics.gpg
	echo "deb [arch=amd64,i386 signed-by=/usr/share/keyrings/intel-graphics.gpg] https://repositories.intel.com/graphics/ubuntu jammy flex" |sudo tee /etc/apt/sources.list.d/intel-gpu-jammy.list
	sudo -E apt-get update
}

#For dowload the packages in package_list*.txt
function download_package(){
	existed_list=()
	download_failure_list=()
	download_success_list=()


	if [ ! -d "${PKG_DIR}" ]
	then
		echo "[*] Create deb package's folder ${PKG_DIR}"
		mkdir -p ${PKG_DIR}
	else
		echo "[*] The ${PKG_DIR} already exists"
	fi


	for package_index in $(cat ${PKG_LSIT})
	do
		ls ${PKG_DIR}/${package_index} > /dev/null 2>&1
		if [ "$?" -eq 0 ]
		then
			echo "[*] The ${package_index} already exists, please check it"
			existed_list[${#existed_list[*]}]=${package_index}
			continue
		fi

		package_name_tmp=${package_index%_*}
		package_name=${package_name_tmp/_/=}

		echo "[*********************************] Downloading $package_index [*********************************]"
		echo "[*] apt download ${package_name}"
		apt download ${package_name}
		if [ -e "${package_index}" ]
		then
			echo "[*] ${package_index} is downloaded successfully"
			download_success_list[${#download_success_list[*]}]=${package_index}
			mv ${package_index} ${PKG_DIR}
		else
			echo "[*] ${package_index} is downloaded failed"
			download_failure_list[${#download_failure_list[*]}]=${package_index}

		fi

		echo "[*********************************] Download $package_index ends [*********************************]"
	done
}

if [ ! $# -eq 1 ]
then
	help
else
	if [ ! -f "${PKG_LSIT}" ]
	then
		echo "The ${PKG_LSIT} does not exist, please check it"
		exit 2
	fi

	install_repository
	download_package

	echo "[**************************************************************************************************************************************]"
	echo "[**************************************************************************************************************************************]"
	echo "[*********************************] Download Summary [*********************************]"
	echo "[---------------------------------] The downloaded packages are in ${PKG_DIR} [---------------------------------]"
	echo "[--------------------------------------------------------------------------------------------------------------------------------------]"
	echo "[---------------------------------] Below packages are downloaded failed: [---------------------------------]"
	echo "[---------------------------------] Total ${#download_failure_list[*]} failed: [---------------------------------]"
	echo "${download_failure_list[*]}"
	echo "[--------------------------------------------------------------------------------------------------------------------------------------]"
	echo "[---------------------------------] Below packages existed before downloading: [---------------------------------]"
	echo "[---------------------------------] Total ${#existed_list[*]} existed before: [---------------------------------]"
	echo "${existed_list[*]}"
	echo "[--------------------------------------------------------------------------------------------------------------------------------------]"
	echo "[---------------------------------] Below packages are successfully : [---------------------------------]"
	echo "[---------------------------------] Total ${#download_success_list[*]} success: [---------------------------------]"
	echo "${download_success_list[*]}"

fi

