#!/usr/bin/env python3
"""Replay the original and wait-only patched exact donor; no hardware timing claim."""
from pathlib import Path
import hashlib, subprocess, sys, tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'WhateverXe/kern_xe.cpp').read_text()
install=s[s.index('bool XE::processDisplay13FramebufferSST('):s.index('uint8_t *XE::display13SSTPort(')]
assert '!display13FramebufferRequested' in install
assert 'waitKnown(wait,sizeof(WaitBefore))' in install
assert 'Display13PortWait::patch(after,before)' in install
assert '!memcmp(wait,after,sizeof(after))' in install
assert '{enable,wrapDisplay13EnablePort,orgDisplay13EnablePort}' in install
print('PASS wait patch guarded inside existing Display13 installer; native wrappers retained')
if len(sys.argv)>1:
    donor=Path(sys.argv[1]).resolve()
    assert hashlib.sha256(donor.read_bytes()).hexdigest()=='c004fc1ac9ee4157e2e12736f40fa6639bde025a279326beeb9dd48c9b445731'
    with tempfile.TemporaryDirectory(prefix='d13-port-wait-') as d:
        binary=Path(d)/'replay'
        subprocess.run(['clang++','-arch','x86_64','-std=c++17','-O1','-g','-I'+str(r/'WhateverXe'),str(r/'tests/display13_port_wait_fixture.cpp'),'-o',str(binary)],check=True)
        subprocess.run([str(binary),str(donor)],check=True)
