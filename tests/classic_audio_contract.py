"""Guard the archived audio implementation while the UI evolves."""
from pathlib import Path
import hashlib
import json
import re

root = Path(__file__).resolve().parents[1]
baseline = json.loads((root / 'tests/fixtures/classic_audio_baseline.json').read_text(encoding='utf-8'))

def digest(text):
    return hashlib.sha256(text.encode('utf-8')).hexdigest().upper()

for name, expected in baseline['files'].items():
    actual = digest((root / name).read_text(encoding='utf-8-sig'))
    assert actual == expected, f'Archived audio changed: {name}'

def section(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    masked = re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/',
                    lambda match: ' ' * len(match[0]), text)
    depth = 0
    for pos in range(opening, len(text)):
        if masked[pos] == '{': depth += 1
        if masked[pos] == '}': depth -= 1
        if depth == 0: return text[start:pos + 1]
    raise AssertionError(signature)

for name, signatures in baseline['sections'].items():
    actual = (root / name).read_text(encoding='utf-8-sig')
    for signature, expected in signatures.items():
        assert digest(section(actual, signature)) == expected, f'Audio path changed: {signature}'

gui = (root / 'apps/gui_main.cpp').read_text(encoding='utf-8-sig')
assert 'applyApprovedProfile' not in gui, 'UI silently adopts a different sound profile'
assert 'setComparison(' not in gui, 'UI introduces level matching'
print('PASS: archived DSP, buffer, stream initialization, live path and profile semantics unchanged')
