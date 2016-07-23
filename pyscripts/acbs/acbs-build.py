#!/bin/env python3
'''
ACBS - AOSC CI Build System
A small alternative system to port abbs to CI environment to prevent
from irregular bash failures
'''
import os
import sys
import subprocess
import argparse
# import time

from lib.acbs_find import *
from lib.acbs_parser import *
from lib.acbs_deps import *
from lib.acbs_utils import *
from lib.acbs_start_build import *

acbs_version = '0.0.1-alpha0'
verbose = 0


def main():
    parser = argparse.ArgumentParser(description=help_msg(), formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('-v', '--version', help='Show the version and exit', action="store_true")
    parser.add_argument('-d', '--debug', help='Increase verbosity to ease debugging process', action="store_true")
    parser.add_argument('packages', nargs='*', help='Packages to be built')
    args = parser.parse_args()
    if args.version:
        print('ACBS version {}'.format(acbs_version))
    if len(args.packages) > 0:
        init_env()
        sys.exit(build_pkgs(args.packages))


def init_env():
    dump_loc = '/var/cache/acbs/tarballs/'
    tmp_loc = '/var/cache/acbs/build/'
    print("----- Welcome to ACBS - %s -----" % (acbs_version))
    if not os.path.isdir(dump_loc):
        os.makedirs(dump_loc)  # Too lazy to handle I/O exceptions here ...
    if not os.path.isdir(tmp_loc):
        os.makedirs(tmp_loc)
    return


def build_pkgs(pkgs):
    for pkg in pkgs:
        matched_pkg = acbs_pkg_match(pkg)
        if matched_pkg is None:
            err_msg('No valid candidate package found for \033[36m{}\033[0m.'.format(pkg))
            # print('[E] No valid candidate package found for {}'.format(pkg))
            return -1
        else:
            return build_ind_pkg(matched_pkg)
    return 0


def build_ind_pkg(pkg):
    print('[I] Start building \033[36m{}\033[0m'.format(pkg))
    pkg_type_res = determine_pkg_type(pkg)
    if pkg_type_res is False:
        err_msg()
    elif pkg_type_res is True:
        pass
    else:
        return build_sub_pkgs(pkg, pkg_type_res)
    try:
        pkg_slug = os.path.basename(pkg)
    except:
        pkg_slug = pkg
    abbs_spec = parse_abbs_spec(pkg, pkg_slug)
    repo_dir = os.path.abspath(pkg)
    if abbs_spec is False:
        err_msg()
        return -1
    # parser_pass_through(abbs_spec,pkg)
    abd_dict = parse_ab3_defines(os.path.join(pkg, 'autobuild/defines'))
    deps_result, try_build = process_deps(abd_dict['BUILDDEP'], abd_dict['PKGDEP'], pkg_slug)
    if (deps_result is False) and (try_build is None):
        err_msg('Failed to process dependencies!')
        return -1
    if try_build is not None:
        import threading
        for sub_pkg in list(try_build):
            dumb_mutex = threading.Lock()
            dumb_mutex.acquire()
            try:
                sub_thread = threading.Thread(target=slave_thread_build, args=[sub_pkg])
                sub_thread.start()
                sub_thread.join()
                dumb_mutex.release()
            except:
                err_msg('Sub-build process using thread {}, building \033[36m{}\033[0m \033[93mfailed!\033[0m'.format(sub_thread.name,sub_pkg))
                return 128
    src_proc_result, tmp_dir_loc = src_dispatcher(abbs_spec)
    if src_proc_result is False:
        err_msg('Failed to fetch and process source files!')
    if not start_ab3(tmp_dir_loc, repo_dir, abbs_spec):
        err_msg('Autobuild process failure!')
        return 1
    
    return 0


def slave_thread_build(pkg):
    import threading
    print('[D] New build thread \033[36m{}\033[0m started for \033[36m{}\033[0m'.format(threading.current_thread().getName(),pkg))
    build_pkgs([pkg])


def build_sub_pkgs(pkg_base, pkgs_array):
    pkg_names = []
    for i in pkgs_array:
        pkg_names.append(pkgs_array[i])
        repo_dir = os.path.abspath(pkg_base + '/' + i + '-' + pkg_names)
    print('[I] Package group detected: contains: \033[36m{}\033[0m'.format(arr2str(pkg_names)))
    pass


def help_msg():
    help_msg = 'ACBS - AOSC CI Build System\nVersion: {}\nA small alternative system to port \
abbs to CI environment to prevent from irregular bash failures'.format(acbs_version)
    return help_msg


if __name__ == '__main__':
    main()
