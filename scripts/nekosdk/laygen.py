# usage: laygen.py <plain or packed save> <layout name>: finds a game's record and text sizes and prints its SaveLayout. Numbers only.
import zlib,struct,sys,re
save,name=sys.argv[1:3]
def un(f):
    d=bytearray(open(f,'rb').read()); key=(len(d)//8+0x22)&0xffffffff
    for i in range(4):
        d[i]^=key&0xff; key=(key<<3)&0xffffffff
    return zlib.decompress(bytes(d[:-4]))
b=un(save)
def rd(p): return struct.unpack_from('<i',b,p)[0]
def sparse(x):
    out=[];i=0
    while i<len(x):
        if x[i]:
            j=i
            while j<len(x) and (x[j] or any(x[j:j+3])): j+=1
            out.append((i,x[i:j].rstrip(b'\0'))); i=j
        else: i+=1
    return out
def cstr(sp): return '{'+', '.join('{%d, "%s"}'%(o,v.hex()) for o,v in sp)+'}'
p=0xcd; p+=4+rd(p); p+=4196; p+=4+rd(p); p+=39596; p+=400
fixed=b[p:p+39600]; p+=39600; p+=40000; count=rd(p); p+=44; start=p
found=None
def walk(SH,RX):
    q=start; recs=[]
    for k in range(count):
        s0=q; slot=rd(q+2); q2=q+10; L=rd(q2)
        if b[q]!=1 or not 0<=L<200: return None
        if L and b[q2+4+L-4:q2+4+L].lower()!=b'.bmp': return None
        q2+=4+L; L2=rd(q2)
        if not 0<=L2<200: return None
        F=q2+4+L2; E=SH+(RX if k==0 else 0); Lan=rd(F+0x1b9+E)
        if not 0<=Lan<20000: return None
        q=F+0x261+E+Lan
        recs.append((slot,F,Lan,b[F:F+0x1b5+E],b[F+0x1bd+E+Lan:q]))
    if recs[0][0]!=-999999 or not all(-1<r[0]<100000 for r in recs[1:]): return None
    return recs,q
for SH in range(-160,129):
    for RX in (0,)+tuple(range(-80,81)):
        try: got=walk(SH,RX)
        except struct.error: got=None
        if got: found=(SH,RX); recs,q=got; break
    if found: break
if not found: raise SystemExit('no record size')
SH,RX=found
print('// SH',SH,RX,'after records at',q,'count',count,[r[0] for r in recs],file=sys.stderr)
a=q
def entry(p,trail):
    p+=4
    for _ in range(3):
        L=rd(p)
        if not 0<=L<4000: return None
        p+=4+L
    return p+trail
hit=None
for trail in (16,12,20):
  for gap in (0x18,):
    for e in range(a,a+3000):
        if b[e] in(0,1) and b[e+1] in(0,1):
            try:
                x=entry(e+2,trail)
                if not x or rd(x+gap)!=60: continue
                cap=x+gap; n=rd(cap+4); pp=cap+8
                if not 0<=n<=60: continue
                for _ in range(n):
                    pp=entry(pp,trail)
                    if pp is None: raise ValueError
                pp+=1; c=0
                while c<400:
                    if b[pp] not in (0,1): break
                    L=rd(pp+1)
                    if not 0<=L<200: break
                    # The trailer and the scene's name come after the last record.
                    L2=rd(pp+16)
                    if 8<L2<80 and b[pp+20:pp+24] in (b'scr\\',b'gt\\s') : hit=(trail,e,c,pp);break
                    pp+=1+4+L+18; c+=1
                if hit: break
            except (ValueError,struct.error): pass
    if hit: break
  if hit: break
if not hit: raise SystemExit('no text layout')
trail,e,chans,p=hit
after=b[a:e]
p+=16; L=rd(p); scene=b[p+4:p+4+L]; p+=4+L; tail=bytearray(b[p:])
print('// channels',chans,file=sys.stderr)
la=rd(843+0) if False else struct.unpack_from('<i',tail,839)[0]; lb=struct.unpack_from('<i',tail,843+la)[0]
tail=tail[:839]+bytes(8)+tail[847+la+lb:]
tail[0:4]=bytes(4); tail[823:839]=bytes(16); tail[819:823]=struct.pack('<i',-1)
print('// sizes: record',SH,'text',trail,'after',len(after),'tail',len(tail),'count',count,scene,file=sys.stderr)
sh=[r for r in recs if r[0]!=-999999]
t=bytearray(sh[0][4]); t[:0x74]=bytes(0x74); t[0xa0:0xa4]=bytes(4)
print('const SaveLayout k%s = {'%name)
print("    %d,  // the extra bytes of this engine's sprite record over the oldest layout seen"%SH)
print('    %s,'%cstr(sparse(fixed[8:])))
print('    %s,'%cstr(sparse(recs[0][3])))
print('    %s,'%cstr(sparse(sh[0][3])))
print('    %s,'%cstr(sparse(bytes(t))))
print('    %d, -1, %s,'%(len(after),cstr(sparse(after))))
print('    %d, %s,'%(len(tail),cstr(sparse(bytes(tail)))))
print('    %d,  // the bytes of a text record after its strings'%trail)
print('};')
