[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$Configuration = "RelWithDebInfo",

    [ValidateSet("x64", "ARM64")]
    [string]$Architecture = $(if ($env:PROCESSOR_ARCHITECTURE -match "ARM64" -or $env:PROCESSOR_ARCHITEW6432 -match "ARM64") { "ARM64" } else { "x64" }),

    [ValidateSet("VisualStudio", "Ninja")]
    [string]$Generator = "VisualStudio",

    [switch]$Clean,
    [switch]$SkipDependencies,
    [switch]$Tests,
    [switch]$ConfigureOnly,
    [switch]$NoRun
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildSuffix = if ($Configuration -eq "Release") { "release" } elseif ($Configuration -eq "Debug") { "debug" } else { "debuginfo" }
$BuildDir = Join-Path $Root "build-windows-$BuildSuffix-$($Architecture.ToLowerInvariant())"
$DepsDir = Join-Path $Root "deps/build-windows-$BuildSuffix-$($Architecture.ToLowerInvariant())"

function Assert-Tool {
    param([Parameter(Mandatory = $true)][string]$Name)
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -eq $command) {
        throw "Ferramenta '$Name' não encontrada no PATH. Instale CMake e o Visual Studio Build Tools, ou use -Generator Ninja com Ninja instalado."
    }
    return $command.Source
}

function Invoke-Native {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [string]$WorkingDirectory = $Root
    )

    Write-Host "`n> $FilePath $($Arguments -join ' ')" -ForegroundColor DarkCyan
    Push-Location $WorkingDirectory
    try {
        & $FilePath @Arguments
        if ($LASTEXITCODE -ne 0) {
            throw "O comando falhou com o código $LASTEXITCODE."
        }
    }
    finally {
        Pop-Location
    }
}

function Get-VisualStudioGenerator {
    $msbuild = Get-Command msbuild -ErrorAction SilentlyContinue
    if ($null -eq $msbuild) {
        throw "MSBuild não encontrado. Abra este script em um Developer PowerShell do Visual Studio ou use -Generator Ninja."
    }

    $versionOutput = @(& $msbuild.Source -version 2>&1)
    $versionLine = $versionOutput | Where-Object { $_ -match "\d+\.\d+\.\d+" } | Select-Object -First 1
    $major = 0
    if ($versionLine -match "(\d+)\.\d+") {
        $major = [int]$Matches[1]
    }

    switch ($major) {
        18 { return "Visual Studio 18 2026" }
        17 { return "Visual Studio 17 2022" }
        16 { return "Visual Studio 16 2019" }
        default { throw "Versão de MSBuild não suportada: $versionLine. Use Visual Studio 2019, 2022 ou 2026." }
    }
}

$cmake = Assert-Tool "cmake"
if ($Generator -eq "Ninja") {
    $ninja = Assert-Tool "ninja"
    $CMakeGenerator = "Ninja Multi-Config"
    $GeneratorArguments = @("-G", $CMakeGenerator)
}
else {
    $CMakeGenerator = Get-VisualStudioGenerator
    $GeneratorArguments = @("-G", $CMakeGenerator, "-A", $Architecture)
}

Write-Host "CRUX3D build" -ForegroundColor Cyan
Write-Host "  Configuração : $Configuration"
Write-Host "  Arquitetura  : $Architecture"
Write-Host "  Gerador      : $CMakeGenerator"
Write-Host "  Build        : $BuildDir"

if ($Clean) {
    Write-Host "Removendo árvores de build anteriores..." -ForegroundColor Yellow
    if (Test-Path $BuildDir) { Remove-Item -Recurse -Force $BuildDir }
    if (-not $SkipDependencies -and (Test-Path $DepsDir)) { Remove-Item -Recurse -Force $DepsDir }
}

$env:CMAKE_POLICY_VERSION_MINIMUM = "3.5"

if (-not $SkipDependencies) {
    New-Item -ItemType Directory -Force -Path $DepsDir | Out-Null
    $depsConfigure = @("-S", (Join-Path $Root "deps"), "-B", $DepsDir) + $GeneratorArguments + @("-DCMAKE_BUILD_TYPE=$Configuration")
    Invoke-Native $cmake $depsConfigure
    Invoke-Native $cmake @("--build", $DepsDir, "--config", $Configuration, "--target", "deps", "--parallel")
}
else {
    Write-Host "Dependências: etapa ignorada por -SkipDependencies." -ForegroundColor Yellow
}

$buildConfigure = @(
    "-S", $Root,
    "-B", $BuildDir
) + $GeneratorArguments + @(
    "-DORCA_TOOLS=ON",
    "-DBUILD_TESTS=$(if ($Tests) { 'ON' } else { 'OFF' })",
    "-DCMAKE_BUILD_TYPE=$Configuration"
)
Invoke-Native $cmake $buildConfigure

if (-not $ConfigureOnly) {
    Invoke-Native $cmake @("--build", $BuildDir, "--config", $Configuration, "--target", "ALL_BUILD", "--parallel")
    Invoke-Native $cmake @("--build", $BuildDir, "--config", $Configuration, "--target", "install")
}
else {
    Write-Host "Configuração concluída por -ConfigureOnly; a compilação foi omitida." -ForegroundColor Yellow
}

if (-not $ConfigureOnly -and -not $NoRun) {
    $candidateExecutables = @(
        (Join-Path $BuildDir "$Configuration/orca-slicer.exe"),
        (Join-Path $BuildDir "$Configuration/OrcaSlicer_app_gui.exe"),
        (Join-Path $BuildDir "src/$Configuration/orca-slicer.exe"),
        (Join-Path $BuildDir "src/$Configuration/OrcaSlicer_app_gui.exe"),
        (Join-Path $BuildDir "orca-slicer.exe")
    )
    $executable = $candidateExecutables | Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($null -eq $executable) {
        throw "Build concluído, mas o executável não foi encontrado. Verifique a pasta: $BuildDir"
    }

    Write-Host "`nIniciando: $executable" -ForegroundColor Green
    Start-Process -FilePath $executable -WorkingDirectory (Split-Path -Parent $executable)
}
