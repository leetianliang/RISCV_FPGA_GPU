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

void test_valid_fill_header() {
    const auto cmd = golden::make_fill_rect_cmd(
        0x10000, 32, 0, 0, 4, 4, golden::Rgba8888::pack(255, 255, 0, 0));
    const auto dec = golden::decode_cmd_header(cmd);
    EXPECT_TRUE(dec.ok);
    EXPECT_TRUE(dec.header.cmd_class == golden::kClassDraw2D);
    EXPECT_TRUE(dec.header.opcode == golden::kOpcodeFillRect);
    EXPECT_TRUE(dec.header.version == golden::kCmdEncodingVersion);
    EXPECT_TRUE(dec.header.length_dw == golden::kCmdLengthDw);
}

void test_bad_version() {
    auto cmd = golden::make_fill_rect_cmd(0x10000, 32, 0, 0, 1, 1,
                                          golden::Rgba8888::from_u32(0xFFFFFFFFu));
    cmd[0] = golden::header_word(golden::kClassDraw2D, golden::kOpcodeFillRect,
                                 2, golden::kCmdLengthDw, 0);
    const auto dec = golden::decode_cmd_header(cmd);
    EXPECT_TRUE(!dec.ok);
    EXPECT_TRUE(dec.fault == golden::FaultCode::BAD_VERSION);
}

void test_bad_length() {
    auto cmd = golden::make_fill_rect_cmd(0x10000, 32, 0, 0, 1, 1,
                                          golden::Rgba8888::from_u32(0xFFFFFFFFu));
    cmd[0] = golden::header_word(golden::kClassDraw2D, golden::kOpcodeFillRect,
                                 golden::kCmdEncodingVersion, 8, 0);
    const auto dec = golden::decode_cmd_header(cmd);
    EXPECT_TRUE(!dec.ok);
    EXPECT_TRUE(dec.fault == golden::FaultCode::BAD_LENGTH);
}

void test_unknown_class() {
    auto cmd = golden::make_fill_rect_cmd(0x10000, 32, 0, 0, 1, 1,
                                          golden::Rgba8888::from_u32(0xFFFFFFFFu));
    cmd[0] = golden::header_word(0x2, golden::kOpcodeFillRect,
                                 golden::kCmdEncodingVersion, golden::kCmdLengthDw, 0);
    const auto dec = golden::decode_cmd_header(cmd);
    EXPECT_TRUE(!dec.ok);
    EXPECT_TRUE(dec.fault == golden::FaultCode::BAD_CMD_CLASS);
}

void test_unknown_opcode() {
    auto cmd = golden::make_fill_rect_cmd(0x10000, 32, 0, 0, 1, 1,
                                          golden::Rgba8888::from_u32(0xFFFFFFFFu));
    cmd[0] = golden::header_word(golden::kClassDraw2D, 0x10,
                                 golden::kCmdEncodingVersion, golden::kCmdLengthDw, 0);
    const auto dec = golden::decode_cmd_header(cmd);
    EXPECT_TRUE(!dec.ok);
    EXPECT_TRUE(dec.fault == golden::FaultCode::BAD_OPCODE);
}

void test_hdr_reserved_nonzero() {
    auto cmd = golden::make_fill_rect_cmd(0x10000, 32, 0, 0, 1, 1,
                                          golden::Rgba8888::from_u32(0xFFFFFFFFu));
    cmd[0] = golden::header_word(golden::kClassDraw2D, golden::kOpcodeFillRect,
                                 golden::kCmdEncodingVersion, golden::kCmdLengthDw,
                                 0x10u);  // reserved bit 4
    const auto dec = golden::decode_cmd_header(cmd);
    EXPECT_TRUE(!dec.ok);
    EXPECT_TRUE(dec.fault == golden::FaultCode::RESERVED_NONZERO);
}

void test_ext_valid_unsupported() {
    auto cmd = golden::make_fill_rect_cmd(0x10000, 32, 0, 0, 1, 1,
                                          golden::Rgba8888::from_u32(0xFFFFFFFFu));
    cmd[0] = golden::header_word(golden::kClassDraw2D, golden::kOpcodeFillRect,
                                 golden::kCmdEncodingVersion, golden::kCmdLengthDw,
                                 golden::kHExtValid);
    cmd[3] = 0x20000000u;
    const auto dec = golden::decode_fill_rect(cmd);
    EXPECT_TRUE(!dec.ok);
    EXPECT_TRUE(dec.fault == golden::FaultCode::UNSUPPORTED_FEATURE);
}

void test_ext_valid_zero_ptr() {
    auto cmd = golden::make_fill_rect_cmd(0x10000, 32, 0, 0, 1, 1,
                                          golden::Rgba8888::from_u32(0xFFFFFFFFu));
    cmd[0] = golden::header_word(golden::kClassDraw2D, golden::kOpcodeFillRect,
                                 golden::kCmdEncodingVersion, golden::kCmdLengthDw,
                                 golden::kHExtValid);
    cmd[3] = 0;
    const auto dec = golden::decode_cmd_header(cmd);
    // EXT valid with ptr=0 is invalid encoding
    EXPECT_TRUE(!dec.ok);
    EXPECT_TRUE(dec.fault == golden::FaultCode::BAD_EXT_PTR ||
                dec.fault == golden::FaultCode::BAD_EXT_PTR);
}

}  // namespace

int main() {
    test_valid_fill_header();
    test_bad_version();
    test_bad_length();
    test_unknown_class();
    test_unknown_opcode();
    test_hdr_reserved_nonzero();
    test_ext_valid_unsupported();
    test_ext_valid_zero_ptr();
    if (g_failures != 0) {
        std::printf("test_command_header: %d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("test_command_header: PASS\n");
    return 0;
}
