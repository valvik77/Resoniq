$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$taskManifest=Get-Content -LiteralPath (Join-Path $taskRoot 'manifest.json') -Raw | ConvertFrom-Json
$taskSeen=@{}
foreach($taskEntry in $taskManifest) {
    $taskPath=[IO.Path]::GetFullPath((Join-Path $taskRoot $taskEntry.path))
    if(-not $taskPath.StartsWith($taskRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {throw 'Ruta de manifiesto fuera del paquete'}
    if($taskSeen.ContainsKey($taskPath)){throw 'Ruta duplicada en manifiesto'}
    $taskSeen[$taskPath]=$true
    if((Get-FileHash -LiteralPath $taskPath -Algorithm SHA256).Hash -ne $taskEntry.sha256){throw ('Archivo modificado: '+$taskEntry.path)}
}
Write-Output ('PASS: integridad de '+$taskSeen.Count+' archivos; no sustituye una firma digital')
