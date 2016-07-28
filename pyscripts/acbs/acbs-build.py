#!/bin/env python3
'''
ACBS - AOSC CI Build System
A small alternative system to port abbs to CI environment to prevent
from irregular bash failures
'''
import os
import sys
import shutil
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
    parser = argparse.ArgumentParser(description=help_msg(
    ), formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('-v', '--version',
                        help='Show the version and exit', action="store_true")
    parser.add_argument(
        '-d', '--debug', help='Increase verbosity to ease debugging process', action="store_true")
    parser.add_argument('-t', '--tree', nargs=1, dest='acbs_tree', help='Specify which abbs-tree to use')
    parser.add_argument('packages', nargs='*', help='Packages to be built')
    args = parser.parse_args()
    if args.version:
        print('ACBS version {}'.format(acbs_version))
    if len(args.packages) > 0:
        if args.acbs_tree is not None:
            init_env(args.acbs_tree)
        else:
            init_env()
        sys.exit(build_pkgs(args.packages))


def init_env(tree='default'):
    dump_loc = '/var/cache/acbs/tarballs/'
    tmp_loc = '/var/cache/acbs/build/'
    print("----- Welcome to ACBS - %s -----" % (acbs_version))
    try:
        if not os.path.isdir(dump_loc):
            os.makedirs(dump_loc)
        if not os.path.isdir(tmp_loc):
            os.makedirs(tmp_loc)
    except:
        raise IOError('\033[93mFailed to make work directory\033[0m!')
    if os.path.exists('/etc/acbs_forest.conf'):
        tree_loc = parse_acbs_conf(tree[0])
        if tree_loc is not None:
            os.chdir(tree_loc)
        else:
            sys.exit(1)
    else:
        if not write_acbs_conf():
            sys.exit(1)
    return


def build_pkgs(pkgs):
    for pkg in pkgs:
        matched_pkg = acbs_pkg_match(pkg)
        if matched_pkg is None:
            err_msg(
                'No valid candidate package found for \033[36m{}\033[0m.'.format(pkg))
            # print('[E] No valid candidate package found for {}'.format(pkg))
            return -1
        else:
            if build_ind_pkg(matched_pkg) == 0:
                continue
            else:
                return -1
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
    deps_result, try_build = process_deps(
        abd_dict['BUILDDEP'], abd_dict['PKGDEP'], pkg_slug)
    if (deps_result is False) and (try_build is None):
        err_msg('Failed to process dependencies!')
        return -1
    if try_build is not None:
        if new_build_thread(try_build) != 0:
            return 128
    src_dispatcher_return = src_dispatcher(abbs_spec)
    if isinstance(src_dispatcher_return, tuple):
        src_proc_result, tmp_dir_loc = src_dispatcher_return
    else:
        src_proc_result = src_dispatcher_return
    if src_proc_result is False:
        err_msg('Failed to fetch and process source files!')
        return 1
    repo_ab_dir = os.path.join(repo_dir, 'autobuild/')
    if not start_ab3(tmp_dir_loc, repo_ab_dir, abbs_spec):
        err_msg('Autobuild process failure!')
        return 1
    return 0


def new_build_thread(try_build):
    import threading
    for sub_pkg in list(try_build):
        dumb_mutex = threading.Lock()
        dumb_mutex.acquire()
        try:
            sub_thread = threading.Thread(
                target=slave_thread_build, args=[sub_pkg])
            sub_thread.start()
            sub_thread.join()
            dumb_mutex.release()
            return 0
        except:
            err_msg(
                'Sub-build process using thread {}, building \033[36m{}\033[0m \033[93mfailed!\033[0m'.format(sub_thread.name, sub_pkg))
            return 128


def slave_thread_build(pkg):
    import threading
    print('[D] New build thread \033[36m{}\033[0m started for \033[36m{}\033[0m'.format(
        threading.current_thread().getName(), pkg))
    build_pkgs([pkg])


def build_sub_pkgs(pkg_base, pkgs_array):
    pkg_tuple = []
    for i in pkgs_array:
        if i < 10:
            str_i = '0' + str(i)
        repo_dir = os.path.abspath(
            pkg_base + '/' + str_i + '-' + pkgs_array[i])
        pkg_tuple.append((pkgs_array[i], repo_dir))
    pkg_names = []
    for i in pkg_tuple:
        pkg_names.append(i[0])
    print('[I] Package group detected\033[36m({})\033[0m: contains: \033[36m{}\033[0m'.format(
        len(pkg_tuple), arr2str(pkg_names)))
    abbs_spec = parse_abbs_spec(pkg_base, os.path.basename(pkg_base))
    pkg_def_loc = []
    sub_repo_dir = []
    for i in pkg_tuple:
        pkg_def_loc.append(i[1] + '/defines')
        sub_repo_dir.append(i[1])
    onion_list = bat_parse_ab3_defines(pkg_def_loc)
    if onion_list is False:
        return 1
    src_dispatcher_return = src_dispatcher(abbs_spec)
    if isinstance(src_dispatcher_return, tuple):
        src_proc_result, tmp_dir_loc = src_dispatcher_return
    else:
        src_proc_result = src_dispatcher_return
    if src_proc_result is False:
        err_msg('Failed to fetch and process source files!')
        return 1
    sub_count = 0
    for abd_sub_dict in onion_list:
        sub_count += 1
        print('[I] [\033[36m{}/{}\033[0m] Building sub package \033[36m{}\033[0m'.format(sub_count,
                                                           len(onion_list), abd_sub_dict['PKGNAME']))
        pkg_slug = abd_sub_dict['PKGNAME']
        deps_result, try_build = process_deps(
            abd_sub_dict['BUILDDEP'], abd_sub_dict['PKGDEP'], pkg_slug)
        if (deps_result is False) and (try_build is None):
            err_msg('Failed to process dependencies!')
            return -1
        if try_build is not None:
            if new_build_thread(try_build) != 0:
                return 128
        if not start_ab3(tmp_dir_loc, sub_repo_dir[sub_count - 1], abbs_spec, rm_abdir=True):
            err_msg('Autobuild process failure on {}!'.format(abd_sub_dict['PKGNAME']))
            return 1
    return 0


def help_msg():
    help_msg = 'ACBS - AOSC CI Build System\nVersion: {}\nA small alternative system to port \
abbs to CI environment to prevent from irregular bash failures'.format(acbs_version)
    return help_msg


if __name__ == '__main__':
    main()
