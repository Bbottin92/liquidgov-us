#!/usr/bin/env python3
"""Static restart-policy regression cases: no shell payload is ever executed.

A literal-command checker, not a general shell interpreter. Variables, aliases,
functions, generated commands, and scripts outside this repository are out of scope.
"""
import importlib.util
from pathlib import Path
import sys
import tempfile

spec = importlib.util.spec_from_file_location("naming_check", Path(__file__).with_name("naming_check.py"))
naming = importlib.util.module_from_spec(spec)
spec.loader.exec_module(naming)

POSITIVE = [
    'systemctl restart liquidgov',
    'sudo systemctl restart liquidgov',
    'if true; then systemctl restart liquidgov; fi',
    'systemctl restart liquidgov.service',
    'systemctl stop liquidgov; systemctl start liquidgov',
    'systemctl stop liquidgov.service\nsudo systemctl start liquidgov',
    'ssh root@host "systemctl restart liquidgov"',
    "ssh host 'sudo systemctl restart liquidgov'",
    'systemctl try-restart liquidgov',
    'systemctl reload-or-restart liquidgov',
    'systemctl restart --no-block liquidgov',
    'systemctl --no-block restart liquidgov',
    'service liquidgov restart',
    'sudo service liquidgov restart',
    'true && sudo systemctl restart liquidgov',
    'false || systemctl restart liquidgov',
    'for x in one; do systemctl restart liquidgov; done',
    'env MODE=check sudo -u root systemctl restart liquidgov',
    'systemctl restart other.service liquidgov.service third.service',
    'sudo systemctl restart \\\n  liquidgov.service',
    'ssh -p 22 host "env MODE=check sudo systemctl reload-or-restart liquidgov.service"',
    'systemctl restart liquidgov; systemctl restart liquidgov.service',
]
POSITIVE += [
    'if true; then echo up; else systemctl restart liquidgov; fi',
    '{ systemctl restart liquidgov; }',
    'true && { sudo systemctl restart liquidgov; }',
    'systemctl try-reload-or-restart liquidgov',
    "env -S 'systemctl restart liquidgov'",
    "env --split-string 'sudo systemctl restart liquidgov.service'",
    'ssh host "systemctl stop liquidgov"\nssh host "systemctl start liquidgov"',
    'sh -c "systemctl stop liquidgov"\nsh -c "systemctl start liquidgov"',
    'systemctl stop liquidgov\nssh host "systemctl start liquidgov"',
    'bash -lc "systemctl restart liquidgov"',
    'bash -l -c "systemctl restart liquidgov"',
    'ssh.exe host "systemctl restart liquidgov"',
    'systemctl restart --job-mode replace liquidgov',
    'sudo -S systemctl restart liquidgov',
]
NEGATIVE = [
    '# sudo systemctl restart liquidgov',
    'echo "systemctl restart liquidgov"',
    "echo 'sudo systemctl restart liquidgov'",
    'echo "ACTIVE=$(systemctl is-active liquidgov)"',
    'systemctl show --property=ActiveState liquidgov',
    'systemctl status liquidgov',
    'systemctl is-active liquidgov',
    'systemctl reload liquidgov',
    'systemctl stop liquidgov',
    'systemctl start liquidgov',
    'true # systemctl restart liquidgov',
    'printf "%s\\n" "systemctl restart liquidgov"',
    'ssh host "systemctl status liquidgov"',
    'echo ssh host "systemctl restart liquidgov"',
    'service liquidgov status',
    'systemctl start liquidgov\nsystemctl stop liquidgov',
    'ssh host "systemctl start liquidgov"\nssh host "systemctl stop liquidgov"',
]

def main():
    failed = 0
    total = 0
    def check(ok, label):
        nonlocal failed, total
        total += 1
        failed += not ok
        print(('PASS ' if ok else 'FAIL ') + label)
    with tempfile.TemporaryDirectory() as td:
        p = Path(td)/'probe.sh'
        for line in POSITIVE:
            p.write_text(line+'\n', encoding='utf-8')
            units = naming.restart_units(str(p))
            expected = ['liquidgov', 'other', 'third'] if line == 'systemctl restart other.service liquidgov.service third.service' else ['liquidgov']
            check(units == expected, repr(line)+' -> '+repr(units))
        for line in NEGATIVE:
            p.write_text(line+'\n', encoding='utf-8')
            check(not naming.restart_units(str(p)), 'negative '+repr(line))
        p.write_text('systemctl restart one.service two.service liquidgov.service\n', encoding='utf-8')
        check(set(naming.restart_units(str(p))) == {'one','two','liquidgov'}, 'all units normalized')
        p.write_text('systemctl restart liquidgov; systemctl restart liquidgov.service\n', encoding='utf-8')
        check(naming.restart_units(str(p)) == ['liquidgov'], 'one path per unit, not one entry per command')
        # Scan a full repository-shaped fixture using the real naming.toml; do not
        # substitute a tiny fake policy for the production scan.
        root = Path(td)/'tree'
        real = Path(__file__).resolve().parents[2]
        cfg = naming.load(str(real))
        for section in ('e2e','cpp','ci_sh','ci_py','deploy'):
            base = root/cfg[section]['root']
            base.mkdir(parents=True, exist_ok=True)
            for name in cfg.get('grandfather',{}).get(section,[]):
                content = 'PORT=18080\n' if section == 'e2e' else ''
                (base/name).write_text(content, encoding='utf-8')
        # Give each grandfathered e2e its own valid port.
        for i,p in enumerate(sorted((root/cfg['e2e']['root']).glob('*.sh'))):
            p.write_text('PORT=%d\n' % (18080+i), encoding='utf-8')
        for rel in cfg['deploy']['restart_allowed']:
            (root/rel).write_text('systemctl restart liquidgov\n', encoding='utf-8')
        server = root/'liquidgov-platform/src/http_server.cpp'
        server.parent.mkdir(parents=True, exist_ok=True)
        server.write_text(cfg['frozen']['button']+'\n'+'\n'.join(cfg['frozen']['assets']),encoding='utf-8')
        tags = root/cfg['frozen']['store_tags_check']
        tags.parent.mkdir(parents=True, exist_ok=True)
        tags.touch()
        hook = root/'.githooks/pre-push'
        hook.parent.mkdir(parents=True, exist_ok=True)
        hook.write_text('lgp1_consistency.py naming_check.py\n', encoding='utf-8')
        clean = naming.scan(str(root), cfg, ['master'])
        check(clean.failed == 0, 'production-policy fixture clean: '+repr(clean.lines))
        (root/'deploy/extra-remote.sh').write_text('sudo systemctl restart liquidgov\n',encoding='utf-8')
        bad = naming.scan(str(root), cfg, ['master'])
        check(any('restart scripts changed' in line for line in bad.lines),
              'tree scan rejects additional sudo restart script: '+repr(bad.lines))
    print('restart_bypass_check: %d checks, %d failed' % (total, failed))
    return int(bool(failed))

if __name__ == '__main__':
    sys.exit(main())
