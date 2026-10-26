"""Check release PE metadata/imports and exercise only rejected Load calls."""
import argparse,ctypes,json,hashlib,struct
from pathlib import Path
from pe import PE

def verify(path, experimental=False):
    data=path.read_bytes();pe=PE(data);assert pe.machine==0x8664
    exports=pe.exports();assert set(exports)=={'F4SEPlugin_Load','F4SEPlugin_Version'}
    m=pe.offset(exports['F4SEPlugin_Version'])
    assert struct.unpack_from('<II',data,m)==(1,0x01000010 if experimental else 0x01000000)
    assert data[m+8:m+264].split(b'\0')[0]==b'HighPolyDismembermentFix'
    assert struct.unpack_from('<II',data,m+0x208)==(0,4)
    expected=((0x010b0dd0,0x010b0f00)+(0,)*14) if experimental else ((0x010b0f00,)+(0,)*15)
    assert struct.unpack_from('<16I',data,m+0x210)==expected
    assert struct.unpack_from('<I',data,m+0x250)[0]==(0x70080 if experimental else 0x70090)
    assert not any(data[m+0x254:m+0x45c])
    imports=pe.imports()
    assert all(n.lower() in ['kernel32.dll','d3d11.dll','d3dcompiler_47.dll','bcrypt.dll'] or n.lower().startswith('api-ms-win-crt-') for n in imports),imports
    lib=ctypes.CDLL(str(path.resolve()))
    class Interface(ctypes.Structure):
        _fields_=[('f4se',ctypes.c_uint32),('runtime',ctypes.c_uint32),('editor',ctypes.c_uint32),('isEditor',ctypes.c_uint32),('query',ctypes.c_void_p),('handle',ctypes.c_void_p)]
    load=lib.F4SEPlugin_Load;load.argtypes=[ctypes.POINTER(Interface)];load.restype=ctypes.c_bool
    assert not load(None)
    # Callback placeholders must never be reached on an unsupported version.
    for runtime in [0,0x010a0a30,0x010b0f10]+([] if experimental else [0x010b0dd0]):
        assert not load(ctypes.byref(Interface(0x70090,runtime,0,0,1,1)))
    for runtime,se in [(0x010b0f00,0x70080),(0x010b0dd0,0x70070)]:
        assert not load(ctypes.byref(Interface(se,runtime,0,0,1,1)))
    assert not load(ctypes.byref(Interface(0x70090,0x010b0f00,0,1,1,1)))
    return dict(file=path.name,sha256=hashlib.sha256(data).hexdigest(),bytes=len(data),experimental=experimental,imports=imports,exports=list(exports),metadata_verified=True,rejection_paths_verified=True)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('dll',type=Path);p.add_argument('--experimental',action='store_true');p.add_argument('--output',type=Path);a=p.parse_args();r=verify(a.dll,a.experimental)
    if a.output:a.output.write_text(json.dumps(r,indent=2)+'\n')
    print(json.dumps(r,indent=2))
