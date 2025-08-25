#pragma once
#include "ast.hpp"
#include "lexer.hpp"
#include <vector>

#ifdef _WIN32
#ifdef LAMINA_CORE_EXPORTS
#define LAMINA_API __declspec(dllexport)
#else
#define LAMINA_API __declspec(dllimport)
#endif
#else
#define LAMINA_API
#endif

class LAMINA_API Parser {
public:
    static std::unique_ptr<ASTNode> parse(const std::vector<Token>& tokens);
    static std::unique_ptr<Expression> parse_expression(const std::vector<Token>& tokens, size_t& i);
    // 解析不同层级的表达式，处理正确的运算符优先级
    static std::unique_ptr<Expression> parse_comparison(const std::vector<Token>& tokens, size_t& i);
    static std::unique_ptr<Expression> parse_addition(const std::vector<Token>& tokens, size_t& i);
    static std::unique_ptr<Expression> parse_term(const std::vector<Token>& tokens, size_t& i);
    static std::unique_ptr<Expression> parse_power(const std::vector<Token>& tokens, size_t& i);
    static std::unique_ptr<Expression> parse_unary(const std::vector<Token>& tokens, size_t& i);
    static std::unique_ptr<Expression> parse_primary(const std::vector<Token>& tokens, size_t& i);
    // 解析一组语句，返回 BlockStmt
    static std::unique_ptr<BlockStmt> parse_block(const std::vector<Token>& tokens, size_t& i, bool is_global);
    static std::unique_ptr<BlockStmt> parse_block(const std::vector<Token>& tokens, size_t& i);// 兼容老代码
    // 解析单条语句
    static std::unique_ptr<Statement> parse_statement(const std::vector<Token>& tokens, size_t& i);
    static std::unique_ptr<Statement> parse_while(const std::vector<Token>& tokens, size_t& i);
    static std::unique_ptr<StructDeclStmt> parse_struct(const std::vector<Token>& tokens, size_t& i);
};
