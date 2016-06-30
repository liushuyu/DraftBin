#!/bin/env python
import os,sys

'''
ACBS Search
'''
def acbs_pkg_match(target):
    if os.path.isdir(target):
        return target
    target_slug = target.split('/')
    if len(target_slug) > 1:
        target = target_slug[1]
    outer_dirlist = os.listdir('.')
    inner_dirlist = []
    global cur_dir
    cur_dir = ''
    for i in outer_dirlist:
        if os.path.isdir(i):
            cur_dir = i
            inner_dirlist = os.listdir(i)
            for j in inner_dirlist:
                if os.path.isdir(i+'/'+j):
                    if j == target:
                        return '{}/{}'.format(cur_dir,j)
    return None
 
