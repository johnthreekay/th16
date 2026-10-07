// The .anm patcher: PNG images from the patches are drawn over the
// textures embedded in .anm files (each entry's THTX, named after its
// image: "title/title_logo.png" in <patch>/th16/), sprite by sprite, and
// <file>.anm.jdiff changes the header (sprite rectangles, entry names,
// blitting modes, script instructions).
//
// Adapted from thcrap (public domain): thcrap_tsa/src/anm.cpp (blitting,
// formats, sprite splitting, header mods, patch_anm, stack_game_png_apply)
// and png_ex.cpp, for the ANM version TH16 uses (TH11 headers, raw THTX
// pixels: no GDI+ encoding). PNGs are read with libpng's simplified API
// as thcrap does.
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <png.h>

#include "thcrap_internal.h"

namespace thcrap
{
namespace
{

#pragma pack(push, 1)
// anm_header11_t (AnmRawEntry in AnmManager.h).
struct AnmHeader11
{
    uint32_t version;
    uint16_t sprites;
    uint16_t scripts;
    uint16_t zero1;
    uint16_t w;
    uint16_t h;
    uint16_t format;
    uint32_t nameoffset;
    uint16_t x;
    uint16_t y;
    uint32_t memorypriority;
    uint32_t thtxoffset;
    uint16_t hasdata;
    uint16_t lowresscale;
    uint32_t nextoffset;
    uint32_t zero3[6];
};

struct ThtxHeader
{
    char magic[4];
    uint16_t zero;
    uint16_t format;
    uint16_t w;
    uint16_t h;
    uint32_t size;
};

struct AnmSprite
{
    uint32_t id;
    float x, y, w, h;
};

struct AnmOffset
{
    int32_t id;
    uint32_t offset;
};

struct AnmInstr
{
    uint16_t type;
    uint16_t length;
    int16_t time;
    uint16_t param_mask;
};
#pragma pack(pop)

enum Format
{
    FORMAT_BGRA8888 = 1,
    FORMAT_RGB565 = 3,
    FORMAT_ARGB4444 = 5,
    FORMAT_GRAY8 = 7,
};

typedef void (*BlitFunc)(uint8_t *dst, const uint8_t *rep, unsigned int pixels, int format);

unsigned int format_bpp(int format)
{
    switch (format)
    {
    case FORMAT_BGRA8888:
        return 4;
    case FORMAT_ARGB4444:
    case FORMAT_RGB565:
        return 2;
    case FORMAT_GRAY8:
        return 1;
    }
    return 0;
}

uint8_t format_alpha_max(int format)
{
    return format == FORMAT_BGRA8888 ? 0xff : format == FORMAT_ARGB4444 ? 0xf : 0;
}

size_t format_alpha_sum(const uint8_t *data, unsigned int pixels, int format)
{
    size_t ret = 0;
    if (format == FORMAT_BGRA8888)
    {
        for (unsigned int i = 0; i < pixels; i++, data += 4)
        {
            ret += data[3];
        }
    }
    else if (format == FORMAT_ARGB4444)
    {
        for (unsigned int i = 0; i < pixels; i++, data += 2)
        {
            ret += (data[1] & 0xf0) >> 4;
        }
    }
    return ret;
}

void format_from_bgra(uint8_t *data, unsigned int pixels, int format)
{
    const uint8_t *in = data;
    if (format == FORMAT_ARGB4444)
    {
        uint8_t *out = data;
        for (unsigned int i = 0; i < pixels; i++, in += 4, out += 2)
        {
            const uint8_t b = in[0] >> 4, g = in[1] >> 4, r = in[2] >> 4, a = in[3] >> 4;
            out[1] = (uint8_t)((a << 4) | r);
            out[0] = (uint8_t)((g << 4) | b);
        }
    }
    else if (format == FORMAT_RGB565)
    {
        uint8_t *out = data;
        for (unsigned int i = 0; i < pixels; i++, in += 4, out += 2)
        {
            const uint16_t b = in[0] >> 3, g = in[1] >> 2, r = in[2] >> 3;
            uint16_t v = (uint16_t)((r << 11) | (g << 5) | b);
            out[0] = (uint8_t)v;
            out[1] = (uint8_t)(v >> 8);
        }
    }
}

void blit_overwrite(uint8_t *dst, const uint8_t *rep, unsigned int pixels, int format)
{
    memcpy(dst, rep, pixels * format_bpp(format));
}

void blit_blend(uint8_t *dst, const uint8_t *rep, unsigned int pixels, int format)
{
    // Alpha adds up (clamped), as GIMP's default composition does.
    if (format == FORMAT_BGRA8888)
    {
        for (unsigned int i = 0; i < pixels; i++, dst += 4, rep += 4)
        {
            const int new_alpha = dst[3] + rep[3];
            const int dst_alpha = 0xff - rep[3];
            dst[0] = (uint8_t)((dst[0] * dst_alpha + rep[0] * rep[3]) >> 8);
            dst[1] = (uint8_t)((dst[1] * dst_alpha + rep[1] * rep[3]) >> 8);
            dst[2] = (uint8_t)((dst[2] * dst_alpha + rep[2] * rep[3]) >> 8);
            dst[3] = (uint8_t)(new_alpha < 0xff ? new_alpha : 0xff);
        }
    }
    else if (format == FORMAT_ARGB4444)
    {
        for (unsigned int i = 0; i < pixels; i++, dst += 2, rep += 2)
        {
            const int rep_a = (rep[1] & 0xf0) >> 4, rep_r = rep[1] & 0x0f;
            const int rep_g = (rep[0] & 0xf0) >> 4, rep_b = rep[0] & 0x0f;
            const int dst_a = (dst[1] & 0xf0) >> 4, dst_r = dst[1] & 0x0f;
            const int dst_g = (dst[0] & 0xf0) >> 4, dst_b = dst[0] & 0x0f;
            const int new_alpha = dst_a + rep_a;
            const int dst_alpha = 0xf - rep_a;
            dst[1] = (uint8_t)(((new_alpha < 0xf ? new_alpha : 0xf) << 4) | ((dst_r * dst_alpha + rep_r * rep_a) >> 4));
            dst[0] = (uint8_t)(((dst_g * dst_alpha + rep_g * rep_a) & 0xf0) | ((dst_b * dst_alpha + rep_b * rep_a) >> 4));
        }
    }
    else
    {
        blit_overwrite(dst, rep, pixels, format);
    }
}

struct SpriteLocal
{
    // NULL: "auto".
    BlitFunc blitmode;
    uint32_t x, y, w, h;
};

struct Image
{
    std::vector<uint8_t> buf;
    uint32_t width = 0;
    uint32_t height = 0;
};

// patch_png_load_for_thtx.
bool png_load_for_format(const std::string &file, int format, Image *image)
{
    png_image png;
    memset(&png, 0, sizeof(png));
    png.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(&png, file.data(), file.size()))
    {
        return false;
    }
    png.format = format == FORMAT_GRAY8 ? PNG_FORMAT_GRAY : PNG_FORMAT_BGRA;
    image->buf.resize(PNG_IMAGE_SIZE(png));
    if (!png_image_finish_read(&png, NULL, image->buf.data(), 0, NULL))
    {
        png_image_free(&png);
        return false;
    }
    image->width = png.width;
    image->height = png.height;
    format_from_bgra(image->buf.data(), png.width * png.height, format);
    return true;
}

struct ImgPatch
{
    uint8_t *img_data;
    int format;
    uint32_t stride;
    uint32_t w, h, x, y;
};

enum SpriteAlpha
{
    SPRITE_ALPHA_EMPTY,
    SPRITE_ALPHA_OPAQUE,
    SPRITE_ALPHA_FULL
};

SpriteAlpha sprite_alpha_analyze(const uint8_t *buf, int format, size_t stride, uint32_t w, uint32_t h)
{
    const size_t opaque_sum = format_alpha_max(format) * w;
    if (buf == NULL)
    {
        return SPRITE_ALPHA_EMPTY;
    }
    if (opaque_sum == 0)
    {
        return SPRITE_ALPHA_OPAQUE;
    }
    SpriteAlpha ret = SPRITE_ALPHA_FULL;
    const uint8_t *p = buf;
    for (uint32_t row = 0; row < h; row++)
    {
        size_t sum = format_alpha_sum(p, w, format);
        if (sum == 0 && ret != SPRITE_ALPHA_OPAQUE)
        {
            ret = SPRITE_ALPHA_EMPTY;
        }
        else if (sum == opaque_sum && ret != SPRITE_ALPHA_EMPTY)
        {
            ret = SPRITE_ALPHA_OPAQUE;
        }
        else
        {
            return SPRITE_ALPHA_FULL;
        }
        p += stride;
    }
    return ret;
}

// sprite_patch_set + sprite_patch: one sprite's rectangle of the PNG over
// the texture. "auto" blends onto fully opaque originals and overwrites
// otherwise, unless the replacement is empty there.
void sprite_patch(const ImgPatch &patch, const SpriteLocal &sprite, const Image &image)
{
    const unsigned int bpp = format_bpp(patch.format);
    const uint32_t rep_x = patch.x + sprite.x;
    const uint32_t rep_y = patch.y + sprite.y;
    if (rep_x >= image.width || rep_y >= image.height)
    {
        return;
    }
    const size_t rep_stride = (size_t)image.width * bpp;
    const size_t dst_stride = patch.stride;
    const uint32_t copy_w = sprite.w < image.width - rep_x ? sprite.w : image.width - rep_x;
    const uint32_t copy_h = sprite.h < image.height - rep_y ? sprite.h : image.height - rep_y;
    uint8_t *dst = patch.img_data + sprite.y * dst_stride + sprite.x * bpp;
    const uint8_t *rep = image.buf.data() + rep_y * rep_stride + rep_x * bpp;
    BlitFunc func = sprite.blitmode;
    if (func == NULL)
    {
        if (sprite_alpha_analyze(rep, patch.format, rep_stride, copy_w, copy_h) == SPRITE_ALPHA_EMPTY)
        {
            return;
        }
        func = sprite_alpha_analyze(dst, patch.format, dst_stride, copy_w, copy_h) == SPRITE_ALPHA_OPAQUE
                   ? blit_blend
                   : blit_overwrite;
    }
    for (uint32_t row = 0; row < copy_h; row++)
    {
        func(dst, rep, copy_w, patch.format);
        dst += dst_stride;
        rep += rep_stride;
    }
}

// --- Header modifications (anm.hpp's documentation) ---

bool blitmode_parse(json_t *blitmode_j, BlitFunc *out)
{
    const char *mode = json_string_value(blitmode_j);
    if (mode == NULL)
    {
        return false;
    }
    if (strcmp(mode, "auto") == 0)
    {
        *out = NULL;
        return true;
    }
    if (strcmp(mode, "blend") == 0)
    {
        *out = blit_blend;
        return true;
    }
    if (strcmp(mode, "overwrite") == 0)
    {
        *out = blit_overwrite;
        return true;
    }
    log("ANM header patching: invalid blitting mode \"%s\"", mode);
    return false;
}

// The binary hack strings of script parameter changes: hex bytes (spaces
// allowed). thcrap's full code string syntax (typed values, options) is
// not needed by any TH16 patch.
bool code_string_render(const char *code, std::vector<uint8_t> *out)
{
    out->clear();
    for (const char *p = code; *p;)
    {
        if (*p == ' ' || *p == '\t')
        {
            p++;
            continue;
        }
        if (!isxdigit((unsigned char)p[0]) || !isxdigit((unsigned char)p[1]))
        {
            return false;
        }
        char hex[3] = {p[0], p[1], 0};
        out->push_back((uint8_t)strtoul(hex, NULL, 16));
        p += 2;
    }
    return !out->empty();
}

struct Script
{
    uint8_t *first_instr = NULL;
    uint8_t *after_last = NULL;
    unsigned int num_instrs = 0;

    static bool is_last(const AnmInstr *instr)
    {
        return instr->type == 0xffff;
    }
    static AnmInstr *next(AnmInstr *instr)
    {
        return (AnmInstr *)((uint8_t *)instr + instr->length);
    }
    void init(uint8_t *first, uint8_t *end)
    {
        AnmInstr *instr = (AnmInstr *)first;
        while ((uint8_t *)instr + sizeof(AnmInstr) <= end && !is_last(instr) && instr->length >= sizeof(AnmInstr))
        {
            num_instrs++;
            instr = next(instr);
        }
        first_instr = first;
        after_last = (uint8_t *)instr + sizeof(AnmInstr);
    }
    AnmInstr *at(unsigned int line)
    {
        AnmInstr *instr = (AnmInstr *)first_instr;
        for (unsigned int i = 0; i < line; i++)
        {
            instr = next(instr);
        }
        return instr;
    }
    uint16_t param_length_of(unsigned int line)
    {
        return (uint16_t)(at(line)->length - sizeof(AnmInstr));
    }
    void delete_line(unsigned int line)
    {
        AnmInstr *instr = at(line);
        AnmInstr *after = next(instr);
        size_t removed = (uint8_t *)after - (uint8_t *)instr;
        memmove(instr, after, after_last - (uint8_t *)after);
        after_last -= removed;
        ((AnmInstr *)(after_last - sizeof(AnmInstr)))->type = 0xffff;
        num_instrs--;
    }
};

struct HeaderMods
{
    size_t entries_seen = 0;
    size_t sprites_seen = 0;
    json_t *entries = NULL;
    json_t *sprites = NULL;
    BlitFunc blitmode = NULL;

    explicit HeaderMods(json_t *patch)
    {
        entries = json_object_get(patch, "entries");
        if (entries != NULL && !json_is_object(entries))
        {
            log("ANM header patching: \"entries\" must be a JSON object");
            entries = NULL;
        }
        sprites = json_object_get(patch, "sprites");
        if (sprites != NULL && !json_is_object(sprites))
        {
            log("ANM header patching: \"sprites\" must be a JSON object");
            sprites = NULL;
        }
        blitmode_parse(json_object_get(patch, "blitmode"), &blitmode);
    }
};

json_t *numkey_get(json_t *object, size_t key)
{
    char buf[24];
    snprintf(buf, sizeof(buf), "%zu", key);
    return json_object_get(object, buf);
}

// entry_mods_t::script_mods + apply_orig for one script.
void apply_script_mods(json_t *scripts, size_t entry_num, int32_t script_num, uint8_t *first, uint8_t *end)
{
    char key[24];
    snprintf(key, sizeof(key), "%d", script_num);
    json_t *mod_j = json_object_get(scripts, key);
    if (!json_is_object(mod_j))
    {
        return;
    }
    Script script;
    script.init(first, end);
    json_t *deletions_j = json_object_get(mod_j, "deletions");
    std::vector<unsigned int> deletions;
    size_t count = json_is_array(deletions_j) ? json_array_size(deletions_j) : (deletions_j != NULL ? 1 : 0);
    for (size_t i = 0; i < count; i++)
    {
        json_t *line_j = json_is_array(deletions_j) ? json_array_get(deletions_j, i) : deletions_j;
        if (!json_is_integer(line_j) || json_integer_value(line_j) < 0 ||
            json_integer_value(line_j) >= script.num_instrs)
        {
            log("ANM header patching: entry %zu, script %d: bad deletion", entry_num, script_num);
            return;
        }
        deletions.push_back((unsigned int)json_integer_value(line_j));
    }
    // Line numbers relative to the previous deletions.
    for (size_t i = 1; i < deletions.size(); i++)
    {
        deletions[i] -= (unsigned int)i;
    }
    struct Change
    {
        unsigned int line;
        bool time;
        int16_t time_value;
        uint16_t addr;
        std::vector<uint8_t> code;
    };
    std::vector<Change> changes;
    const char *change_key;
    json_t *val_j;
    json_object_foreach(json_object_get(mod_j, "changes"), change_key, val_j)
    {
        const char *sep = strchr(change_key, '#');
        char *endptr;
        long line = strtol(change_key, &endptr, 10);
        if (sep == NULL || endptr != sep || line < 0 || (size_t)line >= script.num_instrs - deletions.size())
        {
            log("ANM header patching: entry %zu, script %d: bad change \"%s\"", entry_num, script_num, change_key);
            return;
        }
        Change change;
        change.line = (unsigned int)line;
        if (strcmp(sep + 1, "time") == 0)
        {
            change.time = true;
            change.time_value = (int16_t)json_integer_value(val_j);
            change.addr = 0;
        }
        else
        {
            change.time = false;
            change.time_value = 0;
            change.addr = (uint16_t)strtoul(sep + 1, NULL, 10);
            const char *code = json_string_value(val_j);
            if (code == NULL || !code_string_render(code, &change.code))
            {
                log("ANM header patching: entry %zu, script %d: unsupported code \"%s\"", entry_num, script_num,
                    code != NULL ? code : "");
                continue;
            }
        }
        changes.push_back(change);
    }
    for (unsigned int line : deletions)
    {
        script.delete_line(line);
    }
    for (const Change &change : changes)
    {
        AnmInstr *instr = script.at(change.line);
        if (change.time)
        {
            instr->time = change.time_value;
        }
        else if (change.addr + change.code.size() <= script.param_length_of(change.line))
        {
            memcpy((uint8_t *)instr + sizeof(AnmInstr) + change.addr, change.code.data(), change.code.size());
        }
    }
}

struct Entry
{
    uint32_t x = 0, y = 0, w = 0, h = 0;
    bool hasdata = false;
    uint8_t *next = NULL;
    std::string name;
    size_t thtxoffset = 0;
    std::vector<SpriteLocal> sprites;

    // transform_and_add_sprite: sprites wrapping around the texture's edges
    // split into up to four.
    void add_sprite(const AnmSprite &s, BlitFunc blitmode)
    {
        if (w == 0 || h == 0 || s.w == 0.0f || s.h == 0.0f)
        {
            return;
        }
        SpriteLocal lr = {blitmode, ((uint32_t)s.x) % w, ((uint32_t)s.y) % h, (uint32_t)s.w < w ? (uint32_t)s.w : w,
                          (uint32_t)s.h < h ? (uint32_t)s.h : h};
        int32_t split_w = (int32_t)(lr.x + lr.w) - (int32_t)w;
        int32_t split_h = (int32_t)(lr.y + lr.h) - (int32_t)h;
        if (split_w > 0)
        {
            lr.w = w - lr.x;
        }
        if (split_h > 0)
        {
            lr.h = h - lr.y;
        }
        if (split_w > 0 && split_h > 0)
        {
            sprites.push_back({blitmode, 0, 0, (uint32_t)split_w, (uint32_t)split_h});
        }
        if (split_w > 0)
        {
            sprites.push_back({blitmode, 0, lr.y, (uint32_t)split_w, lr.h});
        }
        if (split_h > 0)
        {
            sprites.push_back({blitmode, lr.x, 0, lr.w, (uint32_t)split_h});
        }
        sprites.push_back(lr);
    }
};

// anm_entry_init: copies the entry's header (and sprites and scripts) to
// the output, applying the header mods, and collects its sprites.
bool entry_init(HeaderMods &hdr, Entry &entry, uint8_t *in, uint8_t *in_end, uint8_t *out)
{
    if (in + sizeof(AnmHeader11) > in_end)
    {
        return false;
    }
    AnmHeader11 *header = (AnmHeader11 *)in;
    // entry_mods_t
    size_t entry_num = hdr.entries_seen++;
    json_t *entry_mod = hdr.entries != NULL ? numkey_get(hdr.entries, entry_num) : NULL;
    BlitFunc entry_blitmode = hdr.blitmode;
    if (json_is_object(entry_mod))
    {
        blitmode_parse(json_object_get(entry_mod, "blitmode"), &entry_blitmode);
    }
    entry.w = header->w;
    entry.h = header->h;
    entry.x = header->x;
    entry.y = header->y;
    entry.next = header->nextoffset != 0 ? in + header->nextoffset : NULL;
    if (entry.next != NULL && entry.next > in_end)
    {
        return false;
    }
    const char *name = (const char *)in + header->nameoffset;
    entry.name = name < (const char *)in_end ? std::string(name, strnlen(name, in_end - (uint8_t *)name)) : "";
    entry.hasdata = header->hasdata != 0;
    size_t thtxoffset = header->thtxoffset;
    entry.hasdata |= !entry.name.empty() && entry.name[0] != '@';
    size_t sprite_orig_num = header->sprites;
    size_t script_num = header->scripts;
    if (thtxoffset != 0)
    {
        ThtxHeader *thtx = (ThtxHeader *)(in + thtxoffset);
        if ((uint8_t *)(thtx + 1) > in_end || memcmp(thtx->magic, "THTX", 4) != 0)
        {
            return false;
        }
        entry.w = thtx->w;
        entry.h = thtx->h;
        entry.thtxoffset = thtxoffset;
        memcpy(out, in, thtxoffset + sizeof(ThtxHeader));
    }
    else if (entry.next != NULL)
    {
        memcpy(out, in, entry.next - in);
    }
    else
    {
        memcpy(out, in, in_end - in);
    }
    if (sprite_orig_num == 0)
    {
        entry.sprites.push_back({entry_blitmode, 0, 0, entry.w, entry.h});
    }
    else
    {
        uint32_t *sprite_in = (uint32_t *)(out + sizeof(AnmHeader11));
        for (size_t i = 0; i < sprite_orig_num; i++, sprite_in++)
        {
            AnmSprite *s_orig = (AnmSprite *)(out + *sprite_in);
            // sprite_mods_t, counted for every sprite of the file.
            size_t sprite_num = hdr.sprites_seen++;
            BlitFunc blitmode = entry_blitmode;
            json_t *mod_j = hdr.sprites != NULL ? numkey_get(hdr.sprites, sprite_num) : NULL;
            json_t *bounds_j = json_is_object(mod_j) ? json_object_get(mod_j, "bounds") : mod_j;
            if (json_is_array(bounds_j))
            {
                bool ok = json_array_size(bounds_j) == 4;
                float b[4];
                for (size_t k = 0; ok && k < 4; k++)
                {
                    json_t *c = json_array_get(bounds_j, k);
                    ok = json_is_integer(c) && json_integer_value(c) >= 0;
                    b[k] = ok ? (float)json_integer_value(c) : 0.0f;
                }
                if (ok)
                {
                    s_orig->x = b[0];
                    s_orig->y = b[1];
                    s_orig->w = b[2];
                    s_orig->h = b[3];
                }
                else
                {
                    log("ANM header patching: sprite %zu: bounds must be [X, Y, width, height]", sprite_num);
                }
            }
            if (json_is_object(mod_j))
            {
                blitmode_parse(json_object_get(mod_j, "blitmode"), &blitmode);
            }
            else if (json_is_string(mod_j))
            {
                blitmode_parse(mod_j, &blitmode);
            }
            // A single sprite covers the whole texture (thcrap: never wrong,
            // and some files' images are larger than their one sprite).
            if (sprite_orig_num <= 1)
            {
                entry.sprites.push_back({blitmode, 0, 0, entry.w, entry.h});
            }
            else
            {
                entry.add_sprite(*s_orig, blitmode);
            }
        }
    }
    json_t *scripts = json_is_object(entry_mod) ? json_object_get(entry_mod, "scripts") : NULL;
    if (json_is_object(scripts))
    {
        AnmOffset *offsets = (AnmOffset *)(out + sizeof(AnmHeader11) + sprite_orig_num * sizeof(uint32_t));
        uint8_t *out_end = out + (entry.next != NULL ? entry.next - in : in_end - in);
        for (size_t i = 0; i < script_num; i++)
        {
            apply_script_mods(scripts, entry_num, offsets[i].id, out + offsets[i].offset, out_end);
        }
    }
    const char *new_name = json_is_object(entry_mod) ? json_string_value(json_object_get(entry_mod, "name")) : NULL;
    if (new_name != NULL)
    {
        entry.name = new_name;
    }
    return true;
}

// stack_game_png_apply: "<dir>/<name>@<anm>@<entry><ext>" (thtk's names)
// if any patch has it, else "<dir>/<name><ext>", from every patch in
// order.
void stack_game_png_apply(ImgPatch &patch, const std::vector<SpriteLocal> &sprites, const std::string &image_name,
                          const char *anm_fn, size_t entry_num)
{
    std::string anm_stem = anm_fn;
    size_t slash = anm_stem.find_last_of("/\\");
    if (slash != std::string::npos)
    {
        anm_stem = anm_stem.substr(slash + 1);
    }
    anm_stem = anm_stem.substr(0, anm_stem.rfind('.'));
    size_t name_slash = image_name.rfind('/');
    std::string dir = name_slash != std::string::npos ? image_name.substr(0, name_slash) : "";
    std::string file = name_slash != std::string::npos ? image_name.substr(name_slash + 1) : image_name;
    size_t dot = file.rfind('.');
    std::string stem = dot != std::string::npos ? file.substr(0, dot) : file;
    std::string ext = dot != std::string::npos ? file.substr(dot) : "";
    std::string thtk_fn = dir + "/" + stem + "@" + anm_stem + "@" + std::to_string(entry_num) + ext;
    for (const std::string &fn : {thtk_fn, image_name})
    {
        bool found = false;
        std::vector<std::string> chain = resolve_chain_game(fn);
        for (const Patch &p : stack())
        {
            for (const std::string &name : chain)
            {
                std::string data;
                Image image;
                if (!patch_file_load(p, name, &data) || !png_load_for_format(data, patch.format, &image))
                {
                    continue;
                }
                for (const SpriteLocal &sprite : sprites)
                {
                    sprite_patch(patch, sprite, image);
                }
                found = true;
            }
        }
        if (found)
        {
            return;
        }
    }
}

} // namespace

int patch_anm(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t *patch)
{
    HeaderMods hdr(patch);
    std::vector<uint8_t> file_in((uint8_t *)file_inout, (uint8_t *)file_inout + size_in);
    memset(file_inout, 0, size_out);
    uint8_t *in = file_in.data();
    uint8_t *in_end = in + size_in;
    uint8_t *out = (uint8_t *)file_inout;
    size_t entry_num = 0;
    size_t images = 0;
    while (in != NULL && in < in_end)
    {
        Entry entry;
        if (!entry_init(hdr, entry, in, in_end, out))
        {
            log("%s: corrupt ANM file, stopping", fn);
            // Keep the rest as it was.
            memcpy(out, in, in_end - in);
            break;
        }
        if (entry.hasdata && entry.thtxoffset != 0)
        {
            ThtxHeader *thtx_in = (ThtxHeader *)(in + entry.thtxoffset);
            ThtxHeader *thtx_out = (ThtxHeader *)(out + entry.thtxoffset);
            uint8_t *pixels = (uint8_t *)(thtx_out + 1);
            size_t size = thtx_in->size;
            if ((uint8_t *)(thtx_in + 1) + size <= in_end && format_bpp(thtx_in->format) != 0)
            {
                memcpy(pixels, thtx_in + 1, size);
                ImgPatch img = {pixels, thtx_in->format, format_bpp(thtx_in->format) * entry.w, entry.w,
                                entry.h, entry.x, entry.y};
                if ((size_t)img.stride * img.h <= size)
                {
                    stack_game_png_apply(img, entry.sprites, entry.name, fn, entry_num);
                    images++;
                }
            }
            else
            {
                memcpy(pixels, thtx_in + 1, in_end - (uint8_t *)(thtx_in + 1));
            }
            size_t out_next = entry.thtxoffset + sizeof(ThtxHeader) + size;
            out += out_next;
        }
        else if (entry.next != NULL)
        {
            out += entry.next - in;
        }
        in = entry.next;
        entry_num++;
    }
    return 1;
}

} // namespace thcrap
