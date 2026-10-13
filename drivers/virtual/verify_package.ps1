# Offline verification of CMS signatures, PE image digest and catalog membership.
# CheckSignature(true) checks cryptography without importing/trusting the certificate.
[CmdletBinding()]
param([string]$PackageDirectory = (Join-Path $PSScriptRoot 'build'))
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Security
$taskSys = [IO.File]::ReadAllBytes((Join-Path $PackageDirectory 'DpsAudioVirtual.sys'))
$taskPe = [BitConverter]::ToInt32($taskSys,60)
if ([BitConverter]::ToUInt32($taskSys,$taskPe) -ne 0x4550 -or [BitConverter]::ToUInt16($taskSys,$taskPe+4) -ne 0x8664) { throw 'Expected x64 PE image.' }
$taskOptional = $taskPe+24
if ([BitConverter]::ToUInt16($taskSys,$taskOptional) -ne 0x20b) { throw 'Expected PE32+.' }
$taskChecksum = $taskOptional+64
$taskSecurity = $taskOptional+112+8*4
$taskCertificateOffset = [BitConverter]::ToInt32($taskSys,$taskSecurity)
$taskCertificateSize = [BitConverter]::ToInt32($taskSys,$taskSecurity+4)
if ($taskCertificateOffset -le 0 -or $taskCertificateSize -le 8 -or $taskCertificateOffset+$taskCertificateSize -gt $taskSys.Length) { throw 'Missing/invalid embedded signature.' }
$taskCmsSize = [BitConverter]::ToInt32($taskSys,$taskCertificateOffset)
$taskCms = [Security.Cryptography.Pkcs.SignedCms]::new()
$taskCms.Decode($taskSys[($taskCertificateOffset+8)..($taskCertificateOffset+$taskCmsSize-1)])
$taskCms.CheckSignature($true)
$taskCertificate = [Security.Cryptography.X509Certificates.X509Certificate2]::new((Join-Path $PackageDirectory 'DpsAudioDevelopment.cer'))
if ($taskCms.SignerInfos.Count -ne 1 -or $taskCms.SignerInfos[0].Certificate.Thumbprint -ne $taskCertificate.Thumbprint) { throw 'Unexpected SYS signer.' }
$taskHeaderSize = [BitConverter]::ToInt32($taskSys,$taskOptional+60)
$taskHasher = [Security.Cryptography.SHA256]::Create()
function Add-DpsHashPart([int]$offset,[int]$count) {
    if ($offset -lt 0 -or $count -lt 0 -or $offset+$count -gt $taskSys.Length) { throw 'Invalid PE hashing region.' }
    if ($count) { [void]$taskHasher.TransformBlock($taskSys,$offset,$count,$taskSys,$offset) }
}
try {
    Add-DpsHashPart 0 $taskChecksum
    Add-DpsHashPart ($taskChecksum+4) ($taskSecurity-$taskChecksum-4)
    Add-DpsHashPart ($taskSecurity+8) ($taskHeaderSize-$taskSecurity-8)
    $taskSectionsStart = $taskOptional+[BitConverter]::ToUInt16($taskSys,$taskPe+20)
    $taskSections = for ($i=0; $i -lt [BitConverter]::ToUInt16($taskSys,$taskPe+6); $i++) {
        $taskSection = $taskSectionsStart+40*$i
        [pscustomobject]@{size=[BitConverter]::ToInt32($taskSys,$taskSection+16);offset=[BitConverter]::ToInt32($taskSys,$taskSection+20)}
    }
    $taskHashed = $taskHeaderSize
    foreach ($taskSection in ($taskSections | Sort-Object offset)) {
        Add-DpsHashPart $taskSection.offset $taskSection.size
        $taskHashed += $taskSection.size
    }
    Add-DpsHashPart $taskHashed ($taskSys.Length-$taskCertificateSize-$taskHashed)
    [void]$taskHasher.TransformFinalBlock([byte[]]@(),0,0)
    $taskImageDigest = [BitConverter]::ToString($taskHasher.Hash)
} finally { $taskHasher.Dispose() }
$taskIndirect = $taskCms.ContentInfo.Content
$taskIndirectHex = [BitConverter]::ToString($taskIndirect)
if ($taskIndirectHex -notmatch '06-09-60-86-48-01-65-03-04-02-01' -or $taskIndirect[-34] -ne 4 -or $taskIndirect[-33] -ne 32) { throw 'Expected SHA256 indirect-data digest.' }
if ([BitConverter]::ToString($taskIndirect[($taskIndirect.Length-32)..($taskIndirect.Length-1)]) -ne $taskImageDigest) { throw 'SYS Authenticode hash mismatch.' }
$taskCat = [Security.Cryptography.Pkcs.SignedCms]::new()
$taskCat.Decode([IO.File]::ReadAllBytes((Join-Path $PackageDirectory 'DpsAudioVirtual.cat')))
$taskCat.CheckSignature($true)
if ($taskCat.SignerInfos.Count -ne 1 -or $taskCat.SignerInfos[0].Certificate.Thumbprint -ne $taskCertificate.Thumbprint) { throw 'Unexpected CAT signer.' }
$taskInfHasher = [Security.Cryptography.SHA256]::Create()
try { $taskInfDigest = [BitConverter]::ToString($taskInfHasher.ComputeHash([IO.File]::ReadAllBytes((Join-Path $PackageDirectory 'DpsAudioVirtual.inf')))) } finally { $taskInfHasher.Dispose() }
$taskCatalogContent = [BitConverter]::ToString($taskCat.ContentInfo.Content)
if (-not $taskCatalogContent.Contains($taskImageDigest) -or -not $taskCatalogContent.Contains($taskInfDigest)) { throw 'Catalog does not contain current SYS/INF digests.' }
$taskCertificate.Dispose()
Write-Output 'PASS: x64 PE, SYS/CAT cryptographic signatures, expected signer, PE image digest and SYS/INF catalog digests.'
