param(
    [Parameter(Mandatory = $true)]
    [string]$Path
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($env:KMS_KEY_PATH)) {
    throw 'KMS_KEY_PATH must contain the full KMS cryptoKeyVersions resource.'
}
$certPath = Join-Path $env:RUNNER_TEMP 'ev_cert.crt'
if (-not (Test-Path -LiteralPath $certPath -PathType Leaf)) {
    throw 'Signing certificate not found.'
}
$signtool = Get-ChildItem "${env:ProgramFiles(x86)}/Windows Kits/10/bin/*/x64/signtool.exe" |
    Sort-Object { [version]$_.Directory.Parent.Name } |
    Select-Object -Last 1 -ExpandProperty FullName
if (-not $signtool) { throw 'signtool.exe not found in Windows Kits.' }

$files = @(Get-ChildItem -Path $Path -File)
if ($files.Count -eq 0) { throw "No signing targets found: $Path" }
foreach ($file in $files) {
    for ($attempt = 1; $attempt -le 3; $attempt++) {
        & $signtool sign /v /fd sha256 /tr http://timestamp.digicert.com /td sha256 `
            /csp 'Google Cloud KMS Provider' /kc $env:KMS_KEY_PATH /f $certPath $file.FullName
        if ($LASTEXITCODE -eq 0) { break }
        if ($attempt -eq 3) { throw "Signing failed after three attempts: $($file.FullName)" }
        Write-Warning "Signing attempt $attempt failed; retrying in 15 seconds."
        Start-Sleep -Seconds 15
    }
    & $signtool verify /v /pa $file.FullName
    if ($LASTEXITCODE -ne 0) { throw "Signature verification failed: $($file.FullName)" }
}