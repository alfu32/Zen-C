
#ifndef LSP_INDEX_H
#define LSP_INDEX_H

#include "parser.h"
#include <stddef.h>

typedef enum
{
    RANGE_DEFINITION,
    RANGE_REFERENCE
} RangeType;

typedef struct LSPRange
{
    int start_line;
    int start_col;
    int end_line;
    int end_col; // Approximation.
    RangeType type;
    int def_line;
    int def_col;
    int def_end_line;
    int def_end_col;
    char *hover_text;
    ASTNode *node;
    struct LSPRange *next;
} LSPRange;

typedef struct LSPIndex
{
    LSPRange *head;
    LSPRange *tail;
    const char *source_start;
    size_t source_len;
} LSPIndex;

// API.
LSPIndex *lsp_index_new();
void lsp_index_free(LSPIndex *idx);
void lsp_index_set_source(LSPIndex *idx, const char *src);
void lsp_index_add_def(LSPIndex *idx, Token t, const char *hover, ASTNode *node);
void lsp_index_add_ref(LSPIndex *idx, Token t, Token def_t, ASTNode *node);
LSPRange *lsp_find_at(LSPIndex *idx, int line, int col);

// Walker.
void lsp_build_index(LSPIndex *idx, ASTNode *root);

#endif
