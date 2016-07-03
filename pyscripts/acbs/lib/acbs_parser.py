import os,sys,io
import subprocess
from configparser import RawConfigParser

from lib.acbs_utils import *
from lib.acbs_src_fetch import src_dispatcher

def parse_abbs_spec(spec_file, pkg_name):
    try:
        fp = open(spec_file, 'rt')
        spec_cont = fp.read()
        fp.close()
    except:
        print('[E] Failed to load spec file! Do you have read permission?')
        return False
    # Stupid but necessary laundry list of possible varibles
    script = spec_cont + gen_laundry_list(['VER','REL','SUBDIR','SRCTBL','GITSRC','GITCO','GITBRCH','SVNSRC','SVNCO','HGSRC','BZRSRC','BZRCO','DUMMYSRC'])
    try:
        spec_out = subprocess.check_output(script, shell=True) # Better to be replaced by subprocess.Popen
    except:
        print('[E] Malformed spec file found! Couldn\'t continue!')
        return False
    spec_fp = io.StringIO('[wrap]\n' + spec_out.decode('utf-8')) #Assume it's UTF-8 since we have no clue of the real world on how it works ...
    config = RawConfigParser()
    config.read_file(spec_fp)
    config_dict={}
    for i in config['wrap']:
        config_dict[i.upper()] = config['wrap'][i]
    config_dict['NAME'] = pkg_name
    res, err_msg = parser_validate(config_dict)
    if res != True:
        print('[E] {}'.format(err_msg))
        return False
    return src_dispatcher(config_dict)
    #return True

def parser_ab3_defines(defines_file):
    
    return True

def parser_validate(in_dict):
    #just a simple validate for now
    if in_dict['NAME'] == '' or in_dict['VER'] == '':
        return False, 'Package name or version not valid!!!'
    if check_empty(1, in_dict, ['SRCTBL','GITSRC','SVNSRC','HGSRC','BZRSRC']) == True:
        return False, 'No source specified!'
    return True,''
