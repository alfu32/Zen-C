
#include "lsp_index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LSPIndex *lsp_index_new()
{
    return calloc(1, sizeof(LSPIndex));
}

void lsp_index_free(LSPIndex *idx)
{
    if (!idx)
    {
        return;
    }
    LSPRange *c = idx->head;
    while (c)
    {
        LSPRange *n = c->next;
        if (c->hover_text)
        {
            free(c->hover_text);
        }
        free(c);
        c = n;
    }
    free(idx);
}

void lsp_index_add(LSPIndex *idx, LSPRange *r)
{
    if (!idx->head)
    {
        idx->head = r;
        idx->tail = r;
    }
    else
    {
        idx->tail->next = r;
        idx->tail = r;
    }
}

void lsp_index_add_def(LSPIndex *idx, Token t, const char *hover, ASTNode *node)
{
    if (t.line <= 0)
    {
        return;
    }
    LSPRange *r = calloc(1, sizeof(LSPRange));
    r->type = RANGE_DEFINITION;
    r->start_line = t.line - 1;
    r->start_col = t.col - 1;
    r->end_line = t.line - 1;
    r->end_col = t.col - 1 + t.len;
    if (hover)
    {
        r->hover_text = strdup(hover);
    }
    r->node = node;

    lsp_index_add(idx, r);
}

void lsp_index_add_ref(LSPIndex *idx, Token t, Token def_t, ASTNode *node)
{
    if (t.line <= 0 || def_t.line <= 0)
    {
        return;
    }
    LSPRange *r = calloc(1, sizeof(LSPRange));
    r->type = RANGE_REFERENCE;
    r->start_line = t.line - 1;
    r->start_col = t.col - 1;
    r->end_line = t.line - 1;
    r->end_col = t.col - 1 + t.len;

    r->def_line = def_t.line - 1;
    r->def_col = def_t.col - 1;
    r->def_end_line = def_t.line - 1;
    r->def_end_col = def_t.col - 1 + def_t.len;
    r->node = node;

    lsp_index_add(idx, r);
}

LSPRange *lsp_find_at(LSPIndex *idx, int line, int col)
{
    LSPRange *curr = idx->head;
    LSPRange *best = NULL;

    while (curr)
    {
        if (line >= curr->start_line && line <= curr->end_line)
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

            best = curr;
        }
        curr = curr->next;
    }
    return best;
}

// Walker.

static void lsp_walk_node(LSPIndex *idx, ASTNode *node);

static void lsp_index_add_method_def(LSPIndex *idx, ASTNode *method, const char *owner,
                                     const char *trait)
{
    if (!method || method->type != NODE_FUNCTION || !owner)
    {
        return;
    }

    char *mname = NULL;
    if (method->token.len > 0)
    {
        mname = token_strdup(method->token);
    }
    if (!mname)
    {
        mname = xstrdup(method->func.name ? method->func.name : "?");
    }

    char hover[256];
    if (trait)
    {
        snprintf(hover, sizeof(hover), "fn %s::%s.%s(...) -> %s", owner, trait, mname,
                 method->func.ret_type ? method->func.ret_type : "void");
    }
    else
    {
        snprintf(hover, sizeof(hover), "fn %s.%s(...) -> %s", owner, mname,
                 method->func.ret_type ? method->func.ret_type : "void");
    }

    lsp_index_add_def(idx, method->token, hover, method);
    free(mname);
}

static void lsp_index_walk_methods(LSPIndex *idx, ASTNode *methods, const char *owner,
                                   const char *trait)
{
    ASTNode *m = methods;
    while (m)
    {
        if (m->type == NODE_FUNCTION)
        {
            lsp_index_add_method_def(idx, m, owner, trait);
            if (m->func.body)
            {
                lsp_walk_node(idx, m->func.body);
            }
        }
        else
        {
            lsp_walk_node(idx, m);
        }
        m = m->next;
    }
}

static void lsp_walk_node(LSPIndex *idx, ASTNode *node)
{
    if (!node)
    {
        return;
    }

    // Definition logic.
    if (node->type == NODE_STRUCT)
    {
        if (node->strct.name)
        {
            char hover[256];
            snprintf(hover, sizeof(hover), "struct %s", node->strct.name);
            lsp_index_add_def(idx, node->token, hover, node);
        }

        ASTNode *field = node->strct.fields;
        while (field)
        {
            if (field->type == NODE_FIELD && field->field.name)
            {
                char hover[256];
                if (field->field.type)
                {
                    snprintf(hover, sizeof(hover), "field %s: %s", field->field.name,
                             field->field.type);
                }
                else
                {
                    snprintf(hover, sizeof(hover), "field %s", field->field.name);
                }
                lsp_index_add_def(idx, field->token, hover, field);
            }
            field = field->next;
        }
    }
    else if (node->type == NODE_ENUM)
    {
        if (node->enm.name)
        {
            char hover[256];
            snprintf(hover, sizeof(hover), "enum %s", node->enm.name);
            lsp_index_add_def(idx, node->token, hover, node);
        }

        ASTNode *variant = node->enm.variants;
        while (variant)
        {
            if (variant->type == NODE_ENUM_VARIANT && variant->variant.name)
            {
                char hover[256];
                if (variant->variant.payload)
                {
                    char *payload = type_to_string(variant->variant.payload);
                    snprintf(hover, sizeof(hover), "variant %s::%s(%s)", node->enm.name,
                             variant->variant.name, payload ? payload : "?");
                    free(payload);
                }
                else
                {
                    snprintf(hover, sizeof(hover), "variant %s::%s", node->enm.name,
                             variant->variant.name);
                }
                lsp_index_add_def(idx, variant->token, hover, variant);
            }
            variant = variant->next;
        }
    }
    else if (node->type == NODE_TRAIT)
    {
        if (node->trait.name)
        {
            char hover[256];
            snprintf(hover, sizeof(hover), "trait %s", node->trait.name);
            lsp_index_add_def(idx, node->token, hover, node);
        }
        lsp_index_walk_methods(idx, node->trait.methods, node->trait.name, NULL);
    }
    else if (node->type == NODE_FUNCTION)
    {
        char hover[256];
        snprintf(hover, sizeof(hover), "fn %s(...) -> %s", node->func.name,
                 node->func.ret_type ? node->func.ret_type : "void");
        lsp_index_add_def(idx, node->token, hover, node);

        // Recurse body.
        lsp_walk_node(idx, node->func.body);
    }
    else if (node->type == NODE_VAR_DECL)
    {
        char hover[256];
        snprintf(hover, sizeof(hover), "var %s", node->var_decl.name);
        lsp_index_add_def(idx, node->token, hover, node);

        lsp_walk_node(idx, node->var_decl.init_expr);
    }
    else if (node->type == NODE_IMPL)
    {
        lsp_index_walk_methods(idx, node->impl.methods, node->impl.struct_name, NULL);
    }
    else if (node->type == NODE_IMPL_TRAIT)
    {
        lsp_index_walk_methods(idx, node->impl_trait.methods, node->impl_trait.target_type,
                               node->impl_trait.trait_name);
    }
    else if (node->type == NODE_CONST)
    {
        char hover[256];
        snprintf(hover, sizeof(hover), "const %s", node->var_decl.name);
        lsp_index_add_def(idx, node->token, hover, node);

        lsp_walk_node(idx, node->var_decl.init_expr);
    }

    // Reference logic.
    if (node->definition_token.line > 0 && node->definition_token.line != node->token.line)
    {
        // It has a definition!
        lsp_index_add_ref(idx, node->token, node->definition_token, node);
    }
    else if (node->definition_token.line > 0)
    {
        lsp_index_add_ref(idx, node->token, node->definition_token, node);
    }

    // General recursion.

    switch (node->type)
    {
    case NODE_ROOT:
        lsp_walk_node(idx, node->root.children);
        break;
    case NODE_BLOCK:
        lsp_walk_node(idx, node->block.statements);
        break;
    case NODE_IF:
        lsp_walk_node(idx, node->if_stmt.condition);
        lsp_walk_node(idx, node->if_stmt.then_body);
        lsp_walk_node(idx, node->if_stmt.else_body);
        break;
    case NODE_WHILE:
        lsp_walk_node(idx, node->while_stmt.condition);
        lsp_walk_node(idx, node->while_stmt.body);
        break;
    case NODE_FOR:
        lsp_walk_node(idx, node->for_stmt.init);
        lsp_walk_node(idx, node->for_stmt.condition);
        lsp_walk_node(idx, node->for_stmt.step);
        lsp_walk_node(idx, node->for_stmt.body);
        break;
    case NODE_FOR_RANGE:
        lsp_walk_node(idx, node->for_range.start);
        lsp_walk_node(idx, node->for_range.end);
        lsp_walk_node(idx, node->for_range.body);
        break;
    case NODE_LOOP:
        lsp_walk_node(idx, node->loop_stmt.body);
        break;
    case NODE_REPEAT:
        lsp_walk_node(idx, node->repeat_stmt.body);
        break;
    case NODE_UNLESS:
        lsp_walk_node(idx, node->unless_stmt.condition);
        lsp_walk_node(idx, node->unless_stmt.body);
        break;
    case NODE_GUARD:
        lsp_walk_node(idx, node->guard_stmt.condition);
        lsp_walk_node(idx, node->guard_stmt.body);
        break;
    case NODE_DO_WHILE:
        lsp_walk_node(idx, node->do_while_stmt.condition);
        lsp_walk_node(idx, node->do_while_stmt.body);
        break;
    case NODE_RETURN:
        lsp_walk_node(idx, node->ret.value);
        break;
    case NODE_EXPR_BINARY:
        lsp_walk_node(idx, node->binary.left);
        lsp_walk_node(idx, node->binary.right);
        break;
    case NODE_EXPR_UNARY:
        lsp_walk_node(idx, node->unary.operand);
        break;
    case NODE_EXPR_CALL:
        lsp_walk_node(idx, node->call.callee);
        lsp_walk_node(idx, node->call.args);
        break;
    case NODE_EXPR_MEMBER:
        lsp_walk_node(idx, node->member.target);
        break;
    case NODE_EXPR_INDEX:
        lsp_walk_node(idx, node->index.array);
        lsp_walk_node(idx, node->index.index);
        break;
    case NODE_EXPR_SLICE:
        lsp_walk_node(idx, node->slice.array);
        lsp_walk_node(idx, node->slice.start);
        lsp_walk_node(idx, node->slice.end);
        break;
    case NODE_EXPR_CAST:
        lsp_walk_node(idx, node->cast.expr);
        break;
    case NODE_EXPR_SIZEOF:
        lsp_walk_node(idx, node->size_of.expr);
        break;
    case NODE_EXPR_STRUCT_INIT:
        lsp_walk_node(idx, node->struct_init.fields);
        break;
    case NODE_EXPR_ARRAY_LITERAL:
        lsp_walk_node(idx, node->array_literal.elements);
        break;
    case NODE_MATCH:
        lsp_walk_node(idx, node->match_stmt.expr);
        lsp_walk_node(idx, node->match_stmt.cases);
        break;
    case NODE_MATCH_CASE:
        lsp_walk_node(idx, node->match_case.guard);
        lsp_walk_node(idx, node->match_case.body);
        break;
    case NODE_TERNARY:
        lsp_walk_node(idx, node->ternary.cond);
        lsp_walk_node(idx, node->ternary.true_expr);
        lsp_walk_node(idx, node->ternary.false_expr);
        break;
    case NODE_TEST:
        lsp_walk_node(idx, node->test_stmt.body);
        break;
    case NODE_ASSERT:
        lsp_walk_node(idx, node->assert_stmt.condition);
        break;
    case NODE_DEFER:
        lsp_walk_node(idx, node->defer_stmt.stmt);
        break;
    case NODE_DESTRUCT_VAR:
        lsp_walk_node(idx, node->destruct.init_expr);
        lsp_walk_node(idx, node->destruct.else_block);
        break;
    case NODE_TRY:
        lsp_walk_node(idx, node->try_stmt.expr);
        break;
    case NODE_AWAIT:
        lsp_walk_node(idx, node->unary.operand);
        break;
    case NODE_REPL_PRINT:
        lsp_walk_node(idx, node->repl_print.expr);
        break;
    default:
        break;
    }

    // Walk next sibling.
    lsp_walk_node(idx, node->next);
}

void lsp_build_index(LSPIndex *idx, ASTNode *root)
{
    lsp_walk_node(idx, root);
}
