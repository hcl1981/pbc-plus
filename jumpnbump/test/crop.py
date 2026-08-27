import zlib,struct,sys
def load(fn):
    d=open(fn,'rb').read(); pos=8; idat=b''
    while pos<len(d):
        ln,=struct.unpack('>I',d[pos:pos+4]); t=d[pos+4:pos+8]; data=d[pos+8:pos+8+ln]; pos+=12+ln
        if t==b'IHDR': w,h,bd,ct=struct.unpack('>IIBB',data[:10])
        elif t==b'IDAT': idat+=data
    raw=zlib.decompress(idat); bpp=3; stride=w*bpp
    out=bytearray(); prev=bytearray(stride); p=0
    for y in range(h):
        f=raw[p]; p+=1; line=bytearray(raw[p:p+stride]); p+=stride
        for i in range(stride):
            a=line[i-bpp] if i>=bpp else 0; b=prev[i]; c=prev[i-bpp] if i>=bpp else 0
            if f==1: line[i]=(line[i]+a)&255
            elif f==2: line[i]=(line[i]+b)&255
            elif f==3: line[i]=(line[i]+(a+b)//2)&255
            elif f==4:
                pa=abs(b-c);pb=abs(a-c);pc=abs(a+b-2*c)
                pr=a if(pa<=pb and pa<=pc) else (b if pb<=pc else c)
                line[i]=(line[i]+pr)&255
        out+=line; prev=line
    return w,h,bytes(out)
def save(fn,w,h,px):
    raw=b''.join(b'\x00'+px[y*w*3:(y+1)*w*3] for y in range(h))
    comp=zlib.compress(raw,9)
    def ch(t,d):
        return struct.pack('>I',len(d))+t+d+struct.pack('>I',zlib.crc32(t+d))
    open(fn,'wb').write(b'\x89PNG\r\n\x1a\n'+ch(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+ch(b'IDAT',comp)+ch(b'IEND',b''))
w,h,px=load(sys.argv[1])
x0,y0,cw,chh,sc=[int(v) for v in sys.argv[3:8]]
out=bytearray()
for y in range(chh*sc):
    for x in range(cw*sc):
        o=((y0+y//sc)*w+(x0+x//sc))*3
        out+=px[o:o+3]
save(sys.argv[2],cw*sc,chh*sc,bytes(out))
