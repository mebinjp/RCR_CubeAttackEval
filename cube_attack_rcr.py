import random
import numpy as np
import sympy as sp
import time
from rcr32_helper import keystream

def SP(f, key, index, k, v):
    s = 0
    v1 = len(index)
    for c in range(2**v1):
        iv = 0
        for i in range(v1):
            iv ^= (((c>>i)&1) << index[i])
        s ^= f(key,iv,k,v)    
    return s
    
def check_linearity_SP(f, index, k, v, itrs):
    while (itrs > 0):
        x=random.SystemRandom().randint(0,(2**k)-1)
        y=random.SystemRandom().randint(0,(2**k)-1)
        while y==x:
            y=random.SystemRandom().randint(0,(2**k)-1)
        z= x ^ y
        if ((SP(f,x,index,k,v) ^ SP(f,y,index,k,v) ^ SP(f,0,index,k,v)) != SP(f,z,index,k,v)):
            return False
        itrs -= 1
    return True

def SP_coff(f, index, k, v):
    out = [SP(f, 0, index, k, v)]
    for i in range(k):
        o = SP(f, 1<<i, index, k, v)
        out.append(o ^ out[0])
    return out

def random_walk(index,f,k,v,tm):
    #print(index)
    if time.perf_counter() > tm+(60*5):
        return False, False
    check_const = 100
    isConstant = True
    rndkey = random.SystemRandom().randint(0,(2**k)-1)
    out = SP(f, rndkey, index, k, v)
    while check_const > 0:
        rndkey = random.SystemRandom().randint(0,(2**k)-1)
        if (out != SP(f, rndkey, index, k, v)):
            isConstant = False
            break
        check_const -= 1
    if (isConstant == True):
        if len(index) == 1:
            return False, False
        return random_walk(random.sample(index,len(index)-1), f, k, v, tm)
    if (check_linearity_SP(f, index, k, v, 100) == False):
        if (len(index) >= max_ind_len):
            return False, False
        iv = range(v)
        while (True):
            ri = random.choice( iv )
            if (ri not in index):
                index.append(ri)
                return random_walk(index, f, k, v, tm)
    return index, SP_coff(f, index, k, v)


def preproc(f, k, v):
    t1 = time.perf_counter()
    S=[]
    while (len(S) < k) & (time.perf_counter() < t1+(96*3600)):
        #inlen = random.choice( range(1,v+1) )
        inlen = random.choice( range(1, max_ind_len) )
        index = random.sample( range(v), inlen )
        tm = time.perf_counter()
        mxind, spoly = random_walk(index,f,k,v,tm)
        if mxind != False:
            if sum(spoly[1:]) > 0:
                tmp = S + [spoly]
                mat = np.array(tmp)
                _, inds = sp.Matrix(mat).T.rref()
                if (len(inds) == len(S)+1):
                    print(mxind,spoly)
                    S += [spoly]
    return S

def kstream(key1, iv1, k, v):
    key = []
    iv = []
    for i in range(int(k/8)):
        key += [( key1>>(8*i) )& (0xff)]
        iv += [( iv1>>(8*i) )& (0xff)]
    for i in range(int(k/8), int(v/8)):
        iv += [( iv1>>(8*i) )& (0xff)]   
    ks = keystream(key, iv, k, v)
    return ks[0]&1

max_ind_len = 30

print(preproc(kstream, 256, 384))
