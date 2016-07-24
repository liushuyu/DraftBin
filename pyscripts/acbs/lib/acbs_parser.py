import os
import sys
import io
import subprocess
from configparser import RawConfigParser

from lib.acbs_utils import *
from lib.acbs_src_fetch import src_dispatcher


def parse_abbs_spec(spec_file_loc, pkg_name):
    try:
        fp = open(spec_file_loc + '/spec', 'rt')
        spec_cont = fp.read()
        fp.close()
    except:
        print('[E] Failed to load spec file! Do you have read permission?')
        return False
    # Stupid but necessary laundry list of possible varibles
    script = spec_cont + gen_laundry_list(['VER', 'REL', 'SUBDIR', 'SRCTBL', 'GITSRC', 'GITCO', 'GITBRCH', 'SVNSRC', 'SVNCO', 'HGSRC', 'BZRSRC', 'BZRCO', 'DUMMYSRC'])
    try:
        spec_out = subprocess.check_output(script, shell=True)  # Better to be replaced by subprocess.Popen
    except:
        print('[E] Malformed spec file found! Couldn\'t continue!')
        return False
    spec_fp = io.StringIO('[wrap]\n' + spec_out.decode('utf-8'))  # Assume it's UTF-8 since we have no clue of the real world on how it works ...
    config = RawConfigParser()
    config.read_file(spec_fp)
    config_dict = {}
    for i in config['wrap']:
        config_dict[i.upper()] = config['wrap'][i]
    config_dict['NAME'] = pkg_name
    res, err_msg = parser_validate(config_dict)
    if res is not True:
        print('[E] {}'.format(err_msg))
        return False
    return config_dict


def parser_pass_through(config_dict, spec_file_loc):
    write_ab = {'VER': config_dict['VER'], 'REL': config_dict['REL']}
    return write_ab3_defines(spec_file_loc + '/autobuild/defines', write_ab)
    # return True# src_dispatcher(config_dict)
    # return True


def parse_ab3_defines(defines_file):  # , pkg_name):
    try:
        fp = open(defines_file, 'rt')
        abd_cont = fp.read()
        fp.close()
    except:
        print('[E] Failed to load autobuild defines file! Do you have read permission?')
        return False
    script = "ARCH={}\n".format(get_arch_name()) + abd_cont + gen_laundry_list(['PKGDEP', 'BUILDDEP'])
    try:
        abd_out = subprocess.check_output(script, shell=True)  # Better to be replaced by subprocess.Popen
    except:
        print('[E] Malformed Autobuild defines file found! Couldn\'t continue!')
        return False
    abd_fp = io.StringIO('[wrap]\n' + abd_out.decode('utf-8'))
    abd_config = RawConfigParser()
    abd_config.read_file(abd_fp)
    abd_config_dict = {}
    for i in abd_config['wrap']:
        abd_config_dict[i.upper()] = abd_config['wrap'][i]
    return abd_config_dict

def parser_validate(in_dict):
    # just a simple validate for now
    if in_dict['NAME'] == '' or in_dict['VER'] == '':
        return False, 'Package name or version not valid!!!'
    if check_empty(1, in_dict, ['SRCTBL','GITSRC','SVNSRC','HGSRC','BZRSRC']) == True:
        return False, 'No source specified!'
    return True, ''

def write_ab3_defines(def_file_loc, in_dict):
    print(def_file_loc)
    str_to_write = ''
    for i in in_dict:
        str_to_write = str_to_write + i + '=' + '\"' + in_dict[i] + '\"\n'
    try:
        fp = open(def_file_loc, 'at')
        fp.write(str_to_write)
    except:
        print('[E] Failed to update information in \033[36m{}\033[0m'.format(def_file_loc))
        return False
    return True


def determine_pkg_type(pkg):
    sub_pkgs = set(os.listdir(pkg)) - set(['spec'])
    if len(sub_pkgs) > 1:
        sub_dict = {}
        for i in sub_pkgs:
            tmp_array = i.split('-', 1)
            try:
                sub_dict[int(tmp_array[0])] = tmp_array[1]
            except:
                print('[E] Expecting numeric value, got {}'.format(tmp_array[0]))
                return False
        return sub_dict
    else:
        return True
