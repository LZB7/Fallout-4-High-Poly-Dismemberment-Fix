"""Stage one selected variant; never publish or install into a game."""
import argparse,hashlib,json,shutil,zipfile
from pathlib import Path
from verify_dll import verify

ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser();p.add_argument('--destination',type=Path,required=True);p.add_argument('--notices',type=Path,required=True);p.add_argument('--experimental',action='store_true');p.add_argument('--refresh',action='store_true');a=p.parse_args()
    variant='experimental' if a.experimental else 'primary'
    version='1.0.1-experimental.1' if a.experimental else '1.0.0'
    label=f'High-Poly Dismemberment Fix - {version} - '+('AE 1.11.221 Experimental and 1.11.240' if a.experimental else 'AE 1.11.240')
    stage=a.destination/variant
    assert not stage.exists() or a.refresh, 'Use --refresh explicitly to update a staged package.'
    plugins=stage/'Data/F4SE/Plugins';plugins.mkdir(parents=True,exist_ok=a.refresh)
    dll=ROOT/'build'/variant/'HighPolyDismembermentFix.dll'
    result=verify(dll,a.experimental)
    shutil.copy2(dll,plugins/dll.name);shutil.copy2(ROOT/'config/HighPolyDismembermentFix.ini',plugins)
    shutil.copy2(ROOT/'docs'/('Nexus-Experimental.txt' if a.experimental else 'Nexus-Primary.txt'),stage/'README.txt')
    shutil.copy2(ROOT/'THIRD_PARTY_NOTICES.txt',stage)
    dest=stage/'ThirdPartyNotices';dest.mkdir(exist_ok=a.refresh)
    for f in a.notices.iterdir():
        if f.is_file():shutil.copy2(f,dest/f.name)
    assert all(f.suffix.lower() not in ['.nif','.ssf','.dxbc','.ba2','.exe','.pdb','.bin'] for f in stage.rglob('*') if f.is_file())
    allowed={'Data/F4SE/Plugins/HighPolyDismembermentFix.dll','Data/F4SE/Plugins/HighPolyDismembermentFix.ini','README.txt','THIRD_PARTY_NOTICES.txt','SHA256SUMS.txt'}|{'ThirdPartyNotices/'+f.name for f in a.notices.iterdir() if f.is_file()}
    assert all(f.relative_to(stage).as_posix() in allowed for f in stage.rglob('*') if f.is_file())
    files={str(f.relative_to(stage)).replace('\\','/'):hashlib.sha256(f.read_bytes()).hexdigest() for f in stage.rglob('*') if f.is_file() and f.name!='SHA256SUMS.txt'}
    (stage/'SHA256SUMS.txt').write_text(''.join(h+'  '+name+'\n' for name,h in sorted(files.items())))
    archive=a.destination/(label+'.zip')
    assert not archive.exists() or a.refresh
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,9) as z:
        for f in sorted(stage.rglob('*')):
            if f.is_file():z.write(f,str(f.relative_to(stage)))
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        assert 'Data/F4SE/Plugins/HighPolyDismembermentFix.dll' in z.namelist()
    result.update(archive=archive.name,archive_sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),files=files)
    (ROOT/'validation'/f'{variant}-package.json').write_text(json.dumps(result,indent=2)+'\n')
    print('Verified',archive)
if __name__=='__main__':main()
