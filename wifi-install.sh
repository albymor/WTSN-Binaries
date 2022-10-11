#!/bin/bash

function die() {
	ret=$1
	shift
	echo -e "$@" >&2
	exit $ret
}

# usage: status_eval RET_VAL MESSAGE
function status_eval() {
	local ret=$1; shift
	if [ $ret -ne 0 ]; then
		echo -e "ERROR: $@"
	else
		echo -e "SUCCESS: $@"
	fi
	return $ret
}

# usage: install_tools TOOLS_DEST
function install_tools(){
	local TOOLS_DIR="$1"
	local TOOLS_DEST="$2"
	local IWLWIFI_DIR="$3"
	local TRACE_CMD_LIB_DIR=""
	local TRACE_CMD_LIB_DIR_OPTS=("/usr/local/lib/trace-cmd/plugins"
				      "/usr/lib/trace-cmd/plugins/"
				      "/usr/local/lib/traceevent/plugins")
	local ALIASES="${HOME}/.bash_aliases"
	local TRACE_CMD_DEST="${TOOLS_DEST}/trace-cmd"
	local TRACE_CMD_PARSE_SCRIPT="${TRACE_CMD_DEST}/create-parsing.py"
	local TRACE_CMD_JSON="${TRACE_CMD_DEST}/iwlwifi.json"

	# find the location where trace-cmd plugins are installed
	for trace_cmd_lib_dir in ${TRACE_CMD_LIB_DIR_OPTS[@]}; do
		if [ -d "${trace_cmd_lib_dir}" ]; then
			TRACE_CMD_LIB_DIR=${trace_cmd_lib_dir}
		fi
	done
	# return in case trace-cmd isn't installed
	if [ -z "${TRACE_CMD_LIB_DIR}" ]; then
		echo "INFO: trace-cmd doesn't seem to be installed on this machine\naborting tools install..."
		return 0
	fi
	if [ ! -d "${TOOLS_DIR}" ]; then
		echo "ERROR: unable to locate tools directory\nexiting..."
		return 1
	fi

	# clean old destination dir
	sudo rm -rf "${TOOLS_DEST}"
	# copy new tools to destination
	sudo cp -a "${TOOLS_DIR}" "${TOOLS_DEST}" ||
		status_eval $? "copying new tools to ${TOOLS_DEST}" || return 1
	# update iwlwifi json according to the iwlwifi source code
	pushd "${IWLWIFI_DIR}" ||
		status_eval $? "pushd ${IWLWIFI_DIR}" || return 1
	python ${TRACE_CMD_PARSE_SCRIPT} > "${TRACE_CMD_JSON}" ||
		status_eval $? "generating trace_cmd iwlwifi.json file" || return 1
	popd # "${IWLWIFI_DIR}"
		status_eval $? "popd ${IWLWIFI_DIR}" || return 1
	# link new trace-cmd plugin
	sudo unlink ${TRACE_CMD_LIB_DIR}/iwlwifi.py
	sudo ln -s ${TOOLS_DEST}/trace-cmd/iwlwifi.py ${TRACE_CMD_LIB_DIR} ||
		status_eval $? "link new trace-cmd plugin" || return 1

	# clean old aliases
	if [ -e "${ALIASES}" ]; then
		sed -i '/^alias tc=/d' ${ALIASES} ||
			status_eval $? "removing tc alias" || return 1
		sed -i '/^alias tp=/d' ${ALIASES} ||
			status_eval $? "removing tp alias" || return 1
		echo -e "old aliases removed successfully"
	fi
	echo -e "trace-cmd plugin installed successfully"
}

# usage: install_modules ESL_REGRESSION
function install_modules(){
	local DRIVER_DIR="$1"
	local ESL_REGRESSION=$2
	local INSTALL_PREBUILD_BIN=$3
	local IWLWIFI_CONF="/etc/modprobe.d/iwlwifi.conf"

	# remove old modules
	sudo rm -rf "${LIBS_PATH}/updates/compat/ ${LIBS_PATH}/updates/net/wireless/ \
		${LIBS_PATH}/updates/drivers/net/wireless/intel/iwlwifi/"
	if [ "${INSTALL_PREBUILD_BIN}" == "true" ]; then
		sudo dpkg -i ${DRIVER_DIR}/iwlwifi-stack-dev*.deb
			status_eval $? "iwlwifi prebuild deb install" || return 1
	else
		# the reason why we are doing the pushd there (and not in the head of the function)
		# is that we like to try again to build and install the src; if we fail in the above deb install.
		pushd ${DRIVER_DIR} ||
			status_eval $? "enter ${DRIVER_DIR} directory" || return 1
		# install new modules
		sudo make install ||
			status_eval $? "make install" || return 1
		popd #${DRIVER_DIR}
	fi

	# removing bus config file
	sudo rm -f /etc/modprobe.d/iwlwifi-disable-fw-restart.conf
	# removing opmod config file
	sudo rm -f $IWLWIFI_CONF

	# creating opmod config file
	echo "adding modprobe config file for DVM and MVM modules"
	echo "# /etc/modprobe.d/iwlwifi.conf" | sudo tee $IWLWIFI_CONF
	echo "# iwlwifi will dynamically load either iwldvm or iwlmvm depending on the" | sudo tee -a $IWLWIFI_CONF
	echo "# microcode file installed on the system.  When removing iwlwifi, first" | sudo tee -a $IWLWIFI_CONF
	echo "# remove the iwl?vm module and then iwlwifi." | sudo tee -a $IWLWIFI_CONF
	echo -e "remove iwlwifi (/sbin/lsmod | grep -o -e ^iwlmvm -e ^iwlfmac -e ^iwldvm -e ^iwlwifi | xargs /sbin/rmmod) \
&& /sbin/modprobe -r mac80211 && /sbin/modprobe -r cfg80211 && /sbin/modprobe -r -q compat || exit 0\n" | sudo tee -a $IWLWIFI_CONF

	echo -e "\n# PCI BUS\n" | sudo tee -a $IWLWIFI_CONF
	echo "modules configured for PCI BUS"

	echo -e "kernel modules installed successfully.\n"
}

# usage: install_hostap HW_SIM
function install_hostap(){
	local HOSTAP_DIR="$1"
	local HW_SIM=$2
	local INSTALL_PREBUILD_BIN=$3
	local REMOVE_OLD_HOSTAP=$4

	local WLANTEST_DIR="$HOSTAP_DIR/wlantest"
	local WLANTEST_FILES="wlantest wlantest_cli"
	local WLANTEST_DEST="/usr/sbin"

	if [ "$HW_SIM" == "true" ]; then
		echo "HW SIM installation..."
		pushd "$WLANTEST_DIR" ||
			status_eval $? "enter $WLANTEST_DIR directory" || return 1
		sudo cp $WLANTEST_FILES "$WLANTEST_DEST" ||
			status_eval $? "copy $WLANTEST_FILES executables to $WLANTEST_DEST" || return 1
		popd # $WLANTEST_DIR
	fi

	if [ "${INSTALL_PREBUILD_BIN}" == "true" ]; then
		if [ "$REMOVE_OLD_HOSTAP" == "true" ]; then
			echo "Uninstalling old versions..."
			sudo apt-get -q -y remove wpasupplicant
		fi
		sudo dpkg -i $PREBUILDS_DIR/hostap/*.deb
			status_eval $? "hostap prebuild deb install" || return 1
	fi

	# remove bgscan option
	sudo sed -i '/^# enable bgscan/d' /etc/wpa_supplicant/wpa_supplicant.conf
	sudo sed -i '/^bgscan=/d' /etc/wpa_supplicant/wpa_supplicant.conf
	# insert new bgscan option
	echo -e '# enable bgscan\nbgscan="simple:64:-72:600"' | sudo tee -a /etc/wpa_supplicant/wpa_supplicant.conf
	#add udev rule to dump the data in case of error
	echo -e 'DRIVER=="iwlwifi", ACTION=="change", RUN+="/usr/bin/fwdump"' | sudo tee /etc/udev/rules.d/85-iwl-fw-dump.rules
	echo -e 'SUBSYSTEM=="devcoredump", ACTION=="add", RUN+="/usr/bin/fwdump"' | sudo tee -a /etc/udev/rules.d/85-iwl-fw-dump.rules
	echo -e "Hostap installation completed sucessfully.\n"
}

# usage: install_firmware USE_FW_OFFLOAD INSTALL_NVM
function install_firmware(){
	local FW_DIR="$1"
	local FW_PATTERN=$2
	local INSTALL_NVM=$3
	local LNP_USNIFFER_FILE="${SCRIPTS_DIR}/ini_file/lnp_usniffer_conf.ini"
	local STP_USNIFFER_FILE="${SCRIPTS_DIR}/ini_file/stp_usniffer_conf.ini"
	local THP_USNIFFER_FILE="${SCRIPTS_DIR}/ini_file/thp_usniffer_conf.ini"
	local QNJ_USNIFFER_FILE="${SCRIPTS_DIR}/ini_file/qnj_usniffer_conf.ini"

	echo "removing old firmware files..."
	sudo rm -f $FW_LOCATION/*iwlwifi-*.ucode
	echo "removing old PNVM files..."
	sudo rm -f $FW_LOCATION/*iwlwifi-*.pnvm

	# See that "${FW_DIR}/ENG" is not empty
	if [ "$(ls -A ${FW_DIR}/ENG)"  ]; then
		sudo rm -rf ${FW_LOCATION}/IWL_ENG
		sudo mkdir -p ${FW_LOCATION}/IWL_ENG ||
			status_eval $? "creating directory ${FW_LOCATION}/IWL_ENG" || return 1
		sudo cp ${FW_DIR}/ENG/* ${FW_LOCATION}/IWL_ENG ||
			status_eval $? "copy firmware debug data" || return 1
	fi

	if [ "${FW_PATTERN}" != "MISSING" ]; then
		local FW_TYPE="${FW_PATTERN//-}"
		echo "installing $FW_TYPE firmware..."
		sudo cp ${FW_DIR}/*${FW_PATTERN}*.ucode ${FW_LOCATION} ||
			status_eval $? "copy the new firmware to ${FW_LOCATION}" || return 1
		FW_FILES=$(find ${FW_LOCATION} -name *${FW_PATTERN}*.ucode)
		for fw_file in $FW_FILES; do
			sudo cp "${fw_file}" "${fw_file//${FW_PATTERN}}" ||
				status_eval $? "$FW_TYPE firmware installation" || return 1
		done
	else
		# install all the new firmware (MVM )
		sudo cp ${FW_DIR}/*iwlwifi-*.ucode ${FW_LOCATION} ||
			status_eval $? "copy the new firmware to ${FW_LOCATION}" || return 1
	fi

	# install all the new PNVM files
	sudo cp ${FW_DIR}/*iwlwifi-*.pnvm ${FW_LOCATION} ||
	    status_eval $? "copy the new PNVM files to ${FW_LOCATION}"

	echo -e "firmware and PNVM files installed successfully.\n"

	if [ -f ${FW_DIR}/regulatory.db ] && [ -f ${FW_DIR}/regulatory.db.p7s ]; then
		sudo cp ${FW_DIR}/regulatory.db* ${FW_LOCATION} ||
			status_eval $? "copy regdb files to ${FW_LOCATION}" || return 1
	fi

	if [ "${INSTALL_NVM}" = "true" ]; then
		if [ -n "${NVM_DIR}" ] && [ -e "${NVM_DIR}" ]; then
			echo "installing NVM file..."
			sudo cp -a ${NVM_DIR}/* ${FW_LOCATION}
		else
			status_eval 1 "locating nvm files" || return 1
		fi
	else
		echo "not installing new nvm file"
	fi
}

# script arguments:
iwl_installation_package=""
iwl_klibs=""
iwl_debug_fw=""
iwl_debug_hw=""
iwl_exclusion_components=""
iwl_hw_sim="false"
iwl_hostap_dbus="false"
iwl_install_nvm="true"
iwl_thp_blank="false"
iwl_random_mac="false"
iwl_use_fw_upload="false"
iwl_cam_mode="false"
# iwl_fw_pattern is used to hold the pattern of the FW type we like to install,
# that can be - unpaged-, fmac- or fmac-signed- , in the case we need to install fmac.
# we will use "MISSING" as its initial value to indicate that the pattern is missing,
# (and we dont need to install fmac/offload stack)
# there is no point to use reset_missing function in this case.
iwl_fw_pattern="MISSING"
iwl_dbgm="false"
iwl_esl_regression="false"
iwl_reboot="true"
iwlwifi_defconfig_count=0
iwl_uninstall_old_hostap="true"

# global parameters:
FW_LOCATION="/lib/firmware"
DBG_CFG_FILE="${FW_LOCATION}/iwl-dbg-cfg.ini"
LIBS_PATH="/lib/modules/$(uname -r)"
OFFLOAD_FILE_PATH="$FW_LOCATION/offload"
UNPAGED_FILE_PATH="$FW_LOCATION/unpaged"
UPLOAD_FILE_PATH="$FW_LOCATION/upload"
MVM_FILE_PATH="$FW_LOCATION/mvm"
TMP_WORK_DIR=${TMP_WORK_DIR:-"/tmp/wifi-core-$(date "+%Y%m%d%H%M%S")"}
IWL_LOG_DIR="${TMP_WORK_DIR}/logs/install-logs"
IWLWIFI_MAKE_PARAMS="-j$(nproc)"
IWLWIFI_MAKE_DEFCONF_CMD=""

SOURCES_DIR="${TMP_WORK_DIR}/src"
PREBUILDS_DIR="${TMP_WORK_DIR}/prebuilds"
NVM_DIR="${TMP_WORK_DIR}/bin/NVM"
THP_DIR="${TMP_WORK_DIR}/bin/THP"
SCRIPTS_DIR="${TMP_WORK_DIR}/scripts"
FUNCTIONS_SCRIPT="${SCRIPTS_DIR}/tools/functions.sh"

statusFile="${IWL_LOG_DIR}/install_status.log"
IWL_STATUS_FILE="$statusFile"

# Variables for iwl-dbg-cfg.ini
FW_DBG_PRESET_VAR="FW_DBG_PRESET"
FW_DBG_PRESET_FLAG="${FW_DBG_PRESET_VAR}=1"
FW_DBG_HEADER="[IWL DEBUG CONFIG DATA]"

# remove old indications
sudo rm -f "$OFFLOAD_FILE_PATH" "$UPLOAD_FILE_PATH" "$MVM_FILE_PATH" "$UNPAGED_FILE_PATH" 2> /dev/null
rm -f "offload" "upload" "mvm" "unpaged" 2> /dev/null

# import wifi-install-usage.sh and create the usage from there
usage=""
if source wifi-install-usage.sh ; then
    usage=$(create_usage "\n" "\t" "\n")
else
	echo "ERROR: importing wifi-install-usage.sh"
fi

[ -n "$1" ] && iwl_installation_package=$(readlink -m -n $1); shift
[ -f "$iwl_installation_package" ] ||
	die 1 "ERROR: iwlwifi package tar file does not exist.\n${usage}\nexiting..."

#parse the arguments
while [ $# -gt 0 ]; do
	if [[ $1 =~ (--klibs=)(.*) ]]; then
		echo "setting argument: klibs=${BASH_REMATCH[2]}"
		iwl_klibs="${BASH_REMATCH[2]}"
		shift
	elif [[ $1 =~ (--debug-hw=)(.*) ]]; then
		echo "setting argument: iwl_debug_hw=${BASH_REMATCH[2]}"
		iwl_debug_hw="${BASH_REMATCH[2]}"
		shift
	elif [[ $1 =~ (--debug-fw=)(.*) ]]; then
		echo "setting argument: iwl_debug_fw=${BASH_REMATCH[2]}"
		iwl_debug_fw="${BASH_REMATCH[2]}"
		shift
	elif [[ $1 =~ (--install-nvm=)(.*) ]]; then
		echo "setting argument: fw=${BASH_REMATCH[2]}"
		iwl_install_nvm="${BASH_REMATCH[2]}"
		shift
	elif [[ $1 =~ (--exclusion-components=)(.*) ]]; then
		echo "setting argument: iwl_exclusion_components=${BASH_REMATCH[2]}"
		iwl_exclusion_components="${BASH_REMATCH[2]}"
		shift
	elif [[ $1 =~ (--thp-blank) ]]; then
		echo "setting argument: iwl_thp_blank=true"
		iwl_thp_blank="true"
		shift
	elif [[ $1 =~ (--random-mac) ]]; then
		echo "setting argument: iwl_random_mac=true"
		iwl_random_mac="true";
		shift
	elif [[ $1 =~ (--use-fw-upload-image) ]]; then
		echo "setting argument: iwl_use_fw_upload=true"
		echo "upload" > "upload"
		sudo cp "upload" "$FW_LOCATION"
		iwl_fw_pattern="upload-"
		shift
	elif [[ $1 =~ (--dbgm-on) ]]; then
		echo "setting argument: iwl_dbgm=true"
		iwl_dbgm="true"
		shift
	elif [[ $1 =~ (--hw-sim) ]]; then
		echo "setting argument: iwl_hw_sim=true"
		iwl_hw_sim="true"
		shift
	elif [[ $1 =~ (--dbus) ]]; then
		echo "setting argument: iwl_hostap_dbus=true"
		iwl_hostap_dbus="true"
		shift
	elif [[ $1 =~ (--esl) ]]; then
		iwl_esl_regression="true"
		IWLWIFI_MAKE_DEFCONF_CMD="make defconfig-esl"
		((iwlwifi_defconfig_count++))
		shift
	elif [[ $1 =~ (--offload) ]]; then
		echo "installing offload UCODE files"
		echo "offload" > "offload"
		sudo cp "offload" "$FW_LOCATION"
		iwl_fw_pattern="fmac-"
		shift
	elif [[ $1 =~ (--unpaged) ]]; then
		echo "installing unpaged UCODE files"
		echo "unpaged" > "unpaged"
		sudo cp "unpaged" "$FW_LOCATION"
		iwl_fw_pattern="csme-"
		shift
	elif [[ $1 =~ (--no-reboot) ]]; then
		echo "setting argument: iwl_reboot=false"
		iwl_reboot="false"
		shift
	elif [[ $1 =~ (--sle) ]]; then
		IWLWIFI_MAKE_DEFCONF_CMD="make defconfig-sle"
		iwl_cam_mode="true"
		((iwlwifi_defconfig_count++))
		shift
	elif [[ $1 =~ (--fpga) ]]; then
		IWLWIFI_MAKE_DEFCONF_CMD="make defconfig-fpga"
		iwl_cam_mode="true"
		((iwlwifi_defconfig_count++))
		shift
	elif [[ $1 =~ (--ipc) ]]; then
		IWLWIFI_MAKE_DEFCONF_CMD="make defconfig-iwlwifi-vendor-mode"
		((iwlwifi_defconfig_count++))
		shift
	elif [[ $1 =~ (--cam-mode) ]]; then
		echo "setting argument: iwl_cam_mode=true"
		iwl_cam_mode="true"
		((iwlwifi_defconfig_count++))
		shift
	elif [[ $1 =~ (--dont-uninstall-old-hostap) ]]; then
		echo "setting argument: iwl_uninstall_old_hostap=flase"
		iwl_uninstall_old_hostap="false"
		shift
	else
		die 1 "ERROR: unknown argument $1\n$usage"
	fi
done

### [Dan] NOT Sure what this is doing...
# error controling arguments (can't specify debug without hardware)
if [ -n "${iwl_debug_fw}" ] && [ -e "${iwl_debug_hw}" ] ; then
	echo -e "$USAGE"
	die 1 "ERROR: in case of debug config installation, the hardware type is mandatory"
fi

# do not allow using both --offload and --unpaged
if [ -e "$OFFLOAD_FILE_PATH" ] && [ -e "$UNPAGED_FILE_PATH" ]; then
	die 1 "ERROR: cannot use both --offload and --unpaged"
fi

if [ ! -e "$OFFLOAD_FILE_PATH" ] && [ ! -e "$UPLOAD_FILE_PATH" ] && [ ! -e "$UNPAGED_FILE_PATH" ]; then
	echo "mvm" > "mvm"
	sudo cp "mvm" "$FW_LOCATION"
fi

if [ "$USE_FW_UPLOAD" == "true" ] && [ "$USE_FW_OFFLOAD" == "true" ]; then
	sudo rm -f "$UPLOAD_FILE_PATH" "$OFFLOAD_FILE_PATH"
	die 1 "ERROR: cannot install both offload and upload.\n$usage"
fi

if [ "$iwlwifi_defconfig_count" -gt 1 ]; then
	die 1 "ERROR: cannot use more than one of --fpga, --esl and --ipc"
fi

# enable xtrace
set -x

# Creating temp directory
echo "Creating temp directory: ${TMP_WORK_DIR}"
[ -e "${TMP_WORK_DIR}" ] && rm -rf "${TMP_WORK_DIR}"
mkdir -p "${TMP_WORK_DIR}" ||
	die $? "ERROR: failed creating new tmp dir ${TMP_WORK_DIR}"
pushd "${TMP_WORK_DIR}" ||
	die $? "ERROR: failed to enter tmp dir ${TMP_WORK_DIR}"

mkdir -p "${IWL_LOG_DIR}" ||
	die $? "ERROR: failed to create logs dir ${IWL_LOG_DIR}"

if [[ $(basename "${iwl_installation_package}") =~ -src ]] ; then
	# Check if we run with root privileges
	if [[ $EUID -eq 0 ]]; then
		die 1 "ERROR: This script must NOT run as root, please don't use sudo."
	fi

	## # deploy the tar
	echo "extracting files..."
	tar -xzf "$iwl_installation_package" > /dev/null 2>&1 ||
		die 1 "ERROR: extracting from file: $iwl_installation_package. System error is $?"

	# importing the functions script. Need the build_iwl_component function.
	source "${FUNCTIONS_SCRIPT}" ||
		die 1 "ERROR: importing $FUNCTIONS_SCRIPT"

	# Workaround: in order to restore the existing mac address that is
	# being deleted by the xvt_service makefile.
	CUR_HW_ADDRESS=$(sudo cat $DBG_CFG_FILE | grep hw_address= | cut -d'=' -f2)

	# iwlwifi stack build & install:
	#===============================
	pushd "$SOURCES_DIR" ||
		die $? "ERROR: failed to enter ${SOURCES_DIR}"
	FW_DIR="$(find -maxdepth 1 -name "*fw-binar*")"
	IWLWIFI_DIR="$(find -maxdepth 1 -name "iwlwifi-stack-*")"
	HOSTAP_DIR="$(find -maxdepth 1 -name "iwlwifi-hostap")"
	#check that the parameters are not empty
	[ -n "$FW_DIR" -a -n "$IWLWIFI_DIR" -a -n "$HOSTAP_DIR" ] ||
		die $? "ERROR: failed to find a mandatory component dir."
	for component in */; do
		# remove trailing / from the value.
		component=${component///}
		if [[ "${iwl_exclusion_components}" =~ "${component}" ]]; then
			echo "Disable installation of component: $component"
		# fw-utils:
		elif [[ $component =~ fw-utils ]]; then
			build_iwl_component $component "false" "scripts/install.sh pack" ||
				die $? "ERROR: failed to build fw-utils"
			build_iwl_component $component "false" "sudo scripts/install.sh undo" ||
				die $? "ERROR: failed to undo old fw-utils"
			build_iwl_component $component "false" "sudo scripts/install.sh release" ||
				die $? "ERROR: failed to install fw-utils"
		#===================================================================
		# iw:
		elif [[ $component =~ -iw$  ]]; then
			build_iwl_component $component "false" "make -j$(nproc)" ||
				die $? "ERROR: failed to build iw"
			build_iwl_component $component "false" "sudo make install" ||
				die $? "ERROR: failed to install iw"
		#===================================================================
		# linuxptp:
		elif [[ $component =~ iwlwifi-linuxptp ]]; then
			echo "touching $component files to set current timestamp"
			find $component -exec touch {} +
			build_iwl_component $component "true" "make -j$(nproc)" ||
				die $? "ERROR: failed to build linuxptp"
			build_iwl_component $component "false" "sudo make install" ||
				die $? "ERROR: failed to build linuxptp"
		#===================================================================
		# xVT_service:
		elif [[ $component =~ xvt_service ]]; then
			if [ -n "${iwl_debug_hw}" ]; then
				HAL_MAKE_OPTIONS="DEBUG_HW=${iwl_debug_hw}"
				if [ -n "${iwl_debug_fw}" ]; then
					HAL_MAKE_OPTIONS+=" DEBUG_FW=${iwl_debug_fw}"
				fi
			fi
			build_iwl_component $component "false" "make -j$(nproc)" ||
				die $? "ERROR: failed to build xVT_service"
			build_iwl_component $component "false" "sudo make install $HAL_MAKE_OPTIONS" ||
				die $? "ERROR: failed to build xVT_service"
		#===================================================================
		# rservice:
		elif [[ $component =~ intel_ccs_tools-rservice ]]; then
			build_iwl_component "${component}/Tools/rservice" "false" "make" ||
				die $? "ERROR: failed to build rservice"
			build_iwl_component "${component}/Tools/rservice" "false" "sudo make install" ||
				die $? "ERROR: failed to install rservice"
		#===================================================================
		# tools
		elif [[ $component =~ -tools$ ]]; then
			install_tools $component "/usr/share/iwl-tools" "$IWLWIFI_DIR" ||
				die $? "ERROR: failed to install new tools."
		#===================================================================
		else
			echo "$component is not supported for installation."
		fi
	done
	# Firmware
	install_firmware "$FW_DIR" $iwl_fw_pattern $iwl_install_nvm ||
		die $? "ERROR: failed to install new firmware."
	#===================================================================
	# iwlwifi:
	# lest see if we are installing from source or bin.
	# set IWL_INSTALL_STATUS to 1 in case we dont have bin package
	IWL_INSTALL_STATUS=1
	IWL_BREBUILD_PACKAGE_NAME=$(find "$PREBUILDS_DIR" -name iwlwifi-stack-dev_*_amd64.deb)
	if [ -n "$IWL_BREBUILD_PACKAGE_NAME" -a "$iwl_esl_regression" == "true" ]; then
		echo "prebuilt installation is enabled."
		install_modules $(dirname "$IWL_BREBUILD_PACKAGE_NAME") $iwl_esl_regression "true"
		IWL_INSTALL_STATUS=$?
		if [ "$IWL_INSTALL_STATUS" -ne 0 ]; then
			echo "prebuilt installation failed. building from source"
		fi
	fi
	# if the above prebuild install was successful then no need to build from source
	if [ "$IWL_INSTALL_STATUS" -ne 0 ]; then
		echo "prebuilt installation is disabled. building from source"
		build_iwl_component "$IWLWIFI_DIR" "false" "make KLIB=${LIBS_PATH} $IWLWIFI_MAKE_PARAMS" "$IWLWIFI_MAKE_DEFCONF_CMD" ||
			die $? "ERROR: failed to build iwlwifi-stack-dev"
		install_modules "$IWLWIFI_DIR" $iwl_esl_regression "false" ||
			die $? "ERROR: failed to install the kernel modules."
	fi
	#===================================================================
	# hostap:
	HOSTAP_INSTALL_STATUS=1
	HOSTAP_BREBUILD_PACKAGE_NAME=$(find "$PREBUILDS_DIR/hostap" -name *_amd64.deb)
	if [ -n "$HOSTAP_BREBUILD_PACKAGE_NAME" -a "$iwl_hw_sim" == "false" ]; then
		echo "prebuilt installation is enabled."
		install_hostap "$HOSTAP_DIR" "$iwl_hw_sim" "true" "$iwl_uninstall_old_hostap"
		HOSTAP_INSTALL_STATUS=$?
		if [ "$HOSTAP_INSTALL_STATUS" -ne 0 ]; then
			echo "prebuilt installation failed. building from source"
		fi
	fi

	if [ "$HOSTAP_INSTALL_STATUS" -ne 0 ]; then
		echo "prebuilt installation is disabled. building from source"
		build_hostap "${HOSTAP_DIR}" "true" "${iwl_hw_sim}" "${iwl_uninstall_old_hostap}" "${iwl_hostap_dbus}" ||
			die $? "ERROR: failed to build hostap"
		install_hostap "$HOSTAP_DIR" $iwl_hw_sim "false" "$iwl_uninstall_old_hostap" ||
			die $? "ERROR: failed to install hostap."
	fi
	#===================================================================
	popd # $SOURCES_DIR
else
	die 1 "ERROR: unidentified package type: $iwl_installation_package\n${usage}"
fi

# Aditional installation configuration:
#=======================================
# copy THP files
if [ "${iwl_thp_blank}" = "true" ]; then
	sudo cp ${THP_DIR}/iwlwifi.conf /etc/modprobe.d/ ||
		die $? "ERROR: Copy of iwlwifi.conf to /etc/modprobe.d/ , System error is $?"
	sudo cp ${THP_DIR}/iwl-dbg-cfg.ini /lib/firmware/ ||
		die $? "ERROR: Copy of iwl-dbg-cfg.ini to /lib/firmware/ , System error is $?"
	sudo cp ${THP_DIR}/iwl_nvm_9000.bin /lib/firmware/ ||
		die $? "ERROR: Copy of iwl_nvm_9000.bin to /lib/firmware/ , System error is $?"
fi

# Add FW debug flag for Yoyo
# skip if the installation is on ESL - WRT collection slows ESL and debug is handled differently with LARC trace instead
if [ "${iwl_esl_regression}" = "false" ]; then
	if [ ! -f "$DBG_CFG_FILE" ]; then
		# File does not exist - add the dbg header and the flag
		echo "${FW_DBG_HEADER}" | sudo tee "$DBG_CFG_FILE"
		echo "${FW_DBG_PRESET_FLAG}" | sudo tee -a "$DBG_CFG_FILE"
	else
		# Get the current value of the dbg cfg flag (if exists)
		fw_dbg_preset_curr=$(cat ${DBG_CFG_FILE} | grep ${FW_DBG_PRESET_VAR})
		# If the flag already exists, then replace the current value, else add the default value.
		if [ -n "${fw_dbg_preset_curr}" ]; then
			sudo sed -i "s/${FW_DBG_PRESET_VAR}.*/${FW_DBG_PRESET_FLAG}/g" "$DBG_CFG_FILE"
			echo "Warning: Replacing the current ${fw_dbg_preset_curr} to ${FW_DBG_PRESET_FLAG}"
		else
			echo "${FW_DBG_PRESET_FLAG}" | sudo tee -a "$DBG_CFG_FILE"
		fi
	fi
fi

if [ "${iwl_cam_mode}" = "true" ]; then
	echo "options iwlmvm power_scheme=1" | sudo tee -a /etc/modprobe.d/iwlwifi.conf ||
		die $? "ERROR: failed to set power_scheme=1 in iwlwifi.conf. system status: $?"
fi

# add a random MAC address or use existing MAC
if [ "${iwl_random_mac}" = "true" ]; then
	hexchar="0123456789abcdef"
	rnd_mac=$(for i in {1..6}; do echo -n ${hexchar:$(( $RANDOM % 16 )):1}; done)
	if [ -f "$DBG_CFG_FILE" ]; then
		if [ -n "$(cat $DBG_CFG_FILE | grep "hw_address=")" ]; then
			# replace current mac address, in an existing ini file, with new random mac address
			sudo sed -i "s/hw_address=.*/hw_address=001122$rnd_mac/g" "$DBG_CFG_FILE"
		else
			# add random mac to an existing ini file without a mac addr
			echo "hw_address=001122$rnd_mac" | sudo tee -a "$DBG_CFG_FILE"
		fi
	else
		# create a new ini file in case there isn't and add a random mac addr
		echo "[IWL DEBUG CONFIG DATA]" | sudo tee "$DBG_CFG_FILE"
		echo "hw_address=001122$rnd_mac" | sudo tee -a "$DBG_CFG_FILE"
	fi
else
	if [ -n "$CUR_HW_ADDRESS" ]; then
		# keep original mac addr from original ini file
		# this is needed in case the ThP ini overrides the original one
		if [ -f "$DBG_CFG_FILE" ]; then
			sudo sed -i "s/hw_address=.*/hw_address=$CUR_HW_ADDRESS/g" "$DBG_CFG_FILE"
		else
			# create a new ini file in case there isn't and add the current mac addr to it
			echo "[IWL DEBUG CONFIG DATA]" | sudo tee "$DBG_CFG_FILE"
			echo "hw_address=$CUR_HW_ADDRESS" | sudo tee -a "$DBG_CFG_FILE"
		fi
	fi
fi

# edit /lib/firmware/iwl-dbg-cfg.ini for ESL
if [ "${iwl_esl_regression}" = "true" ]; then
	if [ ! -f "$DBG_CFG_FILE" ]; then
		echo "[IWL DEBUG CONFIG DATA]" | sudo tee "$DBG_CFG_FILE"
	fi
	echo "disable_wrt_dump=1" | sudo tee -a "$DBG_CFG_FILE"
	# remove hw_address parameter from iwl-dbg-cfg.ini
	sudo sed -i "/hw_address=.*/d" "$DBG_CFG_FILE"
fi

# enable dbgm in the dbg cfg ini file
if [ "${iwl_dbgm}" = "true" ]; then
	if [ ! -f "$DBG_CFG_FILE" ]; then
		echo "[IWL DEBUG CONFIG DATA]" | sudo tee "$DBG_CFG_FILE"
	fi
	echo "fw_dbg_conf=dbg.bin" | sudo tee -a "$DBG_CFG_FILE"
fi

# reload the kernel modules.(skip if the installation is on ESL)
if [ "${iwl_esl_regression}" = "false" ]; then
	sudo modprobe -r iwlwifi
	sudo modprobe iwlwifi ||
		die $? "ERROR: failed to load the kernel modules"
fi

popd # "${TMP_WORK_DIR}"

echo -e "installation completed successfully.\n"

# reboot
if [ "$iwl_reboot" = "true" ]; then
	echo -e "system Will reboot in 10 seconds...\nin case you don't want to restart press CTRL+C now."
	sleep 10
	sudo reboot
else
	echo "system reboot disabled by option."
fi

exit 0
