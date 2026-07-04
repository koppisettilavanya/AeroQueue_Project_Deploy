# AeroQueue - Pure C Backend Server

This backend system is built entirely in **C** using native socket programming (Winsock). It strictly enforces a **FIFO Queue System** for all passenger flight bookings and waitlist promotions.

## Core Features
1. **Zero Dependencies:** Written in standard C. No Node.js, Python, or external frameworks required.
2. **Native HTTP Server:** Handles raw HTTP requests, parses basic JSON bodies, and returns JSON responses with proper CORS headers.
3. **Strict FIFO Queue:** Manages passenger waiting lists structurally via arrays. Automatic waitlist promotion based on `timestamp` order when cancellations occur.

## Folder Structure
```
aero/
│
├── backend_c/
│   ├── aeroqueue.c        # Main C Source Code (Server, Routing, Queue Logic)
│   ├── build.bat          # Windows Compilation Script
│   └── README_C.md        # Documentation (This File)
│
└── [Existing Frontend HTML Files...]
```

## How to Compile & Run (Windows)
1. **Prerequisites:** You must have **GCC** installed (via MinGW or MSYS2) and added to your system `PATH`.
2. Open a Command Prompt or Terminal and navigate to the `backend_c` folder:
   ```cmd
   cd path\to\aero\backend_c
   ```
3. Run the build script to compile the C code:
   ```cmd
   build.bat
   ```
   *(This executes `gcc aeroqueue.c -o aeroqueue_server.exe -lws2_32`)*
4. Run the compiled server executable:
   ```cmd
   aeroqueue_server.exe
   ```
5. You should see `AeroQueue C Backend running on port 8080`.

## Connecting Frontend to Backend
The C Server handles API requests exactly like a standard REST backend. Because you do not want the UI design changed, you should inject JavaScript `<script>` blocks at the bottom of your HTML files before `</body>` to hijack form submissions and make `fetch()` calls to the C server.

Example for `Login.html`:
```html
<script>
document.querySelector('form').addEventListener('submit', async (e) => {
    e.preventDefault();
    const isSignup = document.getElementById('signupFields').classList.contains('active');
    const email = document.querySelector('input[type="email"]').value;
    const password = document.querySelector('input[type="password"]').value;

    const endpoint = isSignup ? 'http://localhost:8080/api/auth/register' : 'http://localhost:8080/api/auth/login';
    
    const response = await fetch(endpoint, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ email: email, password: password })
    });
    
    const data = await response.json();
    if(response.ok) {
        if(isSignup) {
            alert(data.message);
            switchMode('login');
        } else {
            localStorage.setItem('token', data.token);
            window.location.href = 'Search.html';
        }
    } else {
        alert(data.error);
    }
});
</script>
```

## API Endpoint Documentation

### 1. User Registration
- **Endpoint:** `POST /api/auth/register`
- **Request (JSON):** `{"email": "user@test.com", "password": "pass"}`
- **Response:** `201 Created` - `{"message":"User registered"}`

### 2. User Login
- **Endpoint:** `POST /api/auth/login`
- **Request (JSON):** `{"email": "user@test.com", "password": "pass"}`
- **Response:** `200 OK` - `{"message":"Login successful", "token":"token_1"}`

### 3. Get Flights (Search)
- **Endpoint:** `GET /api/flights`
- **Response:** `200 OK` - `{"flights": [{"id":1, "flightNumber":"AQ-101", "capacity":2, "confirmed":0}, ...]}`

### 4. Book Flight (FIFO Enqueue)
- **Endpoint:** `POST /api/bookings/enqueue`
- **Request (JSON):** `{"flightId": "1"}` *(Note: ID must be a string for the simple C JSON parser)*
- **Response (If seats available):** `201 Created` - `{"message":"Booking processed", "bookingId":1, "status":"CONFIRMED"}`
- **Response (If flight full):** `201 Created` - `{"message":"Booking processed", "bookingId":2, "status":"WAITLISTED"}`

### 5. Check Queue Status
- **Endpoint:** `GET /api/bookings/status/<bookingId>`
- **Response:** `200 OK` - `{"bookingId":2, "status":"WAITLISTED", "queuePosition":1}`

### 6. Cancel Booking (FIFO Dequeue & Promotion)
- **Endpoint:** `POST /api/bookings/cancel`
- **Request (JSON):** `{"bookingId": "1"}`
- **Response:** `200 OK` - `{"message":"Cancelled successfully. Queue updated."}`

## End-to-End Testing the FIFO Queue
1. Start `aeroqueue_server.exe`.
2. Open a terminal and use `curl` (or Postman, or your frontend HTML files) to enqueue bookings. Flight `1` (AQ-101) is initialized with a maximum capacity of `2`.
3. **Booking 1:**
   `curl -X POST http://localhost:8080/api/bookings/enqueue -d "{\"flightId\":\"1\"}"` -> Returns `CONFIRMED`.
4. **Booking 2:**
   `curl -X POST http://localhost:8080/api/bookings/enqueue -d "{\"flightId\":\"1\"}"` -> Returns `CONFIRMED`.
5. **Booking 3 (Waitlisted):**
   `curl -X POST http://localhost:8080/api/bookings/enqueue -d "{\"flightId\":\"1\"}"` -> Returns `WAITLISTED` (Flight is full).
6. **Check Booking 3 Status:**
   `curl -X GET http://localhost:8080/api/bookings/status/3` -> Returns `WAITLISTED` with `queuePosition: 1`.
7. **Cancel Booking 1:**
   `curl -X POST http://localhost:8080/api/bookings/cancel -d "{\"bookingId\":\"1\"}"` -> Returns `Cancelled successfully`. This triggers the C FIFO logic to pop Waitlisted Booking 3 and promote it to CONFIRMED.
8. **Verify Booking 3 is Promoted:**
   `curl -X GET http://localhost:8080/api/bookings/status/3` -> Returns `CONFIRMED`.