"""Offline positive and tamper tests; never installs a driver or trusts a cert."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = root / "drivers/virtual/build"
script = root / "drivers/virtual/verify_package.ps1"
files = ("DpsAudioVirtual.sys", "DpsAudioVirtual.inf", "DpsAudioVirtual.cat", "DpsAudioDevelopment.cer")

def verify(path):
    return subprocess.run(["powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                           str(script), "-PackageDirectory", str(path)], capture_output=True)

assert verify(source).returncode == 0, "Original package verification failed"
with tempfile.TemporaryDirectory(prefix="dps-package-test-") as directory:
    target = Path(directory)
    for name in files:
        shutil.copy2(source / name, target / name)
    with (target / files[1]).open("ab") as f:
        f.write(b"\r\n; altered after signing\r\n")
    assert verify(target).returncode != 0, "Modified INF was accepted"
    shutil.copy2(source / files[1], target / files[1])
    image = bytearray((target / files[0]).read_bytes())
    image[0x480] ^= 1
    (target / files[0]).write_bytes(image)
    assert verify(target).returncode != 0, "Modified SYS was accepted"
print("PASS: signed package accepted; modified INF and SYS rejected without changing trust stores")
