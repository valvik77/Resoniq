# Create a development certificate as files only. No certificate-store imports,
# driver installation, boot configuration or global security changes.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$taskBuild = Join-Path $PSScriptRoot 'build'
$taskKit = Join-Path $PSScriptRoot 'packages\Microsoft.Windows.SDK.CPP.10.0.28000.2526\c\bin\10.0.28000.0\x64'
$taskSignTool = Join-Path $taskKit 'signtool.exe'
if (-not (Test-Path -LiteralPath $taskSignTool)) { throw 'Build/restore kits first.' }
foreach ($taskFile in @('DpsAudioVirtual.sys','DpsAudioVirtual.inf','DpsAudioVirtual.cat')) {
    if (-not (Test-Path -LiteralPath (Join-Path $taskBuild $taskFile))) { throw "Missing package file: $taskFile" }
}
$taskPfx = Join-Path $taskBuild 'DpsAudioDevelopment.pfx'
$taskCer = Join-Path $taskBuild 'DpsAudioDevelopment.cer'
if (-not (Test-Path -LiteralPath $taskPfx)) {
    $taskRsa = [System.Security.Cryptography.RSA]::Create(3072)
    try {
        $taskRequest = [System.Security.Cryptography.X509Certificates.CertificateRequest]::new(
            'CN=DPS Audio Development (TEST ONLY)', $taskRsa,
            [System.Security.Cryptography.HashAlgorithmName]::SHA256,
            [System.Security.Cryptography.RSASignaturePadding]::Pkcs1)
        $taskOids = [System.Security.Cryptography.OidCollection]::new()
        [void]$taskOids.Add([System.Security.Cryptography.Oid]::new('1.3.6.1.5.5.7.3.3'))
        $taskRequest.CertificateExtensions.Add([System.Security.Cryptography.X509Certificates.X509EnhancedKeyUsageExtension]::new($taskOids,$false))
        $taskRequest.CertificateExtensions.Add([System.Security.Cryptography.X509Certificates.X509KeyUsageExtension]::new(
            [System.Security.Cryptography.X509Certificates.X509KeyUsageFlags]::DigitalSignature,$true))
        $taskCertificate = $taskRequest.CreateSelfSigned([DateTimeOffset]::UtcNow.AddMinutes(-5),[DateTimeOffset]::UtcNow.AddMonths(6))
        try {
            [System.IO.File]::WriteAllBytes($taskPfx,$taskCertificate.Export([System.Security.Cryptography.X509Certificates.X509ContentType]::Pfx))
            [System.IO.File]::WriteAllBytes($taskCer,$taskCertificate.Export([System.Security.Cryptography.X509Certificates.X509ContentType]::Cert))
        } finally { $taskCertificate.Dispose() }
    } finally { $taskRsa.Dispose() }
}
# Sign SYS first, then rebuild CAT so its hashes refer to the signed image.
& $taskSignTool sign /fd SHA256 /f $taskPfx (Join-Path $taskBuild 'DpsAudioVirtual.sys')
if ($LASTEXITCODE -ne 0) { throw 'SYS signing failed' }
$taskInf2Cat = Join-Path $PSScriptRoot 'packages\Microsoft.Windows.WDK.x64.10.0.28000.2526\c\bin\10.0.28000.0\x86\Inf2Cat.exe'
& $taskInf2Cat "/driver:$taskBuild" /os:10_NI_X64,10_GE_X64
if ($LASTEXITCODE -ne 0) { throw 'Catalog regeneration failed' }
& $taskSignTool sign /fd SHA256 /f $taskPfx (Join-Path $taskBuild 'DpsAudioVirtual.cat')
if ($LASTEXITCODE -ne 0) { throw 'CAT signing failed' }
$taskInfoPath = Join-Path $taskBuild 'build-info.json'
$taskInfo = Get-Content -LiteralPath $taskInfoPath -Raw | ConvertFrom-Json
$taskInfo.signed = $true
$taskInfo | Add-Member -NotePropertyName signing -NotePropertyValue 'Self-signed development certificate, not installed or trusted' -Force
$taskInfo | ConvertTo-Json | Set-Content -LiteralPath $taskInfoPath -Encoding utf8
Write-Output 'Test-signed package prepared. No device installed; no certificate trust or boot settings changed.'
Write-Output 'Keep the PFX private. For distribution use an appropriate production signing process.'
