param (
    [string]$kind = $(throw "-kind is required"),
    [string]$pathPrefix = $(throw "-pathPrefix is required"),
    [string]$cygwinDll = "",
    [string]$arch = "",
    [string]$make = "make"
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
    $hash = ,""
    checkCmd { $hash[0] = (git rev-parse HEAD) }
    [IO.File]::WriteAllLines([string]$pwd + "\BUILD_INFO.txt", (
        "VERSION_SUFFIX=",
        ("COMMIT_HASH=" + $hash[0])
    ))
    $packageName = "winpty-$(Get-Content VERSION.txt)-${kind}"
    if ($cygwinDll) {
        $packageName += "-" + [System.Diagnostics.FileVersionInfo]::GetVersionInfo($cygwinDll).FileVersion
    }
    if ($arch) {
        $packageName += "-${arch}"
    }
    $parallel = "-j" + (Get-WmiObject -Class Win32_ComputerSystem).NumberOfLogicalProcessors
    checkCmd { sh -c "./configure" }
    checkCmd { & $make clean }
    checkCmd { & $make $parallel all tests }
    checkCmd { & $make install PREFIX=ship/packages/$packageName }
    checkCmd { build\trivial_test.exe }
    cd ship\packages
    checkCmd { tar cvfz "${packageName}.tar.gz" $packageName }
    cd ..\..
    rmdir -r ship\packages\$packageName
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
