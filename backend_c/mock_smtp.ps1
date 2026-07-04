# Mock SMTP Server on Port 1025
# Listens for local SMTP connections and logs the transactions to a log file.
$port = 1025
$listener = New-Object System.Net.Sockets.TcpListener([System.Net.IPAddress]::Any, $port)
$logFile = Join-Path $PSScriptRoot "smtp_received.log"

# Clean previous log file
if (Test-Path $logFile) {
    Remove-Item $logFile -Force
}

function Log-Message($text) {
    Write-Host $text
    $text | Out-File -FilePath $logFile -Append -Encoding UTF8
}

try {
    $listener.Start()
    Log-Message "=========================================="
    Log-Message "Mock SMTP Server listening on port $port..."
    Log-Message "Press Ctrl+C to stop."
    Log-Message "=========================================="

    while ($true) {
        $client = $listener.AcceptTcpClient()
        $stream = $client.GetStream()
        $encoding = New-Object System.Text.UTF8Encoding($false)
        $reader = New-Object System.IO.StreamReader($stream, $encoding)
        $writer = New-Object System.IO.StreamWriter($stream, $encoding)
        $writer.AutoFlush = $true

        $writer.Write("220 127.0.0.1 ESMTP ready`r`n")
        $emailData = New-Object System.Text.StringBuilder

        try {
            while ($null -ne ($line = $reader.ReadLine())) {
                Log-Message "SMTP CMD: $line"
                
                if ($line -eq "QUIT") {
                    $writer.Write("221 Goodbye`r`n")
                    break
                }
                elseif ($line -match "^EHLO" -or $line -match "^HELO") {
                    $writer.Write("250-127.0.0.1 hello`r`n")
                    $writer.Write("250 OK`r`n")
                }
                elseif ($line -match "^MAIL FROM:") {
                    $writer.Write("250 OK`r`n")
                }
                elseif ($line -match "^RCPT TO:") {
                    $writer.Write("250 OK`r`n")
                }
                elseif ($line -eq "DATA") {
                    $writer.Write("354 Start mail input; end with <CRLF>.<CRLF>`r`n")
                    while ($null -ne ($dataLine = $reader.ReadLine())) {
                        if ($dataLine -eq ".") {
                            break
                        }
                        $null = $emailData.AppendLine($dataLine)
                    }
                    $writer.Write("250 OK`r`n")
                }
                else {
                    $writer.Write("250 OK`r`n")
                }
            }
        }
        catch {
            Log-Message "Connection error: $_"
        }
        finally {
            $client.Close()
            Log-Message "------------------------------------------"
            Log-Message "RECEIVED EMAIL CONTENT:"
            Log-Message $emailData.ToString()
            Log-Message "=========================================="
        }
    }
}
catch {
    Log-Message "Failed to start listener: $_"
}
finally {
    $listener.Stop()
}
