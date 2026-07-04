param(
    [string]$filePath
)

# Set UTF8 encoding for console and output
$OutputEncoding = [System.Text.Encoding]::UTF8

# Logging helpers
function Log-Error($message) {
    $logPath = Join-Path $PSScriptRoot "email_log.txt"
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    "[$timestamp] ERROR: $message" | Out-File -FilePath $logPath -Append -Encoding UTF8
}

function Log-Success($message) {
    $logPath = Join-Path $PSScriptRoot "email_log.txt"
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    "[$timestamp] SUCCESS: $message" | Out-File -FilePath $logPath -Append -Encoding UTF8
}

if (-not $filePath) {
    Log-Error "No file path provided to mailer script."
    exit 1
}

# Resolve path
$resolvedPath = Resolve-Path $filePath -ErrorAction SilentlyContinue
if (-not $resolvedPath) {
    Log-Error "File path '$filePath' could not be resolved."
    exit 1
}

if (-not (Test-Path $resolvedPath.Path)) {
    Log-Error "Data file '$($resolvedPath.Path)' does not exist."
    exit 1
}

# Read variables from the file
$data = @{}
Get-Content $resolvedPath.Path | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') {
        $key = $Matches[1].Trim()
        $val = $Matches[2].Trim()
        $data[$key] = $val
    }
}

# Clean up/delete the file immediately to prevent race conditions or leftover data
try {
    Remove-Item $resolvedPath.Path -Force -ErrorAction Stop
} catch {
    Log-Error "Failed to delete unique temp mail file: $_"
}

# Read SMTP configuration
$configPath = Join-Path $PSScriptRoot "smtp_config.txt"
$SmtpServer = "localhost"
$SmtpPort = 1025
$SmtpUser = ""
$SmtpPass = ""
$SmtpFrom = "noreply@aeroqueue.com"

if (Test-Path $configPath) {
    Get-Content $configPath | ForEach-Object {
        # Skip comments and empty lines
        if ($_ -match '^[^#\s]+=(.*)$') {
            if ($_ -match '^([^=]+)=(.*)$') {
                $key = $Matches[1].Trim()
                $val = $Matches[2].Trim()
                switch ($key) {
                    "SMTP_SERVER" { $SmtpServer = $val }
                    "SMTP_PORT"   { $SmtpPort = [int]$val }
                    "SMTP_USER"   { $SmtpUser = $val }
                    "SMTP_PASS"   { $SmtpPass = $val }
                    "SMTP_FROM"   { $SmtpFrom = $val }
                }
            }
        }
    }
} else {
    Log-Error "smtp_config.txt not found. Using defaults (localhost:1025)."
}

$to = $data["TO"]
$type = $data["TYPE"]

if (-not $to) {
    Log-Error "No destination email address (TO) specified."
    exit 1
}

$subject = ""
$body = ""

# Define CSS styling inline for emails
$emailStyles = @"
    body { font-family: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; background-color: #f3f4f6; margin: 0; padding: 0; }
    .container { max-width: 600px; margin: 30px auto; background-color: #ffffff; border-radius: 16px; overflow: hidden; box-shadow: 0 4px 12px rgba(0,0,0,0.05); }
    .header { padding: 40px 20px; text-align: center; color: #ffffff; }
    .header h1 { margin: 0; font-size: 26px; font-weight: 800; letter-spacing: -0.5px; }
    .header p { margin: 5px 0 0 0; font-size: 14px; opacity: 0.9; }
    .content { padding: 40px; }
    .status-badge { display: inline-block; padding: 6px 14px; border-radius: 9999px; font-size: 11px; font-weight: 800; text-transform: uppercase; margin-bottom: 20px; letter-spacing: 0.5px; }
    .status-confirmed { background-color: #ecfdf5; color: #047857; }
    .status-waitlisted { background-color: #fffbeb; color: #b45309; }
    .status-cancelled { background-color: #fef2f2; color: #b91c1c; }
    .status-refunded { background-color: #ecfdf5; color: #047857; }
    .details-table { width: 100%; border-collapse: collapse; margin-top: 20px; }
    .details-table td { padding: 14px 0; border-bottom: 1px solid #f1f5f9; font-size: 14px; }
    .details-table td.label { color: #64748b; font-weight: 600; width: 40%; text-transform: uppercase; font-size: 11px; letter-spacing: 0.5px; }
    .details-table td.value { color: #0f172a; font-weight: 700; text-align: right; }
    .footer { background-color: #f8fafc; padding: 25px; text-align: center; font-size: 12px; color: #94a3b8; border-top: 1px solid #e2e8f0; line-height: 1.5; }
"@

if ($type -eq "booking_confirmation") {
    $passengerName = $data["PASSENGER_NAME"]
    $bookingId = $data["BOOKING_ID"]
    $source = $data["SOURCE"]
    $destination = $data["DESTINATION"]
    $travelDate = $data["TRAVEL_DATE"]
    $seatNumber = $data["SEAT_NUMBER"]
    $fareAmount = $data["FARE_AMOUNT"]
    $bookingTime = $data["BOOKING_TIME"]
    $ticketStatus = $data["TICKET_STATUS"]

    $subject = "✈️ Flight Booking Confirmation ($ticketStatus) - $bookingId"
    
    $badgeClass = "status-confirmed"
    if ($ticketStatus -eq "WAITLISTED") {
        $badgeClass = "status-waitlisted"
    }

    $body = @"
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <style>
        $emailStyles
    </style>
</head>
<body>
    <div class="container">
        <div class="header" style="background: linear-gradient(135deg, #3b82f6 0%, #1d4ed8 100%);">
            <h1>✈️ AeroQueue</h1>
            <p>Flight Booking Confirmation</p>
        </div>
        <div class="content">
            <div class="status-badge $badgeClass">$ticketStatus</div>
            <p style="font-size: 16px; color: #1e293b; margin-top: 0; font-weight: 600;">Hi $passengerName,</p>
            <p style="font-size: 14px; color: #64748b; line-height: 1.6; margin-bottom: 25px;">
                Your booking request has been processed successfully. Below are your travel itinerary and ticket details:
            </p>
            
            <table class="details-table">
                <tr>
                    <td class="label">Ticket Number</td>
                    <td class="value" style="color: #2563eb; font-family: monospace; font-size: 15px;">$bookingId</td>
                </tr>
                <tr>
                    <td class="label">From</td>
                    <td class="value">$source</td>
                </tr>
                <tr>
                    <td class="label">To</td>
                    <td class="value">$destination</td>
                </tr>
                <tr>
                    <td class="label">Travel Date</td>
                    <td class="value">$travelDate</td>
                </tr>
                <tr>
                    <td class="label">Seat Choice</td>
                    <td class="value">$seatNumber</td>
                </tr>
                <tr>
                    <td class="label">Amount Paid</td>
                    <td class="value" style="color: #10b981; font-size: 15px;">$fareAmount</td>
                </tr>
                <tr>
                    <td class="label">Booking Date</td>
                    <td class="value">$bookingTime</td>
                </tr>
            </table>
            
            <div style="margin-top: 30px; padding: 15px; background-color: #eff6ff; border-radius: 12px; border-left: 4px solid #3b82f6; font-size: 13px; color: #1e40af; line-height: 1.5;">
                <strong>Queue Information:</strong> In accordance with our FIFO Queue System, if your booking status is WAITLISTED, you will be automatically promoted to CONFIRMED as soon as another passenger cancels.
            </div>
        </div>
        <div class="footer">
            &copy; 2026 AeroQueue System Inc. All rights reserved. <br>
            This is an automated notification. Please do not reply directly to this email.
        </div>
    </div>
</body>
</html>
"@
}
elseif ($type -eq "ticket_cancellation") {
    $bookingId = $data["BOOKING_ID"]
    $route = $data["ROUTE"]
    $travelDate = $data["TRAVEL_DATE"]
    $cancelDate = $data["CANCELLATION_DATE"]
    $cancelStatus = $data["CANCELLATION_STATUS"]

    $subject = "❌ Booking Cancellation Confirmed - $bookingId"

    $body = @"
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <style>
        $emailStyles
    </style>
</head>
<body>
    <div class="container">
        <div class="header" style="background: linear-gradient(135deg, #f87171 0%, #dc2626 100%);">
            <h1>❌ AeroQueue</h1>
            <p>Booking Cancellation</p>
        </div>
        <div class="content">
            <div class="status-badge status-cancelled">$cancelStatus</div>
            <p style="font-size: 16px; color: #1e293b; margin-top: 0; font-weight: 600;">Hello,</p>
            <p style="font-size: 14px; color: #64748b; line-height: 1.6; margin-bottom: 25px;">
                Your flight booking has been successfully cancelled. Below are the details of the cancelled ticket:
            </p>
            
            <table class="details-table">
                <tr>
                    <td class="label">Ticket Number</td>
                    <td class="value" style="color: #dc2626; font-family: monospace; font-size: 15px;">$bookingId</td>
                </tr>
                <tr>
                    <td class="label">Route</td>
                    <td class="value">$route</td>
                </tr>
                <tr>
                    <td class="label">Travel Date</td>
                    <td class="value">$travelDate</td>
                </tr>
                <tr>
                    <td class="label">Cancellation Date</td>
                    <td class="value">$cancelDate</td>
                </tr>
            </table>
            
            <p style="font-size: 13px; color: #b91c1c; line-height: 1.5; margin-top: 35px; background-color: #fef2f2; padding: 15px; border-radius: 12px; border-left: 4px solid #ef4444;">
                <strong>Refund Information:</strong> If your booking was CONFIRMED and you made a successful payment, you are eligible for a full refund. Please head over to the <strong>Refund Portal</strong> on our website to verify eligibility and process your refund transaction.
            </p>
        </div>
        <div class="footer">
            &copy; 2026 AeroQueue System Inc. All rights reserved. <br>
            If you need assistance, please visit the help desk.
        </div>
    </div>
</body>
</html>
"@
}
elseif ($type -eq "refund_confirmation") {
    $bookingId = $data["BOOKING_ID"]
    $refundAmount = $data["REFUND_AMOUNT"]
    $txnDetails = $data["TRANSACTION_DETAILS"]
    $refundStatus = $data["REFUND_STATUS"]

    $subject = "💰 Refund Processed Successfully - $bookingId"

    $body = @"
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <style>
        $emailStyles
    </style>
</head>
<body>
    <div class="container">
        <div class="header" style="background: linear-gradient(135deg, #10b981 0%, #047857 100%);">
            <h1>💰 AeroQueue</h1>
            <p>Refund Confirmation</p>
        </div>
        <div class="content">
            <div class="status-badge status-refunded">$refundStatus</div>
            <p style="font-size: 16px; color: #1e293b; margin-top: 0; font-weight: 600;">Hello,</p>
            <p style="font-size: 14px; color: #64748b; line-height: 1.6; margin-bottom: 25px;">
                Your refund transaction has been approved and processed. Below are the refund details:
            </p>
            
            <table class="details-table">
                <tr>
                    <td class="label">Ticket Number</td>
                    <td class="value" style="color: #059669; font-family: monospace; font-size: 15px;">$bookingId</td>
                </tr>
                <tr>
                    <td class="label">Refund Amount</td>
                    <td class="value" style="color: #059669; font-size: 15px;">$refundAmount</td>
                </tr>
                <tr>
                    <td class="label">Transaction ID</td>
                    <td class="value" style="font-family: monospace; font-size: 14px;">$txnDetails</td>
                </tr>
                <tr>
                    <td class="label">Refund Status</td>
                    <td class="value" style="color: #059669;">$refundStatus</td>
                </tr>
            </table>
            
            <p style="font-size: 13px; color: #047857; line-height: 1.5; margin-top: 35px; background-color: #ecfdf5; padding: 15px; border-radius: 12px; border-left: 4px solid #10b981;">
                <strong>Note:</strong> The refund has been credited back to your original payment source. Depending on your financial institution, it may take 3 to 5 business days for the funds to reflect in your account.
            </p>
        </div>
        <div class="footer">
            &copy; 2026 AeroQueue System Inc. All rights reserved. <br>
            Thank you for flying with AeroQueue.
        </div>
    </div>
</body>
</html>
"@
}
elseif ($type -eq "waitlist_promotion") {
    $passengerName = $data["PASSENGER_NAME"]
    $bookingId = $data["BOOKING_ID"]
    $source = $data["SOURCE"]
    $destination = $data["DESTINATION"]
    $travelDate = $data["TRAVEL_DATE"]
    $seatNumber = $data["SEAT_NUMBER"]
    $fareAmount = $data["FARE_AMOUNT"]
    $bookingTime = $data["BOOKING_TIME"]

    $subject = "🎉 Waitlist Promotion! Flight Booking Confirmed - $bookingId"

    $body = @"
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <style>
        $emailStyles
    </style>
</head>
<body>
    <div class="container">
        <div class="header" style="background: linear-gradient(135deg, #8b5cf6 0%, #4f46e5 100%);">
            <h1>🎉 AeroQueue</h1>
            <p>Waitlist Promotion Success</p>
        </div>
        <div class="content">
            <div class="status-badge status-confirmed">CONFIRMED</div>
            <p style="font-size: 16px; color: #1e293b; margin-top: 0; font-weight: 600;">Hi $passengerName,</p>
            <p style="font-size: 14px; color: #64748b; line-height: 1.6; margin-bottom: 25px;">
                Great news! You have been automatically promoted from the waitlist to <strong>CONFIRMED</strong>. Below are your updated travel itinerary and ticket details:
            </p>
            
            <table class="details-table">
                <tr>
                    <td class="label">Ticket Number</td>
                    <td class="value" style="color: #4f46e5; font-family: monospace; font-size: 15px;">$bookingId</td>
                </tr>
                <tr>
                    <td class="label">From</td>
                    <td class="value">$source</td>
                </tr>
                <tr>
                    <td class="label">To</td>
                    <td class="value">$destination</td>
                </tr>
                <tr>
                    <td class="label">Travel Date</td>
                    <td class="value">$travelDate</td>
                </tr>
                <tr>
                    <td class="label">Seat Choice</td>
                    <td class="value">$seatNumber</td>
                </tr>
                <tr>
                    <td class="label">Amount Paid</td>
                    <td class="value" style="color: #10b981; font-size: 15px;">$fareAmount</td>
                </tr>
                <tr>
                    <td class="label">Booking Date</td>
                    <td class="value">$bookingTime</td>
                </tr>
            </table>
            
            <div style="margin-top: 30px; padding: 15px; background-color: #f5f3ff; border-radius: 12px; border-left: 4px solid #8b5cf6; font-size: 13px; color: #5b21b6; line-height: 1.5;">
                <strong>Promotion Information:</strong> This seat was allocated to you as a result of a passenger cancellation, adhering strictly to our FIFO Queue rules. No further action is required from your side.
            </div>
        </div>
        <div class="footer">
            &copy; 2026 AeroQueue System Inc. All rights reserved. <br>
            This is an automated notification. Please do not reply directly to this email.
        </div>
    </div>
</body>
</html>
"@
}

# Sending Logic using .NET SmtpClient to support SSL/TLS
try {
    $mail = New-Object System.Net.Mail.MailMessage
    $mail.From = New-Object System.Net.Mail.MailAddress $SmtpFrom, "AeroQueue System"
    $mail.To.Add($to)
    $mail.Subject = $subject
    $mail.Body = $body
    $mail.IsBodyHtml = $true
    
    # Configure SMTP Client
    $smtp = New-Object System.Net.Mail.SmtpClient($SmtpServer, $SmtpPort)
    
    # Enable SSL if port is not 1025 (port 1025 is local mock, usually no SSL)
    if ($SmtpPort -ne 1025) {
        $smtp.EnableSsl = $true
    }
    
    # Set credentials if provided
    if ($SmtpUser -and $SmtpPass) {
        $smtp.Credentials = New-Object System.Net.NetworkCredential($SmtpUser, $SmtpPass)
    }
    
    # Send
    $smtp.Send($mail)
    
    # Write successes to standard output and log file
    Write-Host "Success: Email of type '$type' sent to '$to' successfully."
    Log-Success "Email of type '$type' sent to '$to' successfully."
}
catch {
    $innerMsg = ""
    if ($_.Exception -and $_.Exception.InnerException) {
        $innerMsg = " Inner: " + $_.Exception.InnerException.Message
    }
    Log-Error "Failed to send email to '$to' of type '$type' (SMTP: $($SmtpServer):$($SmtpPort)). Error: $_.$innerMsg"
    exit 1
}
