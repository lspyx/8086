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
    class Memory {
    public:
        Memory() = default;
        explicit Memory(const std::string &input_file)
        : size_(load_input_file(input_file, buffer_))
        {}
        uint16_t get_word(uint16_t address) const {
            return buffer_[address];
        }
        uint8_t get_byte(uint16_t address) const {
            return buffer_[address];
        }
        uint8_t *get_byte_mod(uint16_t address) {
            return reinterpret_cast<uint8_t*>(&buffer_[address]);
        }
        size_t get_size() const {
            return size_;
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

    enum class eMovType {
        RegisterMemoryToFromRegister,
        ImmediateToRegisterMemory,
        ImmediateToRegister,
        MemoryToAccumulator,
        AccumulatorToMemory,
        RegisterMemoryToSegmentRegister,
        SegmentRegisterToRegisterMemory
    };
    enum class eArchRegister : int8_t {
        Al = 0, Cl = 1, Dl = 2, Bl = 3,
        Ah = 4, Ch = 5, Dh = 6, Bh = 7,
        Ax = 8, Cx = 9, Dx = 10, Bx = 11,
        Sp = 12, Bp = 13, Si = 14, Di = 15,
    };

    eArchRegister select_reg(std::bitset<3> reg, bool wide) {
        return eArchRegister(reg.to_ulong() + (wide ? 0 : 1) * reg.to_ulong());
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
            wide = int(register_) < 8;
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
    class CPU {
    public:
        CPU() = default;
        CPU(Memory& memory) : memory_(memory) {

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
        void mov(eMovType type, Operand destination, Operand source) {
            switch (type) {
                case eMovType::RegisterMemoryToFromRegister:
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
                case eMovType::ImmediateToRegisterMemory:
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
                case eMovType::ImmediateToRegister:
                {
                    mov_imm_to_reg(destination, source);
                }
                break;
                case eMovType::MemoryToAccumulator:
                {
                    mov_mem_to_reg(destination, source);
                }
                break;
                case eMovType::AccumulatorToMemory: {
                    mov_reg_to_mem(destination, source);
                }
                break;
                case eMovType::RegisterMemoryToSegmentRegister: {
                    std::logic_error("not implemented");
                }
                break;
                case eMovType::SegmentRegisterToRegisterMemory: {
                    std::logic_error("not implemented");
                }
                break;
                default:
                    throw std::logic_error("Unknown mov operation 3");
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
            return ss.str();
        }
        std::string state() {
            std::stringstream ss;
            ss << '\t' << "ax = 0x" << std::setw(4) << std::setfill('0') << std::hex << ax << '(' << int(ax) << ')' << '\n';
            ss << '\t' << "bx = 0x" << std::setw(4) << std::setfill('0') << std::hex << bx << '(' << int(bx) << ')' << '\n';
            ss << '\t' << "cx = 0x" << std::setw(4) << std::setfill('0') << std::hex << cx << '(' << int(cx) << ')' << '\n';
            ss << '\t' << "dx = 0x" << std::setw(4) << std::setfill('0') << std::hex << dx << '(' << int(dx) << ')' << '\n';
            ss << '\t' << "sp = 0x" << std::setw(4) << std::setfill('0') << std::hex << sp << '(' << int(sp) << ')' << '\n';
            ss << '\t' << "bp = 0x" << std::setw(4) << std::setfill('0') << std::hex << bp << '(' << int(bp) << ')' << '\n';
            ss << '\t' << "si = 0x" << std::setw(4) << std::setfill('0') << std::hex << si << '(' << int(si) << ')' << '\n';
            ss << '\t' << "di = 0x" << std::setw(4) << std::setfill('0') << std::hex << di << '(' << int(di) << ')';
            return ss.str();
        }
    private:
        uint16_t ax{}, bx{}, cx{}, dx{}, sp{}, bp{}, si{}, di{};
        uint16_t pax{}, pbx{}, pcx{}, pdx{}, psp{}, pbp{}, psi{}, pdi{};
        Memory &memory_;
        eEffectiveAddressCalculation eac_;
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
    class BinStream {
    public:
        void init(Memory mem, std::size_t length_bytes) {
            idex_ = 0;
            buff_ = std::vector<bool>(length_bytes * 8);
            for (ssize_t i = length_bytes - 1; i >= 0; i--) {
                auto b = static_cast<std::byte>(mem.get_byte(i));
                for (int j = 0; j < 8; j++) {
                    buff_[i*8 + 7 - j] = static_cast<bool>(b & (std::byte(1) << j));
                }
            }
            eof_ = false;
        }
        void init(const Pointerable auto *ptr, std::size_t length_bytes) {
            idex_ = 0;
            buff_ = std::vector<bool>(length_bytes * 8);
            for (ssize_t i = length_bytes - 1; i >= 0; i--) {
                auto b = static_cast<const std::byte>(*((std::byte*)ptr + i));
                for (int j = 0; j < 8; j++) {
                    buff_[i*8 + 7 - j] = static_cast<bool>(b & (std::byte(1) << j));
                }
            }
            eof_ = false;
        }
        void insert(const char *str, std::size_t length = std::numeric_limits<std::size_t>::max()) {
            const char *p = str;
            int len = 0;
            constexpr char ascii_zero_char_pos = 48;
            if (length != std::numeric_limits<std::size_t>::max())
                buff_.reserve(length);
            do {
                if (length == len)
                    break;
                const char val = static_cast<char>(*p) - ascii_zero_char_pos;
                if (val != 0 && val != 1)
                    throw std::invalid_argument("the input string contains characters other than zeroes and ones");
                buff_.push_back(static_cast<bool>(val));
                len++;
            } while (*++p);
            if (len)
                eof_ = false;
        }
        BinStream &operator<<(const char *str) {
            insert(str);
            return *this;
        }
        BinStream &operator<<(const std::string &str) {
            insert(str.c_str(), str.size());
            return *this;
        }
        BinStream &operator<<(Integral auto val) {
            if (val != 0 && val != 1)
                throw std::invalid_argument("the input is not zero or one");
            buff_.push_back(val);
        }
        BinStream &operator>>(bool &output) {
            if (eof_) {
                return *this;
            }
            output = buff_[idex_++];
            if (idex_ >= buff_.size()) {
                eof_ = true;
            }
            return *this;
        }
        BinStream &operator>>(Integral auto &output) {
            if (eof_) {
                return *this;
            }
            output = std::remove_reference_t<decltype(output)>(buff_[idex_++]);
            if (idex_ >= buff_.size()) {
                eof_ = true;
            }
            return *this;
        }
        operator bool() const {
            return !eof_;
        }
        size_t size() const {
            return buff_.size();
        }
        template<int N>
        std::bitset<N> read() {
            std::bitset<N> bitset;
            for (int i = N - 1; i >= 0; i--) {
                int res;
                *this >> res;
                bitset[i] = res;
            }
            return bitset;
        }
    private:
        std::vector<bool> buff_;
        std::size_t idex_ = 0;
        bool eof_ = true;
    };

    class BinTrie {
    public:
        explicit BinTrie(CPU &cpu) : cpu_(cpu) {}
        struct Node;
        using function_t = void (*)(
                Node *caller,
                BinStream &bin_stream,
                CPU& cpu,
                void *);
        struct Node {
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
        }
        template<size_t opcode_bitlength>
        void call(std::bitset<opcode_bitlength> bits, BinStream &bs, void *data = nullptr) {
            Node *ptr = sentinel_.children[0];
            for (int i = opcode_bitlength - 1; i >= 0; i--) {
                const int value = bits[i] ? 1 : 0;
                if (!ptr->children[value])
                    throw std::out_of_range("no function at this opcode");
                ptr = ptr->children[value];
            }
            if (!ptr->function)
                throw std::out_of_range("no function at this opcode");
            ptr->function(ptr, bs, cpu_, data);
        }
        void process_next_instruction(BinStream &bs, void *data) {
            Node *ptr = sentinel_.children[0];
            while (true) {
                int value = 0;
                bs >> value;
                if (!ptr->children[value])
                    throw std::out_of_range("no function at this opcode");
                ptr = ptr->children[value];
                if (ptr->function) {
                    ptr->function(ptr, bs, cpu_, data);
                    return;
                }
            }
        }
    private:
        CPU &cpu_;
        Node sentinel_;
    };

    namespace tests {
        void test_binstream() {
            BinStream bs;
            const char *str = "010101010101";
            const size_t size = strlen(str);
            bs << str;
            int i = 0;
            for (; i < size; i++) {
                int val = 0;
                bs >> val;
                if (bs && str[i] - 48 != val)
                    throw std::runtime_error("error!");
            }
            const int64_t bin_value = 1365;
            const size_t bin_value_size = sizeof(bin_value);
            bs.init(&bin_value, bin_value_size);
            for (i = 0; i < 52; i++) {
                int val = 0;
                bs >> val;
                if (bs && 0 != val)
                    throw std::runtime_error("error!");
            }
            for (i = 0; i < size; i++) {
                int val = 0;
                bs >> val;
                if (bs && str[i] - 48 != val)
                    throw std::runtime_error("error!");
            }
        }


        namespace detail {
            void test_function(BinTrie::Node *node, BinStream &bs, CPU &cpu, void *data) {
                std::cout << "success" << std::endl;
            }
        }
        void test_bin_trie() {
            BinStream bs;
            std::bitset<12> bits{010101010101};
            Memory mem;
            CPU cpu(mem);
            BinTrie bt(cpu);
            bt.add(bits, "test", "test", detail::test_function);
            bt.call(bits, bs, nullptr);
            bits.flip(0);
            try {
                bt.call(bits, bs, nullptr);
            } catch (...) {
                std::cout << "success 2" << std::endl;
            }
        };
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

    void imm_to_register_move(BinTrie::Node *node, BinStream &bs, CPU& cpu, void *data) {
        bool w;
        bs >> w;
        const auto reg = bs.read<3>();
        uint8_t lo = bs.read<8>().to_ulong();
        eMovType mov_type = eMovType::ImmediateToRegister;
        Operand dest_op, source_op;
        dest_op.set_type(Operand::eOperandType::Register);
        dest_op.set_register(reg, w);
        source_op.set_type(Operand::eOperandType::Immediate);
        if (w) {
            uint8_t hi = bs.read<8>().to_ulong();
            const int16_t value = static_cast<int16_t>(hi) << 8 | static_cast<int16_t>(lo);
            std::cout << node->name << ' ' << get_register_name(reg, w) << ", " << int16_t(value);
            source_op.set_mem_or_imm(value, true);
        } else {
            std::cout << node->name << ' ' << get_register_name(reg, w) << ", " << int(int8_t(lo));
            source_op.set_mem_or_imm(int8_t(lo), false);
        }
        {
            cpu.remember_prev_state();
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
    void register_memory_to_from_register(BinTrie::Node *node, BinStream &bs, CPU& cpu, void *data) {
        // MOV
        bool d, w;
        std::bitset<2> mod;
        std::bitset<3> reg, rm;
        bs >> d; // 0->source in REG, 1->dest in REG
        bs >> w; // 0->byte, 1->word
        mod = bs.read<2>();
        reg = bs.read<3>();
        rm = bs.read<3>();
        eMovType mov_type = eMovType::RegisterMemoryToFromRegister;;
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
                possible_displacement = int8_t(bs.read<8>().to_ulong());
                source_op.set_mem_or_imm(possible_displacement, false);
            } else if (mod.to_ulong() == 0b10 || (mod.to_ulong() == 0 && rm.to_ulong() == 0b110)) {
                possible_displacement = int16_t(bs.read<8>().to_ulong() | bs.read<8>().to_ulong() << 8);
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
            cpu.set_effective_address_calculation(rm, mod);
            cpu.mov(mov_type, destination_op, source_op);
            std::cout << " ; " << cpu.state_change();
        }
        std::cout << std::endl;
    }

    void imm_to_register_memory_move(BinTrie::Node *node, BinStream &bs, CPU& cpu, void *data) {
        bool w;
        std::bitset<2> mod;
        std::bitset<3> reg, rm;
        bs >> w;
        mod = bs.read<2>();
        reg = bs.read<3>(); // always 000
        rm = bs.read<3>();
        int16_t possible_data = 0;
        int16_t possible_displacement = 0;
        if (mod.to_ulong() == 0b01) {
            possible_displacement = int8_t(bs.read<8>().to_ulong());
        } else if (mod.to_ulong() == 0b10 || (mod.to_ulong() == 0 && rm.to_ulong() == 0b110)) {
            possible_displacement = int16_t(bs.read<8>().to_ulong() | bs.read<8>().to_ulong() << 8);
        }
        if (w) {
            possible_data = int16_t(bs.read<8>().to_ulong() | bs.read<8>().to_ulong() << 8);
        } else {
            possible_data = int8_t(bs.read<8>().to_ulong());
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
    }

    void accumulator_to_memory(BinTrie::Node *node, BinStream &bs, CPU& cpu, void *data) {
        bool w;
        bs >> w;
        uint16_t address = uint16_t(bs.read<8>().to_ulong() | bs.read<8>().to_ulong() << 8);
        std::cout << node->name << " [" << address << "], ax" << std::endl;
    }

    void memory_to_accumulator(BinTrie::Node *node, BinStream &bs, CPU& cpu, void *data) {
        bool w;
        bs >> w;
        uint16_t address = uint16_t(bs.read<8>().to_ulong() | bs.read<8>().to_ulong() << 8);
        std::cout << node->name << " ax, [" << address << "]" << std::endl;
    }
    void immediate_to_accumulator(BinTrie::Node *node, BinStream &bs, CPU& cpu, void *data) {
        bool w;
        bs >> w;
        if (w) {
            int16_t imm = bs.read<8>().to_ulong() | bs.read<8>().to_ulong() << 8;
            std::cout << node->name << " ax, " << imm << std::endl;
        } else {
            int8_t imm = int8_t(bs.read<8>().to_ulong());
            std::cout << node->name << " al, " << int(imm) << std::endl;
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
    void imm_to_register_memory_add(BinTrie::Node *node, BinStream &bs, CPU& cpu, void *data) {
        bool s = false, w = false;
        std::bitset<2> mod;
        std::bitset<3> reg, rm;
        bs >> s;
        bs >> w;
        mod = bs.read<2>();
        reg = bs.read<3>(); // always 000 for add

        rm = bs.read<3>();
        int16_t possible_data = 0;
        int16_t possible_displacement = 0;
        if (mod.to_ulong() == 0b01) {
            possible_displacement = int8_t(bs.read<8>().to_ulong());
        } else if (mod.to_ulong() == 0b10 || (mod.to_ulong() == 0 && rm.to_ulong() == 0b110)) {
            possible_displacement = int16_t(bs.read<8>().to_ulong() | bs.read<8>().to_ulong() << 8);
        }
        if (!s && w) {
            possible_data = int16_t(bs.read<8>().to_ulong() | bs.read<8>().to_ulong() << 8);
        } else {
            possible_data = int8_t(bs.read<8>().to_ulong());
        }
        if (mod.to_ulong() == 0b11) {
            if (s) {
                int16_t sign_extend = possible_data;
                std::cout << get_op_name(reg) << ' ' << get_register_name(rm, w)
                          << ", "
                          << int(sign_extend)
                          << std::endl;
            } else {
                uint16_t sign_extend = possible_data;
                std::cout << get_op_name(reg) << ' ' << get_register_name(rm, w)
                          << ", "
                          << int(sign_extend)
                          << std::endl;
            }
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
            std::cout << "], "
                      << possible_data
                      << std::endl;
        }
    }
    void conditional_jump(BinTrie::Node *node, BinStream &bs, CPU& cpu, void *data) {
        std::cout << node->name << ' ' << int(bs.read<8>().to_ulong()) << std::endl;
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
    x86::BinStream bs;
    bs.init(memory, memory.get_size());

    x86::CPU cpu(memory);
    x86::BinTrie commands(cpu);
    commands.add(std::bitset<6>{0b100010},
                 "mov",
                 "register/memory to/from register",
                 x86::register_memory_to_from_register);
    commands.add(std::bitset<4>{0b1011},
                 "mov",
                 "immediate to register",
                 x86::imm_to_register_move);
    commands.add(std::bitset<7>{0b1100011},
                 "mov",
                 "immediate to register/memory",
                 x86::imm_to_register_memory_move);
    commands.add(std::bitset<7>{0b1010000},
                 "mov",
                 "memory to accumulator",
                 x86::memory_to_accumulator);
    commands.add(std::bitset<7>{0b1010001},
                 "mov",
                 "accumulator to memory",
                 x86::accumulator_to_memory);

    commands.add(std::bitset<6>{0b000000},
                 "add",
                 "reg/memory with register to either",
                 x86::register_memory_to_from_register);
    commands.add(std::bitset<6>{0b100000},
                 "add",
                 "immediate to register/memory",
                 x86::imm_to_register_memory_add);
    commands.add(std::bitset<7>{0b0000010},
                 "add",
                 "immediate to accumulator",
                 x86::immediate_to_accumulator);

    commands.add(std::bitset<6>{0b001010},
                 "sub",
                 "reg/memory with register to either",
                 x86::register_memory_to_from_register);
    commands.add(std::bitset<7>{0b0010110},
                 "sub",
                 "immediate to accumulator",
                 x86::immediate_to_accumulator);

    commands.add(std::bitset<6>{0b001110},
                 "cmp",
                 "reg/memory with register to either",
                 x86::register_memory_to_from_register);
    commands.add(std::bitset<7>{0b0011110},
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
        commands.add(std::bitset<8>{opcode},
                     name,
                     "jump equals",
                     x86::conditional_jump);
    }
    bs.init(memory, memory.get_size());

    // walk file
    int instruction_n = 1;
    while (instruction_n++, bs) {
        commands.process_next_instruction(bs, nullptr);
    }
}
