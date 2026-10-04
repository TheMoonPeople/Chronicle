#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "dataalloc.hpp"
#include "runscript.hpp"

// editloop3.cpp's event runner; editloop3.hpp needs more of the game's headers than this test does.
int EdSetEventScript(char *common_script, char *map_script, CDataAlloc2<1> *allocator);
int EdEventInit(int event_number, CDataAlloc2<1> *arena, char *program);
int EdRunEvent(int program, CDataAlloc2<1> *arena);

// The script VM on a .stb assembled here in the on-disc layout: a five-word header, the program
// table, then the code section holding the instructions, 16-byte function records and strings,
// every offset a 32-bit word.

namespace {

class Stb {
public:
    // A function record in the code section; returns its offset from the code section.
    int Func(int local, int arg) {
        int at = Here();
        funcs_.push_back(at);
        Emit(0);
        Emit(0);
        Emit(local);
        Emit(arg);
        return at;
    }

    void Start(int func) { code_[func / 4] = Here(); }

    int Here() const { return static_cast<int>(code_.size() * 4); }

    int Op(int op, int arg1 = 0, int arg2 = 0) {
        int at = Here();
        Emit(op);
        Emit(arg1);
        Emit(arg2);
        return at;
    }

    int PushInt(int value) { return Op(RS_OP_PUSH_CONST, 1, value); }

    int PushFloat(float value) {
        int bits;
        std::memcpy(&bits, &value, sizeof(bits));
        return Op(RS_OP_PUSH_CONST, 2, bits);
    }

    int PushStr(int offset) { return Op(RS_OP_PUSH_CONST, 3, offset); }

    void Patch(int instruction, int arg1) { code_[instruction / 4 + 1] = arg1; }

    int String(const char *text) {
        int         at = Here();
        std::string padded(text);
        padded.resize((padded.size() / 4 + 1) * 4, '\0');
        for (size_t i = 0; i < padded.size(); i += 4) {
            int word;
            std::memcpy(&word, padded.data() + i, 4);
            Emit(word);
        }
        return at;
    }

    void Program(int no, int func) { programs_.push_back({no, func}); }

    void Main(int func) { main_ = func; }

    // Header at 0, program table at 0x20, code after it; entry offsets count from the header.
    std::vector<uint32_t> Build() const {
        int                   table = 0x20;
        int                   code = table + static_cast<int>(programs_.size()) * 8;
        std::vector<uint32_t> file(static_cast<size_t>(code / 4) + code_.size(), 0);
        file[0] = 0x425453;
        file[1] = static_cast<uint32_t>(code + main_);
        file[2] = static_cast<uint32_t>(code);
        file[3] = static_cast<uint32_t>(table);
        file[4] = static_cast<uint32_t>(programs_.size());
        for (size_t i = 0; i < programs_.size(); i++) {
            file[table / 4 + i * 2] = static_cast<uint32_t>(programs_[i].first);
            file[table / 4 + i * 2 + 1] = static_cast<uint32_t>(code + programs_[i].second);
        }
        std::memcpy(file.data() + code / 4, code_.data(), code_.size() * 4);
        return file;
    }

private:
    void Emit(int word) { code_.push_back(static_cast<uint32_t>(word)); }

    std::vector<uint32_t>            code_;
    std::vector<int>                 funcs_;
    std::vector<std::pair<int, int>> programs_;
    int                              main_ = 0;
};

struct ExtCall {
    int         ext;
    int         argc;
    int         type;
    int         i;
    float       f;
    std::string s;
};

std::vector<ExtCall> g_calls;

int Record(int ext, RS_STACKDATA *args, int argc) {
    for (int k = 0; k < argc; k++) {
        ExtCall call{ext, argc, args[k].type, 0, 0.0f, {}};
        if (args[k].type == RS_INT) {
            call.i = args[k].i;
        } else if (args[k].type == RS_FLOAT) {
            call.f = args[k].f;
        } else if (args[k].type == RS_STR) {
            call.s = args[k].s;
        }
        g_calls.push_back(call);
    }
    return 1;
}

int Ext0(RS_STACKDATA *args, int argc) {
    return Record(0, args, argc);
}

int Ext1(RS_STACKDATA *args, int argc) {
    return Record(1, args, argc);
}

// Writes 42 through the reference it is handed, as the game's getters do.
int Ext2(RS_STACKDATA *args, int argc) {
    (void) argc;
    args[0].p->type = RS_INT;
    args[0].p->i = 42;
    return 1;
}

int (*g_table[3])(RS_STACKDATA *, int) = {Ext0, Ext1, Ext2};

// Retail's arena carve-outs: 64 quadwords of stack, 384 of call records.
struct Arena {
    unsigned char stack[64 * 16];
    unsigned char call[384 * 16];
};

void Load(CRunScript &script, std::vector<uint32_t> &file, Arena &arena) {
    std::memset(&arena, 0xA5, sizeof(arena));
    script.load(reinterpret_cast<RS_PROG_HEADER *>(file.data()), reinterpret_cast<RS_STACKDATA *>(arena.stack),
                128, reinterpret_cast<RS_CALLDATA *>(arena.call), 512);
    script.ext_func(g_table, 3);
}

bool Untouched(const Arena &arena) {
    for (unsigned char byte : arena.stack) {
        if (byte != 0xA5) {
            return false;
        }
    }
    for (unsigned char byte : arena.call) {
        if (byte != 0xA5) {
            return false;
        }
    }
    return true;
}

} // namespace

// Locals, stores, an integer compare and both jumps: the sum of 1 to 5 through a loop.
TEST(DungeonScriptVm, LoopsWithCompareAndJumps) {
    Stb stb;
    int sum = stb.Func(2, 0);
    stb.Start(sum);
    stb.Op(RS_OP_LOAD_ADDR, 0, RS_ADDR_LOCAL);
    stb.PushInt(1);
    stb.Op(RS_OP_STORE);
    stb.Op(RS_OP_POP);
    stb.Op(RS_OP_LOAD_ADDR, 1, RS_ADDR_LOCAL);
    stb.PushInt(0);
    stb.Op(RS_OP_STORE);
    stb.Op(RS_OP_POP);
    int loop = stb.Op(RS_OP_LOAD, 0, RS_ADDR_LOCAL);
    stb.PushInt(5);
    stb.Op(RS_OP_CMP, RS_CMP_LE);
    int exit = stb.Op(RS_OP_JMP_FALSE, 0, 0);
    stb.Op(RS_OP_LOAD_ADDR, 1, RS_ADDR_LOCAL);
    stb.Op(RS_OP_LOAD, 1, RS_ADDR_LOCAL);
    stb.Op(RS_OP_LOAD, 0, RS_ADDR_LOCAL);
    stb.Op(RS_OP_ADD);
    stb.Op(RS_OP_STORE);
    stb.Op(RS_OP_POP);
    stb.Op(RS_OP_LOAD_ADDR, 0, RS_ADDR_LOCAL);
    stb.Op(RS_OP_LOAD, 0, RS_ADDR_LOCAL);
    stb.PushInt(1);
    stb.Op(RS_OP_ADD);
    stb.Op(RS_OP_STORE);
    stb.Op(RS_OP_POP);
    stb.Op(RS_OP_JMP, loop);
    stb.Patch(exit, stb.Op(RS_OP_LOAD, 1, RS_ADDR_LOCAL));
    stb.Op(RS_OP_RET);
    stb.Program(1, sum);

    std::vector<uint32_t> file = stb.Build();
    CRunScript            script;
    Arena                 arena;
    Load(script, file, arena);
    ASSERT_TRUE(script.check_program(1) == 1);
    ASSERT_TRUE(script.check_program(2) == 0);
    ASSERT_TRUE(script.run(1) == 0);
    ASSERT_TRUE(script.IsEnd() != 0);
    ASSERT_TRUE(script.result == 15);
    ASSERT_TRUE(script.run(2) == -1);
    ASSERT_TRUE(Untouched(arena));
}

// A script call through a 16-byte record placed after another (a 24-byte read would take the
// neighbour's words), an external call with an int and a string, waits and resumes, a float
// constant, and an external getter writing through a reference.
TEST(DungeonScriptVm, CallsWaitsAndExternalFunctions) {
    Stb stb;
    int triple = stb.Func(2, 1);
    int main = stb.Func(1, 0);
    int hello = stb.String("hello");
    stb.Start(triple);
    stb.Op(RS_OP_LOAD, 0, RS_ADDR_LOCAL);
    stb.PushInt(3);
    stb.Op(RS_OP_MUL);
    stb.Op(RS_OP_RET);
    stb.Start(main);
    stb.PushInt(0);
    stb.PushInt(7);
    stb.Op(RS_OP_CALL, 0, triple);
    stb.PushStr(hello);
    stb.Op(RS_OP_EXT, 3);
    stb.Op(RS_OP_WAIT);
    stb.PushInt(1);
    stb.PushFloat(2.5f);
    stb.Op(RS_OP_EXT, 2);
    stb.Op(RS_OP_WAIT);
    stb.PushInt(2);
    stb.Op(RS_OP_LOAD_ADDR, 0, RS_ADDR_LOCAL);
    stb.Op(RS_OP_EXT, 2);
    stb.PushInt(0);
    stb.Op(RS_OP_LOAD, 0, RS_ADDR_LOCAL);
    stb.Op(RS_OP_EXT, 2);
    stb.Op(RS_OP_END);
    stb.Main(main);

    std::vector<uint32_t> file = stb.Build();
    CRunScript            script;
    Arena                 arena;
    Load(script, file, arena);
    g_calls.clear();

    ASSERT_TRUE(script.run(-1) == 1);
    ASSERT_TRUE(script.IsEnd() == 0);
    ASSERT_TRUE(g_calls.size() == 2);
    ASSERT_TRUE(g_calls[0].ext == 0 && g_calls[0].argc == 2);
    ASSERT_TRUE(g_calls[0].type == RS_INT && g_calls[0].i == 21);
    ASSERT_TRUE(g_calls[1].type == RS_STR && g_calls[1].s == "hello");

    script.resume();
    ASSERT_TRUE(script.IsEnd() == 0);
    ASSERT_TRUE(g_calls.size() == 3);
    ASSERT_TRUE(g_calls[2].ext == 1 && g_calls[2].type == RS_FLOAT);
    ASSERT_NEAR(g_calls[2].f, 2.5f, 0.0f);

    script.resume();
    ASSERT_TRUE(script.IsEnd() != 0);
    ASSERT_TRUE(g_calls.size() == 4);
    ASSERT_TRUE(g_calls[3].ext == 0 && g_calls[3].type == RS_INT && g_calls[3].i == 42);
    ASSERT_TRUE(Untouched(arena));
}

// skip runs through waits with external calls and jumps suppressed until SKIP_END, and stops
// after it.
TEST(DungeonScriptVm, SkipRunsToSkipEnd) {
    Stb stb;
    int main = stb.Func(0, 0);
    stb.Start(main);
    stb.Op(RS_OP_WAIT);
    stb.PushInt(0);
    stb.PushInt(9);
    stb.Op(RS_OP_EXT, 2);
    stb.Op(RS_OP_WAIT);
    int jump = stb.Op(RS_OP_JMP, 0);
    stb.Op(RS_OP_SKIP_END);
    stb.PushInt(0);
    stb.PushInt(10);
    stb.Op(RS_OP_EXT, 2);
    stb.Op(RS_OP_WAIT);
    int end = stb.Op(RS_OP_END);
    stb.Patch(jump, end);
    stb.Program(3, main);

    std::vector<uint32_t> file = stb.Build();
    CRunScript            script;
    Arena                 arena;
    Load(script, file, arena);
    g_calls.clear();

    ASSERT_TRUE(script.run(3) == 1);
    ASSERT_TRUE(g_calls.empty());
    script.skip();
    ASSERT_TRUE(g_calls.empty());
    ASSERT_TRUE(script.IsEnd() == 0);
    script.resume();
    ASSERT_TRUE(g_calls.size() == 1 && g_calls[0].i == 10);
    ASSERT_TRUE(script.IsEnd() == 0);
    script.resume();
    ASSERT_TRUE(script.IsEnd() != 0);
}

// Mixed int and float arithmetic, float compares, conversions, modulo, negation and logic.
TEST(DungeonScriptVm, MixedArithmetic) {
    Stb stb;
    int main = stb.Func(0, 0);
    stb.Start(main);
    stb.PushInt(0);
    stb.PushFloat(1.5f);
    stb.PushInt(2);
    stb.Op(RS_OP_ADD);
    stb.Op(RS_OP_FTOI);
    stb.PushInt(2);
    stb.Op(RS_OP_MOD);
    stb.Op(RS_OP_NEG);
    stb.Op(RS_OP_EXT, 2);
    stb.PushInt(0);
    stb.PushInt(3);
    stb.PushFloat(2.5f);
    stb.Op(RS_OP_CMP, RS_CMP_GT);
    stb.PushInt(6);
    stb.PushInt(3);
    stb.Op(RS_OP_AND);
    stb.Op(RS_OP_OR);
    stb.Op(RS_OP_NOT);
    stb.Op(RS_OP_EXT, 2);
    stb.PushInt(1);
    stb.PushInt(7);
    stb.Op(RS_OP_ITOF);
    stb.PushFloat(2.0f);
    stb.Op(RS_OP_DIV);
    stb.PushInt(1);
    stb.Op(RS_OP_SUB);
    stb.Op(RS_OP_EXT, 2);
    stb.PushInt(0);
    stb.Op(RS_OP_SIN);
    stb.Op(RS_OP_RET);
    stb.Program(4, main);

    std::vector<uint32_t> file = stb.Build();
    CRunScript            script;
    Arena                 arena;
    Load(script, file, arena);
    g_calls.clear();

    ASSERT_TRUE(script.run(4) == 0);
    ASSERT_TRUE(g_calls.size() == 3);
    ASSERT_TRUE(g_calls[0].type == RS_INT && g_calls[0].i == -1);
    ASSERT_TRUE(g_calls[1].type == RS_INT && g_calls[1].i == 0);
    ASSERT_TRUE(g_calls[2].ext == 1 && g_calls[2].type == RS_FLOAT);
    ASSERT_NEAR(g_calls[2].f, 2.5f, 0.0f);
    ASSERT_TRUE(script.result == 0);
}

// 128 operand slots and 512 nested calls at host sizes, none of them in the arena retail sized
// for the PS2's records.
TEST(DungeonScriptVm, StacksAreHostSized) {
    Stb stb;
    int down = stb.Func(1, 1);
    int main = stb.Func(0, 0);
    stb.Start(down);
    stb.Op(RS_OP_LOAD, 0, RS_ADDR_LOCAL);
    stb.PushInt(0);
    stb.Op(RS_OP_CMP, RS_CMP_EQ);
    int base = stb.Op(RS_OP_JMP_TRUE, 0, 0);
    stb.Op(RS_OP_LOAD, 0, RS_ADDR_LOCAL);
    stb.PushInt(1);
    stb.Op(RS_OP_SUB);
    stb.Op(RS_OP_CALL, 0, down);
    stb.PushInt(1);
    stb.Op(RS_OP_ADD);
    stb.Op(RS_OP_RET);
    stb.Patch(base, stb.PushInt(0));
    stb.Op(RS_OP_RET);
    stb.Start(main);
    stb.PushInt(100);
    stb.Op(RS_OP_CALL, 0, down);
    stb.Op(RS_OP_RET);
    stb.Program(5, main);

    std::vector<uint32_t> file = stb.Build();
    CRunScript            script;
    Arena                 arena;
    Load(script, file, arena);
    ASSERT_TRUE(script.stack_num == 128 && script.call_num == 512);
    ASSERT_TRUE(reinterpret_cast<unsigned char *>(script.stack) != arena.stack);
    ASSERT_TRUE(script.run(5) == 0);
    ASSERT_TRUE(script.result == 100);
    ASSERT_TRUE(Untouched(arena));
}

TEST(DungeonScriptVm, TownEventReturnsCompletionStatus) {
    Stb stb;
    int complete = stb.Func(0, 0);
    stb.Start(complete);
    stb.Op(RS_OP_END);
    int suspended = stb.Func(0, 0);
    stb.Start(suspended);
    stb.Op(RS_OP_WAIT);
    stb.Op(RS_OP_END);
    stb.Program(1, complete);
    stb.Program(2, suspended);

    std::vector<uint32_t>     file = stb.Build();
    alignas(16) unsigned char storage[16 * 512] = {};
    CDataAlloc2<1>            arena;
    arena.base = storage;
    arena.buffer = storage;
    arena.used = 3;
    arena.limit = 512;

    // EdSetEventScript loads the town's own interpreter, the one EdEventInit and EdRunEvent start.
    char *program = reinterpret_cast<char *>(file.data());
    ASSERT_EQ(EdSetEventScript(program, program, &arena), 1);
    const int used = arena.used;

    for (int tick = 0; tick < 6; tick++) {
        EXPECT_EQ(EdEventInit(1, &arena, program), 0);
        EXPECT_EQ(EdRunEvent(1, &arena), 0);
        EXPECT_EQ(EdEventInit(2, &arena, program), 1);
        EXPECT_EQ(EdRunEvent(2, &arena), 1);
    }

    EXPECT_EQ(EdEventInit(3, &arena, program), 0);
    EXPECT_EQ(EdRunEvent(3, &arena), -1);
    EXPECT_EQ(arena.used, used);
}
