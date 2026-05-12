$s = New-Object -ComObject WScript.Shell
$desktop = [Environment]::GetFolderPath('Desktop')
$sc = $s.CreateShortcut("$desktop\OpenBoard (Finger-Pan).lnk")
$sc.TargetPath = 'C:\openboard-fork\build\build\win32\release\product\OpenBoard.exe'
$sc.WorkingDirectory = 'C:\openboard-fork\build\build\win32\release\product'
$sc.Save()
Write-Output "Shortcut created at: $desktop\OpenBoard (Finger-Pan).lnk"
