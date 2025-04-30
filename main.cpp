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

namespace x86 {
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
        struct Node;
        using function_t = void (*)(
                Node *caller,
                BinStream &bin_stream,
                void *data);
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
            ptr->function(ptr, bs, data);
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
                    ptr->function(ptr, bs, data);
                    return;
                }
            }
        }
    private:
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
            void test_function(BinTrie::Node *node, BinStream &bs, void *data) {
                std::cout << "success" << std::endl;
            }
        }
        void test_bin_trie() {
            BinStream bs;
            std::bitset<12> bits{010101010101};
            BinTrie bt;
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

    void imm_to_register_move(BinTrie::Node *node, BinStream &bs, void *data) {
        bool w;
        bs >> w;
        const auto reg = bs.read<3>();
        uint8_t lo = bs.read<8>().to_ulong();
        if (w) {
            uint8_t hi = bs.read<8>().to_ulong();
            const int16_t value = static_cast<int16_t>(hi) << 8 | static_cast<int16_t>(lo);
            std::cout << node->name << ' ' << get_register_name(reg, w) << ", " << int16_t(value)
                      << std::endl;
        } else {
            std::cout << node->name << ' ' << get_register_name(reg, w) << ", " << int(int8_t(lo)) << std::endl;
        }
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
    void register_memory_to_from_register(BinTrie::Node *node, BinStream &bs, void *data) {
        // MOV
        bool d, w;
        std::bitset<2> mod;
        std::bitset<3> reg, rm;
        bs >> d; // 0->source in REG, 1->dest in REG
        bs >> w; // 0->byte, 1->word
        mod = bs.read<2>();
        reg = bs.read<3>();
        rm = bs.read<3>();
        if (mod.to_ulong() == 0b11) {
            const char *source = d ? get_register_name(rm, w) : get_register_name(reg, w);
            const char *dest = d ? get_register_name(reg, w) : get_register_name(rm, w);
            std::cout << node->name << ' ' << dest << ", " << source << std::endl;
        } else {
            int16_t possible_displacement = 0;
            if (mod.to_ulong() == 0b01) {
                possible_displacement = int8_t(bs.read<8>().to_ulong());
            } else if (mod.to_ulong() == 0b10 || (mod.to_ulong() == 0 && rm.to_ulong() == 0b110)) {
                possible_displacement = int16_t(bs.read<8>().to_ulong() | bs.read<8>().to_ulong() << 8);
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
                std::cout << "]"
                          << std::endl;
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
                          << get_register_name(reg, w)
                          << std::endl;

            }
        }
    }

    void imm_to_register_memory_move(BinTrie::Node *node, BinStream &bs, void *data) {
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

    void accumulator_to_memory(BinTrie::Node *node, BinStream &bs, void *data) {
        bool w;
        bs >> w;
        uint16_t address = uint16_t(bs.read<8>().to_ulong() | bs.read<8>().to_ulong() << 8);
        std::cout << node->name << " [" << address << "], ax" << std::endl;
    }

    void memory_to_accumulator(BinTrie::Node *node, BinStream &bs, void *data) {
        bool w;
        bs >> w;
        uint16_t address = uint16_t(bs.read<8>().to_ulong() | bs.read<8>().to_ulong() << 8);
        std::cout << node->name << " ax, [" << address << "]" << std::endl;
    }
    void immediate_to_accumulator(BinTrie::Node *node, BinStream &bs, void *data) {
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
    void imm_to_register_memory_add(BinTrie::Node *node, BinStream &bs, void *data) {
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
    void conditional_jump(BinTrie::Node *node, BinStream &bs, void *data) {
        std::cout << node->name << ' ' << int(bs.read<8>().to_ulong()) << std::endl;
    }
}

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
    static char memory[256*256] = {};
    const size_t read_size = load_input_file(input_file, memory);
    x86::BinStream bs;
    bs.init(memory, read_size);

    x86::BinTrie commands;
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
    bs.init(memory, read_size);
    // walk file
    int instruction_n = 1;
    while (instruction_n++, bs) {
        commands.process_next_instruction(bs, nullptr);
    }
}
