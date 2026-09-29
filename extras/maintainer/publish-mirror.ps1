#Requires -Version 7
<#
.SYNOPSIS
    Publishes this folder, and only this folder, to the public GitHub repository.

.DESCRIPTION
    This library is developed inside the maintainer's private repository and mirrored here with
    `git subtree split`, so only the history of this folder is pushed - nothing else from the
    private repository travels with it.

    Only COMMITTED work is published. Before pushing, every file in the folder's committed tree is
    checked for text that belongs to the private repository (its remote's host name, its own name
    and its path on disk), read from git at run time so this script names none of them.

.PARAMETER DryRun
    Split and check, then print what would be pushed without pushing.

.EXAMPLE
    pwsh MyDesktopWidgetScreens/extras/maintainer/publish-mirror.ps1 -DryRun
#>
param(
    [switch] $DryRun
)

$ErrorActionPreference = 'Stop'

$Prefix = 'MyDesktopWidgetScreens'
$Remote = 'screens'
$Url = 'https://github.com/mydesktopwidget/mydesktopwidget-screens.git'
$Branch = 'main'

$root = (git rev-parse --show-toplevel).Trim()
Set-Location $root

# The remote, added on first use and checked every time so a push can never go somewhere else.
$existing = git remote get-url $Remote 2>$null
if (-not $existing) {
    git remote add $Remote $Url
    Write-Host "Added remote '$Remote' -> $Url"
}
elseif ($existing.Trim() -ne $Url) {
    throw "Remote '$Remote' points at '$existing', not '$Url'. Fix it before publishing."
}

# Uncommitted changes are not published, which is easy to forget.
$dirty = git status --porcelain -- $Prefix
if ($dirty) {
    Write-Warning "Uncommitted changes in $Prefix are NOT published:`n$($dirty -join "`n")"
}

# What must never appear in the public tree, read from this repository rather than written here.
$forbidden = @()
$origin = git remote get-url origin 2>$null
if ($origin) {
    $forbidden += ([Uri] $origin.Trim()).Host
}
$forbidden += Split-Path $root -Leaf
$forbidden += $root
$forbidden += $root.Replace('/', '\')
$forbidden = $forbidden | Where-Object { $_ } | Sort-Object -Unique

$files = git ls-tree -r --name-only HEAD -- $Prefix
$hits = foreach ($file in $files) {
    $text = git show "HEAD:$file"
    foreach ($word in $forbidden) {
        if ($text -match [Regex]::Escape($word)) { "$file mentions an internal name" }
    }
}
if ($hits) {
    throw "Refusing to publish:`n$(($hits | Sort-Object -Unique) -join "`n")"
}

Write-Host "Splitting $Prefix (this walks the history and can take a minute)..."
$commit = (git subtree split --prefix=$Prefix HEAD).Trim()
if (-not $commit) {
    throw "git subtree split produced no commit."
}

Write-Host "Split commit: $commit"
git log --oneline -5 $commit

if ($DryRun) {
    Write-Host "Dry run: would push $commit to $Remote/$Branch."
    return
}

git push $Remote "${commit}:refs/heads/$Branch"
Write-Host "Published $Prefix to $Url ($Branch)."
