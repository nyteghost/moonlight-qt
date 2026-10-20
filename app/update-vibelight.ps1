# Vibelight updater. Vibelight starts this script when you choose "Install" after an update
# has been downloaded, then closes. The script waits for Vibelight to exit, unpacks the
# downloaded portable zip, copies the new files over the app folder, and starts Vibelight again.
#
# Safety rules:
#   - Nothing is deleted from the app folder. Only files that are in the zip are replaced, so
#     your settings and anything else you keep there are left alone.
#   - portable.dat is never created or changed.
#   - Every file that is replaced is first copied to <app folder>\.update-backup.
#   - If anything fails part-way, the replaced files are restored and the files that were
#     added are removed, then the old version is started again.
#
# Exit codes: 0 updated, 2 Vibelight did not close, 3 the zip could not be unpacked,
# 4 the zip has no Vibelight.exe, 5 copy failed and was rolled back, 6 copy failed and the
# rollback was incomplete, 1 unexpected error. The log is written to %TEMP%\vibelight-update.log.
#
# This file must stay plain ASCII (Windows PowerShell 5.1 reads BOM-less files as ANSI).

param(
    [Parameter(Mandatory = $true)][string]$AppDir,
    [Parameter(Mandatory = $true)][string]$ZipPath,
    [int]$ProcessId = 0,
    [string]$ExeName = 'Vibelight.exe',
    [switch]$NoLaunch,
    [string]$LogPath = (Join-Path ([IO.Path]::GetTempPath()) 'vibelight-update.log')
)

$ErrorActionPreference = 'Stop'

function Write-Log([string]$Message) {
    $line = '{0}  {1}' -f (Get-Date -Format 's'), $Message
    try { Add-Content -LiteralPath $LogPath -Value $line -Encoding UTF8 } catch { }
}

function Start-Vibelight {
    if ($NoLaunch) { return }
    try {
        Start-Process -FilePath (Join-Path $AppDir $ExeName) -WorkingDirectory $AppDir
    } catch {
        Write-Log ("Could not start Vibelight: " + $_)
    }
}

function Remove-Quietly([string]$Path) {
    try {
        if (Test-Path -LiteralPath $Path) { Remove-Item -LiteralPath $Path -Recurse -Force -ErrorAction SilentlyContinue }
    } catch { }
}

$staging = $null

try {
    Write-Log ("Update started. AppDir=" + $AppDir + " Zip=" + $ZipPath + " ProcessId=" + $ProcessId)

    if (-not (Test-Path -LiteralPath $AppDir -PathType Container)) {
        Write-Log "App folder does not exist"
        exit 1
    }
    $AppDir = (Resolve-Path -LiteralPath $AppDir).Path.TrimEnd('\')

    if (-not (Test-Path -LiteralPath $ZipPath -PathType Leaf)) {
        Write-Log "The update zip does not exist"
        exit 3
    }

    # 1. Wait for Vibelight to close, so its files are not in use
    if ($ProcessId -gt 0) {
        $running = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
        if ($running) {
            if (-not $running.WaitForExit(60000)) {
                Write-Log "Vibelight did not close within 60 seconds. Nothing was changed."
                exit 2
            }
        }
        Start-Sleep -Milliseconds 500
    }

    # 2. Unpack the zip somewhere temporary. If it is damaged, the app is still untouched.
    $staging = Join-Path ([IO.Path]::GetTempPath()) ('vibelight-update-' + [Guid]::NewGuid().ToString('N'))
    try {
        Expand-Archive -LiteralPath $ZipPath -DestinationPath $staging -Force
    } catch {
        Write-Log ("Could not unpack the zip: " + $_)
        Remove-Quietly $staging
        Start-Vibelight
        exit 3
    }

    # The zip normally holds the files directly, but accept a single top-level folder too
    $root = $staging
    if (-not (Test-Path -LiteralPath (Join-Path $root $ExeName) -PathType Leaf)) {
        $dirs = @(Get-ChildItem -LiteralPath $staging -Directory)
        if ($dirs.Count -eq 1 -and (Test-Path -LiteralPath (Join-Path $dirs[0].FullName $ExeName) -PathType Leaf)) {
            $root = $dirs[0].FullName
        } else {
            Write-Log ("The zip does not contain " + $ExeName + ". Nothing was changed.")
            Remove-Quietly $staging
            Start-Vibelight
            exit 4
        }
    }

    # 3. Copy over the app folder, keeping a backup of everything that gets replaced
    $backup = Join-Path $AppDir '.update-backup'
    Remove-Quietly $backup
    New-Item -ItemType Directory -Path $backup -Force | Out-Null

    $rootPrefixLength = $root.TrimEnd('\').Length + 1
    $added = New-Object System.Collections.ArrayList
    $replaced = New-Object System.Collections.ArrayList

    try {
        foreach ($file in (Get-ChildItem -LiteralPath $root -Recurse -File)) {
            $relative = $file.FullName.Substring($rootPrefixLength)

            # Never switch someone into (or out of) portable mode
            if ($relative -ieq 'portable.dat') { continue }

            $destination = Join-Path $AppDir $relative
            $destinationDir = Split-Path -Parent $destination
            if (-not (Test-Path -LiteralPath $destinationDir)) {
                New-Item -ItemType Directory -Path $destinationDir -Force | Out-Null
            }

            if (Test-Path -LiteralPath $destination -PathType Leaf) {
                $backupFile = Join-Path $backup $relative
                $backupDir = Split-Path -Parent $backupFile
                if (-not (Test-Path -LiteralPath $backupDir)) {
                    New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
                }
                Copy-Item -LiteralPath $destination -Destination $backupFile -Force
                [void]$replaced.Add($relative)
            } else {
                [void]$added.Add($relative)
            }

            Copy-Item -LiteralPath $file.FullName -Destination $destination -Force
        }
    } catch {
        Write-Log ("Copying failed: " + $_ + ". Rolling back.")
        $rollbackComplete = $true

        foreach ($relative in $added) {
            try { Remove-Item -LiteralPath (Join-Path $AppDir $relative) -Force -ErrorAction SilentlyContinue } catch { }
        }
        foreach ($relative in $replaced) {
            try {
                Copy-Item -LiteralPath (Join-Path $backup $relative) -Destination (Join-Path $AppDir $relative) -Force
            } catch {
                $rollbackComplete = $false
                Write-Log ("Could not restore " + $relative + ": " + $_)
            }
        }

        Remove-Quietly $staging
        Start-Vibelight
        if ($rollbackComplete) {
            Write-Log "Rolled back. The old version is unchanged."
            exit 5
        }
        Write-Log "Rollback incomplete. The previous files are in .update-backup."
        exit 6
    }

    Remove-Quietly $staging
    Write-Log ("Update finished. " + $replaced.Count + " files replaced, " + $added.Count + " added.")
    Start-Vibelight
    exit 0
} catch {
    Write-Log ("Unexpected error: " + $_)
    if ($staging) { Remove-Quietly $staging }
    Start-Vibelight
    exit 1
}
