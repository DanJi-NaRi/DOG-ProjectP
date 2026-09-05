Add-Type -AssemblyName System.Windows.Forms

$host.UI.RawUI.WindowTitle = "ProjectP ServerHealthCheck"

Add-Type @"
using System;
using System.Runtime.InteropServices;

public static class ServerWindowTools {
    [DllImport("user32.dll")]
    public static extern bool MoveWindow(IntPtr hWnd, int X, int Y, int nWidth, int nHeight, bool bRepaint);

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc enumProc, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern int GetWindowText(IntPtr hWnd, System.Text.StringBuilder lpString, int nMaxCount);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
}
"@

$workingArea = [System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea

$screenX = [int]$workingArea.X
$screenY = [int]$workingArea.Y
$screenW = [int]$workingArea.Width
$screenH = [int]$workingArea.Height

$quarterH = [int][Math]::Floor($screenH / 4)
$halfW = [int][Math]::Floor($screenW / 2)

$layout = @(
    @{
        Title = "ProjectPServer.exe"
        X = $screenX
        Y = $screenY
        W = $screenW
        H = $quarterH
    },
    @{
        Title = "npm run server-monitor"
        X = $screenX
        Y = $screenY + $quarterH
        W = $halfW
        H = $quarterH
    },
    @{
        Title = "npm run game-backend"
        X = $screenX
        Y = $screenY + ($quarterH * 2)
        W = $halfW
        H = $quarterH
    },
    @{
        Title = "npm run dungeon-manager"
        X = $screenX
        Y = $screenY + ($quarterH * 3)
        W = $halfW
        H = $screenH - ($quarterH * 3)
    },
    @{
        Title = "ProjectP ServerHealthCheck"
        X = $screenX + $halfW
        Y = $screenY + $quarterH
        W = $screenW - $halfW
        H = $screenH - $quarterH
    }
)

function Get-ServerWindow {
    param(
        [string]$Title
    )

    $script:serverWindowMatch = [IntPtr]::Zero
    $script:serverWindowTitle = $Title

    $callback = [ServerWindowTools+EnumWindowsProc] {
        param(
            [IntPtr]$hWnd,
            [IntPtr]$lParam
        )

        if (-not [ServerWindowTools]::IsWindowVisible($hWnd)) {
            return $true
        }

        $builder = New-Object System.Text.StringBuilder 512
        [ServerWindowTools]::GetWindowText($hWnd, $builder, $builder.Capacity) | Out-Null
        $windowTitle = $builder.ToString()

        if ($windowTitle -like "*$script:serverWindowTitle*") {
            $script:serverWindowMatch = $hWnd
            return $false
        }

        return $true
    }

    [ServerWindowTools]::EnumWindows($callback, [IntPtr]::Zero) | Out-Null

    $result = $script:serverWindowMatch
    Remove-Variable -Name serverWindowMatch -Scope Script -ErrorAction SilentlyContinue
    Remove-Variable -Name serverWindowTitle -Scope Script -ErrorAction SilentlyContinue

    $result
}

foreach ($item in $layout) {
    $windowHandle = [IntPtr]::Zero

    for ($attempt = 1; $attempt -le 20 -and $windowHandle -eq [IntPtr]::Zero; $attempt++) {
        $windowHandle = Get-ServerWindow -Title $item.Title

        if ($windowHandle -eq [IntPtr]::Zero) {
            Start-Sleep -Milliseconds 250
        }
    }

    if ($windowHandle -ne [IntPtr]::Zero) {
        [ServerWindowTools]::MoveWindow(
            $windowHandle,
            [int]$item.X,
            [int]$item.Y,
            [int]$item.W,
            [int]$item.H,
            $true
        ) | Out-Null
    }
    else {
        Write-Host ("Window not found: " + $item.Title)
    }
}
