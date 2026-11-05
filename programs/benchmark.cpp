// Self-checking RV64I(M) benchmark: each kernel stresses one CPI-stack category.
// RISC-V: main returns 0 = pass, N = kernel N failed (exit code via tohost).
// Host:   g++ -DHOST -O1 programs/benchmark.cpp -o ref && ./ref   -> prints the expected checksums
// Kernels 1-6 are pure RV64I (no * / %); kernels 7-11 need M and are skipped on -march=rv64i.
#include <cstdint>

#if defined(HOST) || defined(__riscv_mul)
#define HAS_M 1
#endif

// stops -O1 from computing a value at compile time
static inline uint64_t opaque(uint64_t x) {
    asm volatile("" : "+r"(x));
    return x;
}

// xorshift32: random data without multiply; state carries across kernels, so run order matters
static uint32_t rng = 0x2545F491u;
static uint32_t rnd() {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

// random value in [0, n) without %, by rejection on a power-of-two mask
static uint32_t rnd_below(uint32_t n) {
    uint32_t mask = 1;
    while (mask < n)
        mask <<= 1;
    mask -= 1;
    uint32_t v;
    do {
        v = rnd() & mask;
    } while (v >= n);
    return v;
}

// ---- 1. pointer chase: every iteration is a load-use on p->next ----------------------------
constexpr int CHASE_NODES = 256;
constexpr int CHASE_LAPS  = 4;
struct Node {
    Node*    next;
    uint64_t val;
};
static Node     nodes[CHASE_NODES];
static uint16_t order[CHASE_NODES];

static uint64_t k_pointer_chase() {
    for (int i = 0; i < CHASE_NODES; i++)
        order[i] = uint16_t(i);
    for (int i = CHASE_NODES - 1; i > 0; i--) {         // Sattolo shuffle -> one single random cycle
        uint32_t j = rnd_below(uint32_t(i));
        uint16_t t = order[i];
        order[i]   = order[j];
        order[j]   = t;
    }
    for (int i = 0; i < CHASE_NODES; i++) {
        nodes[order[i]].next = &nodes[order[(i + 1) & (CHASE_NODES - 1)]];
        nodes[order[i]].val  = rnd();
    }
    uint64_t sum = 0;
    Node*    p   = &nodes[opaque(0)];
    for (int i = 0; i < CHASE_NODES * CHASE_LAPS; i++) {
        sum += p->val ^ uint64_t(i);
        p = p->next;
    }
    return sum;
}

// ---- 2. insertion sort: data-dependent branches -> branch flushes --------------------------
constexpr int SORT_N = 128;
static uint32_t sort_a[SORT_N];

static uint64_t k_insertion_sort() {
    for (int i = 0; i < SORT_N; i++)
        sort_a[i] = rnd();
    for (int i = 1; i < SORT_N; i++) {
        uint32_t key = sort_a[i];
        int      j   = i - 1;
        while (j >= 0 && sort_a[j] > key) {
            sort_a[j + 1] = sort_a[j];
            j--;
        }
        sort_a[j + 1] = key;
    }
    uint64_t h = 0;
    for (int i = 0; i < SORT_N; i++) {
        if (i > 0 && sort_a[i - 1] > sort_a[i])
            return 0xBAD;                               // not sorted
        h = (h << 5) ^ (h >> 59) ^ sort_a[i];
    }
    return h;
}

// ---- 3. binary search: unpredictable branches + loads --------------------------------------
constexpr int BS_N       = 512;
constexpr int BS_QUERIES = 256;
static uint32_t bs_a[BS_N];

static uint64_t k_binary_search() {
    uint32_t v = 0;
    for (int i = 0; i < BS_N; i++) {                    // sorted, strictly increasing
        v += 1 + (rnd() & 7);
        bs_a[i] = v;
    }
    uint64_t found = 0, pos_sum = 0;
    for (int q = 0; q < BS_QUERIES; q++) {
        uint32_t key = rnd_below(v + 1);
        int lo = 0, hi = BS_N - 1;
        while (lo <= hi) {
            int mid = (lo + hi) >> 1;
            if (bs_a[mid] == key) {
                found++;
                pos_sum += uint64_t(mid);
                break;
            }
            if (bs_a[mid] < key) lo = mid + 1;
            else                 hi = mid - 1;
        }
    }
    return (found << 32) | pos_sum;
}

// ---- 4. CRC32 (bitwise): long ALU dependency chains -> forwarding --------------------------
constexpr int CRC_BYTES = 256;
static uint8_t crc_buf[CRC_BYTES];

static uint64_t k_crc32() {
    for (int i = 0; i < CRC_BYTES; i++)
        crc_buf[i] = uint8_t(rnd());
    uint32_t crc = 0xFFFFFFFFu;
    for (int i = 0; i < CRC_BYTES; i++) {
        crc ^= crc_buf[i];
        for (int b = 0; b < 8; b++)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1)));
    }
    return ~crc;
}

// ---- 5. recursive fibonacci: jal/jalr + stack loads/stores around every call ---------------
__attribute__((noinline)) static uint64_t fib(uint64_t n) {
    return n < 2 ? n : fib(n - 1) + fib(n - 2);
}

static uint64_t k_fib() {
    return fib(opaque(14));                             // 1219 calls
}

// ---- 6. copy + byte sieve: every load/store width, sequential and strided ------------------
constexpr int COPY_WORDS = 256;
constexpr int SIEVE_N    = 1024;
static uint64_t src64[COPY_WORDS], dst64[COPY_WORDS];
static uint8_t  sieve[SIEVE_N];

static uint64_t k_mem() {
    for (int i = 0; i < COPY_WORDS; i++)
        src64[i] = (uint64_t(rnd()) << 32) | rnd();
    for (int i = 0; i < COPY_WORDS; i++)                // sd/ld stream
        dst64[i] = src64[i];
    uint16_t* h = reinterpret_cast<uint16_t*>(dst64);   // sh/lhu over the same buffer
    for (int i = 0; i < COPY_WORDS * 4; i += 3)
        h[i] = uint16_t(h[i] + 1);
    uint64_t sum = 0;
    for (int i = 0; i < COPY_WORDS; i++)
        sum ^= dst64[i] + uint64_t(i);

    for (int i = 0; i < SIEVE_N; i++)                   // sb, strided
        sieve[i] = 1;
    sieve[0] = sieve[1] = 0;
    for (int p = 2; p < 32; p++)                        // 32 = sqrt(SIEVE_N); p*p would need a multiply
        if (sieve[p])
            for (int m = p + p; m < SIEVE_N; m += p)
                sieve[m] = 0;
    uint64_t primes = 0;
    for (int i = 0; i < SIEVE_N; i++)
        primes += sieve[i];
    return sum ^ (primes << 48);
}

#ifdef HAS_M
static uint64_t rnd64() {
    uint64_t hi = rnd();
    return (hi << 32) | rnd();
}

// ---- 7. FNV-1a hash: xor -> mul -> xor chain, every mul result is needed next -> MulDiv RAW -
constexpr int FNV_BYTES = 1024;
static uint8_t fnv_buf[FNV_BYTES];

static uint64_t k_mul_chain() {
    for (int i = 0; i < FNV_BYTES; i++)
        fnv_buf[i] = uint8_t(rnd());
    uint64_t prime = opaque(0x100000001b3ull);         // runtime value: forces a real mul, not shifts
    uint64_t h     = 0xcbf29ce484222325ull;
    for (int i = 0; i < FNV_BYTES; i++)
        h = (h ^ fnv_buf[i]) * prime;
    return h;
}

// ---- 8. matmul, k unrolled x4: 4 independent muls back to back -> pipelined MUL, WB order ---
constexpr int MM_N = 12;
static uint32_t mm_a[MM_N][MM_N], mm_b[MM_N][MM_N];
static uint64_t mm_c[MM_N][MM_N];

static uint64_t k_matmul() {
    for (int i = 0; i < MM_N; i++)
        for (int j = 0; j < MM_N; j++) {
            mm_a[i][j] = rnd();
            mm_b[i][j] = rnd();
        }
    for (int i = 0; i < MM_N; i++)
        for (int j = 0; j < MM_N; j++) {
            uint64_t s = 0;
            for (int k = 0; k < MM_N; k += 4) {
                uint64_t p0 = uint64_t(mm_a[i][k    ]) * mm_b[k    ][j];
                uint64_t p1 = uint64_t(mm_a[i][k + 1]) * mm_b[k + 1][j];
                uint64_t p2 = uint64_t(mm_a[i][k + 2]) * mm_b[k + 2][j];
                uint64_t p3 = uint64_t(mm_a[i][k + 3]) * mm_b[k + 3][j];
                s += (p0 + p1) + (p2 + p3);
            }
            mm_c[i][j] = s;
        }
    uint64_t h = 0;
    for (int i = 0; i < MM_N; i++)
        for (int j = 0; j < MM_N; j++)
            h = (h << 7 | h >> 57) ^ mm_c[i][j];
    return h;
}

// ---- 9. 128-bit products: mul + mulhu / mulh pairs on the same operands --------------------
static uint64_t k_mul_high() {
    uint64_t x = rnd64(), y = rnd64() | 1, acc = 0;
    for (int i = 0; i < 256; i++) {
        unsigned __int128 p = (unsigned __int128)x * y;          // mul + mulhu
        x = uint64_t(p) ^ uint64_t(p >> 64);
        __int128 s = (__int128)int64_t(x) * int64_t(y);          // mul + mulh (signed)
        acc += uint64_t(s >> 64) ^ uint64_t(s);
        y += 0x9E3779B97F4A7C15ull;
    }
    int32_t w = int32_t(opaque(acc));
    for (int i = 0; i < 64; i++)                                 // mulw: 32-bit, sign-extended
        w = w * int32_t(opaque(0x01000193)) + int32_t(i);
    return acc ^ uint64_t(int64_t(w));
}

// ---- 10. gcd + radix conversion: dependent divu/remu chains on a non-pipelined DIV ---------
static uint64_t k_div() {
    uint64_t sum = 0;
    for (int i = 0; i < 48; i++) {                                // Euclid: each rem needs the last
        uint64_t a = rnd64(), b = rnd64() >> (rnd() & 31);
        while (b) {
            uint64_t t = a % b;
            a = b;
            b = t;
        }
        sum += a;
    }
    for (int i = 0; i < 96; i++) {                                // digits in a runtime radix 3..10
        uint64_t n = rnd64(), r = opaque(3 + (rnd() & 7));
        while (n) {
            sum += n % r;
            n /= r;
        }
    }
    int64_t s32 = 0;
    for (int i = 0; i < 96; i++) {                                // divw / remw, mixed signs
        int32_t a = int32_t(rnd()), b = int32_t(rnd()) >> (rnd() & 23);
        if (b == 0) b = 7;
        s32 += a / b + a % b;
    }
    return sum ^ uint64_t(s32);
}

// ---- 11. M-extension edge cases: /0, INT_MIN/-1, sign mixes; target uses the raw instruction -
enum MOp { MULH_, MULHSU_, MULHU_, MULW_, DIV_, DIVU_, REM_, REMU_, DIVW_, DIVUW_, REMW_, REMUW_, NMOP };

#ifdef HOST
static inline uint64_t sx32(uint64_t v) { return uint64_t(int64_t(int32_t(uint32_t(v)))); }
static uint64_t mop(int op, uint64_t a, uint64_t b) {             // spec reference (no C UB)
    int64_t  sa = int64_t(a), sb = int64_t(b);
    int32_t  a32 = int32_t(a), b32 = int32_t(b);
    uint32_t ua = uint32_t(a), ub = uint32_t(b);
    switch (op) {
        case MULH_  : return uint64_t((__int128(sa) * __int128(sb)) >> 64);
        case MULHSU_: return uint64_t((__int128(sa) * __int128(b)) >> 64);
        case MULHU_ : return uint64_t(((unsigned __int128)a * b) >> 64);
        case MULW_  : return sx32(ua * ub);
        case DIV_   : return b == 0 ? ~0ull : (sa == INT64_MIN && sb == -1) ? a : uint64_t(sa / sb);
        case DIVU_  : return b == 0 ? ~0ull : a / b;
        case REM_   : return b == 0 ? a : (sa == INT64_MIN && sb == -1) ? 0 : uint64_t(sa % sb);
        case REMU_  : return b == 0 ? a : a % b;
        case DIVW_  : return b32 == 0 ? ~0ull : (a32 == INT32_MIN && b32 == -1) ? sx32(ua) : sx32(uint32_t(a32 / b32));
        case DIVUW_ : return ub == 0 ? ~0ull : sx32(ua / ub);
        case REMW_  : return b32 == 0 ? sx32(ua) : (a32 == INT32_MIN && b32 == -1) ? 0 : sx32(uint32_t(a32 % b32));
        case REMUW_ : return ub == 0 ? sx32(ua) : sx32(ua % ub);
    }
    return 0;
}
#else
#define RV(ins) asm volatile(ins " %0, %1, %2" : "=r"(r) : "r"(a), "r"(b)); return r
static uint64_t mop(int op, uint64_t a, uint64_t b) {
    uint64_t r;
    switch (op) {
        case MULH_  : RV("mulh");
        case MULHSU_: RV("mulhsu");
        case MULHU_ : RV("mulhu");
        case MULW_  : RV("mulw");
        case DIV_   : RV("div");
        case DIVU_  : RV("divu");
        case REM_   : RV("rem");
        case REMU_  : RV("remu");
        case DIVW_  : RV("divw");
        case DIVUW_ : RV("divuw");
        case REMW_  : RV("remw");
        case REMUW_ : RV("remuw");
    }
    return 0;
}
#endif

static const uint64_t edge_vals[] = {
    0, 1, ~0ull, 0x8000000000000000ull, 0x7fffffffffffffffull,
    0x0000000080000000ull, 0xffffffff80000000ull, 0x00000000ffffffffull,
    0x123456789abcdef0ull, 0xfedcba9876543210ull,
};
constexpr int N_EDGE = int(sizeof(edge_vals) / sizeof(edge_vals[0]));

static uint64_t k_muldiv_edges() {
    uint64_t h = 0;
    for (int op = 0; op < NMOP; op++)
        for (int i = 0; i < N_EDGE; i++)
            for (int j = 0; j < N_EDGE; j++)
                h = (h << 9 | h >> 55) ^ mop(op, opaque(edge_vals[i]), opaque(edge_vals[j]));
    return h;
}
#endif // HAS_M

// ---- harness --------------------------------------------------------------------------------
struct Kernel {
    const char* name;
    uint64_t (*run)();
    uint64_t expected;                                  // from the host run (-DHOST)
};

static const Kernel kernels[] = {
    {"pointer_chase",  k_pointer_chase,  0x000001e411216678},
    {"insertion_sort", k_insertion_sort, 0x6a2a480582955380},
    {"binary_search",  k_binary_search,  0x0000004000003b63},
    {"crc32",          k_crc32,          0x00000000f66061ea},
    {"fib",            k_fib,            0x0000000000000179},
    {"mem",            k_mem,            0xa757b0352a6877a9},
#ifdef HAS_M
    {"mul_chain",      k_mul_chain,      0x34e208f53062daf2},
    {"matmul",         k_matmul,         0xd822eaf16965b507},
    {"mul_high",       k_mul_high,       0x18db73a56c450620},
    {"div",            k_div,            0xffffffffcaff2b09},
    {"muldiv_edges",   k_muldiv_edges,   0x7fb03a30cdc88bf6},
#endif
};
constexpr int N_KERNELS = int(sizeof(kernels) / sizeof(kernels[0]));

#ifdef HOST
#include <cstdio>
int main() {
    for (int i = 0; i < N_KERNELS; i++)
        std::printf("%-15s 0x%016llx\n", kernels[i].name, (unsigned long long)kernels[i].run());
    return 0;
}
#else
int main() {
    for (int i = 0; i < N_KERNELS; i++)
        if (kernels[i].run() != kernels[i].expected)
            return i + 1;
    return 0;
}
#endif
