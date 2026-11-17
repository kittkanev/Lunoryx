param([Parameter(Mandatory=$true)][string]$ArtifactRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$policyText = & python tools/signing_policy.py windows
if ($LASTEXITCODE -ne 0) { throw 'Signing configuration rejected' }
$policy = $policyText | ConvertFrom-Json
$report = @{status='unsigned'; publisher_verified=$false; files=@(); source_sha=$env:GITHUB_SHA; reason='No publisher certificate configured'}
if ($policy.credentials_present) {
    if (-not $env:RUNNER_TEMP) { throw 'Signing is a CI-only operation' }
    $pfxPath = Join-Path $env:RUNNER_TEMP ('lunoryx-sign-' + [Guid]::NewGuid().ToString('N') + '.pfx')
    $certificate = $null
    $certificates = @()
    try {
        $acl = [Security.AccessControl.FileSecurity]::new()
        $owner = [Security.Principal.WindowsIdentity]::GetCurrent().User
        $acl.SetOwner($owner)
        $acl.SetAccessRuleProtection($true, $false)
        $acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new($owner, 'FullControl', 'Allow'))
        $credential = [Convert]::FromBase64String($env:LUNORYX_WINDOWS_PFX_BASE64)
        $stream = [IO.FileSystemAclExtensions]::Create([IO.FileInfo]::new($pfxPath), [IO.FileMode]::CreateNew, [Security.AccessControl.FileSystemRights]::Write, [IO.FileShare]::None, 4096, [IO.FileOptions]::WriteThrough, $acl)
        try { $stream.Write($credential); $stream.Flush($true) }
        finally { $stream.Dispose(); [Array]::Clear($credential, 0, $credential.Length) }
        $password = ConvertTo-SecureString $env:LUNORYX_WINDOWS_PFX_PASSWORD -AsPlainText -Force
        $certificates = @(Import-PfxCertificate -FilePath $pfxPath -CertStoreLocation 'Cert:\CurrentUser\My' -Password $password)
        $signers = @($certificates | Where-Object {$_.HasPrivateKey})
        if ($signers.Count -ne 1) { throw 'PFX must contain exactly one private signing identity' }
        $certificate = $signers[0]
        if ($certificate.NotAfter.ToUniversalTime() -le [DateTime]::UtcNow) { throw 'Signing certificate expired' }
        $tools = @(Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x64\signtool.exe" | Sort-Object FullName -Descending)
        if ($tools.Count -eq 0) { throw 'Windows SDK signtool is unavailable' }
        $signtool = $tools[0].FullName
        $names = @('engine.exe','Runtime/Luaryx.Worker.exe','Runtime/Luaryx.Worker.dll')
        $verified = @()
        foreach ($name in $names) {
            $file = Join-Path $ArtifactRoot $name
            if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing owned signing target: $name" }
            & $signtool sign /sha1 $certificate.Thumbprint /s My /fd SHA256 /tr $env:LUNORYX_WINDOWS_TIMESTAMP_URL /td SHA256 $file
            if ($LASTEXITCODE -ne 0) { throw "Authenticode signing failed: $name" }
            & $signtool verify /pa /all $file
            if ($LASTEXITCODE -ne 0) { throw "Authenticode verification failed: $name" }
            $signature = Get-AuthenticodeSignature -LiteralPath $file
            if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Thumbprint -ne $certificate.Thumbprint -or -not $signature.TimeStamperCertificate) {
                throw "Publisher/timestamp verification failed: $name"
            }
            $verified += @{name=$name; status='valid'; timestamped=$true}
        }
        $report = @{status='authenticode'; publisher_verified=$true; files=$verified; source_sha=$env:GITHUB_SHA; thumbprint=$certificate.Thumbprint}
    } finally {
        foreach ($imported in $certificates) { Remove-Item -LiteralPath ('Cert:\CurrentUser\My\' + $imported.Thumbprint) -DeleteKey -ErrorAction SilentlyContinue }
        Remove-Item -LiteralPath $pfxPath -Force -ErrorAction SilentlyContinue
    }
}
$report | ConvertTo-Json -Depth 8 | Set-Content -Encoding utf8 (Join-Path $ArtifactRoot 'BUILD-SIGNING.json')
Write-Output ('Signing status: ' + $report.status + '; publisher_verified=' + $report.publisher_verified)
