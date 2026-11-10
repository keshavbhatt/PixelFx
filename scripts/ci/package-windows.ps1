# Packs the Windows build into MSIX packages for the Microsoft Store (ADR-018).
#
#   scripts/ci/package-windows.ps1 -Build build -Out dist
#
# Writes to OUT:
#   PixelFx-<version>-x64.msix       unsigned, for Partner Center, which signs it
#   PixelFx-<version>-x64-test.msix  signed with a certificate made for this run, to try it out
#   PixelFx-test.cer                 that certificate, to trust before installing the test package
#   INSTALL-TEST.txt                 how
# Needs the MSVC developer environment (makeappx, signtool, VCToolsRedistDir) and Qt's bin
# folder (windeployqt) on PATH.
param(
    [Parameter(Mandatory = $true)][string]$Build,
    [Parameter(Mandatory = $true)][string]$Out
)
$ErrorActionPreference = 'Stop'
$root = Resolve-Path "$PSScriptRoot/../.."

$cmakeLists = Get-Content "$root/CMakeLists.txt" -Raw
if ($cmakeLists -notmatch 'project\(\s*pixelfx[\s\S]*?VERSION\s+(\d+\.\d+\.\d+)') {
    throw 'No project(VERSION) in CMakeLists.txt'
}
$version = $Matches[1]
$stage = Join-Path $Out 'stage'
Remove-Item -Recurse -Force $stage -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $stage | Out-Null

# The program, then Qt beside it.
cmake --install $Build --prefix $stage
if ($LASTEXITCODE -ne 0) { throw 'cmake --install failed' }
$bin = Join-Path $stage 'bin'
windeployqt --no-translations --no-system-d3d-compiler --no-opengl-sw --no-compiler-runtime `
    --dir $bin "$bin/pixelfx.exe"
if ($LASTEXITCODE -ne 0) { throw 'windeployqt failed' }

# The image libraries from vcpkg, and the C++ runtime, kept in the package.
$vcpkg = Join-Path $Build 'vcpkg_installed/x64-windows'
Copy-Item "$vcpkg/bin/*.dll" $bin
Copy-Item "$env:VCToolsRedistDir/x64/Microsoft.VC143.CRT/*.dll" $bin

# The lens database, where Application::useShippedLensDatabase looks for it.
$lenses = Join-Path $vcpkg 'share/lensfun'
if (-not (Test-Path "$lenses/version_1")) { throw "No lens database in $lenses" }
New-Item -ItemType Directory -Force "$stage/share" | Out-Null
Copy-Item -Recurse $lenses "$stage/share/lensfun"

Get-ChildItem $stage -Recurse -Include *.pdb, *.lib, *.exp, *.ilk | Remove-Item -Force

# The manifest and its logos. The Store wants the fourth number of the version to be 0.
Copy-Item -Recurse "$root/packaging/windows/Assets" "$stage/Assets"
(Get-Content "$root/packaging/windows/AppxManifest.xml.in" -Raw).Replace('@VERSION@', "$version.0") |
    Set-Content -Encoding utf8 "$stage/AppxManifest.xml"

function Find-SdkTool([string]$name) {
    $found = Get-Command $name -ErrorAction SilentlyContinue
    if ($found) { return $found.Source }
    $candidate = Join-Path $env:WindowsSdkVerBinPath "x64/$name"
    if (Test-Path $candidate) { return $candidate }
    throw "$name not found"
}
$makeappx = Find-SdkTool 'makeappx.exe'
$signtool = Find-SdkTool 'signtool.exe'

$store = Join-Path $Out "PixelFx-$version-x64.msix"
& $makeappx pack /o /d $stage /p $store
if ($LASTEXITCODE -ne 0) { throw 'makeappx failed' }

# A copy signed for trying out: the certificate names the Store publisher, as Windows requires,
# and lives only for this run.
$test = Join-Path $Out "PixelFx-$version-x64-test.msix"
Copy-Item $store $test
$publisher = ([xml](Get-Content "$stage/AppxManifest.xml")).Package.Identity.Publisher
$cert = New-SelfSignedCertificate -Type Custom -Subject $publisher -KeyUsage DigitalSignature `
    -FriendlyName 'PixelFx test package' -CertStoreLocation 'Cert:\CurrentUser\My' `
    -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3', '2.5.29.19={text}')
$password = ConvertTo-SecureString -String ([guid]::NewGuid().ToString()) -Force -AsPlainText
$pfx = Join-Path $env:RUNNER_TEMP 'pixelfx-test.pfx'
Export-PfxCertificate -Cert $cert -FilePath $pfx -Password $password | Out-Null
& $signtool sign /fd SHA256 /f $pfx /p ([System.Net.NetworkCredential]::new('', $password).Password) $test
if ($LASTEXITCODE -ne 0) { throw 'signtool failed' }
Export-Certificate -Cert $cert -FilePath (Join-Path $Out 'PixelFx-test.cer') | Out-Null
Remove-Item $pfx

@"
Trying out PixelFx $version from this folder

1. Open PixelFx-test.cer, choose Install Certificate, Local Machine, then
   "Place all certificates in the following store", Browse, Trusted People. Finish.
2. Open PixelFx-$version-x64-test.msix and choose Install.

To remove it later: uninstall PixelFx from Start, and delete the certificate from Trusted People
(certlm.msc). PixelFx-$version-x64.msix is the package for Partner Center; it does not install
on its own.
"@ | Set-Content -Encoding utf8 (Join-Path $Out 'INSTALL-TEST.txt')

Remove-Item -Recurse -Force $stage
Get-ChildItem $Out | Format-Table Name, Length
