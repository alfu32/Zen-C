
#include "json_rpc.h"
#include "lsp_index.h"
#include "parser.h"
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static LSPIndex *g_index = NULL;

typedef struct Diagnostic
{
    int line;
    int col;
    char *message;
    struct Diagnostic *next;
} Diagnostic;

typedef struct
{
    Diagnostic *head;
    Diagnostic *tail;
} DiagnosticList;

static ParserContext *g_ctx = NULL;
static char *g_last_src = NULL;

static LSPRange *lsp_find_definition_at(LSPIndex *idx, int line, int col)
{
    if (!idx)
    {
        return NULL;
    }

    LSPRange *curr = idx->head;
    LSPRange *best = NULL;
    int best_len = INT_MAX;

    while (curr)
    {
        if (curr->type == RANGE_DEFINITION && line >= curr->start_line && line <= curr->end_line)
        {
            if (line == curr->start_line && col < curr->start_col)
            {
                curr = curr->next;
                continue;
            }
            if (line == curr->end_line && col > curr->end_col)
            {
                curr = curr->next;
                continue;
            }

            int len = (curr->end_line - curr->start_line) * 100000 +
                      (curr->end_col - curr->start_col);
            if (len < best_len)
            {
                best_len = len;
                best = curr;
            }
        }
        curr = curr->next;
    }

    return best;
}

// Callback for parser errors.
void lsp_on_error(void *data, Token t, const char *msg)
{
    DiagnosticList *list = (DiagnosticList *)data;
    Diagnostic *d = xmalloc(sizeof(Diagnostic));
    d->line = t.line > 0 ? t.line - 1 : 0;
    d->col = t.col > 0 ? t.col - 1 : 0;
    d->message = xstrdup(msg);
    d->next = NULL;

    if (!list->head)
    {
        list->head = d;
        list->tail = d;
    }
    else
    {
        list->tail->next = d;
        list->tail = d;
    }
}

void lsp_check_file(const char *uri, const char *json_src)
{
    // Prepare ParserContext (persistent).
    if (g_ctx)
    {

        free(g_ctx);
    }
    g_ctx = calloc(1, sizeof(ParserContext));
    g_ctx->is_fault_tolerant = 1;

    DiagnosticList diagnostics = {0};
    g_ctx->error_callback_data = &diagnostics;
    g_ctx->on_error = lsp_on_error;

    // Prepare Lexer.
    // Cache source.
    if (g_last_src)
    {
        free(g_last_src);
    }
    g_last_src = strdup(json_src);

    Lexer l;
    lexer_init(&l, json_src);

    ASTNode *root = parse_program(g_ctx, &l);

    if (g_index)
    {
        lsp_index_free(g_index);
    }
    g_index = lsp_index_new();
    if (root)
    {
        lsp_build_index(g_index, root);
    }

    // Construct JSON Response (notification)

    char *notification = malloc(128 * 1024);
    char *p = notification;
    p += sprintf(p,
                 "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/"
                 "publishDiagnostics\",\"params\":{\"uri\":\"%s\",\"diagnostics\":[",
                 uri);

    Diagnostic *d = diagnostics.head;
    while (d)
    {

        p += sprintf(p,
                     "{\"range\":{\"start\":{\"line\":%d,\"character\":%d},\"end\":"
                     "{\"line\":%d,"
                     "\"character\":%d}},\"severity\":1,\"message\":\"%s\"}",
                     d->line, d->col, d->line, d->col + 1, d->message);

        if (d->next)
        {
            p += sprintf(p, ",");
        }

        d = d->next;
    }

    p += sprintf(p, "]}}");

    // Send notification.
    long len = strlen(notification);
    fprintf(stdout, "Content-Length: %ld\r\n\r\n%s", len, notification);
    fflush(stdout);

    free(notification);

    // Cleanup.
    Diagnostic *cur = diagnostics.head;
    while (cur)
    {
        Diagnostic *next = cur->next;
        free(cur->message);
        free(cur);
        cur = next;
    }
}

void lsp_goto_definition(const char *id, const char *uri, int line, int col)
{
    LSPRange *r = NULL;
    if (g_index)
    {
        r = lsp_find_at(g_index, line, col);
    }
    if (r && r->type == RANGE_REFERENCE)
    {
        // Found reference, return definition
        char resp[1024];
        int end_line = r->def_end_line;
        int end_col = r->def_end_col;
        if (end_line <= 0 && end_col <= 0)
        {
            end_line = r->def_line;
            end_col = r->def_col;
        }
        sprintf(resp,
                "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":{\"uri\":\"%s\","
                "\"range\":{\"start\":{"
                "\"line\":%d,\"character\":%d},\"end\":{\"line\":%d,\"character\":%"
                "d}}}}",
                id, uri, r->def_line, r->def_col, end_line, end_col);

        fprintf(stderr, "zls: Responding (definition) id=%s\n", id);
        fprintf(stdout, "Content-Length: %ld\r\n\r\n%s", strlen(resp), resp);
        fflush(stdout);
    }
    else if (r && r->type == RANGE_DEFINITION)
    {
        // Already at definition? Return itself.
        char resp[1024];
        sprintf(resp,
                "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":{\"uri\":\"%s\","
                "\"range\":{\"start\":{"
                "\"line\":%d,\"character\":%d},\"end\":{\"line\":%d,\"character\":%"
                "d}}}}",
                id, uri, r->start_line, r->start_col, r->end_line, r->end_col);

        fprintf(stderr, "zls: Responding (definition) id=%s\n", id);
        fprintf(stdout, "Content-Length: %ld\r\n\r\n%s", strlen(resp), resp);
        fflush(stdout);
    }
    else
    {
        // Null result
        char null_resp[256];
        snprintf(null_resp, sizeof(null_resp), "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":null}",
                 id);
        fprintf(stderr, "zls: Responding (definition) id=%s null\n", id);
        fprintf(stdout, "Content-Length: %ld\r\n\r\n%s", strlen(null_resp), null_resp);
        fflush(stdout);
    }
}

void lsp_hover(const char *id, const char *uri, int line, int col)
{
    (void)uri;
    LSPRange *r = NULL;
    if (g_index)
    {
        r = lsp_find_at(g_index, line, col);
    }
    char *text = NULL;

    if (r)
    {
        if (r->type == RANGE_DEFINITION)
        {
            text = r->hover_text;
        }
        else if (r->type == RANGE_REFERENCE)
        {
        LSPRange *def = lsp_find_definition_at(g_index, r->def_line, r->def_col);
        if (def && def->type == RANGE_DEFINITION)
        {
            text = def->hover_text;
        }
        }
    }

    if (text)
    {
        char *json = malloc(16384);
        // content: { kind: markdown, value: text }
        sprintf(json,
                "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":{\"contents\":{\"kind\":"
                "\"markdown\","
                "\"value\":\"```c\\n%s\\n```\"}}}",
                id, text);

        fprintf(stderr, "zls: Responding (hover) id=%s\n", id);
        fprintf(stdout, "Content-Length: %ld\r\n\r\n%s", strlen(json), json);
        fflush(stdout);
        free(json);
    }
    else
    {
        char null_resp[256];
        snprintf(null_resp, sizeof(null_resp), "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":null}",
                 id);
        fprintf(stderr, "zls: Responding (hover) id=%s null\n", id);
        fprintf(stdout, "Content-Length: %ld\r\n\r\n%s", strlen(null_resp), null_resp);
        fflush(stdout);
    }
}

void lsp_completion(const char *id, const char *uri, int line, int col)
{
    (void)uri;
    if (!g_ctx)
    {
        char null_resp[256];
        snprintf(null_resp, sizeof(null_resp), "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":[]}",
                 id);
        fprintf(stderr, "zls: Responding (completion) id=%s empty\n", id);
        fprintf(stdout, "Content-Length: %ld\r\n\r\n%s", strlen(null_resp), null_resp);
        fflush(stdout);
        return;
    }

    // Context-aware completion (Dot access)
    if (g_last_src)
    {
        // Simple line access
        int cur_line = 0;

        char *ptr = g_last_src;
        // Fast forward to line
        while (*ptr && cur_line < line)
        {
            if (*ptr == '\n')
            {
                cur_line++;
            }
            ptr++;
        }

        if (col > 0 && ptr[col - 1] == '.')
        {
            // Found dot!
            // Scan backwards for identifier: [whitespace] [ident] .
            int i = col - 2;
            while (i >= 0 && (ptr[i] == ' ' || ptr[i] == '\t'))
            {
                i--;
            }

            if (i >= 0)
            {
                int end_ident = i;
                while (i >= 0 && (isalnum(ptr[i]) || ptr[i] == '_'))
                {
                    i--;
                }
                int start_ident = i + 1;

                if (start_ident <= end_ident)
                {
                    int len = end_ident - start_ident + 1;
                    char var_name[256];
                    strncpy(var_name, ptr + start_ident, len);
                    var_name[len] = 0;

                    char *type_name = NULL;
                    Symbol *sym = find_symbol_in_all(g_ctx, var_name);

                    if (sym)
                    {
                        if (sym->type_info)
                        {
                            type_name = type_to_string(sym->type_info);
                        }
                        else if (sym->type_name)
                        {
                            type_name = sym->type_name;
                        }
                    }

                    if (type_name)
                    {
                        char clean_name[256];
                        char *src = type_name;
                        if (0 == strncmp(src, "struct ", 7))
                        {
                            src += 7;
                        }
                        char *dst = clean_name;
                        while (*src && *src != '*')
                        {
                            *dst++ = *src++;
                        }
                        *dst = 0;

                        // Lookup struct.
                        StructDef *sd = g_ctx->struct_defs;
                        while (sd)
                        {
                            if (0 == strcmp(sd->name, clean_name))
                            {
                                char *json_fields = malloc(1024 * 1024);
                                char *pj = json_fields;
                                pj += sprintf(pj, "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":[",
                                              id);

                                int ffirst = 1;
                                if (sd->node && sd->node->strct.fields)
                                {
                                    ASTNode *field = sd->node->strct.fields;
                                    while (field)
                                    {
                                        if (!ffirst)
                                        {
                                            pj += sprintf(pj, ",");
                                        }
                                        pj += sprintf(
                                            pj,
                                            "{\"label\":\"%s\",\"kind\":5,\"detail\":\"field %s\"}",
                                            field->field.name, field->field.type); // Kind 5 = Field
                                        ffirst = 0;
                                        field = field->next;
                                    }
                                }

                                pj += sprintf(pj, "]}");
                                fprintf(stderr, "zls: Responding (completion) id=%s\n", id);
                                fprintf(stdout, "Content-Length: %ld\r\n\r\n%s",
                                        strlen(json_fields), json_fields);
                                fflush(stdout);
                                free(json_fields);
                                free(json);
                                return; // Done, yippee.
                            }
                            sd = sd->next;
                        }
                    }
                }
            }
        }
    }

    char *json = xmalloc(1024 * 1024); // 1MB buffer.
    char *p = json;
    p += sprintf(p, "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":[", id);

    int first = 1;

    // Functions
    FuncSig *f = g_ctx->func_registry;
    while (f)
    {
        if (!first)
        {
            p += sprintf(p, ",");
        }
        p += sprintf(p, "{\"label\":\"%s\",\"kind\":3,\"detail\":\"fn %s(...)\"}", f->name,
                     f->name); // Kind 3 = Function
        first = 0;
        f = f->next;
    }

    // Structs
    StructDef *s = g_ctx->struct_defs;
    while (s)
    {
        if (!first)
        {
            p += sprintf(p, ",");
        }
        p += sprintf(p, "{\"label\":\"%s\",\"kind\":22,\"detail\":\"struct %s\"}", s->name,
                     s->name); // Kind 22 = Struct
        first = 0;
        s = s->next;
    }

    p += sprintf(p, "]}");

    fprintf(stderr, "zls: Responding (completion) id=%s\n", id);
    fprintf(stdout, "Content-Length: %ld\r\n\r\n%s", strlen(json), json);
    fflush(stdout);
    free(json);
}
