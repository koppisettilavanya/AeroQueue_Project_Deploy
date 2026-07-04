# AeroQueue E2E API Verification Script

$baseUrl = "http://localhost:8080"
Write-Host "=========================================="
Write-Host "Starting AeroQueue E2E API Verification..."
Write-Host "=========================================="

# 1. Register User
$regPayload = @{
    firstName = "John"
    lastName = "Doe"
    phone = "+1234567890"
    email = "tester@aeroqueue.com"
    password = "Password123"
} | ConvertTo-Json

Write-Host "1. Registering user..." -ForegroundColor Cyan
try {
    $regRes = Invoke-RestMethod -Uri "$baseUrl/api/auth/register" -Method Post -Body $regPayload -ContentType "application/json"
    Write-Host "User Registered. Token: $($regRes.token)" -ForegroundColor Green
    $token = $regRes.token
} catch {
    Write-Host "Registration failed (user might exist): $_" -ForegroundColor Yellow
    # Try logging in instead
}

# 2. Login User
$loginPayload = @{
    email = "tester@aeroqueue.com"
    password = "Password123"
} | ConvertTo-Json

Write-Host "`n2. Logging in..." -ForegroundColor Cyan
try {
    $loginRes = Invoke-RestMethod -Uri "$baseUrl/api/auth/login" -Method Post -Body $loginPayload -ContentType "application/json"
    Write-Host "Login Successful. Token: $($loginRes.token)" -ForegroundColor Green
    $token = $loginRes.token
} catch {
    Write-Host "Login failed: $_" -ForegroundColor Red
    exit 1
}

# 3. Create Booking 1 (Confirmed)
$bookPayload1 = @{
    flightId = "1"
    token = $token
    seatNumber = "5C"
    firstName = "John"
    lastName = "Doe"
    origin = "New York"
    destination = "London"
    travelDate = "2026-06-30"
    passengerEmail = "testpassenger@aeroqueue.com"
    paymentMethod = "card"
    cardNumber = "1111222233334444"
    cardExpiry = "12/28"
    cardCvv = "123"
    fareAmount = "₹4,500"
} | ConvertTo-Json

Write-Host "`n3. Creating Booking 1 (Expected: CONFIRMED)..." -ForegroundColor Cyan
try {
    $bookRes1 = Invoke-RestMethod -Uri "$baseUrl/api/bookings/enqueue" -Method Post -Body $bookPayload1 -ContentType "application/json"
    Write-Host "Booking 1 Created. Ticket: $($bookRes1.ticketNumber), Status: $($bookRes1.status)" -ForegroundColor Green
    $ticketNumber1 = $bookRes1.ticketNumber
} catch {
    Write-Host "Booking 1 failed: $_" -ForegroundColor Red
    exit 1
}

# 4. Create Booking 2 (Confirmed - flight capacity is 2)
$bookPayload2 = @{
    flightId = "1"
    token = $token
    seatNumber = "5C"
    firstName = "Jane"
    lastName = "Doe"
    origin = "New York"
    destination = "London"
    travelDate = "2026-06-30"
    passengerEmail = "testpassenger2@aeroqueue.com"
    paymentMethod = "card"
    cardNumber = "1111222233334444"
    cardExpiry = "12/28"
    cardCvv = "123"
    fareAmount = "₹4,500"
} | ConvertTo-Json

Write-Host "`n4. Creating Booking 2 (Expected: CONFIRMED)..." -ForegroundColor Cyan
try {
    $bookRes2 = Invoke-RestMethod -Uri "$baseUrl/api/bookings/enqueue" -Method Post -Body $bookPayload2 -ContentType "application/json"
    Write-Host "Booking 2 Created. Ticket: $($bookRes2.ticketNumber), Status: $($bookRes2.status)" -ForegroundColor Green
    $ticketNumber2 = $bookRes2.ticketNumber
} catch {
    Write-Host "Booking 2 failed: $_" -ForegroundColor Red
    exit 1
}

# 5. Create Booking 3 (Expected: WAITLISTED - capacity full)
$bookPayload3 = @{
    flightId = "1"
    token = $token
    seatNumber = "5C"
    firstName = "Bob"
    lastName = "Doe"
    origin = "New York"
    destination = "London"
    travelDate = "2026-06-30"
    passengerEmail = "testpassenger3@aeroqueue.com"
    paymentMethod = "card"
    cardNumber = "1111222233334444"
    cardExpiry = "12/28"
    cardCvv = "123"
    fareAmount = "₹4,500"
} | ConvertTo-Json

Write-Host "`n5. Creating Booking 3 (Expected: WAITLISTED)..." -ForegroundColor Cyan
try {
    $bookRes3 = Invoke-RestMethod -Uri "$baseUrl/api/bookings/enqueue" -Method Post -Body $bookPayload3 -ContentType "application/json"
    Write-Host "Booking 3 Created. Ticket: $($bookRes3.ticketNumber), Status: $($bookRes3.status)" -ForegroundColor Green
    $ticketNumber3 = $bookRes3.ticketNumber
} catch {
    Write-Host "Booking 3 failed: $_" -ForegroundColor Red
    exit 1
}

# Wait for background email processing to complete
Start-Sleep -Seconds 2

# 6. Cancel Booking 1 (Triggers Waitlist Promotion for Booking 3)
$cancelPayload = @{
    ticketNumber = $ticketNumber1
} | ConvertTo-Json

Write-Host "`n6. Cancelling Booking 1 (Expected: Booking 3 should be promoted to CONFIRMED)..." -ForegroundColor Cyan
try {
    $cancelRes = Invoke-RestMethod -Uri "$baseUrl/api/bookings/cancel" -Method Post -Body $cancelPayload -ContentType "application/json"
    Write-Host "Cancellation of Booking 1 successful: $($cancelRes.message)" -ForegroundColor Green
} catch {
    Write-Host "Cancellation failed: $_" -ForegroundColor Red
    exit 1
}

# Wait for background email processing
Start-Sleep -Seconds 2

# 7. Request Refund for Cancelled Booking 1
$refundPayload = @{
    ticketNumber = $ticketNumber1
    email = "tester@aeroqueue.com"
} | ConvertTo-Json

Write-Host "`n7. Requesting Refund for Cancelled Booking 1..." -ForegroundColor Cyan
try {
    $refundRes = Invoke-RestMethod -Uri "$baseUrl/api/bookings/refund" -Method Post -Body $refundPayload -ContentType "application/json"
    Write-Host "Refund request successful: $($refundRes.message)" -ForegroundColor Green
} catch {
    Write-Host "Refund failed: $_" -ForegroundColor Red
    exit 1
}

Write-Host "`n=========================================="
Write-Host "E2E API Verification Script Execution Completed."
Write-Host "=========================================="
