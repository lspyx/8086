#include <iostream>
#include <cstdint>
#include <fstream>
#include <vector>
#include <cstring>
#include <numeric>
#include <type_traits>
#include <cstddef>
#include <bitset>
#include <getopt.h>
#include <cassert>
#include <map>
#include <sstream>
#include <iomanip>

namespace x86 {
    class IterableByte {
    public:
        IterableByte(uint8_t val) : value(val) {}
        IterableByte &operator>>(bool &output) {
            if (idex >= 8) {
                return *this;
            }
            output = (value >> idex--) & 1;
            return *this;
        }
        template<int N>
        std::bitset<N> read_bits() {
            std::bitset<N> bitset;
            for (int i = N - 1; i >= 0; i--) {
                bool res;
                *this >> res;
                bitset[i] = res;
            }
            return bitset;
        }
        void skip_bits(int n) {
            if (n < 0)
                throw std::invalid_argument("");
            idex -= n;
        }
    private:
        uint8_t value = 0;
        int8_t idex = 7;
    };
    class IterableWord {
    public:
        IterableWord(uint16_t val) {
            value = (val << 8) | (val >> 8);
        }
        IterableWord &operator>>(bool &output) {
            if (idex < 0) {
                return *this;
            }
            output = (value >> idex--) & 1;
            return *this;
        }
        template<int N>
        std::bitset<N> read_bits() {
            std::bitset<N> bitset;
            for (int i = N - 1; i >= 0; i--) {
                bool res;
                *this >> res;
                bitset[i] = res;
            }
            return bitset;
        }
        void skip_bits(int n) {
            if (n < 0)
                throw std::invalid_argument("");
            idex -= n;
        }
    private:
        uint16_t value = 0;
        int8_t idex = 15;
    };

    class Memory {
    public:
        Memory() = default;
        explicit Memory(const std::string &input_file)
        : size_(load_input_file(input_file, buffer_))
        {}
        IterableWord get_word(uint16_t address) const {
            return IterableWord{*reinterpret_cast<const uint16_t*>(&buffer_[address])};
        }
        IterableByte get_byte(uint16_t address) const {
            return buffer_[address];
        }
        uint8_t *get_byte_mod(uint16_t address) {
            return reinterpret_cast<uint8_t*>(&buffer_[address]);
        }
        size_t get_size() const {
            return size_;
        }
        friend
        std::ostream &operator<<(std::ostream& os, const Memory &mem) {
            for (int i = 0; i < mem.size_; i += 2) {
                auto one = mem.get_byte(i).read_bits<8>();
                os << "0x" << std::hex << std::setfill('0') << std::setw(4) << i << " :: "
                   << "0b" << one << ' ';
                auto two = mem.get_byte(i + 1).read_bits<8>();
                os << "0b" << two << ' ';
                os << "0x" << std::hex << one.to_ulong() << " 0x" << two.to_ulong() << std::endl;
            }
            os << std::dec;
            return os;
        }
    private:
        char buffer_[256*256] = {0};
        size_t size_ = 0;
        size_t load_input_file(const std::string &filename, char (&input_arr)[256*256]) {
            std::ifstream input(filename);
            if (input.bad() || !input.is_open())
                throw std::runtime_error("can't load the given input file " + filename);
            std::string str {
                    std::istreambuf_iterator<char>(input),
                    std::istreambuf_iterator<char>()
            };
            if (str.size() > 256*256)
                throw std::invalid_argument("the input file is too large");
            memcpy(&input_arr, str.c_str(), str.size());
            return str.size();
        }
    };

    enum class eInstructionType {
        RegisterMemoryToFromRegister,
        ImmediateToRegisterMemory,
        ImmediateToRegister,
        MemoryToAccumulator,
        AccumulatorToMemory,
        RegisterMemoryToSegmentRegister,
        SegmentRegisterToRegisterMemory,
        RegisterMemoryWithRegisterToEither,
        ImmediateToAccumulator
    };
    enum class eArchRegister : int8_t {
        Al = 0, Cl = 1, Dl = 2, Bl = 3,
        Ah = 4, Ch = 5, Dh = 6, Bh = 7,
        Ax = 8, Cx = 9, Dx = 10, Bx = 11,
        Sp = 12, Bp = 13, Si = 14, Di = 15,
    };

    eArchRegister select_reg(std::bitset<3> reg, bool wide) {
        return eArchRegister(reg.to_ulong() + (wide ? 0 : 1) * 8);
    }

    class Operand {
    public:
        Operand() = default;
        enum class eOperandType {
            Register,
            Memory,
            Immediate
        };
        template<eOperandType OperandType>
        requires (OperandType == eOperandType::Register)
        explicit Operand(std::bitset<3> reg, bool wide)
                : type_(eOperandType::Register)
                , register_(select_reg(reg, wide))
        {}
        template<eOperandType OperandType>
        requires (OperandType != eOperandType::Register)
        explicit Operand(uint16_t value)
                : type_(OperandType)
                , memory_or_immediate_(value)
        {}
        Operand(eOperandType type) : type_(type) {}
        void set_register(std::bitset<3> reg, bool w) {
            register_ = select_reg(reg, w);
            wide = w;
        }
        void set_mem_or_imm(uint16_t value, bool w) {
            memory_or_immediate_ = value;
            wide = w;
        }
        void set_type(eOperandType type) {
            type_ = type;
        }
        eArchRegister get_register() const {
            return register_;
        }
        eOperandType get_type() const {
            return type_;
        }
        int16_t get_memory_or_immediate_s() const {
            if (wide)
                return int16_t(memory_or_immediate_);
            else
                return int8_t(memory_or_immediate_);
        }
        uint16_t get_memory_or_immediate_u() const {
            if (wide)
                return uint16_t(memory_or_immediate_);
            else
                return uint8_t(memory_or_immediate_);
        }
    private:
        uint16_t memory_or_immediate_ = 0;
        eOperandType type_ {};
        eArchRegister register_ {};
        bool wide = false;
    };
    enum class eEffectiveAddressCalculation {
        BXplusSI = 0,
        BXplusDI = 1,
        BPplusSI = 2,
        BPplusDI = 3,
        SI = 4,
        DI = 5,
        DirectAdress = 6,
        BX = 7,
        BXplusSIplusD8 = 8,
        BXplusDIplusD8 = 9,
        BPplusSIplusD8 = 10,
        BPplusDIplusD8 = 11,
        SIplusD8 = 12,
        DIplusD8 = 13,
        BPplusD8 = 14,
        BXplusD8 = 15,
        BXplusSIplusD16 = 16,
        BXplusDIplusD16 = 17,
        BPplusSIplusD16 = 18,
        BPplusDIplusD16 = 19,
        SIplusD16 = 20,
        DIplusD16 = 21,
        BPplusD16 = 22,
        BXplusD16 = 23,
    };

    class CPU;
    class BinTrie {
    public:
        struct Node;
        using function_t = void (*)(
                Node *caller,
                CPU& cpu,
                void *);
        void set_cpu(CPU *cpu) {
            cpu_ = cpu;
        }
        struct Node {
            int opcode_bitsize;
            std::string name;
            std::string tag; // comment
            function_t function = nullptr;
            Node *children[2] = {};
        };
        template<size_t opcode_bitlength>
        void add(std::bitset<opcode_bitlength> bits,
                 const std::string &name,
                 const std::string &tag,
                 function_t function)
        {
            if (!sentinel_.children[0]) {
                sentinel_.children[0] = new Node;
            }
            Node *ptr = sentinel_.children[0];
            for (int i = opcode_bitlength - 1; i >= 0; i--) {
                const int value = bits[i] ? 1 : 0;
                if (!ptr->children[value]) {
                    ptr->children[value] = new Node();
                }
                ptr = ptr->children[value];
            }
            if (ptr->function)
                throw std::invalid_argument("can't reset already set functions");
            ptr->function = function;
            ptr->name = name;
            ptr->tag = tag;
            ptr->opcode_bitsize = bits.size();
        }
        template<size_t opcode_bitlength>
        void call(std::bitset<opcode_bitlength> bits, void *data = nullptr) {
            Node *ptr = sentinel_.children[0];
            for (int i = opcode_bitlength - 1; i >= 0; i--) {
                const int value = bits[i] ? 1 : 0;
                if (!ptr->children[value])
                    throw std::out_of_range("no function at this opcode");
                ptr = ptr->children[value];
            }
            if (!ptr->function)
                throw std::out_of_range("no function at this opcode");
            ptr->function(ptr, *cpu_, data);
        }
        bool process_next_instruction(Memory &memory, uint16_t ip, void *data) {
            std::vector<bool> history;
            Node *ptr = sentinel_.children[0];
            auto word = memory.get_word(ip);
            if (ip >= memory.get_size())
                return false;
            while (true) {
                bool value = false;
                word >> value;
                history.push_back(value);
                if (!ptr->children[value])
                    throw std::out_of_range("no function at this opcode");
                ptr = ptr->children[value];
                if (ptr->function) {
//                    std::cout << "IP=0x" << std::setw(4) << std::setfill('0') << ip
//                              << std::dec << " Instruction opcode 0b";
//                    for (const auto &el : history) {
//                        std::cout << el;
//                    }
//                    std::cout << std::endl;
                    ptr->function(ptr, *cpu_, data);
                    return true;
                }
            }
        }
    private:
        CPU *cpu_;
        Node sentinel_;
    };

    struct Flags {
        bool cf = false;
        bool zf = false;
        bool sf = false;
        bool of = false;
        bool pf = false;
        bool af = false;
        std::string str() const {
            std::string s;
            if (cf) s += 'C';
            if (zf) s += 'Z';
            if (sf) s += 'S';
            if (of) s += 'O';
            if (pf) s += 'P';
            if (af) s += 'A';
            return s;
        }

        bool get_cf() const {
            return cf;
        }

        void set_cf(bool cf) {
            Flags::cf = cf;
        }

        bool get_zf() const {
            return zf;
        }

        void set_zf(bool zf) {
            Flags::zf = zf;
        }

        bool get_sf() const {
            return sf;
        }

        void set_sf(bool sf) {
            Flags::sf = sf;
        }

        bool get_of() const {
            return of;
        }

        void set_of(bool of) {
            Flags::of = of;
        }

        bool get_pf() const {
            return pf;
        }

        void set_pf(bool pf) {
            Flags::pf = pf;
        }

        bool get_af() const {
            return af;
        }

        void set_af(bool af) {
            Flags::af = af;
        }
    };

    int calculate_parity(uint8_t value) {
        int set_bits = 0;
        for (int i = 0; i < 8; ++i) {
            if ((value >> i) & 1) {
                set_bits++;
            }
        }
        return (set_bits % 2 == 0); // 1 если четное, 0 если нечетное
    }

    class CPU {
    public:
        uint16_t get_cx() const {
            return cx;
        }
        void set_cx(uint16_t new_value) {
            cx = new_value;
        }
        const Flags & get_flags() const {
            return flags;
        }
        void execute() {
            bool proceed = true;
            while (proceed) {
                proceed = isa_.process_next_instruction(memory_, ip, nullptr);
            }
        }
        CPU(Memory& memory) : memory_(memory) {
            isa_.set_cpu(this);
        }
        ~CPU() {
            std::cout << "Final Register State:\n";
            std::cout << state() << std::endl;
        }
        uint16_t calculate_effective_address(int16_t offset = 0) {
            switch (eac_) {
                case eEffectiveAddressCalculation::BXplusSI: return bx + si;
                case eEffectiveAddressCalculation::BXplusDI: return bx + di;
                case eEffectiveAddressCalculation::BPplusSI: return bp + si;
                case eEffectiveAddressCalculation::BPplusDI: return bp + di;
                case eEffectiveAddressCalculation::SI: return si;
                case eEffectiveAddressCalculation::DI: return di;
                case eEffectiveAddressCalculation::DirectAdress: return offset;
                case eEffectiveAddressCalculation::BX: return bx;
                case eEffectiveAddressCalculation::BXplusSIplusD8: return bx + si + int8_t(offset);
                case eEffectiveAddressCalculation::BXplusDIplusD8: return bx + di + int8_t(offset);
                case eEffectiveAddressCalculation::BPplusSIplusD8: return bp + si + int8_t(offset);
                case eEffectiveAddressCalculation::BPplusDIplusD8: return bp + di + int8_t(offset);
                case eEffectiveAddressCalculation::SIplusD8: return si + int8_t(offset);
                case eEffectiveAddressCalculation::DIplusD8: return di + int8_t(offset);
                case eEffectiveAddressCalculation::BPplusD8: return bp + int8_t(offset);
                case eEffectiveAddressCalculation::BXplusD8: return bx + int8_t(offset);
                case eEffectiveAddressCalculation::BXplusSIplusD16: return bx + si + int16_t(offset);
                case eEffectiveAddressCalculation::BXplusDIplusD16: return bx + di + int16_t(offset);
                case eEffectiveAddressCalculation::BPplusSIplusD16: return bp + si + int16_t(offset);
                case eEffectiveAddressCalculation::BPplusDIplusD16: return bp + di + int16_t(offset);
                case eEffectiveAddressCalculation::SIplusD16: return si + int16_t(offset);
                case eEffectiveAddressCalculation::DIplusD16: return di + int16_t(offset);
                case eEffectiveAddressCalculation::BPplusD16: return bp + int16_t(offset);
                case eEffectiveAddressCalculation::BXplusD16: return bx + int16_t(offset);
            }
        }
        uint16_t &select_register(eArchRegister the_reg) {
            static uint16_t * const arr[8] = {&ax, &cx, &dx, &bx, &sp, &bp, &si, &di};
            return *arr[int(the_reg) % 8];
        }
        uint16_t *select_memory(uint16_t address) {
            return reinterpret_cast<uint16_t*>(memory_.get_byte_mod(address));
        }
        void mov_reg_to_reg(Operand destination, Operand source) {
            uint16_t &dst = select_register(destination.get_register());
            const uint16_t &src = select_register(source.get_register());
            dst = src;
        }
        void mov_mem_to_reg(Operand destination, Operand source) {
            uint16_t &dst = select_register(destination.get_register());
            const uint16_t address = calculate_effective_address(source.get_memory_or_immediate_s());
            const uint16_t *const src = select_memory(address);
            dst = *src;
        }
        void mov_imm_to_reg(Operand destination, Operand source) {
            uint16_t &dest = select_register(destination.get_register());
            dest = source.get_memory_or_immediate_s();
        }
        void mov_imm_to_mem(Operand destination, Operand source) {
            const uint16_t address = calculate_effective_address(source.get_memory_or_immediate_s());
            uint16_t *dest = select_memory(address);
            *dest = source.get_memory_or_immediate_s();
        }
        void mov_reg_to_mem(Operand destination, Operand source) {
            const uint16_t address = calculate_effective_address(destination.get_memory_or_immediate_s());
            uint16_t *dest = select_memory(address);
            *dest = select_register(source.get_register());
        }
        void mov(eInstructionType type, Operand destination, Operand source) {
            switch (type) {
                case eInstructionType::RegisterMemoryToFromRegister:
                {
                    if (source.get_type() == Operand::eOperandType::Register) {
                        mov_reg_to_reg(destination, source);
                    } else if (source.get_type() == Operand::eOperandType::Memory) {
                        mov_mem_to_reg(destination, source);
                    } else {
                        throw std::logic_error("Unknown mov operation 1");
                    }
                }
                break;
                case eInstructionType::ImmediateToRegisterMemory:
                {
                    if (destination.get_type() == Operand::eOperandType::Register) {
                        mov_imm_to_reg(destination, source);
                    } else if (destination.get_type() == Operand::eOperandType::Memory) {
                        mov_imm_to_mem(destination, source);
                    } else {
                        throw std::logic_error("Unknown mov operation 2");
                    }
                }
                break;
                case eInstructionType::ImmediateToRegister:
                {
                    mov_imm_to_reg(destination, source);
                }
                break;
                case eInstructionType::MemoryToAccumulator:
                {
                    mov_mem_to_reg(destination, source);
                }
                break;
                case eInstructionType::AccumulatorToMemory: {
                    mov_reg_to_mem(destination, source);
                }
                break;
                case eInstructionType::RegisterMemoryToSegmentRegister: {
                    std::logic_error("not implemented");
                }
                break;
                case eInstructionType::SegmentRegisterToRegisterMemory: {
                    std::logic_error("not implemented");
                }
                break;
                default:
                    throw std::logic_error("Unknown mov operation 3");
            }
        }
        void update_flags(bool add, uint16_t dest, uint16_t src) {
            if (add) {
                uint32_t full_result = (uint32_t)dest + src;
                uint16_t res = full_result;
                flags.set_zf(res == 0);
                flags.set_sf(res & 0x8000);
                flags.set_pf(calculate_parity(uint8_t(res)));
                flags.set_cf(full_result > 0xffff);
                flags.set_af((dest & 0x0f) + (src & 0x0f) > 0x0f);
                bool sign_dest = (dest & 0x8000);
                bool sign_src = (src & 0x8000);
                bool sign_res = (res & 0x8000);
                flags.set_of((sign_dest == sign_src) && (sign_res != sign_dest));
            } else {
                uint32_t full_result = (uint32_t)dest - src;
                uint16_t res = full_result;
                flags.set_zf(res == 0);
                flags.set_sf(res & 0x8000);
                flags.set_pf(calculate_parity(uint8_t(res)));
                flags.set_cf(src > dest);
                flags.set_af((dest & 0x0f) < (src & 0x0f));
                bool sign_dest = (dest & 0x8000);
                bool sign_src = (src & 0x8000);
                bool sign_res = (res & 0x8000);
                flags.set_of((sign_dest != sign_src) && (sign_res != sign_dest));
            }
        }
        void add(Operand destination, Operand source) {
            if (destination.get_type() == Operand::eOperandType::Register
                && source.get_type() == Operand::eOperandType::Register)
            {
                auto &dest = select_register(destination.get_register());
                auto &src = select_register(source.get_register());
                update_flags(true, dest, src);
                dest += src;
            } else {
                assert(source.get_type() == Operand::eOperandType::Immediate);
                auto &dest = select_register(destination.get_register());
                auto src = source.get_memory_or_immediate_s();
                update_flags(true, dest, src);
                dest += src;
            }
        }
        void cmp(Operand destination, Operand source) {
            if (destination.get_type() == Operand::eOperandType::Register
                    && source.get_type() == Operand::eOperandType::Register)
            {
                auto &dest = select_register(destination.get_register());
                auto &src = select_register(source.get_register());
                update_flags(false, dest, src);
            } else {
                assert(source.get_type() == Operand::eOperandType::Immediate);
                auto &dest = select_register(destination.get_register());
                auto src = source.get_memory_or_immediate_u();
                update_flags(false, dest, src);
            }
        }
        void sub(Operand destination, Operand source) {
            if (destination.get_type() == Operand::eOperandType::Register
                && source.get_type() == Operand::eOperandType::Register)
            { // reg to reg
                auto &dest = select_register(destination.get_register());
                auto &src = select_register(source.get_register());
                update_flags(false, dest, src);
                dest -= src;
            } else { // imm to reg
                assert(source.get_type() == Operand::eOperandType::Immediate);
                auto &dest = select_register(destination.get_register());
                auto src = source.get_memory_or_immediate_u();
                update_flags(false, dest, src);
                dest -= src;
            }
        }
        void set_effective_address_calculation(std::bitset<3> rm, std::bitset<2> mod) {
            eac_ = eEffectiveAddressCalculation(mod.to_ulong() * 8 + rm.to_ulong());
        }
        void remember_prev_state() {
            pax = ax;
            pbx = bx;
            pcx = cx;
            pdx = dx;
            psp = sp;
            pbp = bp;
            psi = si;
            pdi = di;
            pflags = flags;
            pip = ip;
        }
        std::string flags_str(bool zero_flag, bool sign_flag) {
            std::string flags_state;
            if (sign_flag)
                flags_state += 'S';
            if (zero_flag)
                flags_state += 'Z';
            return flags_state;
        }
        bool get_zero_flag() const {
            return flags.get_zf();
        }
        bool get_sign_flag() const {
            return flags.get_sf();
        }
        std::string state_change() {
            std::stringstream ss;
            if (ax != pax)
                ss << "ax = 0x" << std::hex << pax
                   << " -> 0x" << std::hex << ax;
            if (bx != pbx)
                ss << "bx = 0x" << std::hex << pbx
                   << " -> 0x" << std::hex << bx;
            if (cx != pcx)
                ss << "cx = 0x" << std::hex << pcx
                   << " -> 0x" << std::hex << cx;
            if (dx != pdx)
                ss << "dx = 0x" << std::hex << pdx
                   << " -> 0x" << std::hex << dx;
            if (sp != psp)
                ss << "sp = 0x" << std::hex << psp
                   << " -> 0x" << std::hex << sp;
            if (bp != pbp)
                ss << "bp = 0x" << std::hex << pbp
                   << " -> 0x" << std::hex << bp;
            if (si != psi)
                ss << "si = 0x" << std::hex << psi
                   << " -> 0x" << std::hex << si;
            if (di != pdi)
                ss << "di = 0x" << std::hex << pdi
                   << " -> 0x" << std::hex << di;
            bool zf_changed = pflags.get_zf() != flags.get_zf();
            bool sf_changed = pflags.get_sf() != flags.get_sf();
            if (zf_changed || sf_changed) {
                const std::string prev_state = pflags.str();
                const std::string cur_state = flags.str();
                ss << " flags = " << prev_state << " -> " << cur_state << ';';
            }
            if (ip != pip)
                ss << " ip = 0x" << std::hex << pip
                   << " -> 0x" << std::hex << ip;
            return ss.str();
        }
        std::string state() {
            std::stringstream ss;
            ss << '\t' << "ax = 0x" << std::setw(4) << std::setfill('0') << std::hex << ax
               << std::dec << '(' << int(ax) << ')' << '\n';
            ss << '\t' << "bx = 0x" << std::setw(4) << std::setfill('0') << std::hex << bx
               << std::dec << '(' << int(bx) << ')' << '\n';
            ss << '\t' << "cx = 0x" << std::setw(4) << std::setfill('0') << std::hex << cx
               << std::dec << '(' << int(cx) << ')' << '\n';
            ss << '\t' << "dx = 0x" << std::setw(4) << std::setfill('0') << std::hex << dx
               << std::dec << '(' << int(dx) << ')' << '\n';
            ss << '\t' << "sp = 0x" << std::setw(4) << std::setfill('0') << std::hex << sp
               << std::dec << '(' << int(sp) << ')' << '\n';
            ss << '\t' << "bp = 0x" << std::setw(4) << std::setfill('0') << std::hex << bp
               << std::dec << '(' << int(bp) << ')' << '\n';
            ss << '\t' << "si = 0x" << std::setw(4) << std::setfill('0') << std::hex << si
               << std::dec << '(' << int(si) << ')' << '\n';
            ss << '\t' << "di = 0x" << std::setw(4) << std::setfill('0') << std::hex << di
               << std::dec << '(' << int(di) << ')' << '\n';
            ss << '\t' << "ip = " << std::setw(4) << std::setfill('0') << std::hex << ip
               << std::dec << '(' << int(ip) << ')' << '\n';
            const std::string cur_state = flags.str();
            ss << "\tflags = " << cur_state;
            return ss.str();
        }
        uint16_t get_ip() {
            return ip;
        }
        void set_ip(uint16_t address) {
            ip = address;
        }
        Memory &get_memory() {
            return memory_;
        }
        BinTrie &get_isa() {
            return isa_;
        }
    private:
        uint16_t ip = 0;
        uint16_t ax{}, bx{}, cx{}, dx{}, sp{}, bp{}, si{}, di{};
        uint16_t pax{}, pbx{}, pcx{}, pdx{}, psp{}, pbp{}, psi{}, pdi{};
        uint16_t pip = 0;
        Memory &memory_;
        eEffectiveAddressCalculation eac_;
        Flags pflags, flags;
        BinTrie isa_;

    };

    template<typename T>
    concept Integral = std::is_integral_v<T>;
    template<typename T>
    concept Pointerable = std::is_void_v<T> || Integral<T>;

    std::string bin(char b) {
        std::string res;
        for (int i = 7; i >= 0; i--)
            res.push_back(b & (1 << i) ? '1' : '0');
        return res;
    }
    std::string hex(char b) {
        char arr[17] = "0123456789ABCDEF";
        std::string res;
        const char quartet = 0x0f;
        const auto low = b & quartet;
        const auto high = (b >> 4) & quartet;
        res += arr[int(high)];
        res += arr[int(low)];
        return res;
    }



    const char *get_register_name(std::bitset<3> enc_reg, bool w) {
        const uint8_t reg = enc_reg.to_ulong();
        assert(reg < 8);
        if (w) {
            static const char * wide_registers[] = {
                    "ax",
                    "cx",
                    "dx",
                    "bx",
                    "sp",
                    "bp",
                    "si",
                    "di"};
            return wide_registers[reg];
        } else {
            static const char * narrow_registers[] = {
                    "al",
                    "cl",
                    "dl",
                    "bl",
                    "ah",
                    "ch",
                    "dh",
                    "bh"};
            return narrow_registers[reg];
        }
    }

    void imm_to_register_move(BinTrie::Node *node, CPU& cpu, void *data) {
        Memory &mem = cpu.get_memory();
        auto word = mem.get_word(cpu.get_ip());
        int ip_offset = 2;
        word.skip_bits(node->opcode_bitsize);
        bool w;
        word >> w;
        const auto reg = word.read_bits<3>();
        uint8_t lo = word.read_bits<8>().to_ulong();
        eInstructionType mov_type = eInstructionType::ImmediateToRegister;
        Operand dest_op, source_op;
        dest_op.set_type(Operand::eOperandType::Register);
        dest_op.set_register(reg, w);
        source_op.set_type(Operand::eOperandType::Immediate);
        if (w) {
            auto byte = mem.get_word(cpu.get_ip() + 2);
            uint8_t hi = byte.read_bits<8>().to_ulong();
            ip_offset++;
            const int16_t value = static_cast<int16_t>(hi) << 8 | static_cast<int16_t>(lo);
            std::cout << node->name << ' ' << get_register_name(reg, w) << ", " << int16_t(value);
            source_op.set_mem_or_imm(value, true);
        } else {
            std::cout << node->name << ' ' << get_register_name(reg, w) << ", " << int(int8_t(lo));
            source_op.set_mem_or_imm(int8_t(lo), false);
        }
        {
            cpu.remember_prev_state();
            cpu.set_ip(cpu.get_ip() + ip_offset);
            cpu.mov(mov_type, dest_op, source_op);
            std::cout << " ; " << cpu.state_change();
        }
        std::cout << std::endl;
    }

    std::string get_displacement_rm(std::bitset<3> rm, std::bitset<2> mod) {
        static const char *effective_address_calc_rm[] = {
                "bx + si",
                "bx + di",
                "bp + si",
                "bp + di",
                "si",
                "di",
                "bp",
                "bx"
        };
        if (rm.to_ulong() == 0b110 && mod.to_ulong() == 0)
            return "";
        std::string res;
        res += effective_address_calc_rm[rm.to_ulong()];
        return res;
    }
    void register_memory_to_from_register(BinTrie::Node *node, CPU& cpu, void *data) {
        auto &mem = cpu.get_memory();
        auto word = mem.get_word(cpu.get_ip());
        int ip_offset = 2;
        word.skip_bits(node->opcode_bitsize);
        bool d, w;
        std::bitset<2> mod;
        std::bitset<3> reg, rm;
        word >> d; // 0->source in REG, 1->dest in REG
        word >> w; // 0->byte, 1->word
        mod = word.read_bits<2>();
        reg = word.read_bits<3>();
        rm = word.read_bits<3>();
        eInstructionType mov_type = eInstructionType::RegisterMemoryToFromRegister;;
        Operand destination_op, source_op;
        if (mod.to_ulong() == 0b11) { // reg to reg
            const char *source = d ? get_register_name(rm, w) : get_register_name(reg, w);
            const char *dest = d ? get_register_name(reg, w) : get_register_name(rm, w);
            if (d) {
                destination_op.set_type(Operand::eOperandType::Register);
                destination_op.set_register(reg, w);
                source_op.set_type(Operand::eOperandType::Register);
                source_op.set_register(rm, w);
            } else {
                destination_op.set_type(Operand::eOperandType::Register);
                destination_op.set_register(rm, w);
                source_op.set_type(Operand::eOperandType::Register);
                source_op.set_register(reg, w);
            }
            std::cout << node->name << ' ' << dest << ", " << source;
        } else {
            int16_t possible_displacement = 0;
            source_op.set_type(Operand::eOperandType::Memory);
            if (mod.to_ulong() == 0b01) {
                auto byte = mem.get_byte(cpu.get_ip() + 2);
                ip_offset++;
                possible_displacement = int8_t(byte.read_bits<8>().to_ulong());
                source_op.set_mem_or_imm(possible_displacement, false);
            } else if (mod.to_ulong() == 0b10 || (mod.to_ulong() == 0 && rm.to_ulong() == 0b110)) {
                word = mem.get_word(cpu.get_ip() + 2);
                ip_offset += 2;
                possible_displacement = int16_t(word.read_bits<8>().to_ulong() | word.read_bits<8>().to_ulong() << 8);
                source_op.set_mem_or_imm(possible_displacement, true);
            }
            if (d) { // load: mov ax, [86]
                std::cout << node->name
                          << ' '
                          << get_register_name(reg, w)
                          << ", ["
                          << get_displacement_rm(rm, mod);
                if (possible_displacement != 0) {
                    if (!(rm.to_ulong() == 0b110 && mod.to_ulong() == 0))
                        std::cout << ' ' << (possible_displacement < 0 ? '-' : '+') << ' ';
                    std::cout << std::abs(possible_displacement);
                }
                std::cout << "]";
                destination_op.set_type(Operand::eOperandType::Register);
                destination_op.set_register(reg, w);
            } else { // store: mov [86], ax
                std::cout << node->name
                          << " ["
                          << get_displacement_rm(rm, mod);
                if (possible_displacement != 0) {
                    if (!(rm.to_ulong() == 0b110 && mod.to_ulong() == 0))
                        std::cout << ' ' << (possible_displacement < 0 ? '-' : '+') << ' ';
                    std::cout << std::abs(possible_displacement);
                }
                std::cout << "], "
                          << get_register_name(reg, w);

            }
        }
        {
            cpu.remember_prev_state();
            cpu.set_ip(cpu.get_ip() + ip_offset);
            cpu.set_effective_address_calculation(rm, mod);
            if (node->name == "mov") {
                cpu.mov(mov_type, destination_op, source_op);
            } else if (node->name == "add") {
                cpu.add(destination_op, source_op);
            } else if (node->name == "cmp") {
                cpu.cmp(destination_op, source_op);
            } else if (node->name == "sub") {
                cpu.sub(destination_op, source_op);
            }
            std::cout << " ; " << cpu.state_change();
        }
        std::cout << std::endl;
    }

    void imm_to_register_memory_move(BinTrie::Node *node, CPU& cpu, void *data) {
        auto &mem = cpu.get_memory();
        auto word = mem.get_word(cpu.get_ip());
        word.skip_bits(node->opcode_bitsize);
        int ip_offset = 2;
        bool w;
        std::bitset<2> mod;
        std::bitset<3> reg, rm;
        word >> w;
        mod = word.read_bits<2>();
        reg = word.read_bits<3>(); // always 000
        rm = word.read_bits<3>();
        int16_t possible_data = 0;
        int16_t possible_displacement = 0;
        if (mod.to_ulong() == 0b01) {
            auto byte = mem.get_byte(cpu.get_ip() + ip_offset);
            ip_offset++;
            possible_displacement = int8_t(byte.read_bits<8>().to_ulong());
        } else if (mod.to_ulong() == 0b10 || (mod.to_ulong() == 0 && rm.to_ulong() == 0b110)) {
            word = mem.get_word(cpu.get_ip() + ip_offset);
            ip_offset += 2;
            possible_displacement = int16_t(word.read_bits<8>().to_ulong() | word.read_bits<8>().to_ulong() << 8);
        }
        if (w) {
            word = mem.get_word(cpu.get_ip() + ip_offset);
            ip_offset += 2;
            possible_data = int16_t(word.read_bits<8>().to_ulong() | word.read_bits<8>().to_ulong() << 8);
        } else {
            auto byte = mem.get_byte(cpu.get_ip() + ip_offset);
            ip_offset++;
            possible_data = int8_t(byte.read_bits<8>().to_ulong());
        }
        std::cout << node->name
                  << " ["
                  << get_displacement_rm(rm, mod);
        if (possible_displacement != 0) {
            std::cout << ' '
                      << (possible_displacement < 0 ? '-' : '+')
                      << ' ' << std::abs(possible_displacement);
        }
        std::cout << "], "
                  << (w ? "word " : "byte ")
                  << possible_data
                  << std::endl;
        cpu.set_ip(cpu.get_ip() + ip_offset);
    }

    void accumulator_to_memory(BinTrie::Node *node, CPU& cpu, void *data) {
        auto &mem = cpu.get_memory();
        auto word = mem.get_word(cpu.get_ip());
        word.skip_bits(node->opcode_bitsize);
        bool w;
        word >> w;
        uint16_t address = uint16_t(word.read_bits<8>().to_ulong() |
                mem.get_byte(cpu.get_ip() + 2).read_bits<8>().to_ulong() << 8);
        std::cout << node->name << " [" << address << "], ax" << std::endl;
        cpu.set_ip(cpu.get_ip() + 3);
    }

    void memory_to_accumulator(BinTrie::Node *node, CPU& cpu, void *data) {
        auto &mem = cpu.get_memory();
        auto word = mem.get_word(cpu.get_ip());
        word.skip_bits(node->opcode_bitsize);
        bool w;
        word >> w;
        uint16_t address = uint16_t(word.read_bits<8>().to_ulong() |
                mem.get_byte(cpu.get_ip() + 2).read_bits<8>().to_ulong() << 8);
        std::cout << node->name << " ax, [" << address << "]" << std::endl;
    }
    void immediate_to_accumulator(BinTrie::Node *node, CPU& cpu, void *data) {
        auto &mem = cpu.get_memory();
        auto word = mem.get_word(cpu.get_ip());
        word.skip_bits(node->opcode_bitsize);
        bool w;
        word >> w;
        if (w) {
            int16_t imm = word.read_bits<8>().to_ulong() |
                    mem.get_byte(cpu.get_ip() + 2).read_bits<8>().to_ulong() << 8;
            std::cout << node->name << " ax, " << imm << std::endl;
            cpu.set_ip(cpu.get_ip() + 3);
        } else {
            int8_t imm = int8_t(word.read_bits<8>().to_ulong());
            std::cout << node->name << " al, " << int(imm) << std::endl;
            cpu.set_ip(cpu.get_ip() + 2);
        }
    }
    const char *get_op_name(std::bitset<3> reg) {
        switch(reg.to_ulong()) {
            case 0: return "add";
            case 0b101: return "sub";
            case 0b111: return "cmp";
            default: return "ERROR";
        }
    }
    void imm_to_register_memory_add(BinTrie::Node *node, CPU& cpu, void *data) {
        auto &mem = cpu.get_memory();
        auto word = mem.get_word(cpu.get_ip());
        word.skip_bits(node->opcode_bitsize);
        int ip_offset = 2;
        bool s = false, w = false;
        std::bitset<2> mod;
        std::bitset<3> reg, rm;
        word >> s;
        word >> w;
        mod = word.read_bits<2>();
        reg = word.read_bits<3>(); // always 000 for add
        rm = word.read_bits<3>();
        int16_t possible_data = 0;
        int16_t possible_displacement = 0;

        Operand destination_op, source_op;
        if (mod.to_ulong() == 0b01) {
            auto byte = mem.get_byte(cpu.get_ip() + ip_offset);
            ip_offset++;
            possible_displacement = int8_t(byte.read_bits<8>().to_ulong());
            destination_op.set_type(Operand::eOperandType::Memory);
            destination_op.set_mem_or_imm(possible_displacement, false);
        } else if (mod.to_ulong() == 0b10 || (mod.to_ulong() == 0 && rm.to_ulong() == 0b110)) {
            word = mem.get_word(cpu.get_ip() + ip_offset);
            ip_offset += 2;
            possible_displacement = int16_t(word.read_bits<8>().to_ulong() | word.read_bits<8>().to_ulong() << 8);
            destination_op.set_type(Operand::eOperandType::Memory);
            destination_op.set_mem_or_imm(possible_displacement, true);
        }
        if (!s && w) {
            word = mem.get_word(cpu.get_ip() + ip_offset);
            ip_offset += 2;
            const int16_t lo = word.read_bits<8>().to_ulong();
            const int16_t hi = word.read_bits<8>().to_ulong() << 8;
            const int16_t value = hi | lo;
            possible_data = value;
            source_op.set_mem_or_imm(possible_data, true);
            source_op.set_type(Operand::eOperandType::Immediate);
        } else {
            auto byte = mem.get_byte(cpu.get_ip() + ip_offset);
            ip_offset++;
            possible_data = int8_t(byte.read_bits<8>().to_ulong());
            source_op.set_mem_or_imm(possible_data, false);
            source_op.set_type(Operand::eOperandType::Immediate);
        }
        if (mod.to_ulong() == 0b11) {
            if (s) {
                int16_t sign_extend = possible_data;
                std::cout << get_op_name(reg) << ' ' << get_register_name(rm, w)
                          << ", "
                          << int(sign_extend);
            } else {
                uint16_t sign_extend = possible_data;
                std::cout << get_op_name(reg) << ' ' << get_register_name(rm, w)
                          << ", "
                          << int(sign_extend);
            }
            destination_op.set_type(Operand::eOperandType::Register);
            destination_op.set_register(rm, w);
        } else {
            std::cout << get_op_name(reg)
                      << (w ? " word" : " byte")
                      << " ["
                      << get_displacement_rm(rm, mod);
            if (possible_displacement != 0) {
                if (rm.to_ulong() == 0b110 && mod.to_ulong() == 0) {
                    std::cout << std::abs(possible_displacement);
                } else
                {
                    std::cout << ' '
                              << (possible_displacement < 0 ? '-' : '+')
                              << ' ' << std::abs(possible_displacement);
                }
            }
            std::cout << "], " << possible_data;
        }
        {
            cpu.remember_prev_state();
            cpu.set_ip(cpu.get_ip() + ip_offset);
            cpu.set_effective_address_calculation(rm, mod);
            switch (reg.to_ulong()) {
                case 0b000: cpu.add(destination_op, source_op); break;
                case 0b101: cpu.sub(destination_op, source_op); break;
                case 0b111: cpu.cmp(destination_op, source_op); break;
            }
            std::cout << " ; " << cpu.state_change();
        }
        std::cout << std::endl;
    }
    void conditional_jump(BinTrie::Node *node, CPU& cpu, void *data) {
        auto &mem = cpu.get_memory();
        auto word = mem.get_word(cpu.get_ip());
        word.skip_bits(node->opcode_bitsize);
        cpu.remember_prev_state();
        int8_t jump_offset = 2;
        const auto &n = node->name;
        const auto &flags = cpu.get_flags();
        const int8_t offset = word.read_bits<8>().to_ulong();
        if (n == "jnz" || n == "jne") {
            if (!flags.get_zf()) {
                jump_offset += offset;
            }
        } else if (n == "jz" || n == "je") {
            if (flags.get_zf()) {
                jump_offset += offset;
            }
        } else if (n == "js") {
            if (flags.get_sf()) {
                jump_offset += offset;
            }
        } else if (n == "jns") {
            if (!flags.get_sf()) {
                jump_offset += offset;
            }
        } else if (n == "jc") {
            if (flags.get_cf()) {
                jump_offset += offset;
            }
        } else if (n == "jnc") {
            if (!flags.get_cf()) {
                jump_offset += offset;
            }
        } else if (n == "jo") {
            if (flags.get_of()) {
                jump_offset += offset;
            }
        } else if (n == "jno") {
            if (!flags.get_of()) {
                jump_offset += offset;
            }
        } else if (n == "jp") {
            if (flags.get_pf()) {
                jump_offset += offset;
            }
        } else if (n == "jnp") {
            if (!flags.get_pf()) {
                jump_offset += offset;
            }
        } else if (n == "ja" || n == "jnbe") {
            if (!flags.get_cf() && !flags.get_zf()) {
                jump_offset += offset;
            }
        } else if (n == "jae" || n == "jnb" || n == "jnc") {
            if (!flags.get_cf()) {
                jump_offset += offset;
            }
        } else if (n == "jb" || n == "jnae" || n == "jc") {
            if (flags.get_cf()) {
                jump_offset += offset;
            }
        } else if (n == "jbe" || n == "jna") {
            if (flags.get_cf() && flags.get_zf()) {
                jump_offset += offset;
            }
        } else if (n == "jg" || n == "jnle") {
            if (flags.get_of() == flags.get_sf() && !flags.get_zf()) {
                jump_offset += offset;
            }
        } else if (n == "jge" || n == "jnl") {
            if (flags.get_sf() == flags.get_of()) {
                jump_offset += offset;
            }
        } else if (n == "jl" || n == "jnge") {
            if (flags.get_sf() != flags.get_of()) {
                jump_offset += offset;
            }
        } else if (n == "jle" || n == "jng") {
            if (flags.get_of() != flags.get_sf() && flags.get_zf()) {
                jump_offset += offset;
            }
        } else if (n == "jcxz") {
            if (cpu.get_cx() == 0) {
                jump_offset += offset;
            }
        } else if (n == "loopz" || n == "loope") {
            const auto cur_cx = cpu.get_cx();
            cpu.set_cx(cur_cx - 1);
            if (cpu.get_cx() && flags.get_zf()) {
                jump_offset += offset;
            }
        } else if (n == "loopnz" || n == "loopne") {
            const auto cur_cx = cpu.get_cx();
            cpu.set_cx(cur_cx - 1);
            if (cpu.get_cx() && !flags.get_zf()) {
                jump_offset += offset;
            }
        }
        cpu.set_ip(cpu.get_ip() + jump_offset);
        std::cout << node->name << " $";
        if (offset > 0)
            std::cout << '+';
        std::cout << int(2+offset) << " ; " << cpu.state_change() << std::endl;
    }
}

int main(int argc, char **argv) {
    int n = 0;
    std::string input_file;
    while (-1 != (n = getopt(argc, argv, "f:"))) {
        switch (n) {
            case 'f': {
                input_file = optarg;
            } break;
            default: {
                std::cout << "unknown option: " << char(n) << std::endl;
            }
        }
    }
    if (input_file.empty()) {
        std::cout << "-f is a required argument\nusage: " << basename(argv[0]) << " -f <filename>" << std::endl;
        return 1;
    }
    x86::Memory memory(input_file);
    x86::CPU cpu(memory);
    x86::BinTrie &isa = cpu.get_isa();
    isa.add(std::bitset<6>{0b100010},
                 "mov",
                 "register/memory to/from register",
                 x86::register_memory_to_from_register);
    isa.add(std::bitset<4>{0b1011},
                 "mov",
                 "immediate to register",
                 x86::imm_to_register_move);
    isa.add(std::bitset<7>{0b1100011},
                 "mov",
                 "immediate to register/memory",
                 x86::imm_to_register_memory_move);
    isa.add(std::bitset<7>{0b1010000},
                 "mov",
                 "memory to accumulator",
                 x86::memory_to_accumulator);
    isa.add(std::bitset<7>{0b1010001},
                 "mov",
                 "accumulator to memory",
                 x86::accumulator_to_memory);

    isa.add(std::bitset<6>{0b000000},
                 "add",
                 "reg/memory with register to either",
                 x86::register_memory_to_from_register);
    isa.add(std::bitset<6>{0b100000},
                 "add",
                 "immediate to register/memory",
                 x86::imm_to_register_memory_add);
    isa.add(std::bitset<7>{0b0000010},
                 "add",
                 "immediate to accumulator",
                 x86::immediate_to_accumulator);

    isa.add(std::bitset<6>{0b001010},
                 "sub",
                 "reg/memory with register to either",
                 x86::register_memory_to_from_register);
    isa.add(std::bitset<7>{0b0010110},
                 "sub",
                 "immediate to accumulator",
                 x86::immediate_to_accumulator);

    isa.add(std::bitset<6>{0b001110},
                 "cmp",
                 "reg/memory with register to either",
                 x86::register_memory_to_from_register);
    isa.add(std::bitset<7>{0b0011110},
                 "cmp",
                 "immediate to accumulator",
                 x86::immediate_to_accumulator);

    // conditional jumps
    const static std::map<uint8_t, std::string> cond_jumps{
            {0b01110100, "je"},
            {0b01111100, "jl"},
            {0b01111110, "jle"},
            {0b01110010, "jb"},
            {0b01110110, "jbe"},
            {0b01111010, "jp"},
            {0b01110000, "jo"},
            {0b01111000, "js"},
            {0b01110101, "jnz"},
            {0b01111101, "jnl"},
            {0b01111111, "jg"},
            {0b01110011, "jnb"},
            {0b01110111, "ja"},
            {0b01111011, "jnp"},
            {0b01110001, "jno"},
            {0b01111001, "jns"},
            {0b11100010, "loop"},
            {0b11100001, "loopz"},
            {0b11100000, "loopnz"},
            {0b11100011, "jcxz"},
    };
    for (const auto &[opcode, name] : cond_jumps) {
        isa.add(std::bitset<8>{opcode},
                     name,
                     "jump equals",
                     x86::conditional_jump);
    }
    std::cout << cpu.get_memory() << std::endl;
    cpu.execute();
}
