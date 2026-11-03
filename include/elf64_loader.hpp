// elf64_loader.hpp
#pragma once
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

// ELF64 on-disk structures (64 and 56 bytes). Fields are ordered so every uint64_t is 8-byte aligned.
struct Elf64_Ehdr {
    uint8_t  e_ident[16];   // 7F 'E' 'L' 'F', class (2 = 64-bit), data (1 = little-endian), version, padding
    uint16_t e_type;        // 2 = executable
    uint16_t e_machine;     // 243 = RISC-V
    uint32_t e_version;
    uint64_t e_entry;       // address of the first instruction (_start)
    uint64_t e_phoff;       // file offset of the program header table
    uint64_t e_shoff;       // file offset of the section header table
    uint32_t e_flags;       // RISC-V: bit 0 = RVC (compressed) used, bits 1-2 = float ABI
    uint16_t e_ehsize;
    uint16_t e_phentsize;   // size of one program header (56)
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};
struct Elf64_Phdr {
    uint32_t p_type;        // 1 = PT_LOAD
    uint32_t p_flags;       // R = 4, W = 2, X = 1 (moved up here vs ELF32 to keep the 64-bit fields aligned)
    uint64_t p_offset;      // where its bytes start in the file
    uint64_t p_vaddr;       // where they go in memory
    uint64_t p_paddr;
    uint64_t p_filesz;      // bytes stored in the file
    uint64_t p_memsz;       // bytes in memory; the extra is .bss and must be zeroed
    uint64_t p_align;
};
struct Elf64_Shdr {
    uint32_t sh_name;       // offset of the section's name in the section-name string table
    uint32_t sh_type;       // 2 = SHT_SYMTAB (symbol table)
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;     // where the section's bytes start in the file
    uint64_t sh_size;
    uint32_t sh_link;       // for a symbol table: index of its string table (symbol names)
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;    // size of one entry (24 for a symbol)
};
struct Elf64_Sym {
    uint32_t st_name;       // offset of the symbol's name in the linked string table
    uint8_t  st_info;
    uint8_t  st_other;
    uint16_t st_shndx;
    uint64_t st_value;      // the symbol's address
    uint64_t st_size;
};
static_assert(sizeof(Elf64_Ehdr) == 64 && sizeof(Elf64_Phdr) == 56, "structs must match the file layout");
static_assert(sizeof(Elf64_Shdr) == 64 && sizeof(Elf64_Sym) == 24, "structs must match the file layout");

inline std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + path);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// Returns the address of symbol `name` (e.g. "tohost"), or 0 if the ELF has no such symbol.
inline uint64_t elf64_symbol(const std::string& path, const char* name) {
    std::vector<uint8_t> file = read_file(path);
    Elf64_Ehdr eh;
    if (file.size() < sizeof eh) return 0;
    std::memcpy(&eh, file.data(), sizeof eh);

    auto section = [&](uint32_t i, Elf64_Shdr& sh) {
        uint64_t at = eh.e_shoff + (uint64_t)i * eh.e_shentsize;
        if (i >= eh.e_shnum || at > file.size() || file.size() - at < sizeof sh) return false;
        std::memcpy(&sh, file.data() + at, sizeof sh);
        return sh.sh_offset <= file.size() && file.size() - sh.sh_offset >= sh.sh_size;
    };

    for (uint32_t i = 0; i < eh.e_shnum; i++) {
        Elf64_Shdr symtab, strtab;
        if (!section(i, symtab) || symtab.sh_type != 2) continue;     // only the symbol table
        if (!section(symtab.sh_link, strtab)) continue;

        for (uint64_t at = 0; at + sizeof(Elf64_Sym) <= symtab.sh_size; at += sizeof(Elf64_Sym)) {
            Elf64_Sym s;
            std::memcpy(&s, file.data() + symtab.sh_offset + at, sizeof s);
            if (s.st_name >= strtab.sh_size) continue;
            const char* sym = (const char*)file.data() + strtab.sh_offset + s.st_name;
            size_t max_len  = strtab.sh_size - s.st_name;              // names must end inside the table
            if (std::memchr(sym, 0, max_len) && std::strcmp(sym, name) == 0) return s.st_value;
        }
    }
    return 0;
}

// Copies every PT_LOAD segment at guest address [base, base + mem_size) into mem[0 .. mem_size)
// and returns the entry point. Throws on bad input.
inline uint64_t load_elf64(const std::string& path, uint8_t* mem, size_t mem_size, uint64_t base) {
    std::vector<uint8_t> file = read_file(path);

    Elf64_Ehdr eh;
    if (file.size() < sizeof eh) throw std::runtime_error("too small to be an ELF64 file");
    std::memcpy(&eh, file.data(), sizeof eh);
    if (std::memcmp(eh.e_ident, "\x7f" "ELF", 4) != 0) throw std::runtime_error("not an ELF file");
    if (eh.e_ident[4] != 2) throw std::runtime_error("not ELF64 (built for RV32?)");
    if (eh.e_ident[5] != 1) throw std::runtime_error("not little-endian");
    if (eh.e_machine != 243) throw std::runtime_error("not a RISC-V ELF");
    if (eh.e_type != 2) throw std::runtime_error("not an executable (was it linked?)");
    if (eh.e_flags & 1) throw std::runtime_error("uses compressed instructions: build without 'c' in -march");

    for (int i = 0; i < eh.e_phnum; i++) {
        uint64_t at = eh.e_phoff + (uint64_t)i * eh.e_phentsize;
        if (at > file.size() || file.size() - at < sizeof(Elf64_Phdr)) throw std::runtime_error("program header outside the file");
        Elf64_Phdr ph;
        std::memcpy(&ph, file.data() + at, sizeof ph);
        if (ph.p_type != 1) continue;                          // only PT_LOAD segments go into memory
        if (ph.p_filesz > ph.p_memsz || ph.p_offset > file.size() || file.size() - ph.p_offset < ph.p_filesz)
            throw std::runtime_error("corrupt segment");
        if (ph.p_vaddr < base)                                  // unsigned: check before subtracting
            throw std::runtime_error("segment below MEM_BASE (check link.ld)");
        uint64_t off = ph.p_vaddr - base;                       // guest address -> index into mem[]
        if (off > mem_size || mem_size - off < ph.p_memsz)      // written this way so it can't overflow
            throw std::runtime_error("segment does not fit in simulated memory (check link.ld)");
        std::memcpy(mem + off, file.data() + ph.p_offset, ph.p_filesz);
        std::memset(mem + off + ph.p_filesz, 0, ph.p_memsz - ph.p_filesz);   // .bss: zero-filled
    }
    if (eh.e_entry < base || eh.e_entry - base > mem_size - 4)
        throw std::runtime_error("entry point outside simulated memory");
    return eh.e_entry;
}