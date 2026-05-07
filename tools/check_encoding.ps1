param(
    [switch]$All
)

$ErrorActionPreference = "Stop"
$utf8Strict = [System.Text.UTF8Encoding]::new($false, $true)
$mojibakePattern = [regex](
    '\u9422\u3126\u57DB|' +  # "user" after UTF-8 text was decoded as GBK
    '\u95B0\u5DB7\u7586|' +
    '\u951B|' +
    '\u9286|' +
    '\uFFFD|' +
    '\u00C3.|\u00C2.|' +
    '[\u00E4\u00E5\u00E6\u00E7].'
)
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
    git -C $root -c core.quotepath=false ls-files | ForEach-Object {
        Get-Item -LiteralPath (Join-Path $root $_)
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
    try {
        $text = $utf8Strict.GetString($bytes)
    } catch {
        $bad.Add("invalid UTF-8: $($file.FullName)")
        continue
    }

    if ($mojibakePattern.IsMatch($text)) {
        $bad.Add("suspicious mojibake: $($file.FullName)")
    }
}

if ($bad.Count -gt 0) {
    $bad | ForEach-Object { [Console]::Error.WriteLine($_) }
    exit 1
}

Write-Host "Encoding check passed."
