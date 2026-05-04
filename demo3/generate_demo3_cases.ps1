param(
    [string]$InputDir = "$PSScriptRoot\input"
)

$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Path $InputDir -Force | Out-Null
Get-ChildItem -Path $InputDir -Filter "input_parameter_*.toml" -File | Remove-Item -Force

$aMin = 0.001
$aMax = 10.0
$aCount = 41
$pe2Min = 0.1
$pe2Max = 1.0e8
$pe2Count = 91
$caseId = 1
$culture = [Globalization.CultureInfo]::InvariantCulture

for ($j = 0; $j -lt $pe2Count; $j++) {
    $pe2 = [math]::Pow(10, [math]::Log10($pe2Min) + ([math]::Log10($pe2Max) - [math]::Log10($pe2Min)) * $j / ($pe2Count - 1))
    for ($i = 0; $i -lt $aCount; $i++) {
        $alpha = [math]::Pow(10, [math]::Log10($aMin) + ([math]::Log10($aMax) - [math]::Log10($aMin)) * $i / ($aCount - 1))
        $path = Join-Path $InputDir ("input_parameter_{0:D4}.toml" -f $caseId)
        $content = @(
            "legacy_marker = 1"
            "lam = 0.033333"
            "Pe = 1000"
            "Pe2 = $($pe2.ToString('G12', $culture))"
            "eps = 0.1"
            "Da = 100"
            "K0 = 1"
            "ny = 8"
            "xpo_l = 0.333333"
            "xpo_r = 0.666667"
            "endT = 0.002"
            "total_count = 1"
            "coeff_dt = 0.1"
            "x_ini_posi = 5"
            "alpha = $($alpha.ToString('G12', $culture))"
            ""
        ) -join "`n"
        Set-Content -Path $path -Value $content -Encoding UTF8
        $caseId++
    }
}

Write-Host "Generated $($caseId - 1) demo3 case files in $InputDir"
