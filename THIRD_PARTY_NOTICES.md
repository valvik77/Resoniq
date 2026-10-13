# Third-party components

The DSP (`include/dps/engine.hpp`, `src/dsp/engine.cpp`) is original project code under MIT. It does not use Bongiovi code.

The virtual driver build reuses Microsoft SysVAD infrastructure from
https://github.com/microsoft/Windows-driver-samples at revision
`2dc3fd3a0cc84a2933f2194e7ec0871584979071`.
Upstream sources retain their copyright notices and are covered by the Microsoft
Public License (MS-PL), not the project's MIT license. `build_driver.py` downloads
the pinned source, copies the infrastructure into an ignored build staging area,
and packages the upstream license as `drivers/virtual/build/MS-PL.txt`.

The driver's endpoint tables and INF are project additions. The build removes
unused sideband branches and the registry override for demo audio recording.
The original sample APOs and keyword detector adapter are not built or packaged.
The upstream sample's simulated hardware loopback pin is not exposed.

Microsoft SDK/WDK and MSVC are build tools subject to their respective license
terms. Their NuGet packages are not part of the runtime redistribution. Development
private keys are local build files and must not be distributed. The explicitly
labelled development MSI includes only the public development certificate,
whose import is gated on an already configured test machine; production
packages omit it.
