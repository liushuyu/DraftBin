import subprocess

def arr2str(array):
    str_out = ''
    for i in array:
        str_out = str_out + i + ' '
    return str_out

def gen_laundry_list(items):  #You know what, 'laundry list' can be a joke in somewhere...
    str_out = '\n\n'
    for i in items:
        str_out = str_out + 'echo ' + i + '=' + '\"' + '$' + i + '\"' + '\n'
    return str_out

def test_progs(cmd):
    try:
        subprocess.check_output(cmd)
    except:
        return False
    
    return True

def check_empty(logic_method, in_dict, in_array):
    '''
logic_method: 1= OR'ed , 2= AND'ed
    '''
    if logic_method == 2:
        for i in in_array:
            if in_dict[i] == '' or in_dict[i] == None:
                return True  # This is empty
    elif logic_method == 1:
        for i in in_array:
            if in_dict[i] != '' and in_dict[i] != None:
                return False
        return True
    else:
        raise ValueError('Value of logic_method is illegal!')
    return False
