"""python pr.py file.c -> compact per-function flow (cmp/test abbreviated, jumps with labels)"""
import sys, subprocess, re, os
f = os.path.abspath(sys.argv[1]); d = os.path.dirname(f); b = os.path.splitext(os.path.basename(f))[0]
flags = sys.argv[2:] or ['-3s', '-d2', '-s']
r = subprocess.run([sys.executable, 'D:/Prog/kegg_recon/tools/dosrun.py', 'wcc386', *flags, b + '.c'], cwd=d, capture_output=True, text=True)
for l in r.stdout.splitlines():
    if re.search(r'(Error|Warning)!', l): print(l)
out = subprocess.run(['C:/tmp/watcom/binnt64/wdis.exe', b + '.obj'], cwd=d, capture_output=True, text=True).stdout
skip = re.compile(r'^(push|pop)\s+e(bx|si|di|bp)$|^mov\s+ebp,esp$|^sub esp,0x00000000$')
res = []
for l in out.splitlines():
    if l.startswith('Segment:') and '_TEXT' not in l: break
    m = re.match(r'^[0-9A-F]{4}  [0-9A-F ]+\t+(.*)$', l)
    if m:
        ins = re.sub(r'\s+', ' ', m.group(1)).strip()
        ins = re.sub(r'dword ptr ', '', ins); ins = re.sub(r'0x0*([0-9A-F])', r'\1', ins)
        if not skip.match(ins): res.append('  ' + ins)
        continue
    m = re.match(r'^[0-9A-F]{4}\s+(\S+):', l)
    if m: res.append(m.group(1) + ':')
print('\n'.join(res))
