# Rend les images des boutons du N73 dans Resources/knobs (×2 de la taille de base de l'interface).
$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $root "Resources\knobs"
New-Item -ItemType Directory -Force $out | Out-Null
Add-Type -TypeDefinition ([IO.File]::ReadAllText((Join-Path $PSScriptRoot "KnobRender.cs"))) -ReferencedAssemblies System.Drawing
$sw = [Diagnostics.Stopwatch]::StartNew()
[KnobRender]::RenderAssets($out, 2.0)
"rendu en {0:0.0} s" -f $sw.Elapsed.TotalSeconds
Get-ChildItem $out -Filter *.png | Measure-Object -Property Length -Sum | ForEach-Object { "{0} images, {1:0} Ko" -f $_.Count, ($_.Sum / 1KB) }
