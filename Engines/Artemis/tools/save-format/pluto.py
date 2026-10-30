import struct
class P:
    def __init__(s, d): s.d=d; s.o=0; s.refs={}; s.maxref=0
    def i(s):
        v=struct.unpack_from('<i',s.d,s.o)[0]; s.o+=4; return v
    def obj(s):
        first=s.i(); ref=s.i()
        if first==0:
            if ref==0: return None
            return ('REF', ref) if ref not in s.refs else s.refs[ref]
        assert ref == s.maxref+1, (ref, s.maxref, s.o)
        s.maxref=ref
        t=s.i()
        if t==1: v=bool(s.i())
        elif t==3: v=struct.unpack_from('<d',s.d,s.o)[0]; s.o+=8
        elif t==4: n=s.i(); v=s.d[s.o:s.o+n].decode('cp932','replace'); s.o+=n
        elif t==5:
            assert s.i()==0
            v={}; s.refs[ref]=v
            mt=s.obj()
            while True:
                k=s.obj()
                if k is None: break
                v[k if not isinstance(k,dict) else id(k)]=s.obj()
        else: raise Exception(f'type {t} at {s.o}')
        s.refs[ref]=v
        return v
def find_var(d, name):
    i=d.index(name.encode())
    n=struct.unpack_from('<I',d,i+len(name))[0]
    return d[i+len(name)+4:i+len(name)+4+n]
if __name__=='__main__':
    import sys
    d=open(sys.argv[1],'rb').read()
    blob=find_var(d, sys.argv[2])
    p=P(blob); v=p.obj()
    print('consumed', p.o, 'of', len(blob), 'refs', p.maxref)
    import json; print(json.dumps(v, ensure_ascii=False, default=str)[:1500])
