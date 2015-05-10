
if [ "X$(basename -- "$0")" == "Xtimo-env.bash" ]; then
    echo "Source this file (do NOT execute it!) to set the VxMicro environment."
    exit
fi

# You can further customize your environment by creating a bash script called
# timo-env_install.bash in your home directory. It will be automatically
# run (if it exists) by this script.

# identify OS source tree root directory
export TIMO_BASE=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

# prepend VxMicro build system tools to PATH, if not already present
timo_linux_bin=${TIMO_BASE}/host/x86-linux2/bin
scripts_path=${TIMO_BASE}/scripts
echo "${PATH}" | grep -q "${timo_linux_bin}"
[ $? != 0 ] && export PATH=${timo_linux_bin}:${PATH}
unset timo_linux_bin
echo "${PATH}" | grep -q "${scripts_path}"
[ $? != 0 ] && export PATH=${scripts_path}:${PATH}
unset scripts_path

# enable custom environment settings
timo_answer_file=~/timo-env_install.bash
[ -f ${timo_answer_file} ] && . ${timo_answer_file}
unset timo_answer_file
