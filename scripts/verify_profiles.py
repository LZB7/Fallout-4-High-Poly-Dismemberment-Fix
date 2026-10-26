"""Verify committed runtime contracts against privately supplied clean EXEs."""
import argparse,hashlib,re,struct
from pathlib import Path
from pe import PE
ROOT=Path(__file__).resolve().parents[1]
EXPECTED={'240':'fdcef37ac1230af6d0b0050eb2142b139ef3a867b37b9211fb6edfcc646072f8','221':'428f9996cc4248e26c0f62f9fdd3eaf0e5eb305834b67ee5996538e593218b61'}
def fnv(data):
    h=14695981039346656037
    for b in data:h=((h^b)*1099511628211)&0xffffffffffffffff
    return h
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--reference240',type=Path,required=True);ap.add_argument('--reference221',type=Path,required=True);a=ap.parse_args();text=(ROOT/'src/RuntimeProfiles.h').read_text()
    for v,path in [('240',a.reference240),('221',a.reference221)]:
        raw=path.read_bytes();assert hashlib.sha256(raw).hexdigest()==EXPECTED[v]
        pe=PE(raw);block=re.search(r'contracts'+v+r'\[\]=\{(.*?)\n\};',text,re.S)[1]
        spans=re.findall(r'\{"[^"]+",(0x[0-9a-f]+),(0x[0-9a-f]+),(0x[0-9a-f]+)ull\}',block);assert len(spans)==26
        for addr,size,digest in spans:
            r,n,h=int(addr,16),int(size,16),int(digest,16);off=pe.offset(r);assert fnv(raw[off:off+n])==h
        print('PASS runtime',v,': 26 exact executable contracts and reference SHA-256')
if __name__=='__main__':main()
