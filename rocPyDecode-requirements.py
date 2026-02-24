# Copyright (c) 2023 - 2024 Advanced Micro Devices, Inc. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

import os
import sys
import argparse
import platform
import traceback
import shutil
from pathlib import Path

if sys.version_info[0] < 3:
    import commands
else:
    import subprocess

__copyright__ = "Copyright (c) 2024, AMD ROCm rocPyDecode"
__version__ = "0.8.0"
__status__ = "Shipping"

# error check calls
def ERROR_CHECK(waitval):
    if(waitval != 0): # return code and signal flags
        print('ERROR_CHECK failed with status:'+str(waitval))
        traceback.print_stack()
        status = ((waitval >> 8) | waitval) & 255 # combine exit code and wait flags into single non-zero byte
        exit(status)

# Arguments
parser = argparse.ArgumentParser()
parser.add_argument('--rocm_path', type=str, default='/opt/rocm',
                    help='ROCm Installation Path - optional (default:/opt/rocm) - ROCm Installation Required')

args = parser.parse_args()
ROCM_PATH = args.rocm_path

# Detect TheRock layout first: default install prefix at $HOME/install
home_install = os.path.join(str(Path.home()), 'install')
the_rock_sysdeps_home = os.path.join(home_install, 'lib', 'rocm_sysdeps', 'lib')
USING_THE_ROCK = False
if os.path.exists(the_rock_sysdeps_home):
    USING_THE_ROCK = True
    ROCM_PATH = home_install
    os.environ['ROCM_PATH'] = ROCM_PATH  # keep in-process environment consistent
    print("\nTheRock ROCm installation detected (found: " + the_rock_sysdeps_home + ")\n")
else:
    # CMake-style ROCM_PATH selection: env override, then user arg, else default (/opt/rocm from argparse)
    if "ROCM_PATH" in os.environ:
        ROCM_PATH = os.environ.get('ROCM_PATH')
    elif args.rocm_path:
        ROCM_PATH = args.rocm_path
    else:
        ROCM_PATH = '/opt/rocm'

print("\nROCm PATH set to -- "+ROCM_PATH+"\n")

# check ROCm installation
if os.path.exists(ROCM_PATH):
    print("\nROCm Installation Found -- " + ROCM_PATH + "\n")
    os.system('echo ROCm Info -- && ' + ROCM_PATH + '/bin/rocminfo')
else:
    print(
        "WARNING: If ROCm installed, set ROCm Path with \"--rocm_path\" option for full installation [Default:/opt/rocm]\n")
    print("ERROR: rocPyDecode Setup requires ROCm install\n")
    exit(-1)

# Detect TheRock style ROCm installation (bundles libs under rocm_sysdeps)
THE_ROCK_SYSDEPS = os.path.join(ROCM_PATH, 'lib', 'rocm_sysdeps', 'lib')
if os.path.exists(THE_ROCK_SYSDEPS):
    USING_THE_ROCK = True
    print("\nTheRock ROCm installation detected (found: " + THE_ROCK_SYSDEPS + ")\n")

# get platform info
platformInfo = platform.platform()

# sudo requirement check
sudoLocation = ''
userName = ''
if sys.version_info[0] < 3:
    status, sudoLocation = commands.getstatusoutput("which sudo")
    if sudoLocation != '/usr/bin/sudo':
        status, userName = commands.getstatusoutput("whoami")
else:
    status, sudoLocation = subprocess.getstatusoutput("which sudo")
    if sudoLocation != '/usr/bin/sudo':
        status, userName = subprocess.getstatusoutput("whoami")

# check os version
os_info_data = 'NOT Supported'
if os.path.exists('/etc/os-release'):
    with open('/etc/os-release', 'r') as os_file:
        os_info_data = os_file.read().replace('\n', ' ')
        os_info_data = os_info_data.replace('"', '')

# setup for Linux
linuxSystemInstall = ''
linuxCMake = 'cmake'
linuxSystemInstall_check = ''
linuxFlag = ''
sudoValidate = 'sudo -v'
osUpdate = ''
if "centos" in os_info_data or "redhat" in os_info_data or "Oracle" in os_info_data:
    linuxSystemInstall = 'yum -y'
    linuxSystemInstall_check = '--nogpgcheck'
    osUpdate = 'makecache'
    if "VERSION_ID=7" in os_info_data:
        linuxCMake = 'cmake3'
        sudoValidate = 'sudo -k'
        platformInfo = platformInfo+'-redhat-7'
    elif "VERSION_ID=8" in os_info_data:
        platformInfo = platformInfo+'-redhat-8'
    elif "VERSION_ID=9" in os_info_data:
        platformInfo = platformInfo+'-redhat-9'
    else:
        platformInfo = platformInfo+'-redhat-centos-undefined-version'
elif "Ubuntu" in os_info_data:
    linuxSystemInstall = 'apt-get -y'
    linuxSystemInstall_check = '--allow-unauthenticated'
    linuxFlag = '-S'
    osUpdate = 'update'
    if "VERSION_ID=20" in os_info_data:
        platformInfo = platformInfo+'-Ubuntu-20'
    elif "VERSION_ID=22" in os_info_data:
        platformInfo = platformInfo+'-Ubuntu-22'
    elif "VERSION_ID=24" in os_info_data:
        platformInfo = platformInfo+'-Ubuntu-24'
    else:
        platformInfo = platformInfo+'-Ubuntu-undefined-version'
elif "SLES" in os_info_data:
    linuxSystemInstall = 'zypper -n'
    linuxSystemInstall_check = '--no-gpg-checks'
    platformInfo = platformInfo+'-SLES'
    osUpdate = 'refresh'
elif "Mariner" in os_info_data:
    linuxSystemInstall = 'tdnf -y'
    linuxSystemInstall_check = '--nogpgcheck'
    platformInfo = platformInfo+'-Mariner'
    osUpdate = 'makecache'
else:
    print("\rocPyDecode Setup on "+platformInfo+" is unsupported\n")
    print("\rocPyDecode Setup Supported on: Ubuntu 20/22, RedHat 8/9, & SLES 15\n")
    exit(-1)

# rocPyDecode Setup
print("\nrocPyDecode Setup on: "+platformInfo+"\n")
print("\nrocPyDecode Dependencies Installation with rocPyDecode-setup.py V-"+__version__+"\n")

if userName == 'root':
    ERROR_CHECK(os.system(linuxSystemInstall+' update'))
    ERROR_CHECK(os.system(linuxSystemInstall+' install sudo'))

# source install - common package dependencies
commonPackages = [
    'cmake',
    'pkg-config'
]

# Debian packages
coreDebianPackages = [
    'rocdecode-dev',
    'rocdecode-host',
    'rocjpeg-dev',
    'python3-dev',
    'python3-pip',
    'python3-pybind11',
    'libdlpack-dev',
    'python3-numpy'
]

# core RPM packages
# TODO: dlpack package missing in RPM
coreRPMPackages = [
    'rocdecode-devel',
    'rocjpeg-devel',
    'python3-devel',
    'python3-pybind11',
    'python3-pip',
    'python3-numpy'
]

# TheRock installs bundle rocdecode/rocjpeg into rocm_sysdeps, so skip those packages
if USING_THE_ROCK:
    coreDebianPackages = [pkg for pkg in coreDebianPackages if not pkg.startswith(('rocdecode', 'rocjpeg'))]
    coreRPMPackages = [pkg for pkg in coreRPMPackages if not pkg.startswith(('rocdecode', 'rocjpeg'))]
    print("Skipping rocdecode/rocjpeg package installs for TheRock ROCm layout\n")

# update
ERROR_CHECK(os.system('sudo '+linuxFlag+' '+linuxSystemInstall +' '+linuxSystemInstall_check+' '+osUpdate))

# common packages
ERROR_CHECK(os.system(sudoValidate))
for i in range(len(commonPackages)):
    ERROR_CHECK(os.system('sudo '+linuxFlag+' '+linuxSystemInstall +
            ' '+linuxSystemInstall_check+' install '+ commonPackages[i]))

# setup directory path
setupDir_deps = '~/rocpydecode-deps'
deps_dir = os.path.expanduser(setupDir_deps)
deps_dir = os.path.abspath(deps_dir)

# Define deps_dir safely using absolute path
deps_dir = os.path.abspath(os.path.expanduser(deps_dir))
# safety check: ensure it's under home directory or a known safe location
home_dir = str(Path.home())
if os.path.isdir(deps_dir):
    if deps_dir.startswith(home_dir):
        shutil.rmtree(deps_dir)
        print("rocpydecode Setup: Removing Previous Install -- "+deps_dir+"\n")
    else:
        raise ValueError(f"Refusing to delete unsafe path: {deps_dir}")

# dlpack - https://github.com/dmlc/dlpack
if "ubuntu" in platformInfo:
    ERROR_CHECK(os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +' install libdlpack-dev'))
elif "sles" in platformInfo:
    ERROR_CHECK(os.system('sudo '+linuxFlag+' '+linuxSystemInstall+' '+linuxSystemInstall_check +' install dlpack-devel'))
else:
    try:
        os.makedirs(deps_dir, exist_ok=True)
    except Exception as e:
        print(f"Error creating directory {deps_dir}: {e}")
        sys.exit(1)
    try:
        subprocess.run(
            ['bash', '-c', f'cd {deps_dir} && git clone -b v1.0 https://github.com/dmlc/dlpack.git'],
            check=True
        )
        exit_code = 0
    except subprocess.CalledProcessError as e:
        exit_code = e.returncode
    ERROR_CHECK(exit_code)
    try:
        cmd = (
            f'cd {deps_dir}/dlpack && '
            'mkdir -p build && '
            'cd build && '
            f'{linuxCMake} .. && '
            'make -j$(nproc) && '
            'sudo make install'
        )
        subprocess.run(['bash', '-c', cmd], check=True)
        exit_code = 0
    except subprocess.CalledProcessError as e:
        exit_code = e.returncode
    ERROR_CHECK(exit_code)

# rocPyDecode Requirements
ERROR_CHECK(os.system(sudoValidate))
if "Ubuntu" in platformInfo:
    # core debian packages
    for i in range(len(coreDebianPackages)):
        ERROR_CHECK(os.system('sudo '+linuxFlag+' '+linuxSystemInstall +
                    ' '+linuxSystemInstall_check+' install '+ coreDebianPackages[i]))
elif "redhat" in platformInfo:
    # core RPM packages
    for i in range(len(coreRPMPackages)):
            ERROR_CHECK(os.system('sudo '+linuxFlag+' '+linuxSystemInstall +
                    ' '+linuxSystemInstall_check+' install '+ coreRPMPackages[i]))

# Tests requirements
#ERROR_CHECK(os.system('python3 -m pip install -i https://test.pypi.org/simple hip-python'))

# clean up temp folders
deps_dir = os.path.abspath(os.path.expanduser(deps_dir)) # Resolve deps_dir to absolute path
# Safety check: allow deletion only inside user's home directory
home_dir = str(Path.home())
if os.path.isdir(deps_dir):
    if deps_dir.startswith(home_dir):
        shutil.rmtree(deps_dir)
        print("rocpydecode Setup: Removing Previous Install -- "+deps_dir+"\n")
    else:
        raise ValueError(f"Refusing to delete unsafe path: {deps_dir}")

print("rocPyDecode Dependencies Installed with rocPyDecode-setup.py V-"+__version__)
