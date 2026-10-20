# Tests app/update-vibelight.ps1 against throwaway folders. Run on Windows:
#   powershell -NoProfile -ExecutionPolicy Bypass -File tests/test-updater.ps1
$ErrorActionPreference = 'Stop'
$script = (Resolve-Path (Join-Path $PSScriptRoot '..\app\update-vibelight.ps1')).Path
$base = Join-Path ([IO.Path]::GetTempPath()) ('vl-updater-test-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $base | Out-Null
$failed = 0

function Check([bool]$Condition, [string]$Name) {
    if ($Condition) { Write-Host "PASS  $Name" } else { Write-Host "FAIL  $Name"; $script:failed++ }
}

function New-App([string]$Name) {
    $dir = Join-Path $base $Name
    New-Item -ItemType Directory -Path $dir | Out-Null
    Set-Content -LiteralPath (Join-Path $dir 'Vibelight.exe') -Value 'OLD' -NoNewline
    Set-Content -LiteralPath (Join-Path $dir 'lib.dll') -Value 'OLD' -NoNewline
    Set-Content -LiteralPath (Join-Path $dir 'settings.keep') -Value 'KEEP' -NoNewline
    Set-Content -LiteralPath (Join-Path $dir 'portable.dat') -Value 'P' -NoNewline
    return $dir
}

function New-Zip([string]$Name, [string[]]$Files, [string]$Prefix = '') {
    $src = Join-Path $base ($Name + '-src')
    New-Item -ItemType Directory -Path $src | Out-Null
    $inner = $src
    if ($Prefix) { $inner = Join-Path $src $Prefix; New-Item -ItemType Directory -Path $inner | Out-Null }
    foreach ($f in $Files) { Set-Content -LiteralPath (Join-Path $inner $f) -Value 'NEW' -NoNewline }
    $zip = Join-Path $base ($Name + '.zip')
    Compress-Archive -Path (Join-Path $src '*') -DestinationPath $zip
    return $zip
}

function Run-Updater([string]$App, [string]$Zip, [int]$ProcId = 0) {
    $log = Join-Path $base 'log.txt'
    $p = Start-Process -FilePath 'powershell.exe' -Wait -PassThru -WindowStyle Hidden -ArgumentList @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $script,
        '-AppDir', $App, '-ZipPath', $Zip, '-ProcessId', $ProcId, '-NoLaunch', '-LogPath', $log)
    return $p.ExitCode
}

function Read-Text([string]$Path) { return [IO.File]::ReadAllText($Path) }

# 1. Normal update: replaces, adds, keeps other files, never touches portable.dat
$app = New-App 'a1'
$zip = New-Zip 'z1' @('Vibelight.exe', 'lib.dll', 'extra.dll', 'portable.dat')
$code = Run-Updater $app $zip
Check ($code -eq 0) 'update succeeds (exit 0)'
Check ((Read-Text (Join-Path $app 'Vibelight.exe')) -eq 'NEW') 'exe replaced'
Check ((Read-Text (Join-Path $app 'extra.dll')) -eq 'NEW') 'new file added'
Check ((Read-Text (Join-Path $app 'settings.keep')) -eq 'KEEP') 'unrelated file kept'
Check ((Read-Text (Join-Path $app 'portable.dat')) -eq 'P') 'portable.dat untouched'
Check ((Read-Text (Join-Path $app '.update-backup\Vibelight.exe')) -eq 'OLD') 'old exe backed up'

# 2. Zip with a single top-level folder
$app = New-App 'a2'
$zip = New-Zip 'z2' @('Vibelight.exe', 'lib.dll') 'Vibelight'
$code = Run-Updater $app $zip
Check ($code -eq 0) 'top-level folder zip succeeds'
Check ((Read-Text (Join-Path $app 'Vibelight.exe')) -eq 'NEW') 'exe replaced from nested zip'
Check (-not (Test-Path (Join-Path $app 'Vibelight'))) 'no nested folder created'

# 3. No portable.dat created when the app did not have one
$app = New-App 'a3'
Remove-Item (Join-Path $app 'portable.dat')
$zip = New-Zip 'z3' @('Vibelight.exe', 'portable.dat')
$code = Run-Updater $app $zip
Check ($code -eq 0) 'update without portable.dat succeeds'
Check (-not (Test-Path (Join-Path $app 'portable.dat'))) 'portable.dat not created'

# 4. Corrupt zip: untouched
$app = New-App 'a4'
$zip = Join-Path $base 'bad.zip'
Set-Content -LiteralPath $zip -Value 'this is not a zip'
$code = Run-Updater $app $zip
Check ($code -eq 3) 'corrupt zip gives exit 3'
Check ((Read-Text (Join-Path $app 'Vibelight.exe')) -eq 'OLD') 'app untouched after corrupt zip'

# 5. Zip without the exe: untouched
$app = New-App 'a5'
$zip = New-Zip 'z5' @('lib.dll', 'other.dll')
$code = Run-Updater $app $zip
Check ($code -eq 4) 'zip without exe gives exit 4'
Check ((Read-Text (Join-Path $app 'lib.dll')) -eq 'OLD') 'app untouched after zip without exe'

# 6. A locked file makes the copy fail: everything is rolled back
$app = New-App 'a6'
$zip = New-Zip 'z6' @('Vibelight.exe', 'aaa-added.dll', 'lib.dll')
$lock = [IO.File]::Open((Join-Path $app 'lib.dll'), 'Open', 'Read', 'None')
try { $code = Run-Updater $app $zip } finally { $lock.Close() }
Check ($code -eq 5) 'locked file gives exit 5 (rolled back)'
Check ((Read-Text (Join-Path $app 'Vibelight.exe')) -eq 'OLD') 'exe restored after rollback'
Check ((Read-Text (Join-Path $app 'lib.dll')) -eq 'OLD') 'locked file unchanged'
Check (-not (Test-Path (Join-Path $app 'aaa-added.dll'))) 'added file removed by rollback'

# 7. Waits for the app process to exit
$app = New-App 'a7'
$zip = New-Zip 'z7' @('Vibelight.exe')
$sleeper = Start-Process -FilePath 'powershell.exe' -PassThru -WindowStyle Hidden -ArgumentList @('-NoProfile', '-Command', 'Start-Sleep -Seconds 4')
$watch = [Diagnostics.Stopwatch]::StartNew()
$code = Run-Updater $app $zip $sleeper.Id
$watch.Stop()
Check ($code -eq 0) 'update after waiting for process succeeds'
Check ($watch.Elapsed.TotalSeconds -ge 2) 'updater waited for the process'

Remove-Item -LiteralPath $base -Recurse -Force -ErrorAction SilentlyContinue
if ($failed -gt 0) { Write-Host "$failed check(s) failed"; exit 1 }
Write-Host 'All updater checks passed'
