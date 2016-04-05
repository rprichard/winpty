param (
    [string]$pathPrefix = $(throw "-pathPrefix is required")
)

$ErrorActionPreference = "Stop"

function checkCmd([scriptblock]$cmd, [string]$errorMessage = "Error executing command: " + $cmd) {
    echo ("+" + $cmd)
    & $cmd
    if ($LastExitCode -ne 0) {
        throw $errorMessage
    }
}

function doBuild() {
    $parallel = "-j" + (Get-WmiObject -Class Win32_ComputerSystem).NumberOfLogicalProcessors
    checkCmd { sh -c "./configure" }
    checkCmd { & make clean }
    checkCmd { & make $parallel USE_PCH=0 all tests }
    checkCmd { build\trivial_test.exe }
}

# Save and restore the Path and CWD so we don't propagate them to a parent
# Powershell environment.
$oldPath = $env:Path
$oldCwd = Get-Location
try {
    $env:Path = $pathPrefix + ";" + $env:Path
    doBuild
} finally {
    $env:Path = $oldPath
    Set-Location $oldCwd
}
