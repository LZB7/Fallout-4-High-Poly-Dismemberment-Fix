"""Fetch pinned SDK source directly from upstream into ignored local storage."""
import hashlib,json,urllib.request
from pathlib import Path
root=Path(__file__).resolve().parents[1]
d=json.loads((root/'dependencies.json').read_text())
for f in d['sdk_files']:
 target=root/'vendor/f4se'/f['path']
 if target.exists() and hashlib.sha256(target.read_bytes()).hexdigest()==f['sha256']:continue
 url='https://raw.githubusercontent.com/ianpatt/f4se/'+d['sdk_commit']+'/'+f['path']
 raw=urllib.request.urlopen(url,timeout=30).read()
 assert hashlib.sha256(raw).hexdigest()==f['sha256'],f['path']
 target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(raw)
print('Pinned SDK source verified. Do not commit vendor/f4se.')
