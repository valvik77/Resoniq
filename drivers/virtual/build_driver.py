"""Reproducible x64 development build. Does not install or sign the driver."""
from pathlib import Path
import argparse
import json
import re
import shutil
import subprocess
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
KIT = "10.0.28000.2526"
HEADERS = "10.0.28000.0"
COMMIT = "2dc3fd3a0cc84a2933f2194e7ec0871584979071"

def run(args, **kwargs):
    subprocess.run([str(a) for a in args], check=True, **kwargs)

def acquire():
    nuget = ROOT / "tools/nuget.exe"
    nuget.parent.mkdir(parents=True, exist_ok=True)
    if not nuget.exists():
        urllib.request.urlretrieve("https://dist.nuget.org/win-x86-commandline/v6.14.0/nuget.exe", nuget)
    run([nuget, "restore", HERE / "packages.config", "-PackagesDirectory", HERE / "packages",
         "-Source", "https://api.nuget.org/v3/index.json", "-NonInteractive"])
    repo = ROOT / "third_party/windows-driver-samples"
    if not repo.exists():
        run(["git", "clone", "--filter=blob:none", "--sparse",
             "https://github.com/microsoft/Windows-driver-samples.git", repo])
    run(["git", "-C", repo, "sparse-checkout", "set", "audio/sysvad"])
    run(["git", "-C", repo, "fetch", "--depth", "1", "origin", COMMIT])
    run(["git", "-C", repo, "checkout", "--detach", COMMIT])
    return repo

def prepare(repo):
    vendor = HERE / "vendor/sysvad"
    source = repo / "audio/sysvad"
    # Copy only sources/resources; do not import sample APO packages or install scripts.
    for f in source.rglob("*"):
        relative = f.relative_to(source)
        if f.is_file() and relative.parts[0] not in ("APO", "KeywordDetectorAdapter") and f.suffix.lower() in (".cpp", ".h", ".rc", ".ico"):
            dst = vendor / relative
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(f, dst)
    for f in (HERE / "overlay").glob("*.h"):
        shutil.copy2(f, vendor / "TabletAudioSample" / f.name)
    shutil.copy2(repo / "LICENSE", HERE / "build" / "MS-PL.txt")
    # Samples include dormant disk recording code; force it off regardless of registry.
    adapter = vendor / "adapter.cpp"
    text = adapter.read_text(encoding="utf-8-sig")
    query = '{ NULL,   RTL_QUERY_REGISTRY_DIRECT | RTL_QUERY_REGISTRY_TYPECHECK, L"DoNotCreateDataFiles", &g_DoNotCreateDataFiles, (REG_DWORD << RTL_QUERY_REGISTRY_TYPECHECK_SHIFT) | REG_DWORD, &g_DoNotCreateDataFiles, sizeof(ULONG)},'
    if query not in text:
        raise RuntimeError("Pinned upstream changed: disk recording query not found")
    adapter.write_text(text.replace(query, "// DPS: disk recording remains disabled; no registry override."), encoding="utf-8")
    # Upstream engine-node helpers assume sideband support is compiled in. Remove
    # only those optional branches; this endpoint has no sideband device.
    engine = vendor / "EndpointsCommon/MiniportAudioEngineNode.cpp"
    text = engine.read_text(encoding="utf-8-sig")
    pattern = r"if \(IsSidebandDevice\(\) && m_pSidebandDevice->Is(?:Volume|Mute)Supported\(m_DeviceType\)\)"
    removed = 0
    while match := re.search(pattern, text):
        begin = text.index("{", match.end())
        depth, end = 1, begin + 1
        while depth:
            if text[end] == "{": depth += 1
            elif text[end] == "}": depth -= 1
            end += 1
        tail = re.match(r"\s*else\s*", text[end:])
        if not tail and removed != 7: raise RuntimeError("Unexpected sideband branch structure")
        text = text[:match.start()] + "// DPS: non-sideband path only.\n" + text[end + (tail.end() if tail else 0):]
        removed += 1
    if removed != 8: raise RuntimeError(f"Unexpected sideband branch count: {removed}")
    engine.write_text(text, encoding="utf-8")
    return vendor

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--no-restore", action="store_true", help="Use already downloaded kits and pinned source")
    args = parser.parse_args()
    out = HERE / "build"
    out.mkdir(exist_ok=True)
    repo = ROOT / "third_party/windows-driver-samples" if args.no_restore else acquire()
    actual = subprocess.check_output(["git", "-C", str(repo), "rev-parse", "HEAD"], text=True).strip()
    if actual != COMMIT:
        raise RuntimeError("Unexpected SysVAD source revision")
    source = prepare(repo)
    vswhere = Path("C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe")
    vs = Path(subprocess.check_output([str(vswhere), "-latest", "-products", "*", "-requires",
         "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], text=True).strip())
    version = (vs / "VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt").read_text().strip()
    msvc = vs / "VC/Tools/MSVC" / version
    tools = msvc / "bin/Hostx64/x64"
    wdk = HERE / "packages" / f"Microsoft.Windows.WDK.x64.{KIT}" / "c"
    sdk = HERE / "packages" / f"Microsoft.Windows.SDK.CPP.{KIT}" / "c"
    includes = [source / "TabletAudioSample", source / "EndpointsCommon", source,
                wdk / "Include" / HEADERS / "km/crt", wdk / "Include" / HEADERS / "km", wdk / "Include/wdf/kmdf/1.15",
                sdk / "Include" / HEADERS / "shared", sdk / "Include" / HEADERS / "um",
                msvc / "include"]
    files = [source / name for name in ("adapter.cpp", "basetopo.cpp", "common.cpp", "hw.cpp",
            "kshelper.cpp", "savedata.cpp", "tonegenerator.cpp")]
    files += [source / "EndpointsCommon" / name for name in ("minwavert.cpp", "minwavertstream.cpp",
              "mintopo.cpp", "speakertopo.cpp", "NewDelete.cpp", "AudioModuleHelper.cpp",
              "MiniportAudioEngineNode.cpp", "MiniportStreamAudioEngineNode.cpp")]
    objects = []
    for f in files:
        obj = out / (f.stem + ".obj")
        command = [tools / "cl.exe", "/nologo", "/c", "/kernel", "/O2", "/Z7", "/W4", "/WX", "/wd4324", "/wd4595", "/GS",
                   "/Zl", "/GR-", "/EHs-c-", "/std:c++17", "/Qspectre", "/fp:precise",
                   "/D_AMD64_", "/DAMD64", "/D_WIN64", "/D_WIN32", "/DUNICODE", "/D_UNICODE",
                   "/DPC_IMPLEMENTATION", "/D_USE_WAVERT_", "/D_USE_IPortClsRuntimePower",
                   "/D_NEW_DELETE_OPERATORS_", "/DNTDDI_VERSION=0x0A00000C", "/D_WIN32_WINNT=0x0A00",
                   "/DKMDF_VERSION_MAJOR=1", "/DKMDF_VERSION_MINOR=15", "/DDEBUG_LEVEL=0",
                   *[f"/I{p}" for p in includes], f"/Fo{obj}", f]
        result = subprocess.run([str(a) for a in command], capture_output=True)
        (out / (f.stem + ".log")).write_bytes(result.stdout + result.stderr)
        print(f"Compile {f.name}: {'OK' if result.returncode == 0 else 'FAILED'}", flush=True)
        if result.returncode:
            print((result.stdout + result.stderr).decode(errors="replace"))
            raise RuntimeError(f"Compilation failed: {f.name}")
        objects.append(obj)
    resource = out / "DpsAudioVirtual.res"
    run([sdk / "bin" / HEADERS / "x64/rc.exe", "/nologo", "/D_WIN64", "/D_AMD64_",
         *[f"/I{p}" for p in includes], f"/fo{resource}", HERE / "DpsAudioVirtual.rc"])
    libraries = wdk / "Lib" / HEADERS / "km/x64"
    wdf = wdk / "Lib/wdf/kmdf/x64/1.15"
    run([tools / "link.exe", "/nologo", "/kernel", "/DRIVER", "/SUBSYSTEM:NATIVE,10.00", "/ENTRY:FxDriverEntry",
         "/MACHINE:X64", "/NODEFAULTLIB", "/MANIFEST:NO", "/DYNAMICBASE", "/NXCOMPAT", "/INTEGRITYCHECK",
         "/OPT:REF", "/OPT:ICF", "/DEBUG", f"/OUT:{out / 'DpsAudioVirtual.sys'}",
         *objects, resource, *[libraries / name for name in ("ntoskrnl.lib", "hal.lib", "wmilib.lib",
         "portcls.lib", "stdunk.lib", "libcntpr.lib", "BufferOverflowFastFailK.lib")],
         wdf / "WdfDriverEntry.lib", wdf / "WdfLdr.lib"])
    shutil.copy2(HERE / "DpsAudioVirtual.inf", out)
    run([wdk / "tools" / HEADERS / "x64/infverif.exe", "/w", out / "DpsAudioVirtual.inf"])
    run([wdk / "bin" / HEADERS / "x86/Inf2Cat.exe", f"/driver:{out}", "/os:10_NI_X64,10_GE_X64"])
    (out / "build-info.json").write_text(json.dumps({"sysvad_commit": COMMIT, "kit": KIT,
        "msvc": version, "architecture": "x64", "signed": False, "installed": False,
        "infverif": "passed /w", "inf2cat": "passed 10_NI_X64,10_GE_X64"}, indent=2))
    print("Built development driver. Unsigned; not installed:", out / "DpsAudioVirtual.sys")

if __name__ == "__main__":
    main()
