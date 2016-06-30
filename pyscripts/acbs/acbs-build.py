#!/bin/env python3
'''
ACBS - AOSC CI Build System
A small alternative system to port abbs to CI environment to prevent
from irregular bash failures
'''
import os, sys, subprocess
import argparse

from lib.acbs_find import *

acbs_version = '0.0.1-alpha0'
verbose = 0

def main():
    parser = argparse.ArgumentParser(description=help_msg(), formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('-v','--version', help='Show the version and exit',action="store_true")
    parser.add_argument('-d','--debug', help='Increase verbosity to ease debugging process',action="store_true")
    parser.add_argument('build_pkgs',nargs='*', help='Packages to be built')
    args = parser.parse_args()
    if args.version:
        print('ACBS version {}'.format(acbs_version))
    if len(args.build_pkgs) > 0:
        init_env()
        build_pkgs(args.build_pkgs)
    
def init_env():
    print("----- Welcome to ACBS - %s -----" % (acbs_version) )
    return

def build_pkgs(pkgs):
    for pkg in pkgs:
        build_ind_pkg(acbs_pkg_match(pkg))
    return 0

def build_ind_pkg(pkg):
    print('[I] Start building \033[36m{}\033[0m'.format(pkg))
    return 0

def help_msg():
    help_msg = 'ACBS - AOSC CI Build System\nVersion: {}\nA small alternative system to port \
abbs to CI environment to prevent from irregular bash failures'.format(acbs_version)
    return help_msg

    
if __name__ == '__main__':
    main()
