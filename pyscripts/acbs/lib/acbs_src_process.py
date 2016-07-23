import os
# import sys
import magic
import subprocess
import re
import tempfile
from acbs_utils import test_progs, group_match


def src_proc_dispatcher(pkg_name, src_tbl_name, src_loc):
    tobj = tempfile.TemporaryDirectory(dir='/var/cache/acbs/', prefix='acbs.')
    shadow_ark_loc = os.path.join(tobj.name, src_tbl_name)
    os.symlink(src_loc, shadow_ark_loc)
    return decomp_file(shadow_ark_loc, tobj.name), tobj.name


def file_type(file_loc):
    mco = magic.open(magic.MIME_TYPE)
    mco.load()
    try:
        tp = mco.file(file_loc)
    except:
        print('[W] Unable to determine the file type!')
        return 'unknown/unknown'
    return tp


def file_type_full(file_loc):
    mco = magic.open(magic.NONE)
    mco.load()
    try:
        tp = mco.file(file_loc)
    except:
        print('[W] Unable to determine the file type!')
        return 'data'
    return tp


def decomp_file(file_loc, dest):
    file_type_name = file_type(file_loc)
    ext_list = ['x-tar*', 'zip*', 'x-zip*', 'x-cpio*', 'x-gzip*', 'x-bzip*', 'x-xz*']
    if re.match('*application', file_type_name[0]) and group_match(ext_list):
        # x-tar*|zip*|x-*zip*|x-cpio*|x-gzip*|x-bzip*|x-xz*
        pass
    else:
        print('[W] ACBS don\'t know how to decompress {} file, will leave it as is!'.format(file_type_full(file_loc)))
        return True
    return decomp_lib(file_loc, dest)


def decomp_ext(file_loc, dest):
    if not test_progs(['bsdtar', '-h']):
        print('[E] Unable to use bsdtar. Can\'t decompress files... :-(')
        return False
    try:
        subprocess.check_call(['bsdtar', '-xf', file_loc, '-C', dest])
    except:
        print('[E] Unable to decompress file! File corrupted?! Permission?!')
        return False
    return True


def decomp_lib(file_loc, dest):
    try:
        import libarchive
        import os
    except:
        print('[W] Failed to load libarchive library! Fall back to bsdtar!')
        return decomp_ext(file_loc, dest)
    # Begin
    os.chdir(dest)
    try:
        libarchive.extract.extract_file(file_loc)
    except:
        print('[E] Extraction failure!')
        return False
    return True
