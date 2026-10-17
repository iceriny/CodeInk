# Invoked as an explicit PowerShell -Command by the native bootstrapper.
# No execution-policy, Defender, Office, PATH or machine-wide changes.
param([Parameter(Mandatory=$true)][string]$Root, [switch]$Diagnostics)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
try {
    $version = 'v24.21.0'
    $archive = "node-$version-win-x64.zip"
    $runtime = Join-Path $Root 'runtime'
    $downloads = Join-Path $Root '_downloads'
    $node = Join-Path $runtime "node-$version-win-x64\node.exe"
    $marker = Join-Path $runtime '.codeink-verified-runtime.txt'
    New-Item -ItemType Directory -Force -Path $runtime, $downloads | Out-Null
    if ((Test-Path -LiteralPath $node -PathType Leaf) -and (Test-Path -LiteralPath $marker -PathType Leaf)) {
        # Reuse the verified private runtime on repair; never rely on system PATH.
        exit 0
    }
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $base = "https://nodejs.org/dist/$version/"
    $zip = Join-Path $downloads $archive
    $sums = Join-Path $downloads 'SHASUMS256.txt'
    Invoke-WebRequest -UseBasicParsing -Uri ($base + 'SHASUMS256.txt') -OutFile $sums
    Invoke-WebRequest -UseBasicParsing -Uri ($base + $archive) -OutFile $zip
    $matching = @(Get-Content -LiteralPath $sums | Where-Object { $_ -match ('^[a-fA-F0-9]{64}\s+' + [regex]::Escape($archive) + '$') })
    if ($matching.Count -ne 1) { throw 'Official Node.js checksum entry was not found.' }
    $expected = ($matching[0] -split '\s+')[0]
    $actual = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash
    if ($actual -ine $expected) { throw 'Node.js ZIP SHA256 does not match the official checksum.' }
    Expand-Archive -LiteralPath $zip -DestinationPath $runtime -Force
    if (-not (Test-Path -LiteralPath $node -PathType Leaf)) { throw 'Node.js extraction did not produce node.exe.' }
    [IO.File]::WriteAllText($marker, "$version`r`n$actual`r`n")
    Remove-Item -LiteralPath $zip, $sums -Force
    exit 0
} catch {
    if ($Diagnostics) {
        try { [IO.File]::WriteAllText((Join-Path $Root 'bootstrap-error.txt'), $_.Exception.Message, [Text.Encoding]::UTF8) } catch {}
    }
    exit 1
}
