#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
 
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <arpa/inet.h>
#endif
 
#define PORT 8080
#define BUFFER_SIZE 16384
#define RESPONSE_BUFFER_SIZE 32768
#define MAX_USERS 200
#define MAX_FLIGHTS 100
#define MAX_BOOKINGS 1000
 
// Prototypes
int str_case_cmp(const char* s1, const char* s2);
 
// ==========================================
// DATA STRUCTURES
// ==========================================
 
typedef struct {
    int id;
    char firstName[50];
    char lastName[50];
    char email[100];
    char phone[20];
    char password[100]; // Stores SHA-256 hash or "google_oauth"
    char token[100];    // Simple session token
    char status[20];
} User;
 
typedef struct {
    int id;
    char flightNumber[20];
    char origin[50];
    char destination[50];
    int capacity;
    int confirmedCount;
} Flight;
 
typedef struct {
    int id;
    int userId;
    int flightId;
    char ticketNumber[30];
    char seatNumber[10];
    char firstName[50];
    char lastName[50];
    char origin[50];
    char destination[50];
    char travelDate[30];
    int isConfirmed; // 1 = Confirmed, 0 = Waitlisted, -1 = Cancelled
    long timestamp;  // For FIFO ordering
    char paymentStatus[20];
    char passengerEmail[100];
} Booking;
 
// Global State
User users[MAX_USERS];
int userCount = 0;
 
Flight flights[MAX_FLIGHTS];
int flightCount = 0;
 
Booking bookings[MAX_BOOKINGS];
int bookingCount = 0;
 
// ==========================================
// SHA-256 IMPLEMENTATION
// ==========================================
#define ROTRIGHT(word,bits) (((word) >> (bits)) | ((word) << (32-(bits))))
#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTRIGHT(x,2) ^ ROTRIGHT(x,13) ^ ROTRIGHT(x,22))
#define EP1(x) (ROTRIGHT(x,6) ^ ROTRIGHT(x,11) ^ ROTRIGHT(x,25))
#define SIG0(x) (ROTRIGHT(x,7) ^ ROTRIGHT(x,18) ^ ((x) >> 3))
#define SIG1(x) (ROTRIGHT(x,17) ^ ROTRIGHT(x,19) ^ ((x) >> 10))
 
typedef struct {
    uint8_t data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
} SHA256_CTX;
 
void sha256_transform(SHA256_CTX *ctx, const uint8_t data[]) {
    uint32_t a, b, c, d, e, f, g, h, i, j, t1, t2, m[64];
 
    for (i = 0, j = 0; i < 16; ++i, j += 4)
        m[i] = (data[j] << 24) | (data[j + 1] << 16) | (data[j + 2] << 8) | (data[j + 3]);
    for ( ; i < 64; ++i)
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];
 
    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];
 
    uint32_t k[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
 
    for (i = 0; i < 64; ++i) {
        t1 = h + EP1(e) + CH(e,f,g) + k[i] + m[i];
        t2 = EP0(a) + MAJ(a,b,c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
 
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}
 
void sha256_init(SHA256_CTX *ctx) {
    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
}
 
void sha256_update(SHA256_CTX *ctx, const uint8_t data[], size_t len) {
    uint32_t i;
 
    for (i = 0; i < len; ++i) {
        ctx->data[ctx->datalen] = data[i];
        ctx->datalen++;
        if (ctx->datalen == 64) {
            sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}
 
void sha256_final(SHA256_CTX *ctx, uint8_t hash[]) {
    uint32_t i;
 
    i = ctx->datalen;
 
    if (ctx->datalen < 56) {
        ctx->data[i++] = 0x80;
        while (i < 56)
            ctx->data[i++] = 0x00;
    }
    else {
        ctx->data[i++] = 0x80;
        while (i < 64)
            ctx->data[i++] = 0x00;
        sha256_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    }
 
    ctx->bitlen += ctx->datalen * 8;
    ctx->data[63] = ctx->bitlen;
    ctx->data[62] = ctx->bitlen >> 8;
    ctx->data[61] = ctx->bitlen >> 16;
    ctx->data[60] = ctx->bitlen >> 24;
    ctx->data[59] = ctx->bitlen >> 32;
    ctx->data[58] = ctx->bitlen >> 40;
    ctx->data[57] = ctx->bitlen >> 48;
    ctx->data[56] = ctx->bitlen >> 56;
    sha256_transform(ctx, ctx->data);
 
    for (i = 0; i < 4; ++i) {
        hash[i]      = (ctx->state[0] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 4]  = (ctx->state[1] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 8]  = (ctx->state[2] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 20] = (ctx->state[5] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 24] = (ctx->state[6] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 28] = (ctx->state[7] >> (24 - i * 8)) & 0x000000ff;
    }
}
 
void hash_password(const char* password, char* out_hex) {
    SHA256_CTX ctx;
    uint8_t hash[32];
    sha256_init(&ctx);
    sha256_update(&ctx, (const uint8_t*)password, strlen(password));
    sha256_final(&ctx, hash);
    for (int i = 0; i < 32; i++) {
        sprintf(out_hex + (i * 2), "%02x", hash[i]);
    }
    out_hex[64] = '\0';
}
 
// ==========================================
// FILE PERSISTENCE & SPLIT HELPER
// ==========================================
 
void safe_split(char* line, char fields[][100], int max_fields) {
    int field_idx = 0;
    int char_idx = 0;
    char* p = line;
    while (field_idx < max_fields) {
        if (*p == '|' || *p == '\n' || *p == '\r' || *p == '\0') {
            fields[field_idx][char_idx] = '\0';
            field_idx++;
            char_idx = 0;
            if (*p == '\0' || *p == '\n' || *p == '\r') {
                break;
            }
            p++;
        } else {
            if (char_idx < 99) {
                fields[field_idx][char_idx++] = *p;
            }
            p++;
        }
    }
    while (field_idx < max_fields) {
        fields[field_idx++][0] = '\0';
    }
}
 
char users_file_path[256] = "users.txt";
char bookings_file_path[256] = "bookings.txt";
char email_dir_prefix[30] = "";
 
void detect_file_paths() {
    FILE* f_test = fopen("users.txt", "r");
    if (f_test) {
        fclose(f_test);
        strcpy(users_file_path, "users.txt");
        strcpy(email_dir_prefix, "");
    } else {
        f_test = fopen("backend_c/users.txt", "r");
        if (f_test) {
            strcpy(users_file_path, "backend_c/users.txt");
            strcpy(email_dir_prefix, "backend_c/");
            fclose(f_test);
        } else {
            // Default to backend_c if directory structure is standard
            FILE* d_test = fopen("backend_c/README_C.md", "r");
            if (d_test) {
                fclose(d_test);
                strcpy(users_file_path, "backend_c/users.txt");
                strcpy(email_dir_prefix, "backend_c/");
            } else {
                strcpy(users_file_path, "users.txt");
                strcpy(email_dir_prefix, "");
            }
        }
    }
 
    f_test = fopen("bookings.txt", "r");
    if (f_test) {
        fclose(f_test);
        strcpy(bookings_file_path, "bookings.txt");
    } else {
        f_test = fopen("backend_c/bookings.txt", "r");
        if (f_test) {
            strcpy(bookings_file_path, "backend_c/bookings.txt");
            fclose(f_test);
        } else {
            FILE* d_test = fopen("backend_c/README_C.md", "r");
            if (d_test) {
                fclose(d_test);
                strcpy(bookings_file_path, "backend_c/bookings.txt");
            } else {
                strcpy(bookings_file_path, "bookings.txt");
            }
        }
    }
    printf("[STORAGE] Resolved database paths -> Users: '%s', Bookings: '%s'\n", users_file_path, bookings_file_path);
}
 
void load_users() {
    FILE* f = fopen(users_file_path, "r");
    if (!f) {
        printf("[STORAGE] [WARNING] Could not open users file '%s' for reading\n", users_file_path);
        return;
    }
    userCount = 0;
    char line[512];
    while (fgets(line, sizeof(line), f) && userCount < MAX_USERS) {
        // Strip trailing \r and \n
        line[strcspn(line, "\r\n")] = '\0';
        if (strlen(line) == 0) continue;
 
        char fields[8][100];
        safe_split(line, fields, 8);
        
        if (strlen(fields[0]) == 0 || strlen(fields[3]) == 0) {
            continue;
        }
        
        User* u = &users[userCount];
        u->id = atoi(fields[0]);
        strcpy(u->firstName, fields[1]);
        strcpy(u->lastName, fields[2]);
        strcpy(u->email, fields[3]);
        strcpy(u->phone, fields[4]);
        strcpy(u->password, fields[5]);
        strcpy(u->token, fields[6]);
        strcpy(u->status, fields[7]);
        
        userCount++;
    }
    fclose(f);
    printf("[STORAGE] Loaded %d users from '%s'\n", userCount, users_file_path);
}
 
void save_users() {
    FILE* f = fopen(users_file_path, "w");
    if (!f) {
        printf("[STORAGE] [ERROR] Could not open users file '%s' for writing\n", users_file_path);
        return;
    }
    for (int i = 0; i < userCount; i++) {
        User* u = &users[i];
        fprintf(f, "%d|%s|%s|%s|%s|%s|%s|%s\n",
                u->id, u->firstName, u->lastName, u->email, u->phone, u->password, u->token, u->status);
    }
    fclose(f);
    printf("[STORAGE] Saved %d users to '%s'\n", userCount, users_file_path);
}
 
void load_bookings() {
    FILE* f = fopen(bookings_file_path, "r");
    if (!f) {
        printf("[STORAGE] [WARNING] Could not open bookings file '%s' for reading\n", bookings_file_path);
        return;
    }
    bookingCount = 0;
    char line[512];
    while (fgets(line, sizeof(line), f) && bookingCount < MAX_BOOKINGS) {
        // Strip trailing \r and \n
        line[strcspn(line, "\r\n")] = '\0';
        if (strlen(line) == 0) continue;
 
        char fields[14][100];
        safe_split(line, fields, 14);
        
        if (strlen(fields[0]) == 0 || strlen(fields[3]) == 0) {
            continue;
        }
        
        Booking* b = &bookings[bookingCount];
        b->id = atoi(fields[0]);
        b->userId = atoi(fields[1]);
        b->flightId = atoi(fields[2]);
        strcpy(b->ticketNumber, fields[3]);
        strcpy(b->seatNumber, fields[4]);
        strcpy(b->firstName, fields[5]);
        strcpy(b->lastName, fields[6]);
        strcpy(b->origin, fields[7]);
        strcpy(b->destination, fields[8]);
        strcpy(b->travelDate, fields[9]);
        b->isConfirmed = atoi(fields[10]);
        b->timestamp = atol(fields[11]);
        if (strlen(fields[12]) > 0) {
            strcpy(b->paymentStatus, fields[12]);
        } else {
            strcpy(b->paymentStatus, "SUCCESS");
        }
        
        if (strlen(fields[13]) > 0) {
            strcpy(b->passengerEmail, fields[13]);
        } else {
            strcpy(b->passengerEmail, "");
        }
        
        // Dynamic Flight Reconstruction at Startup
        int flightId = b->flightId;
        int found = 0;
        for (int i = 0; i < flightCount; i++) {
            if (flights[i].id == flightId) {
                found = 1;
                break;
            }
        }
        if (!found && flightCount < MAX_FLIGHTS) {
            Flight* fl = &flights[flightCount];
            fl->id = flightId;
            snprintf(fl->flightNumber, sizeof(fl->flightNumber), "AQ-%d", flightId);
            strcpy(fl->origin, b->origin);
            strcpy(fl->destination, b->destination);
            fl->capacity = 50; // default capacity
            fl->confirmedCount = 0;
            flightCount++;
            printf("[FLIGHT] Restored flight ID %d (%s -> %s) from bookings database\n", fl->id, fl->origin, fl->destination);
        }
        
        bookingCount++;
    }
    fclose(f);
    printf("[STORAGE] Loaded %d bookings from '%s'\n", bookingCount, bookings_file_path);
}
 
void save_bookings() {
    FILE* f = fopen(bookings_file_path, "w");
    if (!f) {
        printf("[STORAGE] [ERROR] Could not open bookings file '%s' for writing\n", bookings_file_path);
        return;
    }
    for (int i = 0; i < bookingCount; i++) {
        Booking* b = &bookings[i];
        fprintf(f, "%d|%d|%d|%s|%s|%s|%s|%s|%s|%s|%d|%ld|%s|%s\n",
                b->id, b->userId, b->flightId, b->ticketNumber, b->seatNumber, b->firstName, b->lastName, b->origin, b->destination, b->travelDate, b->isConfirmed, b->timestamp, b->paymentStatus, b->passengerEmail);
    }
    fclose(f);
    printf("[STORAGE] Saved %d bookings to '%s'\n", bookingCount, bookings_file_path);
}
 
// ==========================================
// FIFO QUEUE LOGIC
// ==========================================
 
// Helper function to send email by writing temp file and spawning powershell in background
void trigger_email_async(const char* type, const char* to_email, const char* keys[], const char* values[], int count) {
    char filename[128];
    snprintf(filename, sizeof(filename), "%smail_%ld_%d.txt", email_dir_prefix, (long)time(NULL), rand() % 1000);
    FILE* f = fopen(filename, "w");
    if (!f) {
        printf("[EMAIL] [ERROR] Failed to write temporary email data file '%s'\n", filename);
        return;
    }
    fprintf(f, "TYPE=%s\n", type);
    fprintf(f, "TO=%s\n", to_email);
    for (int i = 0; i < count; i++) {
        fprintf(f, "%s=%s\n", keys[i], values[i]);
    }
    fclose(f);
 
    // Call PowerShell script asynchronously in background
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "powershell.exe -ExecutionPolicy Bypass -File \"%ssend_email.ps1\" \"%s\"", email_dir_prefix, filename);
    printf("[EMAIL] [TRIGGER] Spawning mailer process: %s\n", cmd);
 
#ifdef _WIN32
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    ZeroMemory(&pi, sizeof(pi));
 
    if (CreateProcess(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        printf("[EMAIL] [ERROR] CreateProcess failed with error code %ld\n", GetLastError());
    }
#else
    // POSIX fallback
    strcat(cmd, " &");
    int ret = system(cmd);
    if (ret != 0) {
        printf("[EMAIL] [WARNING] system() returned non-zero code %d\n", ret);
    }
#endif
}
 
// Process the waitlist for a specific flight seat (FIFO Promotion)
void process_waitlist(int flightId, const char* seatNumber, const char* origin, const char* destination, const char* travelDate) {
    // Find the flight
    Flight* f = NULL;
    for (int i = 0; i < flightCount; i++) {
        if (flights[i].id == flightId) {
            f = &flights[i];
            break;
        }
    }
    
    // Proactively register flight if missing (rebuilt on cancellation check)
    if (!f) {
        if (flightCount < MAX_FLIGHTS) {
            f = &flights[flightCount];
            f->id = flightId;
            snprintf(f->flightNumber, sizeof(f->flightNumber), "AQ-%d", flightId);
            strcpy(f->origin, origin);
            strcpy(f->destination, destination);
            f->capacity = 50;
            f->confirmedCount = 0;
            flightCount++;
            printf("[FLIGHT] Dynamically restored flight ID %d on cancellation check\n", flightId);
        } else {
            return;
        }
    }
 
    int oldestIdx = -1;
    long oldestTime = 2147483647; // Max long
 
    // Find the earliest waitlisted booking for THIS flight, route, date and seat
    for (int i = 0; i < bookingCount; i++) {
        if (bookings[i].flightId == flightId && 
            bookings[i].isConfirmed == 0 && 
            str_case_cmp(bookings[i].seatNumber, seatNumber) == 0 &&
            str_case_cmp(bookings[i].origin, origin) == 0 &&
            str_case_cmp(bookings[i].destination, destination) == 0 &&
            str_case_cmp(bookings[i].travelDate, travelDate) == 0) {
            
            if (bookings[i].timestamp < oldestTime) {
                oldestTime = bookings[i].timestamp;
                oldestIdx = i;
            }
        }
    }
 
    // If a waitlisted booking was found, promote it
    if (oldestIdx != -1) {
        bookings[oldestIdx].isConfirmed = 1;
        f->confirmedCount++;
        save_bookings();
        printf("[QUEUE] Promoted Booking %s to CONFIRMED for Flight %d Seat %s on date %s\n", 
            bookings[oldestIdx].ticketNumber, flightId, seatNumber, travelDate);
 
        // Find user email for waitlist promotion notification
        User* pu = NULL;
        for (int i = 0; i < userCount; i++) {
            if (users[i].id == bookings[oldestIdx].userId) {
                pu = &users[i];
                break;
            }
        }
        if (pu) {
            char bookingTimeStr[100];
            time_t rawtime = time(NULL);
            struct tm * timeinfo = localtime(&rawtime);
            strftime(bookingTimeStr, sizeof(bookingTimeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
 
            const char* keys[] = {
                "PASSENGER_NAME", "BOOKING_ID", "SOURCE", "DESTINATION",
                "TRAVEL_DATE", "SEAT_NUMBER", "FARE_AMOUNT", "BOOKING_TIME", "TICKET_STATUS"
            };
            char passenger_name[120];
            snprintf(passenger_name, sizeof(passenger_name), "%s %s", bookings[oldestIdx].firstName, bookings[oldestIdx].lastName);
            
            const char* values[] = {
                passenger_name, bookings[oldestIdx].ticketNumber, bookings[oldestIdx].origin, bookings[oldestIdx].destination,
                bookings[oldestIdx].travelDate, bookings[oldestIdx].seatNumber, "₹4,500", bookingTimeStr, "CONFIRMED"
            };
            
            trigger_email_async("waitlist_promotion", bookings[oldestIdx].passengerEmail[0] ? bookings[oldestIdx].passengerEmail : pu->email, keys, values, 9);
        }
    }
}
 
// ==========================================
// HELPER FUNCTIONS
// ==========================================
 
int str_case_cmp(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        char c1 = *s1;
        char c2 = *s2;
        if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    char c1 = *s1;
    char c2 = *s2;
    if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
    if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
    return c1 - c2;
}
 
// Robust JSON value extraction helper
void extract_json_value(const char* json, const char* key, char* output, int max_len) {
    output[0] = '\0';
    char searchKey[100];
    snprintf(searchKey, sizeof(searchKey), "\"%s\"", key);
    
    const char* pos = strstr(json, searchKey);
    if (!pos) return;
    
    pos += strlen(searchKey);
    while (*pos == ' ' || *pos == '\t' || *pos == '\r' || *pos == '\n') pos++;
    if (*pos != ':') return;
    pos++; // skip ':'
    while (*pos == ' ' || *pos == '\t' || *pos == '\r' || *pos == '\n') pos++;
    
    int is_string = 0;
    if (*pos == '\"') {
        is_string = 1;
        pos++; // skip opening quote
    }
    
    int i = 0;
    if (is_string) {
        while (*pos != '\0') {
            if (*pos == '\"') {
                break;
            }
            if (*pos == '\\' && *(pos + 1) == '\"') {
                if (i < max_len - 1) {
                    output[i++] = '\"';
                }
                pos += 2;
            } else {
                if (i < max_len - 1) {
                    output[i++] = *pos;
                }
                pos++;
            }
        }
    } else {
        while (*pos != '\0' && *pos != ',' && *pos != '}' && *pos != ']' && 
               *pos != ' ' && *pos != '\t' && *pos != '\r' && *pos != '\n') {
            if (i < max_len - 1) {
                output[i++] = *pos;
            }
            pos++;
        }
    }
    output[i] = '\0';
}
 
void send_json_response(int clientSocket, int statusCode, const char* statusText, const char* jsonBody) {
    char response[RESPONSE_BUFFER_SIZE];
    snprintf(response, sizeof(response),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s", statusCode, statusText, jsonBody);
    
    send(clientSocket, response, strlen(response), 0);
}
 
int get_user_id(const char* token) {
    if (!token || strlen(token) == 0) return -1;
    for (int i = 0; i < userCount; i++) {
        if (strcmp(users[i].token, token) == 0) return users[i].id;
    }
    return -1;
}
 
// ==========================================
// ROUTES
// ==========================================
 
void handle_request(int clientSocket, const char* method, const char* path, const char* body) {
    printf("--> %s %s\n", method, path);
 
    // CORS Preflight
    if (strcmp(method, "OPTIONS") == 0) {
        send_json_response(clientSocket, 200, "OK", "{}");
        return;
    }
 
    if (strcmp(path, "/api/auth/register") == 0 && strcmp(method, "POST") == 0) {
        char firstName[50], lastName[50], email[100], phone[20], password[100];
        extract_json_value(body, "firstName", firstName, sizeof(firstName));
        extract_json_value(body, "lastName", lastName, sizeof(lastName));
        extract_json_value(body, "phone", phone, sizeof(phone));
        extract_json_value(body, "email", email, sizeof(email));
        extract_json_value(body, "password", password, sizeof(password));
 
        if (strlen(email) == 0 || strlen(password) == 0) {
            printf("[AUTH] Registration failed: Missing fields\n");
            send_json_response(clientSocket, 400, "Bad Request", "{\"error\":\"Missing fields\"}");
            return;
        }
 
        // Prevent duplicate registration (case-insensitive)
        for (int i = 0; i < userCount; i++) {
            if (str_case_cmp(users[i].email, email) == 0) {
                printf("[AUTH] Registration failed: Email '%s' already exists\n", email);
                send_json_response(clientSocket, 400, "Bad Request", "{\"error\":\"Email already registered\"}");
                return;
            }
        }
 
        if (userCount >= MAX_USERS) {
            send_json_response(clientSocket, 500, "Internal Server Error", "{\"error\":\"User limit reached\"}");
            return;
        }
 
        // Add user
        users[userCount].id = userCount + 1;
        strcpy(users[userCount].firstName, firstName);
        strcpy(users[userCount].lastName, lastName);
        strcpy(users[userCount].phone, phone);
        strcpy(users[userCount].email, email);
        
        // Hash password securely
        char hashed[65];
        hash_password(password, hashed);
        strcpy(users[userCount].password, hashed);
        
        strcpy(users[userCount].status, "Active");
        sprintf(users[userCount].token, "token_%d", users[userCount].id);
        
        printf("[AUTH] User registered: %s (ID: %d)\n", email, users[userCount].id);
        
        char json[200];
        snprintf(json, sizeof(json), "{\"message\":\"User registered\", \"token\":\"%s\"}", users[userCount].token);
 
        userCount++;
        save_users();
        send_json_response(clientSocket, 201, "Created", json);
        return;
    }
 
    if (strcmp(path, "/api/auth/login") == 0 && strcmp(method, "POST") == 0) {
        char email[100], password[100];
        extract_json_value(body, "email", email, sizeof(email));
        extract_json_value(body, "password", password, sizeof(password));
 
        char hashed[65];
        hash_password(password, hashed);
 
        for (int i = 0; i < userCount; i++) {
            if (str_case_cmp(users[i].email, email) == 0 && strcmp(users[i].password, hashed) == 0) {
                printf("[AUTH] Login successful: %s\n", email);
                char json[200];
                snprintf(json, sizeof(json), "{\"message\":\"Login successful\", \"token\":\"%s\"}", users[i].token);
                send_json_response(clientSocket, 200, "OK", json);
                return;
            }
        }
        printf("[AUTH] Login failed for email: %s\n", email);
        send_json_response(clientSocket, 401, "Unauthorized", "{\"error\":\"Invalid credentials\"}");
        return;
    }
 
    if (strcmp(path, "/api/auth/google") == 0 && strcmp(method, "POST") == 0) {
        char firstName[50], lastName[50], email[100];
        extract_json_value(body, "firstName", firstName, sizeof(firstName));
        extract_json_value(body, "lastName", lastName, sizeof(lastName));
        extract_json_value(body, "email", email, sizeof(email));
 
        if (strlen(email) == 0) {
            printf("[AUTH] Google Sign-In failed: Missing email\n");
            send_json_response(clientSocket, 400, "Bad Request", "{\"error\":\"Missing email\"}");
            return;
        }
 
        // Check if user already exists (case-insensitive)
        int existingUserIdx = -1;
        for (int i = 0; i < userCount; i++) {
            if (str_case_cmp(users[i].email, email) == 0) {
                existingUserIdx = i;
                break;
            }
        }
 
        char json[200];
        if (existingUserIdx != -1) {
            // Existing user: log in directly
            printf("[AUTH] Google login successful for existing user: %s\n", email);
            snprintf(json, sizeof(json), "{\"message\":\"Login successful\", \"token\":\"%s\"}", users[existingUserIdx].token);
            send_json_response(clientSocket, 200, "OK", json);
        } else {
            // New user: register automatically
            if (userCount >= MAX_USERS) {
                send_json_response(clientSocket, 500, "Internal Server Error", "{\"error\":\"User limit reached\"}");
                return;
            }
            users[userCount].id = userCount + 1;
            strcpy(users[userCount].firstName, firstName);
            strcpy(users[userCount].lastName, lastName);
            strcpy(users[userCount].phone, "Google User");
            strcpy(users[userCount].email, email);
            strcpy(users[userCount].password, "google_oauth"); // Special password indicator
            strcpy(users[userCount].status, "Active");
            sprintf(users[userCount].token, "token_%d", users[userCount].id);
 
            printf("[AUTH] Google user automatically registered: %s (ID: %d)\n", email, users[userCount].id);
            snprintf(json, sizeof(json), "{\"message\":\"User registered\", \"token\":\"%s\"}", users[userCount].token);
 
            userCount++;
            save_users();
            send_json_response(clientSocket, 201, "Created", json);
        }
        return;
    }
 
    if (strcmp(path, "/api/flights") == 0 && strcmp(method, "GET") == 0) {
        char json[16384] = "{\"flights\":[";
        for (int i = 0; i < flightCount; i++) {
            // Calculate confirmed count dynamically based on current bookings
            int confirmed = 0;
            for (int j = 0; j < bookingCount; j++) {
                if (bookings[j].flightId == flights[i].id && bookings[j].isConfirmed == 1) {
                    confirmed++;
                }
            }
            char fstr[256];
            snprintf(fstr, sizeof(fstr), "{\"id\":%d,\"flightNumber\":\"%s\",\"origin\":\"%s\",\"destination\":\"%s\",\"capacity\":%d,\"confirmed\":%d}%s",
                flights[i].id, flights[i].flightNumber, flights[i].origin, flights[i].destination, flights[i].capacity, confirmed,
                (i == flightCount - 1) ? "" : ",");
            strcat(json, fstr);
        }
        strcat(json, "]}");
        send_json_response(clientSocket, 200, "OK", json);
        return;
    }
 
    if (strcmp(path, "/api/bookings/occupied") == 0 && strcmp(method, "POST") == 0) {
        char flightIdStr[20], origin[50], destination[50], travelDate[30];
        extract_json_value(body, "flightId", flightIdStr, sizeof(flightIdStr));
        extract_json_value(body, "origin", origin, sizeof(origin));
        extract_json_value(body, "destination", destination, sizeof(destination));
        extract_json_value(body, "travelDate", travelDate, sizeof(travelDate));
 
        int flightId = atoi(flightIdStr);
 
        // ── DEBUG LOGGING: shows exactly what the seat check is searching for,
        // and what is currently stored in every booking record, so any
        // mismatch (origin/destination/travelDate string formatting) is visible.
        printf("[OCCUPIED] [QUERY] flightId=%d origin='%s' destination='%s' travelDate='%s'\n",
            flightId, origin, destination, travelDate);
        for (int dbg = 0; dbg < bookingCount; dbg++) {
            printf("[OCCUPIED] [DB ROW %d] flightId=%d origin='%s' destination='%s' travelDate='%s' seat='%s' confirmed=%d\n",
                dbg, bookings[dbg].flightId, bookings[dbg].origin, bookings[dbg].destination,
                bookings[dbg].travelDate, bookings[dbg].seatNumber, bookings[dbg].isConfirmed);
        }
 
        char json[16384] = "{\"occupied\":[";
        int count = 0;
        for (int i = 0; i < bookingCount; i++) {
            if (bookings[i].flightId == flightId && 
                strcmp(bookings[i].seatNumber, "") != 0 &&
                str_case_cmp(bookings[i].origin, origin) == 0 &&
                str_case_cmp(bookings[i].destination, destination) == 0 &&
                str_case_cmp(bookings[i].travelDate, travelDate) == 0 &&
                bookings[i].isConfirmed == 1) {
                
                if (count > 0) strcat(json, ",");
                char seatStr[30];
                snprintf(seatStr, sizeof(seatStr), "\"%s\"", bookings[i].seatNumber);
                strcat(json, seatStr);
                count++;
            }
        }
        strcat(json, "]}");
 
        printf("[OCCUPIED] [RESULT] %d seat(s) matched and returned as occupied\n", count);
 
        send_json_response(clientSocket, 200, "OK", json);
        return;
    }
 
    if (strcmp(path, "/api/bookings/enqueue") == 0 && strcmp(method, "POST") == 0) {
        char flightIdStr[20], token[100], seatNumber[10], firstName[50], lastName[50], passengerEmail[100];
        char origin[50], destination[50], travelDate[30], fareAmount[30];
        
        char paymentMethod[20], cardNumber[30], cardExpiry[10], cardCvv[10];
        char bankName[50], accountNumber[30], branchName[50], ifscCode[20], upiId[50];
        
        extract_json_value(body, "flightId", flightIdStr, sizeof(flightIdStr));
        extract_json_value(body, "token", token, sizeof(token));
        extract_json_value(body, "seatNumber", seatNumber, sizeof(seatNumber));
        extract_json_value(body, "firstName", firstName, sizeof(firstName));
        extract_json_value(body, "lastName", lastName, sizeof(lastName));
        extract_json_value(body, "passengerEmail", passengerEmail, sizeof(passengerEmail));
        extract_json_value(body, "origin", origin, sizeof(origin));
        extract_json_value(body, "destination", destination, sizeof(destination));
        extract_json_value(body, "travelDate", travelDate, sizeof(travelDate));
        extract_json_value(body, "fareAmount", fareAmount, sizeof(fareAmount));
        
        extract_json_value(body, "paymentMethod", paymentMethod, sizeof(paymentMethod));
        extract_json_value(body, "cardNumber", cardNumber, sizeof(cardNumber));
        extract_json_value(body, "cardExpiry", cardExpiry, sizeof(cardExpiry));
        extract_json_value(body, "cardCvv", cardCvv, sizeof(cardCvv));
        extract_json_value(body, "bankName", bankName, sizeof(bankName));
        extract_json_value(body, "accountNumber", accountNumber, sizeof(accountNumber));
        extract_json_value(body, "branchName", branchName, sizeof(branchName));
        extract_json_value(body, "ifscCode", ifscCode, sizeof(ifscCode));
        extract_json_value(body, "upiId", upiId, sizeof(upiId));
 
        int userId = get_user_id(token);
        if (userId == -1) {
            printf("[BOOKING] Enqueue failed: Invalid token '%s'\n", token);
            send_json_response(clientSocket, 401, "Unauthorized", "{\"error\":\"Invalid token. Please login again.\"}");
            return;
        }
 
        int flightId = atoi(flightIdStr);
        
        // Verbose Logging: Payment Initiation
        printf("[PAYMENT] [INITIATION] User ID: %d requests booking/payment for Flight ID: %d, Seat: %s, Date: %s, Route: %s -> %s\n",
               userId, flightId, seatNumber, travelDate, origin, destination);
 
        // ── DEBUG LOGGING: shows the EXACT strings being saved for this booking,
        // so you can compare them against what /occupied later searches for.
        printf("[ENQUEUE] [SAVE] flightId=%d origin='%s' destination='%s' travelDate='%s' seat='%s'\n",
            flightId, origin, destination, travelDate, seatNumber);
        
        // Verbose Logging: Payment Validation
        printf("[PAYMENT] [VALIDATION] Validating payment method: '%s'...\n", paymentMethod);
        
        int paymentValid = 1;
        char paymentErrorMsg[100] = "";
        
        if (strcmp(paymentMethod, "card") == 0) {
            int numDigits = 0;
            for (int i = 0; cardNumber[i]; i++) {
                if (cardNumber[i] >= '0' && cardNumber[i] <= '9') numDigits++;
            }
            if (numDigits < 15 || numDigits > 16) {
                paymentValid = 0;
                strcpy(paymentErrorMsg, "Invalid card number digit length.");
            } else if (strlen(cardExpiry) != 5 || cardExpiry[2] != '/') {
                paymentValid = 0;
                strcpy(paymentErrorMsg, "Invalid expiry date format. Expected MM/YY.");
            } else if (strlen(cardCvv) != 3) {
                paymentValid = 0;
                strcpy(paymentErrorMsg, "Invalid CVV. Expected 3 digits.");
            }
        } else if (strcmp(paymentMethod, "netbanking") == 0) {
            if (strlen(bankName) == 0) {
                paymentValid = 0;
                strcpy(paymentErrorMsg, "Bank name cannot be empty.");
            } else if (strlen(accountNumber) < 8) {
                paymentValid = 0;
                strcpy(paymentErrorMsg, "Account number too short.");
            } else if (strlen(ifscCode) != 11) {
                paymentValid = 0;
                strcpy(paymentErrorMsg, "IFSC code must be exactly 11 characters.");
            }
        } else if (strcmp(paymentMethod, "phonepe") == 0) {
            if (strchr(upiId, '@') == NULL) {
                paymentValid = 0;
                strcpy(paymentErrorMsg, "Invalid UPI ID. Must contain '@'.");
            }
        } else {
            paymentValid = 0;
            strcpy(paymentErrorMsg, "Unknown or missing payment method.");
        }
        
        if (!paymentValid) {
            // Verbose Logging: Payment Failure
            printf("[PAYMENT] [FAILURE] Payment validation failed. Reason: %s\n", paymentErrorMsg);
            char errJson[200];
            snprintf(errJson, sizeof(errJson), "{\"error\":\"Payment validation failed: %s\"}", paymentErrorMsg);
            send_json_response(clientSocket, 400, "Bad Request", errJson);
            return;
        }
        
        // Verbose Logging: Payment Success
        printf("[PAYMENT] [SUCCESS] Payment validation succeeded. Transaction ID: TXN-%ld. Processing booking...\n", (long)time(NULL));
 
        Flight* f = NULL;
        for (int i = 0; i < flightCount; i++) {
            if (flights[i].id == flightId) { f = &flights[i]; break; }
        }
 
        // Dynamically register flight if it is a fallback flight not present in backend list
        if (!f) {
            if (flightCount < MAX_FLIGHTS) {
                f = &flights[flightCount];
                f->id = flightId;
                snprintf(f->flightNumber, sizeof(f->flightNumber), "AQ-%d", flightId);
                strcpy(f->origin, origin);
                strcpy(f->destination, destination);
                f->capacity = 50;
                f->confirmedCount = 0;
                flightCount++;
                printf("[FLIGHT] Dynamically registered flight ID %d (%s -> %s)\n", f->id, f->origin, f->destination);
            } else {
                printf("[BOOKING] Enqueue failed: Maximum flight limit reached.\n");
                send_json_response(clientSocket, 500, "Internal Server Error", "{\"error\":\"Flight limit reached\"}");
                return;
            }
        }
 
        if (bookingCount >= MAX_BOOKINGS) {
            send_json_response(clientSocket, 500, "Internal Server Error", "{\"error\":\"Booking limit reached\"}");
            return;
        }
 
        Booking* b = &bookings[bookingCount];
        b->id = bookingCount + 1;
        b->userId = userId;
        b->flightId = flightId;
        b->timestamp = (long)time(NULL) + bookingCount;
        strcpy(b->seatNumber, seatNumber);
        strcpy(b->firstName, firstName);
        strcpy(b->lastName, lastName);
        strcpy(b->origin, origin);
        strcpy(b->destination, destination);
        strcpy(b->travelDate, travelDate);
        strcpy(b->paymentStatus, "SUCCESS");
        strcpy(b->passengerEmail, passengerEmail);
        
        // Seat taken checks isolated by flight, route, date, and seat number (case-insensitive)
        int seatTaken = 0;
        for (int i = 0; i < bookingCount; i++) {
            if (bookings[i].flightId == flightId && 
                str_case_cmp(bookings[i].seatNumber, seatNumber) == 0 && 
                str_case_cmp(bookings[i].origin, origin) == 0 &&
                str_case_cmp(bookings[i].destination, destination) == 0 &&
                str_case_cmp(bookings[i].travelDate, travelDate) == 0 &&
                bookings[i].isConfirmed == 1) {
                
                seatTaken = 1;
                break;
            }
        }
 
        if (!seatTaken) {
            b->isConfirmed = 1;
            f->confirmedCount++;
        } else {
            b->isConfirmed = 0; // Waitlisted
        }
        
        snprintf(b->ticketNumber, sizeof(b->ticketNumber), "AQ-TKT-%d-%d", (int)time(NULL), b->id);
        
        // Verbose Logging: Booking Creation after Payment
        printf("[BOOKING] [CREATION] Creating booking record ID: %d for User ID: %d, Flight ID: %d, Seat: %s, travelDate: %s, Status: %s\n",
               b->id, b->userId, b->flightId, b->seatNumber, b->travelDate, b->isConfirmed ? "CONFIRMED" : "WAITLISTED");
        
        // Verbose Logging: Ticket Generation after Booking
        printf("[TICKET] [GENERATION] Generated ticket number: %s for Booking ID: %d\n", b->ticketNumber, b->id);
        
        bookingCount++;
        save_bookings();
 
        // Trigger Booking Confirmation Email
        User* bu = NULL;
        for (int i = 0; i < userCount; i++) {
            if (users[i].id == userId) {
                bu = &users[i];
                break;
            }
        }
        if (bu) {
            char bookingTimeStr[100];
            time_t rawtime = time(NULL);
            struct tm * timeinfo = localtime(&rawtime);
            strftime(bookingTimeStr, sizeof(bookingTimeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
 
            const char* keys[] = {
                "PASSENGER_NAME", "BOOKING_ID", "SOURCE", "DESTINATION",
                "TRAVEL_DATE", "SEAT_NUMBER", "FARE_AMOUNT", "BOOKING_TIME", "TICKET_STATUS"
            };
            char passenger_name[120];
            snprintf(passenger_name, sizeof(passenger_name), "%s %s", b->firstName, b->lastName);
            
            const char* values[] = {
                passenger_name, b->ticketNumber, b->origin, b->destination,
                b->travelDate, b->seatNumber, strlen(fareAmount) > 0 ? fareAmount : "₹4,500", bookingTimeStr,
                b->isConfirmed ? "CONFIRMED" : "WAITLISTED"
            };
            
            trigger_email_async("booking_confirmation", b->passengerEmail[0] ? b->passengerEmail : bu->email, keys, values, 9);
        }
 
        char res[300];
        snprintf(res, sizeof(res), "{\"message\":\"Booking processed\", \"bookingId\":%d, \"ticketNumber\":\"%s\", \"status\":\"%s\"}", 
            b->id, b->ticketNumber, b->isConfirmed ? "CONFIRMED" : "WAITLISTED");
        send_json_response(clientSocket, 201, "Created", res);
        return;
    }
 
    if (strncmp(path, "/api/bookings/status/", 21) == 0 && strcmp(method, "GET") == 0) {
        const char* ticketNumber = path + 21;
        
        Booking* b = NULL;
        for (int i = 0; i < bookingCount; i++) {
            if (str_case_cmp(bookings[i].ticketNumber, ticketNumber) == 0) { b = &bookings[i]; break; }
        }
 
        if (!b) {
            send_json_response(clientSocket, 404, "Not Found", "{\"error\":\"Booking not found\"}");
            return;
        }
 
        int position = 0;
        if (b->isConfirmed == 0) {
            // Calculate FIFO Queue Position, isolated by flight, route, date, and seat number (case-insensitive)
            for (int i = 0; i < bookingCount; i++) {
                if (bookings[i].flightId == b->flightId && 
                    bookings[i].isConfirmed == 0 && 
                    str_case_cmp(bookings[i].seatNumber, b->seatNumber) == 0 &&
                    str_case_cmp(bookings[i].origin, b->origin) == 0 &&
                    str_case_cmp(bookings[i].destination, b->destination) == 0 &&
                    str_case_cmp(bookings[i].travelDate, b->travelDate) == 0) {
                    
                    if (bookings[i].timestamp <= b->timestamp) {
                        position++;
                    }
                }
            }
        }
 
        char res[300];
        snprintf(res, sizeof(res), "{\"ticketNumber\":\"%s\", \"seatNumber\":\"%s\", \"firstName\":\"%s\", \"lastName\":\"%s\", \"status\":\"%s\", \"queuePosition\":%d}", 
            b->ticketNumber, b->seatNumber, b->firstName, b->lastName, b->isConfirmed == 1 ? "CONFIRMED" : (b->isConfirmed == -1 ? "CANCELLED" : "WAITLISTED"), position);
        send_json_response(clientSocket, 200, "OK", res);
        return;
    }
 
    if (strcmp(path, "/api/bookings/cancel") == 0 && strcmp(method, "POST") == 0) {
        char ticketNumber[30];
        extract_json_value(body, "ticketNumber", ticketNumber, sizeof(ticketNumber));
 
        Booking* b = NULL;
        for (int i = 0; i < bookingCount; i++) {
            if (str_case_cmp(bookings[i].ticketNumber, ticketNumber) == 0) { b = &bookings[i]; break; }
        }
 
        if (!b || b->isConfirmed == -1) {
            send_json_response(clientSocket, 400, "Bad Request", "{\"error\":\"Invalid booking\"}");
            return;
        }
 
        int wasConfirmed = b->isConfirmed == 1;
        b->isConfirmed = -1; // CANCELLED
        save_bookings();
 
        if (wasConfirmed) {
            for (int i = 0; i < flightCount; i++) {
                if (flights[i].id == b->flightId) {
                    flights[i].confirmedCount--;
                    break;
                }
            }
            // FIFO promotion isolated by flight, route, date, and seat number (case-insensitive)
            process_waitlist(b->flightId, b->seatNumber, b->origin, b->destination, b->travelDate);
        }
 
        // Find user email and send cancellation notification
        User* cu = NULL;
        for (int i = 0; i < userCount; i++) {
            if (users[i].id == b->userId) {
                cu = &users[i];
                break;
            }
        }
        if (cu) {
            char cancelTimeStr[100];
            time_t rawtime = time(NULL);
            struct tm * timeinfo = localtime(&rawtime);
            strftime(cancelTimeStr, sizeof(cancelTimeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
 
            const char* keys[] = {
                "BOOKING_ID", "ROUTE", "TRAVEL_DATE", "CANCELLATION_DATE", "CANCELLATION_STATUS"
            };
            char route[120];
            snprintf(route, sizeof(route), "%s -> %s", b->origin, b->destination);
 
            const char* values[] = {
                b->ticketNumber, route, b->travelDate, cancelTimeStr, "CANCELLED"
            };
 
            trigger_email_async("ticket_cancellation", b->passengerEmail[0] ? b->passengerEmail : cu->email, keys, values, 5);
        }
 
        send_json_response(clientSocket, 200, "OK", "{\"message\":\"Cancelled successfully. Queue updated.\"}");
        return;
    }
 
    if (strcmp(path, "/api/bookings/refund") == 0 && strcmp(method, "POST") == 0) {
        char ticketNumber[30], email[100];
        extract_json_value(body, "ticketNumber", ticketNumber, sizeof(ticketNumber));
        extract_json_value(body, "email", email, sizeof(email));
 
        Booking* b = NULL;
        for (int i = 0; i < bookingCount; i++) {
            if (str_case_cmp(bookings[i].ticketNumber, ticketNumber) == 0) {
                b = &bookings[i];
                break;
            }
        }
 
        if (!b) {
            send_json_response(clientSocket, 404, "Not Found", "{\"error\":\"Booking not found.\"}");
            return;
        }
 
        // Validate user email matches
        User* u = NULL;
        for (int i = 0; i < userCount; i++) {
            if (users[i].id == b->userId) {
                u = &users[i];
                break;
            }
        }
 
        if (!u || str_case_cmp(u->email, email) != 0) {
            send_json_response(clientSocket, 400, "Bad Request", "{\"error\":\"Email does not match booking records.\"}");
            return;
        }
 
        if (b->isConfirmed != -1) {
            send_json_response(clientSocket, 400, "Bad Request", "{\"error\":\"Booking is not cancelled. Please cancel your booking before requesting a refund.\"}");
            return;
        }
 
        if (strcmp(b->paymentStatus, "REFUNDED") == 0) {
            send_json_response(clientSocket, 400, "Bad Request", "{\"error\":\"Refund has already been processed for this booking.\"}");
            return;
        }
 
        // Process Refund
        strcpy(b->paymentStatus, "REFUNDED");
        save_bookings();
 
        // Trigger Refund Email
        char txnId[50];
        snprintf(txnId, sizeof(txnId), "TXN-REF-%ld", (long)time(NULL));
 
        const char* keys[] = {
            "BOOKING_ID", "REFUND_AMOUNT", "TRANSACTION_DETAILS", "REFUND_STATUS"
        };
        const char* values[] = {
            b->ticketNumber, "₹4,500", txnId, "SUCCESS"
        };
 
        trigger_email_async("refund_confirmation", b->passengerEmail[0] ? b->passengerEmail : u->email, keys, values, 4);
 
        send_json_response(clientSocket, 200, "OK", "{\"message\":\"Refund processed successfully. A confirmation email has been sent.\"}");
        return;
    }
 
    if (strcmp(path, "/api/user/dashboard") == 0 && strcmp(method, "POST") == 0) {
        char token[100];
        extract_json_value(body, "token", token, sizeof(token));
        int userId = get_user_id(token);
        if (userId == -1) {
            printf("[DASHBOARD] Access failed: Invalid token '%s'\n", token);
            send_json_response(clientSocket, 401, "Unauthorized", "{\"error\":\"Invalid token\"}");
            return;
        }
 
        User* u = NULL;
        for (int i = 0; i < userCount; i++) {
            if (users[i].id == userId) {
                u = &users[i];
                break;
            }
        }
 
        if (!u) {
            send_json_response(clientSocket, 404, "Not Found", "{\"error\":\"User not found\"}");
            return;
        }
 
        char json[16384];
        snprintf(json, sizeof(json), "{\"id\":%d,\"firstName\":\"%s\",\"lastName\":\"%s\",\"email\":\"%s\",\"phone\":\"%s\",\"status\":\"%s\",\"bookings\":[",
            u->id, u->firstName, u->lastName, u->email, u->phone, u->status);
        int count = 0;
        for (int i = 0; i < bookingCount; i++) {
            if (bookings[i].userId == userId) {
                if (count > 0) strcat(json, ",");
                Flight* f = NULL;
                for (int j = 0; j < flightCount; j++) {
                    if (flights[j].id == bookings[i].flightId) { f = &flights[j]; break; }
                }
                char bstr[500];
                int position = 0;
                if (bookings[i].isConfirmed == 0) {
                    for (int k = 0; k < bookingCount; k++) {
                        if (bookings[k].flightId == bookings[i].flightId && 
                            bookings[k].isConfirmed == 0 && 
                            str_case_cmp(bookings[k].seatNumber, bookings[i].seatNumber) == 0 && 
                            str_case_cmp(bookings[k].origin, bookings[i].origin) == 0 &&
                            str_case_cmp(bookings[k].destination, bookings[i].destination) == 0 &&
                            str_case_cmp(bookings[k].travelDate, bookings[i].travelDate) == 0 &&
                            bookings[k].timestamp <= bookings[i].timestamp) {
                            
                            position++;
                        }
                    }
                }
                snprintf(bstr, sizeof(bstr), "{\"ticketNumber\":\"%s\",\"flightNumber\":\"%s\",\"origin\":\"%s\",\"destination\":\"%s\",\"seatNumber\":\"%s\",\"firstName\":\"%s\",\"lastName\":\"%s\",\"travelDate\":\"%s\",\"status\":\"%s\",\"queuePosition\":%d}",
                    bookings[i].ticketNumber, f ? f->flightNumber : "", bookings[i].origin, bookings[i].destination, bookings[i].seatNumber, bookings[i].firstName, bookings[i].lastName,
                    bookings[i].travelDate,
                    bookings[i].isConfirmed == 1 ? "CONFIRMED" : (bookings[i].isConfirmed == -1 ? "CANCELLED" : "WAITLISTED"), position);
                strcat(json, bstr);
                count++;
            }
        }
        strcat(json, "]}");
        send_json_response(clientSocket, 200, "OK", json);
        return;
    }
 
    // Static file serving fallback
    if (strcmp(method, "GET") == 0) {
        char cleanFilename[256];
        strncpy(cleanFilename, path[0] == '/' ? path + 1 : path, sizeof(cleanFilename) - 1);
        cleanFilename[sizeof(cleanFilename) - 1] = '\0';
        char* q = strchr(cleanFilename, '?');
        if (q) *q = '\0';
        char* h = strchr(cleanFilename, '#');
        if (h) *h = '\0';

        if (strlen(cleanFilename) == 0) {
            strcpy(cleanFilename, "index.html");
        }

        if (strstr(cleanFilename, "..") == NULL) {
            char filepath[512];
            snprintf(filepath, sizeof(filepath), "%s%s", email_dir_prefix, cleanFilename);
            FILE* file = fopen(filepath, "rb");
            if (file) {
                fseek(file, 0, SEEK_END);
                long fileSize = ftell(file);
                fseek(file, 0, SEEK_SET);

                const char* ext = strrchr(cleanFilename, '.');
                const char* contentType = "application/octet-stream";
                if (ext) {
                    if (str_case_cmp(ext, ".html") == 0) contentType = "text/html";
                    else if (str_case_cmp(ext, ".css") == 0) contentType = "text/css";
                    else if (str_case_cmp(ext, ".js") == 0) contentType = "application/javascript";
                    else if (str_case_cmp(ext, ".png") == 0) contentType = "image/png";
                    else if (str_case_cmp(ext, ".jpg") == 0 || str_case_cmp(ext, ".jpeg") == 0) contentType = "image/jpeg";
                    else if (str_case_cmp(ext, ".gif") == 0) contentType = "image/gif";
                    else if (str_case_cmp(ext, ".svg") == 0) contentType = "image/svg+xml";
                    else if (str_case_cmp(ext, ".ico") == 0) contentType = "image/x-icon";
                }

                char responseHeader[512];
                snprintf(responseHeader, sizeof(responseHeader),
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: %s\r\n"
                    "Content-Length: %ld\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "Connection: close\r\n"
                    "\r\n", contentType, fileSize);
                
                send(clientSocket, responseHeader, strlen(responseHeader), 0);

                char fileBuffer[4096];
                size_t bytesRead;
                while ((bytesRead = fread(fileBuffer, 1, sizeof(fileBuffer), file)) > 0) {
                    send(clientSocket, fileBuffer, bytesRead, 0);
                }
                fclose(file);
                return;
            }
        }
    }

    // Default 404
    send_json_response(clientSocket, 404, "Not Found", "{\"error\":\"Not Found\"}");
}
 
// ==========================================
// SERVER INITIALIZATION
// ==========================================
 
void seed_data() {
    flights[flightCount++] = (Flight){1, "AQ-101", "New York", "London", 2, 0}; // Capacity 2 for easy FIFO testing
    flights[flightCount++] = (Flight){2, "AQ-202", "Tokyo", "Seoul", 50, 0};
}
 
int main() {
    srand((unsigned int)time(NULL));
    setvbuf(stdout, NULL, _IONBF, 0);
    detect_file_paths();
    seed_data();
    load_users();
    load_bookings();
 
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed.\n");
        return 1;
    }
#endif
 
    int serverSocket, clientSocket;
    struct sockaddr_in serverAddr, clientAddr;
    
#ifdef _WIN32
    int clientLen = sizeof(clientAddr);
#else
    socklen_t clientLen = sizeof(clientAddr);
#endif
 
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        printf("Socket creation failed.\n");
        return 1;
    }
 
    // Allow socket address reuse
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
 
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);
 
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        printf("Bind failed.\n");
        return 1;
    }
 
    if (listen(serverSocket, 10) < 0) {
        printf("Listen failed.\n");
        return 1;
    }
 
    printf("AeroQueue C Backend running on port %d\n", PORT);
    printf("FIFO Waitlist system active.\n");
 
    while (1) {
        clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientSocket < 0) continue;
 
        char buffer[BUFFER_SIZE] = {0};
        int bytesRead = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
        if (bytesRead <= 0) {
#ifdef _WIN32
            closesocket(clientSocket);
#else
            close(clientSocket);
#endif
            continue;
        }
 
        // Parse content length if present
        char* contentLengthPos = strstr(buffer, "Content-Length:");
        if (!contentLengthPos) {
            contentLengthPos = strstr(buffer, "content-length:");
        }
        
        int contentLength = 0;
        if (contentLengthPos) {
            sscanf(contentLengthPos + 15, "%d", &contentLength);
        }
 
        char* body = strstr(buffer, "\r\n\r\n");
        if (body) {
            body += 4;
            int headerLen = body - buffer;
            int bodyBytesRead = bytesRead - headerLen;
            while (bodyBytesRead < contentLength && bytesRead < BUFFER_SIZE - 1) {
                int n = recv(clientSocket, buffer + bytesRead, BUFFER_SIZE - 1 - bytesRead, 0);
                if (n <= 0) break;
                bytesRead += n;
                bodyBytesRead += n;
            }
            buffer[bytesRead] = '\0'; // ensure null-termination
        } else {
            body = "";
        }
 
        // Simple HTTP Parser
        char method[10], path[255];
        sscanf(buffer, "%9s %254s", method, path);
 
        handle_request(clientSocket, method, path, body);
 
#ifdef _WIN32
        closesocket(clientSocket);
#else
        close(clientSocket);
#endif
    }
 
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}