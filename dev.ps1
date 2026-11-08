param(
    [ValidateSet('build', 'flash', 'monitor', 'menuconfig', 'size', 'clean', 'ports', 'backup', 'demo-build', 'demo-flash')]
    [string]$Action = 'build',
    [string]$Port = '',
    # SDK board profile: devices/sdkconfig.muse-<Board>
    [string]$Board = 'm5stack-stopwatch'
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$python = Join-Path $root '.venv\Scripts\python.exe'
$idfDir = Join-Path $root '.tools\esp-idf-v6.0.3'
$fwDir = Join-Path $root 'muse-gadget-sdk\esp32'
$buildDir = "build-muse-$Board"

function Require-Port { if (-not $Port) { throw 'Specify the device port, for example: -Port COM5' } }

function Use-Idf {
    if (-not (Test-Path -LiteralPath (Join-Path $idfDir 'export.ps1'))) { throw 'ESP-IDF 6.0.3 is missing under .tools/.' }
    $env:IDF_TOOLS_PATH = Join-Path $root '.tools\espressif'
    . (Join-Path $idfDir 'export.ps1') *> $null
}

function Invoke-Idf([string[]]$IdfArgs) {
    Use-Idf
    $defaults = "sdkconfig.defaults;devices/sdkconfig.muse;devices/sdkconfig.muse-$Board;devices/sdkconfig.musestopwatch"
    Push-Location $fwDir
    try {
        # Our own components live one level down (components/muse_diy_stopwatch/*).
        idf.py -B $buildDir -DIDF_TARGET=esp32s3 "-DSDKCONFIG=$buildDir/sdkconfig" "-DSDKCONFIG_DEFAULTS=$defaults" `
            "-DEXTRA_COMPONENT_DIRS=components/muse_diy_stopwatch" @IdfArgs
    } finally { Pop-Location }
}

# Windows PowerShell 5.1 turns any stderr line of a native tool into an error;
# with 'Stop' that aborts the build. Rely on $LASTEXITCODE instead.
$ErrorActionPreference = 'Continue'

switch ($Action) {
    # Muse firmware (fork of the official SDK), ESP-IDF 6.0.3
    'build' { Invoke-Idf @('build') }
    'size' { Invoke-Idf @('size') }
    'menuconfig' { Invoke-Idf @('menuconfig') }
    'clean' { Invoke-Idf @('fullclean') }
    'flash' { Require-Port; Invoke-Idf @('-p', $Port, 'flash') }
    'monitor' { Require-Port; Invoke-Idf @('-p', $Port, 'monitor') }
    'ports' { & $python -m serial.tools.list_ports -v }
    'backup' {
        # Full 16MB flash read. Reads only; never erases or writes.
        Require-Port
        $dir = Join-Path $root 'backups'
        New-Item -ItemType Directory -Force $dir | Out-Null
        $file = Join-Path $dir ("flash-{0}-{1}.bin" -f $Port, (Get-Date -Format 'yyyyMMdd-HHmmss'))
        & $python -m esptool --chip esp32s3 --port $Port read_flash 0 0x1000000 $file
        if ($LASTEXITCODE -eq 0) { (Get-FileHash -Algorithm SHA256 $file).Hash + "  " + $file }
    }
    # Factory demo (M5StopWatch-UserDemo), PlatformIO + ESP-IDF 5.5.5, for restoring the factory firmware
    'demo-build' { & $python -X utf8 (Join-Path $root 'tools\pio.py') run -d (Join-Path $root 'upstream\M5StopWatch-UserDemo') -e m5stack-stopwatch }
    'demo-flash' {
        Require-Port
        & $python -X utf8 (Join-Path $root 'tools\pio.py') run -d (Join-Path $root 'upstream\M5StopWatch-UserDemo') -e m5stack-stopwatch -t upload --upload-port $Port
    }
}
exit $LASTEXITCODE
