"""Small read-only PE export/import parser for package verification."""
from pathlib import Path
import struct

ROOT=Path(__file__).resolve().parents[1]

class PE:
    def __init__(self,data):
        self.data=data;self.pe=struct.unpack_from('<I',data,0x3c)[0];self.opt=self.pe+24
        self.machine,self.count=struct.unpack_from('<HH',data,self.pe+4)
        size=struct.unpack_from('<H',data,self.pe+20)[0];self.sections=[]
        for i in range(self.count):
            off=self.opt+size+40*i
            virtual,rva,raw,fileoff=struct.unpack_from('<IIII',data,off+8)
            self.sections.append((rva,max(virtual,raw),fileoff))
    def offset(self,rva):
        for start,size,fileoff in self.sections:
            if start<=rva<start+size:return fileoff+rva-start
        if rva<0x400:return rva
        raise ValueError(hex(rva))
    def string(self,rva):
        off=self.offset(rva);return self.data[off:self.data.index(b'\0',off)].decode('ascii')
    def u32(self,rva):return struct.unpack_from('<I',self.data,self.offset(rva))[0]
    def directory(self,n):return struct.unpack_from('<II',self.data,self.opt+112+8*n)
    def exports(self):
        rva,_=self.directory(0);off=self.offset(rva)
        _,_,_,_,_,_,functions,names,addresses,name_table,ordinals=struct.unpack_from('<IIHHIIIIIII',self.data,off)
        assert functions==names==2
        out={}
        for i in range(names):
            name=self.string(self.u32(name_table+4*i))
            ordinal=struct.unpack_from('<H',self.data,self.offset(ordinals+2*i))[0]
            out[name]=self.u32(addresses+4*ordinal)
        return out
    def imports(self):
        rva,_=self.directory(1);out=[]
        while True:
            entry=struct.unpack_from('<IIIII',self.data,self.offset(rva))
            if not any(entry):break
            out.append(self.string(entry[3]));rva+=20
        return out

