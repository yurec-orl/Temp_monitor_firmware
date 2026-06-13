#!/usr/bin/env pwsh
<#
.SYNOPSIS
    ESP32 Temperature Logger - Log Manager CLI wrapper

.DESCRIPTION
    Activates the project virtual environment and runs esp32_log_manager.py.
    All arguments are forwarded directly to the Python script.

.EXAMPLE
    # Show help
    .\run_log_manager.ps1 --help

    # List log files (auto-detect port)
    .\run_log_manager.ps1 list

    # List log files on a specific port
    .\run_log_manager.ps1 -p COM3 list

    # Download a single log file
    .\run_log_manager.ps1 -p COM3 get log_0001.csv

    # Download a single log file to a custom path
    .\run_log_manager.ps1 -p COM3 get log_0001.csv -o C:\Logs\my_log.csv

    # Download all log files to a directory
    .\run_log_manager.ps1 -p COM3 get-all -d C:\Logs

    # Get system status
    .\run_log_manager.ps1 -p COM3 status

    # Delete a specific log file
    .\run_log_manager.ps1 -p COM3 delete log_0001.csv

    # Delete all log files
    .\run_log_manager.ps1 -p COM3 delete *
#>

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$VenvPython = Join-Path $ScriptDir ".venv\Scripts\python.exe"
$PythonScript = Join-Path $ScriptDir "esp32_log_manager.py"

# Verify venv exists
if (-not (Test-Path $VenvPython)) {
    Write-Error "Virtual environment not found at: $VenvPython"
    Write-Host "Create it by running:"
    Write-Host "  python -m venv .venv"
    Write-Host "  .\.venv\Scripts\Activate.ps1"
    Write-Host "  pip install pyserial"
    exit 1
}

# Verify the Python script exists
if (-not (Test-Path $PythonScript)) {
    Write-Error "Script not found: $PythonScript"
    exit 1
}

# Run with all forwarded arguments
& $VenvPython $PythonScript @args
exit $LASTEXITCODE
