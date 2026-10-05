#Requires -Version 7
<#
.SYNOPSIS
    Publishes this folder, and only this folder, to the public GitHub repository.

.DESCRIPTION
    This library is developed inside the maintainer's private repository and mirrored here with
    `git subtree split`, so only the history of this folder is pushed - nothing else from the
    private repository travels with it.

    Only COMMITTED work is published. Before pushing, every file in every public commit - the
    history, not only the latest tree - is checked for text that belongs to the private repository
    (its remote's host name, its own name and its path on disk), read from git at run time so this
    script names none of them. Build leftovers (__pycache__, *.pyc) are left out of every public
    commit: a compiled Python file carries the absolute path of its source.

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

# Commit messages travel with the history, and a message is easier to slip a private path into than a
# file: "the contract in <folder>/..." reads as an ordinary note. So a public message may not name any
# other top-level folder of this repository either - read here, so this script names none of them.
$privateFolders = git ls-tree -d --name-only HEAD | Where-Object { $_ -and $_ -ne $Prefix } |
    ForEach-Object { "$_/" }

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

# The public history carries the project's address, never a personal one. Each split commit is
# re-created with the same tree, message and dates and only the e-mail changed, so the same private
# history always produces the same public commits and a later publish is still a fast-forward.
$PublicEmail = 'support@mydesktopwidget.com'
$map = @{}

# Never public, whatever was committed: a .pyc records the absolute path it was compiled from, which is
# a path on the maintainer's machine. Two were committed by mistake on 2026-10-05 and are dropped here,
# deterministically, so the same private history still makes the same public commits.
$neverPublic = '(^|/)__pycache__/|\.pyc$'

function Get-PublicTree([string] $tree) {
    $drop = @(git ls-tree -r --name-only $tree | Where-Object { $_ -match $neverPublic })
    if ($drop.Count -eq 0) { return $tree }

    $index = New-TemporaryFile
    try {
        $env:GIT_INDEX_FILE = $index.FullName
        git read-tree $tree
        git rm --cached --quiet -- @drop | Out-Null
        return (git write-tree).Trim()
    }
    finally {
        Remove-Item Env:GIT_INDEX_FILE -ErrorAction SilentlyContinue
        Remove-Item $index -ErrorAction SilentlyContinue
    }
}
$message = New-TemporaryFile

try {
    foreach ($old in git rev-list --reverse --topo-order $commit) {
        $tree = Get-PublicTree (git log -1 --format=%T $old).Trim()

        # Every public commit's files, not only the latest tree's: the history is published too, and a
        # file removed later would otherwise still carry a private name into it.
        foreach ($word in $forbidden) {
            $leaks = @(git grep -l -a -F -e $word $tree 2>$null) |
                Sort-Object -Unique
            if ($leaks) {
                throw ("Refusing to publish: commit $((git log -1 --format=%h $old).Trim()) has files naming " +
                    "something private:`n$($leaks -join "`n")")
            }
        }
        $parents = @((git log -1 --format=%P $old).Trim() -split ' ' | Where-Object { $_ } |
            ForEach-Object { '-p'; $map[$_] })

        $body = (git log -1 --format=%B $old) -join "`n"

        foreach ($word in @($forbidden) + @($privateFolders)) {
            if ($body.Contains($word)) {
                throw ("Refusing to publish: the message of commit $((git log -1 --format=%h $old).Trim()) " +
                    "('$((git log -1 --format=%s $old).Trim())') names something private. Correct it " +
                    "without rewriting the private history: git replace the commit with one whose " +
                    "message does not, then run this again.")
            }
        }

        $body | Set-Content -NoNewline -Path $message

        $env:GIT_AUTHOR_NAME = (git log -1 --format=%an $old).Trim()
        $env:GIT_AUTHOR_EMAIL = $PublicEmail
        $env:GIT_AUTHOR_DATE = (git log -1 --format=%ad --date=raw $old).Trim()
        $env:GIT_COMMITTER_NAME = (git log -1 --format=%cn $old).Trim()
        $env:GIT_COMMITTER_EMAIL = $PublicEmail
        $env:GIT_COMMITTER_DATE = (git log -1 --format=%cd --date=raw $old).Trim()

        $map[$old] = (git commit-tree $tree @parents -F $message).Trim()
    }
}
finally {
    Remove-Item $message -ErrorAction SilentlyContinue
    'GIT_AUTHOR_NAME', 'GIT_AUTHOR_EMAIL', 'GIT_AUTHOR_DATE',
    'GIT_COMMITTER_NAME', 'GIT_COMMITTER_EMAIL', 'GIT_COMMITTER_DATE' |
        ForEach-Object { Remove-Item "Env:$_" -ErrorAction SilentlyContinue }
}

$commit = $map[$commit]

Write-Host "Public commit: $commit"
git log --format='%h %an <%ae> %s' -5 $commit

if ($DryRun) {
    Write-Host "Dry run: would push $commit to $Remote/$Branch."
    return
}

git push $Remote "${commit}:refs/heads/$Branch"
Write-Host "Published $Prefix to $Url ($Branch)."
