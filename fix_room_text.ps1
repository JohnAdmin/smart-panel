$file = "src\web_server.cpp"
$c = [System.IO.File]::ReadAllText($file, [System.Text.Encoding]::UTF8)
$o = $c.Length

# Modal header
$c = $c.Replace([char]0xe0e0+'', '')  # noop, placeholder
$c = $c.Replace("`u{0E40}`u{0E18}`u{0E1A}`u{0E11}`u{0E1A}`u{0E14}`u{0E1A}`u{0E12}`u{0E1A}`u{0E23}`u{0E1A}`u{0E0B}`u{0E47}`u{0E1A}`u{0E0D}`u{0E1A}`u{0E07}", 'Manage Rooms')

[System.IO.File]::WriteAllText($file, $c, (New-Object System.Text.UTF8Encoding $false))
Write-Host "diff=$($o - $c.Length)"
