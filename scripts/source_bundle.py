"""Create an allowlisted source archive without local SDKs/private fixtures."""
import argparse,hashlib,json,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser();p.add_argument('output',type=Path);a=p.parse_args()
    roots=['src','scripts','tests','docs','config','vendor/dxbc-checksum']
    files=[ROOT/n for n in ['LICENSE','README.md','Build.ps1','CONTRIBUTING.md','SUPPORT.md','THIRD_PARTY_NOTICES.txt','dependencies.json','.gitignore']]
    for d in roots:
        files.extend(f for f in (ROOT/d).rglob('*') if f.is_file() and '__pycache__' not in f.parts)
    files=sorted(set(files));assert len(files)>15
    for f in files:
        assert f.suffix.lower() not in ['.dll','.exe','.pdb','.ba2','.dxbc','.bin','.nif','.ssf','.pyc']
        raw=f.read_bytes()
        for separator in (47,92):
            personal_prefix=bytes([67,58,separator])+b'Users'+bytes([separator])
            assert personal_prefix not in raw, f.name
    with zipfile.ZipFile(a.output,'w',zipfile.ZIP_DEFLATED,9) as z:
        for f in files:z.write(f,f.relative_to(ROOT).as_posix())
    with zipfile.ZipFile(a.output) as z:
        assert z.testzip() is None
        assert not any(n.startswith(('vendor/f4se/','validation/','build/')) for n in z.namelist())
    result=dict(archive=a.output.name,sha256=hashlib.sha256(a.output.read_bytes()).hexdigest(),files=[f.relative_to(ROOT).as_posix() for f in files],license='MIT')
    (ROOT/'validation/source-bundle.json').write_text(json.dumps(result,indent=2)+'\n')
    print('Verified source-only archive:',a.output,';',len(files),'files')
if __name__=='__main__':main()
