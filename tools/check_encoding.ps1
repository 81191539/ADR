param(
    [switch]$All,
    [switch]$FailOnBom
)

$ErrorActionPreference = "Stop"
$utf8Strict = [System.Text.UTF8Encoding]::new($false, $true)
$textExtensions = @(
    ".bat", ".cmake", ".cmd", ".cpp", ".css", ".cu", ".h", ".html",
    ".js", ".json", ".m", ".md", ".ps1", ".py", ".toml", ".txt", ".yml", ".yaml"
)
$ignoredParts = @(
    "\.git\", "\build\", "\cmake-build-", "\hyperui\", "\docs\html\",
    "\node_modules\", "\output\", "\results_old\", "\__pycache__\", "\.venv\", "\venv\"
)

function Test-IgnoredPath {
    param([string]$Path)
    $normalizedPath = $Path.TrimEnd("\") + "\"
    foreach ($part in $ignoredParts) {
        if ($normalizedPath -like "*$part*") {
            return $true
        }
    }
    return $false
}

$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

function Get-AllCandidateFiles {
    $queue = New-Object System.Collections.Generic.Queue[System.IO.DirectoryInfo]
    $queue.Enqueue((Get-Item -LiteralPath $root))

    while ($queue.Count -gt 0) {
        $dir = $queue.Dequeue()
        foreach ($childDir in Get-ChildItem -LiteralPath $dir.FullName -Directory -Force) {
            if (-not (Test-IgnoredPath $childDir.FullName)) {
                $queue.Enqueue($childDir)
            }
        }
        foreach ($childFile in Get-ChildItem -LiteralPath $dir.FullName -File -Force) {
            $childFile
        }
    }
}

$files = if ($All) {
    Get-AllCandidateFiles
} else {
    git -C $root -c core.quotepath=false ls-files --cached --others --exclude-standard | ForEach-Object {
        $path = Join-Path $root $_
        if (Test-Path -LiteralPath $path) {
            Get-Item -LiteralPath $path
        }
    }
}

$bad = New-Object System.Collections.Generic.List[string]

foreach ($file in $files) {
    if (Test-IgnoredPath $file.FullName) {
        continue
    }
    if ($textExtensions -notcontains $file.Extension.ToLowerInvariant()) {
        continue
    }

    $bytes = [System.IO.File]::ReadAllBytes($file.FullName)
    if ($FailOnBom -and
        $file.Extension.ToLowerInvariant() -ne ".toml" -and
        $bytes.Length -ge 3 -and
        $bytes[0] -eq 0xEF -and
        $bytes[1] -eq 0xBB -and
        $bytes[2] -eq 0xBF) {
        $bad.Add("UTF-8 BOM is not allowed: $($file.FullName)")
        continue
    }
    try {
        $text = $utf8Strict.GetString($bytes)
    } catch {
        $bad.Add("invalid UTF-8: $($file.FullName)")
        continue
    }

    if ($text.Contains([string][char]0xFFFD)) {
        $bad.Add("replacement character found: $($file.FullName)")
    }
}

if ($bad.Count -gt 0) {
    $bad | ForEach-Object { [Console]::Error.WriteLine($_) }
    exit 1
}

Write-Host "Encoding check passed."
