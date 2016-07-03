import subprocess,os

from lib.acbs_utils import test_progs

dump_loc = '/var/cache/acbs/tarballs/'
def src_dispatcher(pkg_info):
    if pkg_info['DUMMYSRC'] != '':
        print('[I] Not fetching dummy source as required.')
        return
    if pkg_info['SRCTBL'] != '':
        src_url = pkg_info['SRCTBL']
        return src_url_dispatcher(src_url,pkg_info)
    return
    
def src_url_dispatcher(url,pkg_info):
    #url_array = url.split('\n') #for future usage
    pkg_name = pkg_info['NAME']
    pkg_ver = pkg_info['VER']
    try:
        proto = url.split('://')[0].lower()
    except:
        print('[E] Illegal source URL!!!')
        return False
    if proto == 'http' or proto == 'https':
        return src_tbl_fetch(url, pkg_name + '-' + pkg_ver)
    elif proto == 'git': # or proto == 'git+https'
        print('[W] In spec file: This source seems like a Git repository, while you misplaced it.')
        return src_git_fetch(url, pkg_info)
    elif proto == 'hg':
        return src_hg_fetch(url, pkg_name)
    elif proto == 'svn':
        return src_svn_fetch(url, pkg_name)
    else:
        print('[E] Unknown protocol {}'.format(proto))
        return False
    return True

def src_git_fetch(url, pkg_info):
    
    return

def src_tbl_fetch(url,pkg_slug):
    use_progs = test_downloaders()
    src_name = url.split('/')[-1]
    full_path = os.path.join(dump_loc, src_name)
    for i in use_progs:
        if i == 'aria':
            try:
                aria_get(url, output=full_path)
            except:
                continue
            break
        elif i == 'curl':
            try:
                curl_get(url, output=full_path)
            except:
                continue
            break
        elif i == 'wget':
            try:
                wget_get(url, output=full_path)
            except:
                continue
            break
        elif i == 'axel':
            try:
                axel_get(url, output=full_path)
            except:
                continue
            break
        else:
            raise ValueError('...')
    return

def src_svn_fetch(url):
    
    return

def src_hg_fetch(url):
    
    return

def src_bzr_fetch(url):
    
    return

def src_bk_fetch(url):
    
    return



'''
External downloaders
'''
def test_downloaders():
    use_progs = []
    if test_progs(['aria2c','-h','>','/dev/null']):
        use_progs.append('aria')
    if test_progs(['axel','-h','>','/dev/null']):
        use_progs.append('axel')
    if test_progs(['wget','-h','>','/dev/null']):
        use_progs.append('wget')
    if test_progs(['curl','-h','>','/dev/null']):
        use_progs.append('curl')
    return use_progs

def axel_get(url, threads=4, output=None):
    axel_cmd = ['axel', '-n', threads, '-a' , url]
    if output != None:
        axel_cmd.insert(4, '-o')
        axel_cmd.insert(5, output)
    try:
        subprocess.check_call(axel_cmd)
    except:
        raise AssertionError('Failed to fetch source with Axel!')
    return

def curl_get(url, output=None):
    curl_cmd = ['curl', url] #, '-k'
    if output != None:
        curl_cmd.insert(2, '-o')
        curl_cmd.insert(3, output)
    else:
        curl_cmd.insert(2, '-O')
    try:
        subprocess.check_call(curl_cmd)
    except:
        raise AssertionError('Failed to fetch source with cURL!')
    return

def wget_get(url,output):
    wget_cmd = ['wget','-c',url] #,'--no-check-certificate'
    if output != None:
        axel_cmd.insert(2, '-O')
        axel_cmd.insert(3, output)
    try:
        subprocess.check_call(wget_cmd)
    except:
        raise AssertionError('Failed to fetch source with Wget!')
    return

def aria_get(url, threads=3, output=None):
    if os.path.exists(output) and not os.path.exists(output+'.aria2'):
        return
    aria_cmd = ['aria2c','--max-connection-per-server={}'.format(threads),url,'--auto-file-renaming=false'] #,'--check-certificate=false'
    if output != None:
        aria_cmd.insert(2, '-d')
        aria_cmd.insert(3, dump_loc)
        aria_cmd.insert(4, '-o')
        aria_cmd.insert(5, output.split('/')[-1])
    try:
        subprocess.check_call(aria_cmd)
    except:
        raise AssertionError('Failed to fetch source with Aria2!')
    return

