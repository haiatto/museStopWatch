param(
    [ValidateSet('build', 'upload', 'ports', 'monitor', 'backup', 'clean')]
    [string]$Action = 'build',
    [string]$Port = '',
    # Project folder that holds platformio.ini (relative to this script).
    [string]$Project = 'upstream\M5StopWatch-UserDemo',
    [string]$Environment = 'm5stack-stopwatch'
)
$ErrorActionPreference = 'Stop'
$python = Join-Path $PSScriptRoot '.venv\Scripts\python.exe'
$runner = Join-Path $PSScriptRoot 'tools\pio.py'
$projectDir = Join-Path $PSScriptRoot $Project
if (-not (Test-Path -LiteralPath $python)) { throw 'Workspace Python environment (.venv) is missing.' }

function Require-Port { if (-not $Port) { throw 'Specify the device port, for example: -Port COM5' } }
# Windows PowerShell 5.1 turns any stderr line of a native tool into an error;
# with 'Stop' that aborts the build. Rely on $LASTEXITCODE instead.
$ErrorActionPreference = 'Continue'

switch ($Action) {
    'build' { & $python -X utf8 $runner run -d $projectDir -e $Environment }
    'clean' { & $python -X utf8 $runner run -d $projectDir -e $Environment -t clean }
    'upload' {
        Require-Port
        & $python -X utf8 $runner run -d $projectDir -e $Environment -t upload --upload-port $Port
    }
    'ports' { & $python -X utf8 $runner device list }
    'monitor' {
        Require-Port
        & $python -X utf8 $runner device monitor -d $projectDir -e $Environment --port $Port
    }
    'backup' {
        # Full 16MB flash read. Reads only; never erases or writes.
        Require-Port
        $dir = Join-Path $PSScriptRoot 'backups'
        New-Item -ItemType Directory -Force $dir | Out-Null
        $file = Join-Path $dir ("flash-{0}-{1}.bin" -f $Port, (Get-Date -Format 'yyyyMMdd-HHmmss'))
        & $python -m esptool --chip esp32s3 --port $Port read_flash 0 0x1000000 $file
        if ($LASTEXITCODE -eq 0) { (Get-FileHash -Algorithm SHA256 $file).Hash + "  " + $file }
    }
}
exit $LASTEXITCODE
