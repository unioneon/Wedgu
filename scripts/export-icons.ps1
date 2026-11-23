param([string]$OutputDirectory = (Join-Path $PSScriptRoot '..\build\svg-icons'))
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$out = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force $out | Out-Null
$culture = [Globalization.CultureInfo]::InvariantCulture
function N([single]$v) { $v.ToString('0.####',$culture) }
function Path-Data($path) {
    $points=$path.PathPoints; $types=$path.PathTypes
    $sb=[Text.StringBuilder]::new()
    for($i=0;$i -lt $points.Length;$i++) {
        $t=$types[$i] -band 7
        if($t -eq 0) { [void]$sb.Append("M$(N $points[$i].X) $(N $points[$i].Y)") }
        elseif($t -eq 1) { [void]$sb.Append("L$(N $points[$i].X) $(N $points[$i].Y)") }
        elseif($t -eq 3) {
            [void]$sb.Append("C$(N $points[$i].X) $(N $points[$i].Y) $(N $points[$i+1].X) $(N $points[$i+1].Y) $(N $points[$i+2].X) $(N $points[$i+2].Y)")
            $i+=2
        } else { throw "Unsupported path point type $t" }
        if(($types[$i] -band 128) -ne 0) { [void]$sb.Append('Z') }
    }
    $sb.ToString()
}
function Text-Path([string]$text,$family,[int]$style,[single]$x,[single]$y,[single]$width,[single]$height) {
    $p=[Drawing.Drawing2D.GraphicsPath]::new()
    $p.AddString($text,$family,$style,[single]2048,[Drawing.PointF]::new(0,0),[Drawing.StringFormat]::GenericTypographic)
    $r=$p.GetBounds()
    if($r.Width -le 0 -or $r.Height -le 0) { throw "Empty outline for $text" }
    $scale=[Math]::Min($width/$r.Width,$height/$r.Height)
    $dx=$x+($width-$r.Width*$scale)/2-$r.X*$scale
    $dy=$y+($height-$r.Height*$scale)/2-$r.Y*$scale
    $matrix=[Drawing.Drawing2D.Matrix]::new([single]$scale,0,0,[single]$scale,[single]$dx,[single]$dy)
    $p.Transform($matrix); $matrix.Dispose()
    return ,$p
}
function Write-Svg([string]$name,[string]$title,[string]$body,[int]$size=24,[string]$color='#3c3c3c') {
    $xml='<svg xmlns="http://www.w3.org/2000/svg" width="'+$size+'" height="'+$size+'" viewBox="0 0 '+$size+' '+$size+'" color="'+$color+'" role="img"><title>'+$title+'</title>'+$body+'</svg>'
    [IO.File]::WriteAllText((Join-Path $out ($name+'.svg')),$xml,[Text.UTF8Encoding]::new($false))
    [xml]$check=$xml
    if($check.svg.GetAttribute('viewBox') -eq '') { throw 'Missing SVG viewBox' }
}
$available=[Drawing.FontFamily]::Families.Name
$face=if($available -contains 'Segoe Fluent Icons'){'Segoe Fluent Icons'}else{'Segoe MDL2 Assets'}
$family=[Drawing.FontFamily]::new($face)
$glyphs=[ordered]@{
    'headphones'=@(0xE7F6,'Headphones')
    'mouse'=@(0xE962,'Mouse')
    'keyboard'=@(0xE765,'Keyboard')
    'bluetooth'=@(0xE702,'Bluetooth')
    'devices'=@(0xE772,'Devices')
    'charging'=@(0xE945,'Charging')
    'battery-unknown'=@(0xE996,'Battery unknown')
    'gamepad-system'=@(0xE7FC,'Original system gamepad glyph')
}
$paths=@{}
foreach($name in $glyphs.Keys) {
    $p=Text-Path ([string][char]$glyphs[$name][0]) $family 0 2 2 20 20
    $paths[$name]=$p
    Write-Svg $name $glyphs[$name][1] ('<path fill="currentColor" fill-rule="evenodd" d="'+(Path-Data $p)+'"/>')
}
$body='M7.4 5H16.6C19.3 5 20.5 6.4 21.4 9.5C22.3 12.3 23 15.5 22.5 17.4C22.1 19 20.5 19.5 19.5 18.3L17.9 15.8C17.5 15.2 17.1 14.9 16.4 14.9H7.6C6.9 14.9 6.5 15.2 6.1 15.8L4.5 18.3C3.5 19.5 1.9 19 1.5 17.4C1 15.5 1.7 12.3 2.6 9.5C3.5 6.4 4.7 5 7.4 5Z'
Write-Svg 'gamepad' 'Custom gamepad - current v4 design' ('<path d="'+$body+'" fill="none" stroke="currentColor" stroke-width=".95" stroke-linecap="round" stroke-linejoin="round"/><g fill="currentColor"><rect x="4.6" y="9.45" width="1.85" height=".9"/><rect x="7.55" y="9.45" width="1.85" height=".9"/><rect x="6.55" y="7.3" width=".9" height="1.85"/><rect x="6.55" y="10.65" width=".9" height="1.85"/></g><circle cx="17.5" cy="9.7" r="1.7" fill="none" stroke="currentColor" stroke-width=".85"/>')
Write-Svg 'app-battery' 'Application battery icon' '<rect x="1" y="4" width="12.5" height="8" fill="#fff" stroke="#3c3c3c" stroke-width="1.2"/><path fill="#3c3c3c" d="M13.5 6.5H15V9.5H13.5Z"/><path fill="#1ebe5a" d="M2.5 5.5H9.5V10.5H2.5Z"/>' 16
$textFamily=[Drawing.FontFamily]::new('Segoe UI')
$tray=[ordered]@{
    'tray-50'=@('50',50,'currentColor','#1ebe5a',1.0)
    'tray-low-10'=@('10',10,'#f0323c','#e82030',1.0)
    'tray-charging-50'=@('50',50,'currentColor','#00aaff',1.0)
    'tray-unknown'=@('?',-1,'currentColor','#1ebe5a',0.549)
    'tray-placeholder'=@('-',-1,'currentColor','currentColor',0.549)
}
foreach($name in $tray.Keys) {
    $v=$tray[$name];$p=Text-Path $v[0] $textFamily 1 0 0 16 13
    $fill=if($v[1] -lt 0){0}else{[Math]::Max(1,[Math]::Floor(($v[1]*16+50)/100))}
    $xml='<g opacity="'+$v[4].ToString($culture)+'"><path fill="'+$v[2]+'" fill-rule="evenodd" d="'+(Path-Data $p)+'"/><rect y="14" width="16" height="2" fill="currentColor" opacity=".27451"/>'
    if($fill -gt 0){$xml+='<rect y="14" width="'+$fill+'" height="2" fill="'+$v[3]+'"/>'}
    Write-Svg $name $name ($xml+'</g>') 16 '#000000'
    $p.Dispose()
}
$readme=@"
SVG icons exported from Wireless Battery - Windows Style 1.0.4.4.

Primary: headphones.svg, mouse.svg, keyboard.svg, gamepad.svg.
Utility: bluetooth.svg, devices.svg, charging.svg, battery-unknown.svg.
Application: app-battery.svg (vector geometry from res/make_icon.ps1).
Legacy: gamepad-system.svg (the original system-font glyph, no longer used in the taskbar).
Tray examples: tray-50.svg, tray-low-10.svg, tray-charging-50.svg,
tray-unknown.svg, tray-placeholder.svg. The actual tray percentage is generated dynamically.

All files contain vector paths/shapes; no embedded bitmap, SVG text or font dependency.
Glyph outlines were exported from $face, the font selected by the current app.
The custom gamepad uses the current v4 geometry from src/widget.cpp, not an earlier draft.
The application battery follows res/make_icon.ps1. Tray examples vectorize the text/level-bar layout.

Transparent backgrounds. Monochrome assets use currentColor, default #3c3c3c;
set color to #dcdcdc for the app's dark-theme foreground. App/tray status colors are preserved.

System glyph source: Microsoft $face. Custom gamepad and application geometry:
Wireless Battery project and Windows Style modifications. Export does not change asset authorship.
Project: https://github.com/unioneon/Wedgu
Original: https://github.com/krisgfx9/Wireless-Device-Battery-Monitor
"@
[IO.File]::WriteAllText((Join-Path $out 'README.txt'),$readme,[Text.UTF8Encoding]::new($false))
$cells=@('headphones','mouse','keyboard','gamepad','bluetooth','devices','charging','battery-unknown','app-battery','gamepad-system','tray-50','tray-low-10','tray-charging-50','tray-unknown','tray-placeholder')
$tiles=foreach($name in $cells){'<div><img src="'+$name+'.svg" alt="'+$name+'"><span>'+$name+'</span></div>'}
$html='<!doctype html><html lang="en"><meta charset="utf-8"><title>Wireless Battery SVG icons</title><style>body{font:14px Segoe UI,Arial;background:#f3f5f8;color:#3c3c3c;padding:32px}main{display:grid;grid-template-columns:repeat(5,160px);gap:16px}main div{background:white;border:1px solid #e2e6ed;border-radius:12px;padding:20px 8px;text-align:center}img{display:block;width:64px;height:64px;margin:0 auto 18px}h1{font-weight:600}span{font-size:12px}</style><h1>Wireless Battery · SVG icons</h1><p>Current gamepad design · self-contained vector paths</p><main>'+($tiles -join '')+'</main></html>'
[IO.File]::WriteAllText((Join-Path $out 'preview.html'),$html,[Text.UTF8Encoding]::new($false))
foreach($p in $paths.Values){$p.Dispose()};$family.Dispose();$textFamily.Dispose()
Write-Output "Exported $((Get-ChildItem $out -Filter '*.svg').Count) SVG icons from $face to $out"
