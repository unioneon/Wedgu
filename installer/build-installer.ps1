# Builds the local release packages; never uploads or publishes them.
param([string]$Version=(Get-Content (Join-Path $PSScriptRoot '..\VERSION') -Raw).Trim(),[string]$Compiler)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
Set-Location -LiteralPath $root
if (-not (Test-Path -LiteralPath "$root\Wedgu.exe")) { throw 'Build Wedgu.exe first.' }
if ((Get-Item -LiteralPath "$root\Wedgu.exe").VersionInfo.ProductVersion -ne $Version) { throw 'EXE and release versions do not match.' }
$hscSha='e371da043710ddb8fc70af77f2ccad1cd03f8c6f49ba423e08764ad7a94a3c40'
$hscDir=Join-Path $root 'third_party\headsetcontrol'
$hscExe=Join-Path $hscDir 'headsetcontrol.exe'
New-Item -ItemType Directory -Force -Path $hscDir | Out-Null
if (-not (Test-Path -LiteralPath $hscExe)) { Invoke-WebRequest 'https://github.com/Sapd/HeadsetControl/releases/download/4.3.0/headsetcontrol-windows-x86_64.exe' -OutFile $hscExe }
if ((Get-FileHash -LiteralPath $hscExe -Algorithm SHA256).Hash.ToLowerInvariant() -ne $hscSha) { throw 'HeadsetControl checksum mismatch.' }
if (-not $Compiler) {
 $candidates=@("$root\..\tools\InnoSetup7\ISCC.exe")
 foreach ($v in 7,6) { foreach ($base in ${env:ProgramFiles(x86)},$env:ProgramFiles,"$env:LOCALAPPDATA\Programs") {$candidates+="$base\Inno Setup $v\ISCC.exe"} }
 $Compiler=$candidates | Where-Object {Test-Path -LiteralPath $_} | Select-Object -First 1
 if (-not $Compiler) {$Compiler=(Get-Command ISCC.exe -ErrorAction SilentlyContinue).Source}
}
if (-not $Compiler) {throw 'Inno Setup 6/7 compiler not found.'}
$out=Join-Path $root "dist\Wedgu-$Version"
New-Item -ItemType Directory -Force -Path $out | Out-Null
& $Compiler "/DAppVersion=$Version" "/O$out" "$PSScriptRoot\Wedgu.iss"
if ($LASTEXITCODE -ne 0) {throw 'Installer compilation failed.'}
$stage=Join-Path $root 'build\release-portable'
$absolute=[IO.Path]::GetFullPath($stage)
if ($absolute -ne [IO.Path]::GetFullPath("$root\build\release-portable") -or -not $absolute.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)) {throw 'Invalid staging path.'}
if (Test-Path -LiteralPath $absolute) {Remove-Item -LiteralPath $absolute -Recurse -Force}
New-Item -ItemType Directory -Path $stage | Out-Null
Copy-Item -LiteralPath "$root\Wedgu.exe",$hscExe,"$PSScriptRoot\README.txt","$PSScriptRoot\THIRD_PARTY_NOTICES.txt","$root\vendor\json-LICENSE.txt" -Destination $stage
Copy-Item -LiteralPath "$root\LICENSE" -Destination "$stage\LICENSE.txt"
Compress-Archive -Path "$stage\*" -DestinationPath "$out\Wedgu-$Version-windows-x64-portable.zip" -Force
# Source allowlist deliberately excludes local profiles, registry exports, diagnostics and build output.
$sourceStage=Join-Path $root 'build\release-source'
$absolute=[IO.Path]::GetFullPath($sourceStage)
if ($absolute -ne [IO.Path]::GetFullPath("$root\build\release-source") -or -not $absolute.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)) {throw 'Invalid source staging path.'}
if (Test-Path -LiteralPath $absolute) {Remove-Item -LiteralPath $absolute -Recurse -Force}
New-Item -ItemType Directory -Path $sourceStage | Out-Null
foreach ($name in 'src','res','tests','installer','scripts','vendor','docs','.github','Makefile','build.bat','VERSION','LICENSE','README.md','CONTRIBUTING.md','LOCAL-CHANGES.md','UPSTREAM-README.md','CHANGELOG.md','.gitignore') {if(Test-Path -LiteralPath "$root\$name"){Copy-Item -LiteralPath "$root\$name" -Destination $sourceStage -Recurse}}
Compress-Archive -Path "$sourceStage\*" -DestinationPath "$out\Wedgu-$Version-source.zip" -Force
$hscSource="$out\headsetcontrol-4.3.0.tar.gz"
if (-not (Test-Path -LiteralPath $hscSource)) {Invoke-WebRequest 'https://github.com/Sapd/HeadsetControl/releases/download/4.3.0/headsetcontrol-4.3.0.tar.gz' -OutFile $hscSource}
if ((Get-FileHash -LiteralPath $hscSource -Algorithm SHA256).Hash.ToLowerInvariant() -ne '918dcacf8aef347744b36b2d259d76880fa64b3869f0f8ef26b6576b873d28a7') {throw 'HeadsetControl source checksum mismatch.'}
Copy-Item -LiteralPath "$root\docs\RELEASE-$Version.md" -Destination "$out\RELEASE-NOTES.md"
$lines=Get-ChildItem -LiteralPath $out -File | Where-Object Name -ne 'SHA256SUMS.txt' | Sort-Object Name | ForEach-Object {'{0}  {1}' -f (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant(),$_.Name}
$lines | Set-Content -LiteralPath "$out\SHA256SUMS.txt" -Encoding ascii
Write-Output "Release files prepared in $out (not published)."
