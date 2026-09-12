#include "golden/command_decoder.hpp"
#include "golden/gpu_isa.hpp"

#include <cstdio>

namespace {

int g_failures = 0;

#define EXPECT_TRUE(cond)                                                     \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
            ++g_failures;                                                     \
        }                                                                     \
    } while (0)

void test_valid_fills() {
    const auto cmd = golden::make_fill_rect_cmd(0x10000, 32, 0, 0, 4, 4,
                                                golden::Rgba8888::pack(255, 1, 2, 3));
    const auto d = golden::decode_draw_2d(cmd);
    EXPECT_TRUE(d.ok);
    EXPECT_TRUE(d.state.op == golden::DrawOp::FILL_RECT);
}

void test_bad_version_length() {
    auto cmd = golden::make_fill_rect_cmd(0x10000, 32, 0, 0, 1, 1,
                                          golden::Rgba8888::from_u32(0xFFFFFFFFu));
    cmd[0] = golden::header_word(golden::kClassDraw2D, golden::kOpcodeFillRect, 2,
                                 golden::kCmdLengthDw, 0);
    EXPECT_TRUE(golden::decode_cmd_header(cmd).fault == golden::FaultCode::BAD_VERSION);
    cmd[0] = golden::header_word(golden::kClassDraw2D, golden::kOpcodeFillRect, 1, 8, 0);
    EXPECT_TRUE(golden::decode_cmd_header(cmd).fault == golden::FaultCode::BAD_LENGTH);
}

void test_opcode_class() {
    auto cmd = golden::make_fill_rect_cmd(0x10000, 32, 0, 0, 1, 1,
                                          golden::Rgba8888::from_u32(0xFFFFFFFFu));
    cmd[0] = golden::header_word(0x2, 0, 1, 16, 0);
    EXPECT_TRUE(golden::decode_cmd_header(cmd).fault == golden::FaultCode::BAD_CMD_CLASS);

    cmd[0] = golden::header_word(golden::kClassDraw2D, 0x11, 1, 16, 0);
    EXPECT_TRUE(golden::decode_cmd_header(cmd).fault == golden::FaultCode::BAD_OPCODE);

    cmd[0] = golden::header_word(golden::kClassDraw2D, golden::kOpcodeBlitExt, 1, 16, 0);
    EXPECT_TRUE(golden::decode_cmd_header(cmd).fault ==
                golden::FaultCode::UNSUPPORTED_FEATURE);

    cmd[0] = golden::header_word(golden::kClassDraw2D, golden::kOpcodeTileFrame, 1, 16, 0);
    EXPECT_TRUE(golden::decode_cmd_header(cmd).fault ==
                golden::FaultCode::UNSUPPORTED_FEATURE);

    cmd[0] = golden::header_word(golden::kClassDraw2D, golden::kOpcodeBlit, 1, 16, 0);
    EXPECT_TRUE(golden::decode_cmd_header(cmd).ok);
}

void test_strict_reserved() {
    auto cmd = golden::make_fill_rect_cmd(0x10000, 32, 0, 0, 1, 1,
                                          golden::Rgba8888::from_u32(0xFFFFFFFFu));
    // non-strict: reserved hdr flags ignored
    cmd[0] = golden::header_word(golden::kClassDraw2D, golden::kOpcodeFillRect, 1, 16, 0x10u);
    auto h = golden::decode_cmd_header(cmd);
    EXPECT_TRUE(h.ok);
    // strict: reserved hdr flags fault
    cmd[0] = golden::header_word(golden::kClassDraw2D, golden::kOpcodeFillRect, 1, 16,
                                 golden::kHStrict | 0x10u);
    h = golden::decode_cmd_header(cmd);
    EXPECT_TRUE(!h.ok);
    EXPECT_TRUE(h.fault == golden::FaultCode::RESERVED_NONZERO);
}

void test_strict_ext_ptr() {
    auto cmd = golden::make_fill_rect_cmd(0x10000, 32, 0, 0, 1, 1,
                                          golden::Rgba8888::from_u32(0xFFFFFFFFu));
    // non-strict + ext_ptr nonzero without H_EXT_VALID: ignore per strict-only rule
    cmd[0] = golden::header_word(golden::kClassDraw2D, golden::kOpcodeFillRect, 1, 16, 0);
    cmd[3] = 0x1000;
    EXPECT_TRUE(golden::decode_cmd_header(cmd).ok);
    // strict: fault
    cmd[0] = golden::header_word(golden::kClassDraw2D, golden::kOpcodeFillRect, 1, 16,
                                 golden::kHStrict);
    EXPECT_TRUE(golden::decode_cmd_header(cmd).fault == golden::FaultCode::BAD_EXT_PTR);
}

void test_blit_unsupported_scale() {
    golden::BlitCmdDesc d;
    d.src_base = 0x20000;
    d.dst_base = 0x10000;
    d.src_stride = 16;
    d.dst_stride = 32;
    d.w = 4;
    d.h = 4;
    auto cmd = golden::make_blit_cmd(d);
    cmd[10] = golden::pack_wh(8, 4);  // scale
    cmd[11] = golden::pack_wh(4, 4);
    const auto dec = golden::decode_draw_2d(cmd);
    EXPECT_TRUE(!dec.ok);
    EXPECT_TRUE(dec.fault == golden::FaultCode::UNSUPPORTED_FEATURE);
}

}  // namespace

int main() {
    test_valid_fills();
    test_bad_version_length();
    test_opcode_class();
    test_strict_reserved();
    test_strict_ext_ptr();
    test_blit_unsupported_scale();
    if (g_failures) {
        std::printf("golden_test_command_header FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_command_header PASS\n");
    return 0;
}
