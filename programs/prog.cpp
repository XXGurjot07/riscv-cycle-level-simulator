// Self-checking RV64I benchmark: each kernel stresses one CPI-stack category.
// RISC-V: main returns 0 = pass, N = kernel N failed (exit code via tohost).
// Host:   g++ -DHOST -O1 programs/benchmark.cpp -o ref && ./ref   -> prints the expected checksums
// Rules: no * / % (RV64I has no M, and -nostdlib has no libgcc), no libc, globals only.
#include <cstdint>

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
