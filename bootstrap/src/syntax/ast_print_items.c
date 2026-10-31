// `--emit=ast` for items (functions, types, imports, ...).

#include "syntax/ast_print_internal.h"
#include "syntax/ast_text.h"

static void symbols(Printer *p, const char *label, SymbolList list, const char *sep) {
    if (!list.len) {
        return;
    }
    line_start(p);
    fprintf(p->out, "%s ", label);
    for (uint32_t i = 0; i < list.len; i++) {
        fprintf(p->out, "%s%s", i > 0 ? sep : "", name(p, list.items[i]));
    }
    fputc('\n', p->out);
}

static void attributes(Printer *p, AttributeList attrs) {
    for (uint32_t i = 0; i < attrs.len; i++) {
        const Attribute *a = &attrs.items[i];
        StrBuf buf = {0};
        strbuf_append(&buf, symbol_name(p->names, a->name));
        for (uint32_t j = 0; j < a->args.len; j++) {
            strbuf_append_cstr(&buf, j == 0 ? "(" : ", ");
            literal_text(&buf, a->args.items[j], p->names);
        }
        strbuf_append_cstr(&buf, a->args.len ? ")" : "");
        text_line(p, "attr", &buf);
        strbuf_free(&buf);
    }
}

static const char *mode_text(Mode mode) {
    return mode == MODE_MUT ? "mut " : mode == MODE_MOVE ? "move " : "";
}

static void param(Printer *p, const Param *param_) {
    StrBuf buf = {0};
    strbuf_appendf(&buf, "%s: %s", name(p, param_->name), mode_text(param_->mode));
    type_text(&buf, param_->type, p->names);
    text_line(p, "param", &buf);
    strbuf_free(&buf);
    if (param_->default_value) {
        p->depth++;
        line(p, "default");
        p->depth++;
        print_expr(p, param_->default_value);
        p->depth -= 2;
    }
}

static void fn_decl(Printer *p, const FnDecl *fn) {
    line_start(p);
    fprintf(p->out, "fn %s%s%s%s\n", name(p, fn->name), fn->is_pub ? " pub" : "",
            fn->is_unsafe ? " unsafe" : "", fn->abi.len ? " extern" : "");
    p->depth++;
    attributes(p, fn->attributes);
    symbols(p, "type_params", fn->type_params, ", ");
    if (fn->has_self) {
        line_start(p);
        fprintf(p->out, "self %s\n", mode_text(fn->self_mode));
    }
    for (uint32_t i = 0; i < fn->params.len; i++) {
        param(p, &fn->params.items[i]);
    }
    type_line(p, "returns", fn->ret);
    if (fn->body) {
        print_block(p, "body", fn->body);
    }
    p->depth--;
}

static void fields(Printer *p, const Item *item) {
    for (uint32_t i = 0; i < item->as.struct_.fields.len; i++) {
        const FieldDecl *f = &item->as.struct_.fields.items[i];
        StrBuf buf = {0};
        if (f->name.id) {
            strbuf_appendf(&buf, "%s: ", name(p, f->name));
        }
        type_text(&buf, f->type, p->names);
        text_line(p, f->is_pub ? "field pub" : "field", &buf);
        strbuf_free(&buf);
        if (f->default_value) {
            p->depth++;
            print_expr(p, f->default_value);
            p->depth--;
        }
    }
}

static void methods(Printer *p, FnList fns) {
    for (uint32_t i = 0; i < fns.len; i++) {
        fn_decl(p, fns.items[i]);
    }
}

static void variants(Printer *p, const Item *item) {
    for (uint32_t i = 0; i < item->as.enum_.variants.len; i++) {
        const Variant *v = &item->as.enum_.variants.items[i];
        StrBuf buf = {0};
        strbuf_append(&buf, symbol_name(p->names, v->name));
        for (uint32_t j = 0; v->has_fields && j < v->fields.len; j++) {
            strbuf_append_cstr(&buf, j == 0 ? "(" : ", ");
            if (v->fields.items[j].name.id) {
                strbuf_appendf(&buf, "%s: ", name(p, v->fields.items[j].name));
            }
            type_text(&buf, v->fields.items[j].type, p->names);
        }
        strbuf_append_cstr(&buf, v->has_fields ? (v->fields.len ? ")" : "()") : "");
        text_line(p, "variant", &buf);
        if (v->discriminant) {
            p->depth++;
            print_expr(p, v->discriminant);
            p->depth--;
        }
        strbuf_free(&buf);
    }
}

static void type_item(Printer *p, const Item *item, const char *keyword) {
    bool is_enum = item->kind == ITEM_ENUM;
    bool is_copy = is_enum ? item->as.enum_.is_copy : item->as.struct_.is_copy;
    line_start(p);
    fprintf(p->out, "%s %s%s%s%s\n", keyword, name(p, item->name), item->is_pub ? " pub" : "",
            is_copy ? " copy" : "", !is_enum && item->as.struct_.is_ref ? " ref" : "");
    p->depth++;
    attributes(p, item->attributes);
    symbols(p, "type_params", is_enum ? item->as.enum_.type_params : item->as.struct_.type_params,
            ", ");
    if (is_enum) {
        variants(p, item);
        methods(p, item->as.enum_.methods);
    } else {
        fields(p, item);
        methods(p, item->as.struct_.methods);
    }
    p->depth--;
}

static void value_item(Printer *p, const Item *item, const char *keyword) {
    line_start(p);
    fprintf(p->out, "%s %s%s\n", keyword, name(p, item->name), item->is_pub ? " pub" : "");
    p->depth++;
    type_line(p, "type", item->as.const_.type);
    print_expr(p, item->as.const_.value);
    p->depth--;
}

static void item(Printer *p, const Item *it) {
    switch (it->kind) {
    case ITEM_FN: fn_decl(p, it->as.fn); break;
    case ITEM_STRUCT: type_item(p, it, "struct"); break;
    case ITEM_TUPLE_STRUCT: type_item(p, it, "tuple_struct"); break;
    case ITEM_ENUM: type_item(p, it, "enum"); break;
    case ITEM_CONST: value_item(p, it, "const"); break;
    case ITEM_GLOBAL: value_item(p, it, "global"); break;
    case ITEM_TYPE_ALIAS:
        line_start(p);
        fprintf(p->out, "type %s%s\n", name(p, it->name), it->is_pub ? " pub" : "");
        p->depth++;
        type_line(p, "=", it->as.alias.type);
        p->depth--;
        break;
    case ITEM_IMPORT:
        symbols(p, "import", it->as.import.path, ".");
        if (it->as.import.alias.id) {
            p->depth++;
            line_start(p);
            fprintf(p->out, "as %s\n", name(p, it->as.import.alias));
            p->depth--;
        }
        break;
    case ITEM_EXTERN:
        line_start(p);
        fprintf(p->out, "extern \"" SV_FMT "\"\n", SV_ARG(it->as.extern_.abi));
        p->depth++;
        methods(p, it->as.extern_.fns);
        p->depth--;
        break;
    }
}

void print_module(const Module *module, const Interner *names, FILE *out) {
    Printer p = {.out = out, .names = names};
    line(&p, "module");
    p.depth++;
    for (uint32_t i = 0; i < module->docs.len; i++) {
        line_start(&p);
        fprintf(out, "doc \"" SV_FMT "\"\n", SV_ARG(module->docs.items[i]));
    }
    for (uint32_t i = 0; i < module->items.len; i++) {
        item(&p, module->items.items[i]);
    }
}
