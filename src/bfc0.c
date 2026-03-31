#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    FILE_BASE = 0x08048000u,
    CODE_FILE_OFFSET = 0x00000100u,
    TAPE_SIZE = 0x01000000u,
    OP_SIZE = 7u,
    EXIT_CODE_SIZE = 9u,
};

typedef struct {
    unsigned char *data;
    size_t len;
    size_t cap;
} Buffer;

static const char *RUNTIME_ERROR_MSG = "runtime error: pointer underflow\n";

static void die_errno(const char *context) {
    fprintf(stderr, "%s: %s\n", context, strerror(errno));
    exit(1);
}

static void die_compile(const char *message) {
    fprintf(stderr, "compile error: %s\n", message);
    exit(1);
}

static void reserve(Buffer *buffer, size_t extra) {
    size_t need = buffer->len + extra;
    if (need <= buffer->cap) {
        return;
    }
    size_t next = buffer->cap ? buffer->cap : 256;
    while (next < need) {
        next *= 2;
    }
    unsigned char *data = realloc(buffer->data, next);
    if (!data) {
        die_errno("realloc");
    }
    buffer->data = data;
    buffer->cap = next;
}

static void emit_u8(Buffer *buffer, uint8_t value) {
    reserve(buffer, 1);
    buffer->data[buffer->len++] = value;
}

static void emit_u16(Buffer *buffer, uint16_t value) {
    reserve(buffer, 2);
    buffer->data[buffer->len++] = (unsigned char)(value & 0xffu);
    buffer->data[buffer->len++] = (unsigned char)((value >> 8) & 0xffu);
}

static void emit_u32(Buffer *buffer, uint32_t value) {
    reserve(buffer, 4);
    buffer->data[buffer->len++] = (unsigned char)(value & 0xffu);
    buffer->data[buffer->len++] = (unsigned char)((value >> 8) & 0xffu);
    buffer->data[buffer->len++] = (unsigned char)((value >> 16) & 0xffu);
    buffer->data[buffer->len++] = (unsigned char)((value >> 24) & 0xffu);
}

static void emit_bytes(Buffer *buffer, const unsigned char *data, size_t len) {
    reserve(buffer, len);
    memcpy(buffer->data + buffer->len, data, len);
    buffer->len += len;
}

static void patch_u32(Buffer *buffer, size_t offset, uint32_t value) {
    if (offset + 4 > buffer->len) {
        die_compile("internal patch range");
    }
    buffer->data[offset + 0] = (unsigned char)(value & 0xffu);
    buffer->data[offset + 1] = (unsigned char)((value >> 8) & 0xffu);
    buffer->data[offset + 2] = (unsigned char)((value >> 16) & 0xffu);
    buffer->data[offset + 3] = (unsigned char)((value >> 24) & 0xffu);
}

static int is_bf_char(int c) {
    switch (c) {
        case '>':
        case '<':
        case '+':
        case '-':
        case '.':
        case ',':
        case '[':
        case ']':
            return 1;
        default:
            return 0;
    }
}

static uint32_t code_addr(size_t offset) {
    return FILE_BASE + CODE_FILE_OFFSET + (uint32_t)offset;
}

static void emit_mov_eax_imm(Buffer *buffer, uint32_t value) {
    emit_u8(buffer, 0xb8u);
    emit_u32(buffer, value);
}

static void emit_call_eax(Buffer *buffer) {
    emit_u8(buffer, 0xffu);
    emit_u8(buffer, 0xd0u);
}

static void emit_helper_call(Buffer *buffer, uint32_t helper_addr) {
    emit_mov_eax_imm(buffer, helper_addr);
    emit_call_eax(buffer);
}

static void emit_startup(Buffer *buffer, uint32_t tape_addr, uint32_t main_addr) {
    emit_u8(buffer, 0xbeu);
    emit_u32(buffer, tape_addr);
    emit_u8(buffer, 0x89u);
    emit_u8(buffer, 0xf7u);
    emit_mov_eax_imm(buffer, main_addr);
    emit_u8(buffer, 0xffu);
    emit_u8(buffer, 0xe0u);
}

static void emit_helper_right(Buffer *buffer) {
    static const unsigned char code[] = {0x47u, 0xc3u};
    emit_bytes(buffer, code, sizeof(code));
}

static void emit_helper_left(Buffer *buffer, uint32_t message_addr, uint32_t message_len) {
    static const unsigned char prefix[] = {
        0x39u, 0xf7u,       /* cmp edi, esi */
        0x75u, 0x22u,       /* jne ok */
        0xb8u, 0x04u, 0x00u, 0x00u, 0x00u, /* mov eax, 4 */
        0xbbu, 0x02u, 0x00u, 0x00u, 0x00u, /* mov ebx, 2 */
        0xb9u, 0x00u, 0x00u, 0x00u, 0x00u, /* mov ecx, msg */
        0xbau, 0x00u, 0x00u, 0x00u, 0x00u, /* mov edx, len */
        0xcdu, 0x80u,
        0xb8u, 0x01u, 0x00u, 0x00u, 0x00u, /* mov eax, 1 */
        0xbbu, 0x01u, 0x00u, 0x00u, 0x00u, /* mov ebx, 1 */
        0xcdu, 0x80u,
        0x4fu,
        0xc3u,
    };
    emit_bytes(buffer, prefix, sizeof(prefix));
    patch_u32(buffer, buffer->len - 25, message_addr);
    patch_u32(buffer, buffer->len - 20, message_len);
}

static void emit_helper_inc(Buffer *buffer) {
    static const unsigned char code[] = {0xfeu, 0x07u, 0xc3u};
    emit_bytes(buffer, code, sizeof(code));
}

static void emit_helper_dec(Buffer *buffer) {
    static const unsigned char code[] = {0xfeu, 0x0fu, 0xc3u};
    emit_bytes(buffer, code, sizeof(code));
}

static void emit_helper_out(Buffer *buffer) {
    static const unsigned char code[] = {
        0xb8u, 0x04u, 0x00u, 0x00u, 0x00u,
        0xbbu, 0x01u, 0x00u, 0x00u, 0x00u,
        0x89u, 0xf9u,
        0xbau, 0x01u, 0x00u, 0x00u, 0x00u,
        0xcdu, 0x80u,
        0xc3u,
    };
    emit_bytes(buffer, code, sizeof(code));
}

static void emit_helper_in(Buffer *buffer) {
    static const unsigned char code[] = {
        0xb8u, 0x03u, 0x00u, 0x00u, 0x00u,
        0x31u, 0xdbu,
        0x89u, 0xf9u,
        0xbau, 0x01u, 0x00u, 0x00u, 0x00u,
        0xcdu, 0x80u,
        0x83u, 0xf8u, 0x01u,
        0x74u, 0x03u,
        0xc6u, 0x07u, 0x00u,
        0xc3u,
    };
    emit_bytes(buffer, code, sizeof(code));
}

static void emit_helper_loop_start(Buffer *buffer, uint32_t loop_start_addr, uint32_t loop_end_addr) {
    static const unsigned char code[] = {
        0x80u, 0x3fu, 0x00u,
        0x75u, 0x29u,
        0x8bu, 0x1cu, 0x24u,
        0xb9u, 0x01u, 0x00u, 0x00u, 0x00u,
        0x8bu, 0x43u, 0x01u,
        0x3du, 0x00u, 0x00u, 0x00u, 0x00u,
        0x74u, 0x11u,
        0x3du, 0x00u, 0x00u, 0x00u, 0x00u,
        0x75u, 0x0bu,
        0x49u,
        0x75u, 0x08u,
        0x83u, 0xc3u, 0x07u,
        0x89u, 0x1cu, 0x24u,
        0xc3u,
        0x41u,
        0x83u, 0xc3u, 0x07u,
        0xebu, 0xdfu,
        0xc3u,
    };
    emit_bytes(buffer, code, sizeof(code));
    patch_u32(buffer, buffer->len - 30, loop_start_addr);
    patch_u32(buffer, buffer->len - 23, loop_end_addr);
}

static void emit_helper_loop_end(Buffer *buffer, uint32_t loop_start_addr, uint32_t loop_end_addr) {
    static const unsigned char code[] = {
        0x80u, 0x3fu, 0x00u,
        0x74u, 0x29u,
        0x8bu, 0x1cu, 0x24u,
        0x83u, 0xebu, 0x0eu,
        0xb9u, 0x01u, 0x00u, 0x00u, 0x00u,
        0x8bu, 0x43u, 0x01u,
        0x3du, 0x00u, 0x00u, 0x00u, 0x00u,
        0x74u, 0x0eu,
        0x3du, 0x00u, 0x00u, 0x00u, 0x00u,
        0x75u, 0x08u,
        0x49u,
        0x75u, 0x05u,
        0x89u, 0x1cu, 0x24u,
        0xc3u,
        0x41u,
        0x83u, 0xebu, 0x07u,
        0xebu, 0xe2u,
        0xc3u,
    };
    emit_bytes(buffer, code, sizeof(code));
    patch_u32(buffer, buffer->len - 27, loop_end_addr);
    patch_u32(buffer, buffer->len - 20, loop_start_addr);
}

static void emit_exit(Buffer *buffer) {
    static const unsigned char code[] = {
        0xb8u, 0x01u, 0x00u, 0x00u, 0x00u,
        0x31u, 0xdbu,
        0xcdu, 0x80u,
    };
    emit_bytes(buffer, code, sizeof(code));
}

static unsigned char *read_filtered_source(size_t *out_len) {
    Buffer ops = {0};
    int c;
    while ((c = fgetc(stdin)) != EOF) {
        if (is_bf_char(c)) {
            emit_u8(&ops, (uint8_t)c);
        }
    }
    if (ferror(stdin)) {
        die_errno("stdin");
    }
    *out_len = ops.len;
    return ops.data;
}

static void validate_brackets(const unsigned char *ops, size_t op_count) {
    size_t depth = 0;
    for (size_t i = 0; i < op_count; ++i) {
        if (ops[i] == '[') {
            depth += 1;
        } else if (ops[i] == ']') {
            if (depth == 0) {
                die_compile("unmatched ']'");
            }
            depth -= 1;
        }
    }
    if (depth != 0) {
        die_compile("unmatched '['");
    }
}

static void emit_main(Buffer *buffer,
                      const unsigned char *ops,
                      size_t op_count,
                      uint32_t helper_right,
                      uint32_t helper_left,
                      uint32_t helper_inc,
                      uint32_t helper_dec,
                      uint32_t helper_out,
                      uint32_t helper_in,
                      uint32_t helper_loop_start,
                      uint32_t helper_loop_end) {
    for (size_t i = 0; i < op_count; ++i) {
        unsigned char op = ops[i];
        switch (op) {
            case '>':
                emit_helper_call(buffer, helper_right);
                break;
            case '<':
                emit_helper_call(buffer, helper_left);
                break;
            case '+':
                emit_helper_call(buffer, helper_inc);
                break;
            case '-':
                emit_helper_call(buffer, helper_dec);
                break;
            case '.':
                emit_helper_call(buffer, helper_out);
                break;
            case ',':
                emit_helper_call(buffer, helper_in);
                break;
            case '[': {
                emit_helper_call(buffer, helper_loop_start);
                break;
            }
            case ']':
                emit_helper_call(buffer, helper_loop_end);
                break;
            default:
                die_compile("internal opcode");
        }
    }
}

static void write_elf_header(Buffer *out, uint32_t file_size, uint32_t memory_size, uint32_t entry_addr) {
    emit_u8(out, 0x7fu);
    emit_u8(out, 'E');
    emit_u8(out, 'L');
    emit_u8(out, 'F');
    emit_u8(out, 1u);
    emit_u8(out, 1u);
    emit_u8(out, 1u);
    emit_u8(out, 0u);
    emit_u8(out, 0u);
    emit_u8(out, 0u);
    emit_u8(out, 0u);
    emit_u8(out, 0u);
    emit_u8(out, 0u);
    emit_u8(out, 0u);
    emit_u8(out, 0u);
    emit_u8(out, 0u);
    emit_u16(out, 2u);
    emit_u16(out, 3u);
    emit_u32(out, 1u);
    emit_u32(out, entry_addr);
    emit_u32(out, 52u);
    emit_u32(out, 0u);
    emit_u32(out, 0u);
    emit_u16(out, 52u);
    emit_u16(out, 32u);
    emit_u16(out, 1u);
    emit_u16(out, 0u);
    emit_u16(out, 0u);
    emit_u16(out, 0u);

    emit_u32(out, 1u);
    emit_u32(out, 0u);
    emit_u32(out, FILE_BASE);
    emit_u32(out, FILE_BASE);
    emit_u32(out, file_size);
    emit_u32(out, memory_size);
    emit_u32(out, 7u);
    emit_u32(out, 0x1000u);

    while (out->len < CODE_FILE_OFFSET) {
        emit_u8(out, 0u);
    }
}

int main(void) {
    size_t op_count = 0;
    unsigned char *ops = read_filtered_source(&op_count);
    validate_brackets(ops, op_count);

    uint32_t main_size = (uint32_t)(op_count * OP_SIZE);

    Buffer layout = {0};
    uint32_t startup_offset = 0u;
    emit_startup(&layout, 0u, 0u);
    uint32_t helper_right_offset = (uint32_t)layout.len;
    emit_helper_right(&layout);
    uint32_t helper_left_offset = (uint32_t)layout.len;
    emit_helper_left(&layout, 0u, 0u);
    uint32_t helper_inc_offset = (uint32_t)layout.len;
    emit_helper_inc(&layout);
    uint32_t helper_dec_offset = (uint32_t)layout.len;
    emit_helper_dec(&layout);
    uint32_t helper_out_offset = (uint32_t)layout.len;
    emit_helper_out(&layout);
    uint32_t helper_in_offset = (uint32_t)layout.len;
    emit_helper_in(&layout);
    uint32_t helper_loop_start_offset = (uint32_t)layout.len;
    emit_helper_loop_start(&layout, 0u, 0u);
    uint32_t helper_loop_end_offset = (uint32_t)layout.len;
    emit_helper_loop_end(&layout, 0u, 0u);
    uint32_t message_offset = (uint32_t)layout.len;
    emit_bytes(&layout, (const unsigned char *)RUNTIME_ERROR_MSG, strlen(RUNTIME_ERROR_MSG));

    uint32_t code_offset = (uint32_t)layout.len;
    uint32_t main_offset = code_offset;
    code_offset += main_size;
    code_offset += EXIT_CODE_SIZE;

    uint32_t file_size = CODE_FILE_OFFSET + code_offset;
    uint32_t tape_addr = FILE_BASE + file_size;
    uint32_t memory_size = file_size + TAPE_SIZE;

    Buffer out = {0};
    write_elf_header(&out, file_size, memory_size, code_addr(startup_offset));
    emit_startup(&out, tape_addr, code_addr(main_offset));
    emit_helper_right(&out);
    emit_helper_left(&out, code_addr(message_offset), (uint32_t)strlen(RUNTIME_ERROR_MSG));
    emit_helper_inc(&out);
    emit_helper_dec(&out);
    emit_helper_out(&out);
    emit_helper_in(&out);
    emit_helper_loop_start(&out, code_addr(helper_loop_start_offset), code_addr(helper_loop_end_offset));
    emit_helper_loop_end(&out, code_addr(helper_loop_start_offset), code_addr(helper_loop_end_offset));
    emit_bytes(&out, (const unsigned char *)RUNTIME_ERROR_MSG, strlen(RUNTIME_ERROR_MSG));
    emit_main(&out, ops, op_count,
              code_addr(helper_right_offset),
              code_addr(helper_left_offset),
              code_addr(helper_inc_offset),
              code_addr(helper_dec_offset),
              code_addr(helper_out_offset),
              code_addr(helper_in_offset),
              code_addr(helper_loop_start_offset),
              code_addr(helper_loop_end_offset));
    emit_exit(&out);

    if (out.len != file_size) {
        free(ops);
        free(out.data);
        die_compile("internal file size mismatch");
    }

    if (fwrite(out.data, 1, out.len, stdout) != out.len) {
        free(ops);
        free(out.data);
        die_errno("stdout");
    }
    if (fflush(stdout) != 0) {
        free(ops);
        free(layout.data);
        free(out.data);
        die_errno("stdout");
    }

    free(ops);
    free(layout.data);
    free(out.data);
    return 0;
}
