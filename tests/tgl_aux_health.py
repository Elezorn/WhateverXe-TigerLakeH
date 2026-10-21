#!/usr/bin/env python3
"""Replay production AUX readiness and command failure propagation."""
from pathlib import Path
import re,subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'WhateverXe/kern_xe.cpp').read_text()
body='\n'.join(re.search(pattern,s,re.S).group() for pattern in [r'bool XE::tglDPAUXRequired\(.*?\n\}',r'bool XE::tglDPAUXReady\(.*?\n\}',r'uint32_t XE::wrapTGLRunAUX\(.*?\n\}'])
f=(r/'tests/tgl_aux_health_fixture.cpp').read_text().replace('// PRODUCTION',body)
with tempfile.TemporaryDirectory(prefix='tgl-aux-health-') as d:
 d=Path(d);(d/'check.cpp').write_text(f)
 subprocess.run(['clang++','-std=c++14','-fsanitize=address,undefined','-I',str(r/'WhateverXe'),str(d/'check.cpp'),'-o',str(d/'check')],check=True)
 subprocess.run([str(d/'check')],check=True)
