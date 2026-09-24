$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
function Get-Luminance([string]$HexColor) {
    $channels = @(1, 3, 5 | ForEach-Object {
        $value = [Convert]::ToInt32($HexColor.Substring($_, 2), 16) / 255.0
        if ($value -le 0.04045) { $value / 12.92 }
        else { [Math]::Pow(($value + 0.055) / 1.055, 2.4) }
    })
    return 0.2126 * $channels[0] + 0.7152 * $channels[1] + 0.0722 * $channels[2]
}
$checks = 0
foreach ($file in @('docs/NEXUS-DESCRIPTION-v1.3.2-KO.html', 'docs/NEXUS-CHANGELOG-v1.3.3-KO.html')) {
    $html = Get-Content -LiteralPath (Join-Path $repo $file) -Raw
    if (-not $html.Contains('color-scheme:light dark') -or
        -not $html.Contains('@media(prefers-color-scheme:dark)') -or
        -not $html.Contains('color:CanvasText') -or
        $html -match '(?i)(?<![-\w])color\s*:\s*(?:white\b|#fff(?:fff)?\b)') {
        throw "Missing adaptive theme or fixed white foreground in $file"
    }
    $palettes = [regex]::Matches($html, ':root\{([^}]+)\}')
    if ($palettes.Count -lt 2) { throw "Missing light/dark palettes in $file" }
    for ($mode = 0; $mode -lt 2; ++$mode) {
        $colors = @{}
        foreach ($color in [regex]::Matches($palettes[$mode].Groups[1].Value, '--([\w-]+):(#\w{6})')) {
            $colors[$color.Groups[1].Value] = $color.Groups[2].Value
        }
        # Standard browser CanvasText under light/dark color-scheme.
        $colors['body-text'] = if ($mode -eq 0) { '#000000' } else { '#ffffff' }
        $pairs = @()
        foreach ($background in @('bg', 'panel', 'notice', 'table-head', 'code-bg')) {
            if (-not $colors.ContainsKey($background)) { continue }
            foreach ($foreground in @('body-text', 'title', 'accent', 'sub', 'muted', 'code-text')) {
                if ($colors.ContainsKey($foreground)) { $pairs += ,@($foreground, $background) }
            }
        }
        foreach ($pair in $pairs) {
            $first = Get-Luminance $colors[$pair[0]]
            $second = Get-Luminance $colors[$pair[1]]
            $ratio = ([Math]::Max($first, $second) + 0.05) / ([Math]::Min($first, $second) + 0.05)
            if ($ratio -lt 4.5) {
                throw "Low contrast $file mode=$mode $($pair -join '/') ratio=$ratio"
            }
            ++$checks
        }
    }
}
"Documentation themes passed: $checks light/dark foreground-background contrast checks (>= 4.5:1)."
