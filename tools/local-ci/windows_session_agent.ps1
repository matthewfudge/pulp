param(
    [string]$RemoteRoot = '%LOCALAPPDATA%\Pulp\desktop-automation-agent',
    [int]$IdleExitSecs = 5,
    [int]$PollIntervalMs = 500
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Expand-ConfiguredPath {
    param([string]$RawPath)
    return [Environment]::ExpandEnvironmentVariables($RawPath)
}

function Ensure-Directory {
    param([string]$Path)
    if (-not (Test-Path $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

function Get-IsoTimestamp {
    return [DateTimeOffset]::Now.ToString("o")
}

function Write-AgentLog {
    param([string]$Message)
    $line = "[{0}] {1}" -f (Get-IsoTimestamp), $Message
    Add-Content -LiteralPath $script:AgentLogPath -Value $line
}

function Wait-ForRequiredFiles {
    param(
        [string[]]$Paths,
        [double]$TimeoutSecs
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSecs)
    while ([DateTime]::UtcNow -lt $deadline) {
        $missing = @()
        foreach ($path in $Paths) {
            if (-not (Test-Path $path)) {
                $missing += $path
            }
        }
        if ($missing.Count -eq 0) {
            return
        }
        Start-Sleep -Milliseconds 100
    }
    throw "Timed out waiting for required artifacts: $($Paths -join ', ')"
}

function Get-DirectLaunchCommandParts {
    param([string]$Command)

    $trimmed = $Command.Trim()
    if ([string]::IsNullOrWhiteSpace($trimmed)) {
        return $null
    }
    if ($trimmed -match '[|&<>]') {
        return $null
    }
    if ($trimmed.StartsWith('"')) {
        $closing = $trimmed.IndexOf('"', 1)
        if ($closing -lt 1) {
            return $null
        }
        $file = $trimmed.Substring(1, $closing - 1)
        $args = $trimmed.Substring($closing + 1).Trim()
        return @{ file = $file; args = $args }
    }
    $space = $trimmed.IndexOf(' ')
    if ($space -lt 0) {
        return @{ file = $trimmed; args = '' }
    }
    return @{ file = $trimmed.Substring(0, $space); args = $trimmed.Substring($space + 1).Trim() }
}

function New-ProcessStartInfo {
    param(
        [pscustomobject]$Request,
        [string]$StdoutPath,
        [string]$StderrPath
    )

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $directLaunch = $null
    if ($Request.execution.capture_mode -eq 'window-capture') {
        $directLaunch = Get-DirectLaunchCommandParts -Command ([string]$Request.command)
    }
    if ($directLaunch) {
        $psi.FileName = $directLaunch.file
        $psi.Arguments = $directLaunch.args
    } else {
        $psi.FileName = 'cmd.exe'
        $psi.Arguments = '/c ' + $Request.command + ' 1> "' + $StdoutPath + '" 2> "' + $StderrPath + '"'
    }
    $psi.WorkingDirectory = $Request.cwd
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true

    if ($Request.PSObject.Properties.Name -contains 'env' -and $Request.env) {
        foreach ($property in $Request.env.PSObject.Properties) {
            $psi.EnvironmentVariables[$property.Name] = [string]$property.Value
        }
    }

    return $psi
}

function Ensure-WindowsDesktopApi {
    if (-not ('PulpDesktopAutomationNative' -as [type])) {
        Add-Type -AssemblyName System.Drawing
        Add-Type -AssemblyName System.Windows.Forms
        Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class PulpDesktopAutomationNative {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern int GetWindowTextLength(IntPtr hWnd);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder text, int count);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int cmd);

    [DllImport("user32.dll")]
    public static extern bool SetCursorPos(int x, int y);

    [DllImport("user32.dll")]
    public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extra);

    [DllImport("user32.dll")]
    public static extern bool PostMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);

    public const int SW_RESTORE = 9;
    public const uint MOUSEEVENTF_LEFTDOWN = 0x0002;
    public const uint MOUSEEVENTF_LEFTUP = 0x0004;
    public const uint WM_CLOSE = 0x0010;
}
"@
    }
}

function Get-DesktopWindows {
    Ensure-WindowsDesktopApi
    $windows = New-Object System.Collections.ArrayList
    $callback = [PulpDesktopAutomationNative+EnumWindowsProc]{
        param([IntPtr]$hWnd, [IntPtr]$lParam)
        if (-not [PulpDesktopAutomationNative]::IsWindowVisible($hWnd)) {
            return $true
        }
        $length = [PulpDesktopAutomationNative]::GetWindowTextLength($hWnd)
        if ($length -le 0) {
            return $true
        }
        $builder = New-Object System.Text.StringBuilder ($length + 1)
        [void][PulpDesktopAutomationNative]::GetWindowText($hWnd, $builder, $builder.Capacity)
        $title = $builder.ToString().Trim()
        if ([string]::IsNullOrWhiteSpace($title)) {
            return $true
        }
        $rect = New-Object 'PulpDesktopAutomationNative+RECT'
        if (-not [PulpDesktopAutomationNative]::GetWindowRect($hWnd, [ref]$rect)) {
            return $true
        }
        $width = $rect.Right - $rect.Left
        $height = $rect.Bottom - $rect.Top
        if ($width -le 0 -or $height -le 0) {
            return $true
        }
        [uint32]$windowPid = 0
        [void][PulpDesktopAutomationNative]::GetWindowThreadProcessId($hWnd, [ref]$windowPid)
        [void]$windows.Add([pscustomobject]@{
            handle = [int64]$hWnd.ToInt64()
            process_id = [int]$windowPid
            title = $title
            bounds = [ordered]@{
                left = $rect.Left
                top = $rect.Top
                right = $rect.Right
                bottom = $rect.Bottom
                width = $width
                height = $height
            }
        })
        return $true
    }
    [void][PulpDesktopAutomationNative]::EnumWindows($callback, [IntPtr]::Zero)
    return @($windows)
}

function Get-DesktopWindowByHandle {
    param([long]$Handle)
    return Get-DesktopWindows | Where-Object { $_.handle -eq $Handle } | Select-Object -First 1
}

function Focus-DesktopWindow {
    param([pscustomobject]$Window)
    Ensure-WindowsDesktopApi
    $handle = [IntPtr]$Window.handle
    [void][PulpDesktopAutomationNative]::ShowWindow($handle, [PulpDesktopAutomationNative]::SW_RESTORE)
    [void][PulpDesktopAutomationNative]::SetForegroundWindow($handle)
    Start-Sleep -Milliseconds 150
}

function Save-WindowScreenshot {
    param([pscustomobject]$Window, [string]$Path)
    Ensure-WindowsDesktopApi
    Ensure-Directory (Split-Path -Parent $Path)
    $width = [int]$Window.bounds.width
    $height = [int]$Window.bounds.height
    $bitmap = New-Object System.Drawing.Bitmap $width, $height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CopyFromScreen([int]$Window.bounds.left, [int]$Window.bounds.top, 0, 0, $bitmap.Size)
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function Invoke-WindowClick {
    param([pscustomobject]$Window, [string]$PointSpec)
    Ensure-WindowsDesktopApi
    if ($PointSpec -notmatch '^\s*(-?\d+)\s*,\s*(-?\d+)\s*$') {
        throw "Invalid click point: $PointSpec"
    }
    $x = [int]$Matches[1]
    $y = [int]$Matches[2]
    $screenX = [int]$Window.bounds.left + $x
    $screenY = [int]$Window.bounds.top + $y
    Focus-DesktopWindow -Window $Window
    [void][PulpDesktopAutomationNative]::SetCursorPos($screenX, $screenY)
    Start-Sleep -Milliseconds 50
    [PulpDesktopAutomationNative]::mouse_event([PulpDesktopAutomationNative]::MOUSEEVENTF_LEFTDOWN, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 50
    [PulpDesktopAutomationNative]::mouse_event([PulpDesktopAutomationNative]::MOUSEEVENTF_LEFTUP, 0, 0, 0, [UIntPtr]::Zero)
    return @{ point = @{ x = $x; y = $y }; screen_point = @{ x = $screenX; y = $screenY } }
}

function Wait-ForNewDesktopWindow {
    param([long[]]$KnownHandles, [double]$TimeoutSecs)
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSecs)
    while ([DateTime]::UtcNow -lt $deadline) {
        $candidate = Get-DesktopWindows | Where-Object { $KnownHandles -notcontains $_.handle } | Select-Object -First 1
        if ($candidate) {
            return $candidate
        }
        Start-Sleep -Milliseconds 200
    }
    throw 'Timed out waiting for a visible top-level window'
}

function Close-WindowCaptureTarget {
    param([pscustomobject]$Window)
    if (-not $Window) {
        return
    }
    Ensure-WindowsDesktopApi
    [void][PulpDesktopAutomationNative]::PostMessage([IntPtr]$Window.handle, [PulpDesktopAutomationNative]::WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 500
    if ($Window.process_id) {
        Stop-Process -Id $Window.process_id -Force -ErrorAction SilentlyContinue
    }
}

function Invoke-PulpAppRequest {
    param(
        [pscustomobject]$Request,
        [string]$ResultRoot,
        [string]$StdoutPath,
        [string]$StderrPath
    )

    $requiredArtifacts = @($Request.outputs.screenshot)
    if ($Request.execution.capture_before -and $Request.outputs.before_screenshot) {
        $requiredArtifacts += $Request.outputs.before_screenshot
    }
    if ($Request.execution.capture_ui_snapshot -and $Request.outputs.ui_snapshot) {
        $requiredArtifacts += $Request.outputs.ui_snapshot
    }

    $psi = New-ProcessStartInfo -Request $Request -StdoutPath $StdoutPath -StderrPath $StderrPath
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $psi
    $started = $process.Start()
    if (-not $started) {
        throw "Failed to start command: $($Request.command)"
    }
    $process.BeginOutputReadLine()
    $process.BeginErrorReadLine()

    try {
        Wait-ForRequiredFiles -Paths $requiredArtifacts -TimeoutSecs ([double]$Request.timeout_secs + [double]$Request.settle_secs + 2.0)
        [void]$process.WaitForExit([int](($Request.timeout_secs + 2.0) * 1000))
        return @{
            pid = $process.Id
            exit_code = if ($process.HasExited) { $process.ExitCode } else { $null }
            status = 'pass'
        }
    } finally {
        if (-not $process.HasExited) {
            & taskkill.exe /PID $process.Id /T /F | Out-Null
        }
    }
}

function Invoke-WindowCaptureRequest {
    param(
        [pscustomobject]$Request,
        [string]$ResultRoot,
        [string]$StdoutPath,
        [string]$StderrPath
    )

    $psi = New-ProcessStartInfo -Request $Request -StdoutPath $StdoutPath -StderrPath $StderrPath
    $knownHandles = @(Get-DesktopWindows | ForEach-Object { $_.handle })
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $psi
    if (-not $process.Start()) {
        throw "Failed to start command: $($Request.command)"
    }
    $process.BeginOutputReadLine()
    $process.BeginErrorReadLine()

    $window = $null
    try {
        $window = Wait-ForNewDesktopWindow -KnownHandles $knownHandles -TimeoutSecs ([double]$Request.timeout_secs + 5.0)
        Focus-DesktopWindow -Window $window
        if ($Request.execution.capture_before -and $Request.outputs.before_screenshot) {
            Save-WindowScreenshot -Window $window -Path (Expand-ConfiguredPath $Request.outputs.before_screenshot)
        }
        $interaction = @{ mode = 'window-capture' }
        if ($Request.interaction.click_point) {
            $interaction.click = Invoke-WindowClick -Window $window -PointSpec ([string]$Request.interaction.click_point)
        }
        if ([double]$Request.settle_secs -gt 0) {
            Start-Sleep -Milliseconds ([int]([double]$Request.settle_secs * 1000.0))
        }
        $window = Get-DesktopWindowByHandle -Handle ([long]$window.handle)
        if (-not $window) {
            throw 'Window disappeared before capture'
        }
        Save-WindowScreenshot -Window $window -Path (Expand-ConfiguredPath $Request.outputs.screenshot)
        return @{
            pid = if ($window.process_id) { $window.process_id } elseif ($process.Id) { $process.Id } else { $null }
            exit_code = if ($process.HasExited) { $process.ExitCode } else { $null }
            status = 'pass'
            window = $window
            interaction = $interaction
        }
    } finally {
        Close-WindowCaptureTarget -Window $window
        if (-not $process.HasExited) {
            & taskkill.exe /PID $process.Id /T /F | Out-Null
        }
    }
}

function Write-ResultManifest {
    param(
        [string]$ManifestPath,
        [hashtable]$Payload
    )

    $json = $Payload | ConvertTo-Json -Depth 8
    Set-Content -LiteralPath $ManifestPath -Value $json -Encoding UTF8
}

function Move-BadRequestFile {
    param(
        [System.IO.FileInfo]$RequestFile,
        [string]$Reason
    )

    $badRoot = Join-Path $LogsDir 'bad-jobs'
    Ensure-Directory $badRoot
    $suffix = '{0}-{1}' -f [DateTimeOffset]::Now.ToUnixTimeMilliseconds(), $RequestFile.Name
    $destination = Join-Path $badRoot $suffix
    Move-Item -LiteralPath $RequestFile.FullName -Destination $destination -Force
    Write-AgentLog "bad job quarantined path=$destination reason=$Reason"
}

function Process-RequestFile {
    param([System.IO.FileInfo]$RequestFile)

    $startedAt = Get-IsoTimestamp
    $requestText = Get-Content -LiteralPath $RequestFile.FullName -Raw
    try {
        $request = $requestText | ConvertFrom-Json
    } catch {
        Move-BadRequestFile -RequestFile $RequestFile -Reason "invalid json: $($_.Exception.Message)"
        return
    }
    if (-not $request.outputs) {
        Move-BadRequestFile -RequestFile $RequestFile -Reason 'missing outputs payload'
        return
    }
    if (-not $request.execution) {
        Move-BadRequestFile -RequestFile $RequestFile -Reason 'missing execution payload'
        return
    }
    $requiredOutputKeys = @('result_root', 'manifest', 'stdout', 'stderr', 'screenshot')
    foreach ($key in $requiredOutputKeys) {
        if (-not ($request.outputs.PSObject.Properties.Name -contains $key)) {
            Move-BadRequestFile -RequestFile $RequestFile -Reason "missing outputs.$key"
            return
        }
    }
    if (-not ($request.execution.PSObject.Properties.Name -contains 'capture_mode')) {
        Move-BadRequestFile -RequestFile $RequestFile -Reason 'missing execution.capture_mode'
        return
    }

    $resultRoot = Expand-ConfiguredPath $request.outputs.result_root
    $manifestPath = Expand-ConfiguredPath $request.outputs.manifest
    $stdoutPath = Expand-ConfiguredPath $request.outputs.stdout
    $stderrPath = Expand-ConfiguredPath $request.outputs.stderr
    $screenshotPath = Expand-ConfiguredPath $request.outputs.screenshot
    $beforePath = if ($request.outputs.PSObject.Properties.Name -contains 'before_screenshot') { Expand-ConfiguredPath $request.outputs.before_screenshot } else { $null }
    $uiSnapshotPath = if ($request.outputs.PSObject.Properties.Name -contains 'ui_snapshot') { Expand-ConfiguredPath $request.outputs.ui_snapshot } else { $null }

    Ensure-Directory $resultRoot
    Ensure-Directory (Join-Path $resultRoot 'screenshots')
    Move-Item -LiteralPath $RequestFile.FullName -Destination (Join-Path $resultRoot 'request.json') -Force

    $status = 'pass'
    $errorText = $null
    $processId = $null
    $exitCode = $null
    $window = $null
    $interaction = $null

    try {
        switch ($request.execution.capture_mode) {
            'pulp-app' {
                $runResult = Invoke-PulpAppRequest -Request $request -ResultRoot $resultRoot -StdoutPath $stdoutPath -StderrPath $stderrPath
            }
            'window-capture' {
                $runResult = Invoke-WindowCaptureRequest -Request $request -ResultRoot $resultRoot -StdoutPath $stdoutPath -StderrPath $stderrPath
            }
            default {
                throw "Unsupported capture mode: $($request.execution.capture_mode)"
            }
        }
        $processId = $runResult.pid
        $exitCode = $runResult.exit_code
        $status = $runResult.status
        if ($runResult.window) {
            $window = $runResult.window
        }
        if ($runResult.interaction) {
            $interaction = $runResult.interaction
        }
    } catch {
        $status = 'error'
        $errorText = $_.Exception.Message
        Write-AgentLog "job $($request.job_id) failed: $errorText"
    }

    $payload = @{
        schema = 1
        job_id = $request.job_id
        target = $request.target
        action = $request.action
        label = $request.label
        status = $status
        started_at = $startedAt
        completed_at = Get-IsoTimestamp
        pid = $processId
        exit_code = $exitCode
        execution = @{
            capture_mode = $request.execution.capture_mode
        }
        interaction = if ($interaction) {
            $interaction
        } else {
            @{
                click_point = $request.interaction.click_point
                view_id = $request.interaction.view_id
                view_type = $request.interaction.view_type
                view_text = $request.interaction.view_text
                view_label = $request.interaction.view_label
            }
        }
        outputs = @{
            result_root = $resultRoot
            screenshot = $screenshotPath
            stdout = $stdoutPath
            stderr = $stderrPath
            manifest = $manifestPath
        }
    }
    if ($beforePath) {
        $payload.outputs.before_screenshot = $beforePath
    }
    if ($uiSnapshotPath) {
        $payload.outputs.ui_snapshot = $uiSnapshotPath
    }
    if ($window) {
        $payload.window = $window
    }
    if ($errorText) {
        $payload.error = $errorText
    }

    Write-ResultManifest -ManifestPath $manifestPath -Payload $payload
}

$ExpandedRoot = Expand-ConfiguredPath $RemoteRoot
$JobsDir = Join-Path $ExpandedRoot 'jobs'
$ResultsDir = Join-Path $ExpandedRoot 'results'
$LogsDir = Join-Path $ExpandedRoot 'logs'
Ensure-Directory $ExpandedRoot
Ensure-Directory $JobsDir
Ensure-Directory $ResultsDir
Ensure-Directory $LogsDir
$script:AgentLogPath = Join-Path $LogsDir 'agent.log'
Ensure-Directory (Split-Path -Parent $script:AgentLogPath)

$mutex = New-Object System.Threading.Mutex($false, 'Global\PulpDesktopAutomationAgent')
$hasHandle = $false
try {
    $hasHandle = $mutex.WaitOne(0)
    if (-not $hasHandle) {
        exit 0
    }

    Write-AgentLog "agent start remote_root=$ExpandedRoot"
    $idleDeadline = [DateTime]::UtcNow.AddSeconds($IdleExitSecs)
    while ([DateTime]::UtcNow -lt $idleDeadline) {
        $jobs = @(Get-ChildItem -LiteralPath $JobsDir -Filter '*.json' -File -ErrorAction SilentlyContinue | Sort-Object LastWriteTime)
        if ($jobs.Count -eq 0) {
            Start-Sleep -Milliseconds $PollIntervalMs
            continue
        }

        foreach ($job in $jobs) {
            try {
                Process-RequestFile -RequestFile $job
            } catch {
                Write-AgentLog "job loop failure path=$($job.FullName) error=$($_.Exception.Message)"
                try {
                    Move-BadRequestFile -RequestFile $job -Reason "loop failure: $($_.Exception.Message)"
                } catch {
                    Write-AgentLog "job quarantine failure path=$($job.FullName) error=$($_.Exception.Message)"
                }
            }
        }
        $idleDeadline = [DateTime]::UtcNow.AddSeconds($IdleExitSecs)
    }
    Write-AgentLog 'agent idle exit'
} finally {
    if ($hasHandle) {
        $mutex.ReleaseMutex() | Out-Null
    }
    $mutex.Dispose()
}
