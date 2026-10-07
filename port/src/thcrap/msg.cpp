// The .msg patcher: dialogue (st*.msg) and endings (e*.msg) from
// <file>.msg.jdiff. Each jdiff entry ("<entry>": {"<time>_<index>":
// {"lines": [...]}}) replaces the lines of one text box; extra lines get
// new instructions, missing ones are dropped. Strings are encrypted the
// way the game decodes them (decode_msg_string, Gui.cpp).
//
// Adapted from thcrap (public domain): thcrap_tsa/src/th06_msg.cpp, the
// formats TH16 uses (MSG_TH14 for dialogue, END_TH10 for endings). TL
// notes in lines are cut off (not shown in the port).
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "thcrap_internal.h"

namespace thcrap
{
namespace
{

#pragma pack(push, 1)
struct MsgInstr
{
    uint16_t time;
    uint8_t type;
    uint8_t length;
    uint8_t data[1];
};

struct BubblePos
{
    uint16_t time;
    uint8_t type;
    uint8_t length;
    float x;
    float y;
};
#pragma pack(pop)

const size_t MSG_INSTR_HEADER = 4;

enum OpCmd
{
    OP_UNKNOWN = 0,
    OP_HARD_LINE,
    OP_AUTO_LINE,
    OP_AUTO_END,
    OP_DELETE,
    OP_SIDE_LEFT,
    OP_SIDE_RIGHT,
    OP_BUBBLE_POS,
    OP_BUBBLE_SHAPE,
};

struct OpInfo
{
    uint8_t op;
    OpCmd cmd;
    const char *type;
};

struct MsgFormat
{
    uint32_t entry_offset_mul;
    uint32_t header_size_mul;
    std::vector<OpInfo> opcodes;
};

// msg_crypt_th09: util_xor(0x77, 7, 16).
void msg_crypt(uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        const size_t ip = i - 1;
        data[i] ^= (uint8_t)(0x77 + i * 7 + (ip * ip + ip) / 2 * 16);
    }
}

const MsgFormat MSG_TH14 = {2, 1,
                            {
                                {7, OP_SIDE_LEFT, NULL},
                                {8, OP_SIDE_RIGHT, NULL},
                                {9, OP_SIDE_LEFT, NULL},
                                {11, OP_AUTO_END, NULL},
                                {17, OP_AUTO_LINE, NULL},
                                {25, OP_DELETE, NULL},
                                {28, OP_BUBBLE_POS, NULL},
                                {32, OP_BUBBLE_SHAPE, NULL},
                            }};

const MsgFormat END_TH10 = {2, 1,
                            {
                                {3, OP_AUTO_LINE, NULL},
                                {5, OP_AUTO_END, NULL},
                                {6, OP_AUTO_END, NULL},
                                {9, OP_AUTO_END, NULL},
                            }};

enum Side
{
    SIDE_LEFT = -1,
    SIDE_NONE = 0,
    SIDE_RIGHT = 1
};

size_t instr_full_len(const MsgInstr *msg)
{
    return msg != NULL ? MSG_INSTR_HEADER + msg->length : 0;
}

MsgInstr *instr_advance(const MsgInstr *msg)
{
    return (MsgInstr *)((uint8_t *)msg + instr_full_len(msg));
}

const OpInfo *get_op_info(const MsgFormat *format, uint8_t op)
{
    for (const OpInfo &info : format->opcodes)
    {
        if (info.op == op)
        {
            return &info;
        }
    }
    return NULL;
}

struct Replacer
{
    int extra_param_len;
    bool hard_line;
};

const Replacer REP_HARD_LINE = {4, true};
const Replacer REP_AUTO_LINE = {0, false};

struct MsgState
{
    const MsgFormat *format;
    json_t *font_dialog_id;
    const json_t *diff_entry = NULL;
    const json_t *diff_lines = NULL;
    MsgInstr *cmd_in = NULL;
    MsgInstr *cmd_out = NULL;
    int entry = 0, time = 0, ind = 0;
    Side side = SIDE_NONE;
    unsigned int line_widths[4] = {0, 0, 0, 0};
    BubblePos *bubble_pos = NULL;
    const MsgInstr *last_line_cmd = NULL;
    const OpInfo *last_line_op = NULL;
    size_t cur_line = 0;

    // diff_line_cur: the current diff line, trimmed to whole code points
    // that fit into an instruction with its terminator; empty if none.
    bool diff_line_cur(int extra_param_len, std::string *out)
    {
        const char *line = json_string_value(json_array_get(diff_lines, cur_line));
        if (line == NULL)
        {
            return false;
        }
        size_t length = json_string_length(json_array_get(diff_lines, cur_line));
        // Valid ruby syntax only: the game strchr()s for two commas.
        if (line[0] == '|')
        {
            const char *p2 = strchr(line + 1, ',');
            if (p2 == NULL || strchr(p2 + 1, ',') == NULL)
            {
                return false;
            }
        }
        for (size_t i = 0; i < length; i++)
        {
            if (line[i] == 0x14 || line[i] == 0x12)
            {
                length = i;
                break;
            }
        }
        size_t len_trimmed = 0;
        size_t limit = 0xff - 1 - extra_param_len;
        while (len_trimmed < length)
        {
            size_t old = len_trimmed;
            len_trimmed++;
            while (len_trimmed < length && (line[len_trimmed] & 0xc0) == 0x80)
            {
                len_trimmed++;
            }
            if (len_trimmed > limit)
            {
                len_trimmed = old;
                break;
            }
        }
        out->assign(line, len_trimmed);
        return true;
    }

    void replace_line_text(char *dst, const std::string &rep)
    {
        memcpy(dst, rep.data(), rep.size());
        dst[rep.size()] = '\0';
        if (bubble_pos != NULL && dst[0] != '|' && json_is_integer(font_dialog_id) &&
            cur_line < sizeof(line_widths) / sizeof(*line_widths))
        {
            int width = text_extent_for_font_id(dst, (int)json_integer_value(font_dialog_id)) / 2;
            line_widths[cur_line] = width;
        }
        msg_crypt((uint8_t *)dst, rep.size() + 1);
    }

    void replace_line(MsgInstr &out, const Replacer &replacer, const std::string &line)
    {
        if (replacer.hard_line)
        {
            uint16_t linenum = (uint16_t)cur_line;
            memcpy(out.data + 2, &linenum, 2);
            out.length = (uint8_t)(line.size() + 4 + 1);
            replace_line_text((char *)out.data + 4, line);
        }
        else
        {
            out.length = (uint8_t)(line.size() + 1);
            replace_line_text((char *)out.data, line);
        }
        last_line_cmd = &out;
    }
};

// Returns whether the output advances.
int replace_next_diff_line(MsgInstr *cmd_out, MsgState *state, const Replacer &replacer)
{
    const OpInfo *cur_op = get_op_info(state->format, cmd_out->type);
    // The first line of a new box: look up its lines.
    if (cur_op != NULL && !json_is_array(state->diff_lines))
    {
        char key[64];
        state->ind++;
        if (cur_op->type != NULL)
        {
            snprintf(key, sizeof(key), "%d_%s_%d", state->time, cur_op->type, state->ind);
        }
        else
        {
            snprintf(key, sizeof(key), "%d_%d", state->time, state->ind);
        }
        state->diff_lines = json_object_get(state->diff_entry, key);
        if (json_is_object(state->diff_lines))
        {
            state->diff_lines = json_object_get(state->diff_lines, "lines");
        }
    }
    if (json_is_array(state->diff_lines))
    {
        std::string line;
        bool ret = state->diff_line_cur(replacer.extra_param_len, &line);
        if (ret)
        {
            state->replace_line(*cmd_out, replacer, line);
            state->last_line_op = cur_op;
        }
        state->cur_line++;
        return ret;
    }
    return 1;
}

void box_end(MsgState *state)
{
    if (state->last_line_op == NULL)
    {
        return;
    }
    // Extra lines in the patch: new instructions after the last line.
    while (state->cur_line < json_array_size(state->diff_lines))
    {
        bool hard_line = state->last_line_op->cmd == OP_HARD_LINE;
        const Replacer &replacer = hard_line ? REP_HARD_LINE : REP_AUTO_LINE;
        std::string line;
        if (state->diff_line_cur(replacer.extra_param_len, &line))
        {
            MsgInstr *new_line_cmd = instr_advance(state->last_line_cmd);
            size_t line_len_full = line.size() + 1;
            ptrdiff_t move_len = (uint8_t *)instr_advance(state->cmd_out) - (uint8_t *)new_line_cmd;
            size_t line_offset = MSG_INSTR_HEADER + replacer.extra_param_len;
            memmove((uint8_t *)new_line_cmd + line_offset + line_len_full, new_line_cmd, move_len);
            memcpy(new_line_cmd, state->last_line_cmd, line_offset);
            state->replace_line(*new_line_cmd, replacer, line);
            state->cmd_out = (MsgInstr *)((uint8_t *)state->cmd_out + instr_full_len(new_line_cmd));
        }
        state->cur_line++;
    }
    if (state->bubble_pos != NULL)
    {
        unsigned int box_len = 0;
        for (unsigned int &width : state->line_widths)
        {
            box_len = width > box_len ? width : box_len;
            width = 0;
        }
        // text.anm's limit, and TH16's padding.
        box_len = box_len < 512 ? box_len : 512;
        box_len += 19;
        float overhang = 0.0f;
        bool do_shift = false;
        if (state->side == SIDE_LEFT)
        {
            overhang = (state->bubble_pos->x + box_len) - 640.0f;
            do_shift = overhang > 0.0f;
        }
        else if (state->side == SIDE_RIGHT)
        {
            overhang = state->bubble_pos->x - box_len;
            do_shift = overhang < 0.0f;
        }
        if (do_shift)
        {
            state->bubble_pos->x -= overhang;
        }
        state->bubble_pos = NULL;
    }
    state->cur_line = 0;
    state->diff_lines = NULL;
}

int op_auto_end(MsgState *state)
{
    if (state->last_line_op == NULL || state->last_line_op->cmd == OP_AUTO_LINE)
    {
        box_end(state);
    }
    return 1;
}

int process_op(const OpInfo *cur_op, MsgState *state)
{
    int ret;
    switch (cur_op->cmd)
    {
    case OP_DELETE:
        return 0;
    // End the box before changing sides (TH16's Extra midboss dialogue
    // shows a box without a wait first).
    case OP_SIDE_LEFT:
        ret = op_auto_end(state);
        state->side = SIDE_LEFT;
        return ret;
    case OP_SIDE_RIGHT:
        ret = op_auto_end(state);
        state->side = SIDE_RIGHT;
        return ret;
    case OP_BUBBLE_SHAPE:
    {
        uint32_t shape;
        memcpy(&shape, state->cmd_out->data, 4);
        state->side = (shape & 1) ? SIDE_RIGHT : SIDE_LEFT;
        return op_auto_end(state);
    }
    case OP_BUBBLE_POS:
        state->bubble_pos = (BubblePos *)state->cmd_out;
        return 1;
    case OP_AUTO_LINE:
        if (state->last_line_op != NULL && state->last_line_op->cmd == OP_HARD_LINE)
        {
            box_end(state);
        }
        return replace_next_diff_line(state->cmd_out, state, REP_AUTO_LINE);
    case OP_HARD_LINE:
    {
        uint16_t linenum;
        memcpy(&linenum, state->cmd_out->data + 2, 2);
        if (linenum == 0)
        {
            box_end(state);
            if (state->last_line_op != NULL)
            {
                const char *last_op_type = state->last_line_op->type;
                if ((!last_op_type && last_op_type != cur_op->type) || (last_op_type && !cur_op->type) ||
                    (last_op_type && cur_op->type && !strcmp(last_op_type, cur_op->type)))
                {
                    state->ind = -1;
                }
            }
        }
        state->cur_line = linenum;
        return replace_next_diff_line(state->cmd_out, state, REP_HARD_LINE);
    }
    case OP_AUTO_END:
        return op_auto_end(state);
    default:
        return 1;
    }
}

json_t *font_dialog_id()
{
    return json_object_get(json_object_get(json_object_get(runconfig(), "breakpoints"), "ruby_offset"),
                           "font_dialog");
}

int patch_msg(void *file_inout, size_t size_out, size_t size_in, json_t *patch, const MsgFormat *format)
{
    if (file_inout == NULL || patch == NULL || size_in < 4)
    {
        return 0;
    }
    uint32_t *msg_out = (uint32_t *)file_inout;
    std::vector<uint8_t> copy((uint8_t *)file_inout, (uint8_t *)file_inout + size_in);
    copy.resize(size_in + 256, 0);
    uint32_t *msg_in = (uint32_t *)copy.data();
    MsgState state;
    state.format = format;
    state.font_dialog_id = font_dialog_id();
    size_t entry_count = msg_in[0];
    uint32_t *entry_offsets_in = msg_in + format->header_size_mul;
    size_t entry_offset_size = entry_count * format->entry_offset_mul;
    if ((format->header_size_mul + entry_offset_size) * 4 > size_in)
    {
        return 0;
    }
    state.cmd_in = (MsgInstr *)(msg_in + format->header_size_mul + entry_offset_size);
    memcpy(msg_out, msg_in, format->header_size_mul * 4);
    uint32_t *entry_offsets_out = msg_out + format->header_size_mul;
    memcpy(entry_offsets_out, entry_offsets_in, entry_offset_size * 4);
    state.cmd_out = (MsgInstr *)(msg_out + format->header_size_mul + entry_offset_size);
    bool entry_new = true;
    for (;;)
    {
        const ptrdiff_t offset_in = (uint8_t *)state.cmd_in - (uint8_t *)msg_in;
        const ptrdiff_t offset_out = (uint8_t *)state.cmd_out - (uint8_t *)msg_out;
        int advance_out = 1;
        if ((size_t)offset_in + MSG_INSTR_HEADER > size_in)
        {
            break;
        }
        if ((size_t)offset_out + MSG_INSTR_HEADER + state.cmd_in->length > size_out)
        {
            log("msg patch overflows its buffer; stopping");
            break;
        }
        if (state.cmd_in->time == 0 && state.cmd_in->type == 0)
        {
            // The entry's last instruction: close any open box here, not in
            // the next entry.
            entry_new = true;
            box_end(&state);
        }
        if (entry_new && state.cmd_in->type != 0)
        {
            for (size_t i = 0; i < entry_count; i++)
            {
                if (offset_in == (ptrdiff_t)entry_offsets_in[i * format->entry_offset_mul])
                {
                    state.entry = (int)i;
                    char key[16];
                    snprintf(key, sizeof(key), "%d", state.entry);
                    state.diff_entry = json_object_get(patch, key);
                    entry_offsets_out[i * format->entry_offset_mul] = (uint32_t)offset_out;
                    break;
                }
            }
            entry_new = false;
            state.time = -1;
        }
        if (state.cmd_in->time != state.time)
        {
            state.time = state.cmd_in->time;
            state.ind = -1;
        }
        memcpy(state.cmd_out, state.cmd_in, MSG_INSTR_HEADER + state.cmd_in->length);
        const OpInfo *cur_op = get_op_info(state.format, state.cmd_in->type);
        if (cur_op != NULL && json_is_object(state.diff_entry))
        {
            advance_out = process_op(cur_op, &state);
        }
        state.cmd_in = instr_advance(state.cmd_in);
        if (advance_out)
        {
            state.cmd_out = instr_advance(state.cmd_out);
        }
    }
    return 1;
}

} // namespace

int patch_msg_dlg(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t *patch)
{
    return patch_msg(file_inout, size_out, size_in, patch, &MSG_TH14);
}

int patch_msg_end(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t *patch)
{
    return patch_msg(file_inout, size_out, size_in, patch, &END_TH10);
}

} // namespace thcrap
