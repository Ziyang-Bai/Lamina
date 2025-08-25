#include "lexer.hpp"
#include <cctype>
#include <iostream>
#include <map>

static std::map<std::string, TokenType> keywords;
static bool keywords_registered = false;

void registerKeywords() {
    if (keywords_registered) return;
    keywords["var"] = TokenType::Var;
    keywords["func"] = TokenType::Func;
    keywords["if"] = TokenType::If;
    keywords["else"] = TokenType::Else;
    keywords["while"] = TokenType::While;
    keywords["for"] = TokenType::For;
    keywords["return"] = TokenType::Return;
    keywords["include"] = TokenType::Include;
    keywords["break"] = TokenType::Break;
    keywords["continue"] = TokenType::Continue;
    keywords["define"] = TokenType::Define;
    keywords["bigint"] = TokenType::Bigint;
    keywords["struct"] = TokenType::Struct;
    keywords["true"] = TokenType::True;
    keywords["false"] = TokenType::False;
    keywords["null"] = TokenType::Null;
    keywords_registered = true;
}

std::vector<Token> Lexer::tokenize(const std::string& src) {
    std::vector<Token> tokens;
    size_t i = 0;
    int line = 1, col = 1;
    registerKeywords();
    // Clear log for debugging
    // Debug: std::cerr << "Starting tokenization of " << src.length() << " characters" << std::endl;
    while (i < src.size()) {
        if (src[i] == '\n') {
            ++line;
            col = 1;
            ++i;
            continue;
        }
        if (isspace(src[i])) {
            ++col;
            ++i;
            continue;
        }
        size_t start_col = col;

        if (isalpha(src[i]) || src[i] == '_') {
            size_t j = i + 1;
            while (j < src.size() && (isalnum(src[j]) || src[j] == '_')) ++j;
            std::string ident = src.substr(i, j - i);
            TokenType type = TokenType::Identifier;
            if (keywords.count(ident)) type = keywords[ident];
            tokens.emplace_back(type, ident, line, start_col);
            col += (j - i);
            i = j;
        } else if (src[i] == '=' && i + 1 < src.size() && src[i + 1] == '=') {
            tokens.emplace_back(TokenType::Equal, "==", line, start_col);
            i += 2;
            col += 2;
        } else if (src[i] == '!' && i + 1 < src.size() && src[i + 1] == '=') {
            tokens.emplace_back(TokenType::NotEqual, "!=", line, start_col);
            i += 2;
            col += 2;
        } else if (src[i] == '<' && i + 1 < src.size() && src[i + 1] == '=') {
            tokens.emplace_back(TokenType::LessEqual, "<=", line, start_col);
            i += 2;
            col += 2;
        } else if (src[i] == '>' && i + 1 < src.size() && src[i + 1] == '=') {
            tokens.emplace_back(TokenType::GreaterEqual, ">=", line, start_col);
            i += 2;
            col += 2;
        } else if (src[i] == '<') {
            tokens.emplace_back(TokenType::Less, "<", line, start_col);
            ++i;
            ++col;
        } else if (src[i] == '>') {
            tokens.emplace_back(TokenType::Greater, ">", line, start_col);
            ++i;
            ++col;
        } else if (src[i] == '=') {
            tokens.emplace_back(TokenType::Assign, "=", line, start_col);
            ++i;
            ++col;
        } else if (src[i] == '>') {
            tokens.emplace_back(TokenType::Greater, ">", line, start_col);
            ++i;
            ++col;
        } else if (isdigit(src[i]) || (src[i] == '.' && i + 1 < src.size() && isdigit(src[i + 1]))) {
            size_t j = i;
            bool has_dot = false;

            // Handle decimal numbers including scientific notation
            // Parse mantissa (before 'e' or 'E')
            while (j < src.size() && (isdigit(src[j]) || (src[j] == '.' && !has_dot))) {
                if (src[j] == '.') has_dot = true;
                ++j;
            }

            // Check for scientific notation (e or E)
            if (j < src.size() && (src[j] == 'e' || src[j] == 'E')) {
                size_t e_pos = j;
                ++j;// Skip 'e' or 'E'
                // Check for optional + or - in exponent
                if (j < src.size() && (src[j] == '+' || src[j] == '-')) {
                    ++j;// Skip sign
                }
                // Parse exponent digits
                if (j < src.size() && isdigit(src[j])) {
                    while (j < src.size() && isdigit(src[j])) {
                        ++j;
                    }
                } else {
                    // Invalid scientific notation - backtrack to before 'e'
                    // This handles cases like "7e" without digits after
                    j = e_pos;
                }
            }

            tokens.emplace_back(TokenType::Number, src.substr(i, j - i), line, start_col);
            col += (j - i);
            i = j;
        } else if (src[i] == '(') {
            tokens.emplace_back(TokenType::LParen, "(", line, start_col);
            ++i;
            ++col;
        } else if (src[i] == ')') {
            tokens.emplace_back(TokenType::RParen, ")", line, start_col);
            ++i;
            ++col;
        } else if (src[i] == ';') {
            tokens.emplace_back(TokenType::Semicolon, ";", line, start_col);
            ++i;
            ++col;
        } else if (src[i] == '"' || src[i] == '\'') {
            char quote_type = src[i];
            size_t j = i + 1;
            std::string str_content;

            while (j < src.size() && src[j] != quote_type) {
                if (src[j] == '\\' && j + 1 < src.size()) {
                    // Handle escape sequences
                    char next = src[j + 1];
                    switch (next) {
                        case 'n':
                            str_content += '\n';
                            break;
                        case 't':
                            str_content += '\t';
                            break;
                        case 'r':
                            str_content += '\r';
                            break;
                        case '\\':
                            str_content += '\\';
                            break;
                        case '"':
                            str_content += '"';
                            break;
                        case '\'':
                            str_content += '\'';
                            break;
                        default:
                            str_content += '\\';
                            str_content += next;
                            break;
                    }
                    j += 2;
                } else {
                    str_content += src[j];
                    j++;
                }
            }

            if (j >= src.size()) {// Unterminated string
                std::cerr << "Error: Unterminated string literal at line " << line << std::endl;
                i = src.size();// Stop tokenizing
            } else {
                tokens.emplace_back(TokenType::String, str_content, line, start_col);
                col += (j - i + 1);
                i = j + 1;
            }
        } else if (src[i] == '+') {
            tokens.emplace_back(TokenType::Plus, "+", line, start_col);
            ++i;
            ++col;
        } else if (src[i] == '-') {
            tokens.emplace_back(TokenType::Minus, "-", line, start_col);
            ++i;
            ++col;
        } else if (src[i] == '*') {
            tokens.emplace_back(TokenType::Star, "*", line, start_col);
            ++i;
            ++col;
        } else if (src[i] == '/' && i + 1 < src.size() && src[i + 1] == '/') {
            // Single line comment with //
            size_t j = i + 2;
            while (j < src.size() && src[j] != '\n') ++j;
            i = j;
            continue;
        } else if (src[i] == '/' && i + 1 < src.size() && src[i + 1] == '*') {
            // Block comment with /* */
            size_t j = i + 2;
            while (j + 1 < src.size()) {
                if (src[j] == '*' && src[j + 1] == '/') {
                    j += 2;
                    break;
                }
                if (src[j] == '\n') {
                    line++;
                    col = 1;
                } else {
                    col++;
                }
                j++;
            }
            i = j;
            continue;
        } else if (src[i] == '/') {
            tokens.emplace_back(TokenType::Slash, "/", line, start_col);
            ++i;
            ++col;
        } else if (src[i] == '%') {
            tokens.emplace_back(TokenType::Percent, "%", line, start_col);
            ++i;
            ++col;
        } else if (src[i] == '^') {
            tokens.emplace_back(TokenType::Caret, "^", line, start_col);
            ++i;
            ++col;
        } else if (src[i] == '!') {
            tokens.emplace_back(TokenType::Bang, "!", line, start_col);
            ++i;
            ++col;
        } else if (src[i] == '{') {
            tokens.emplace_back(TokenType::LBrace, "{", line, start_col);
            ++i;
            ++col;
        } else if (src[i] == '}') {
            tokens.emplace_back(TokenType::RBrace, "}", line, start_col);
            ++i;
            ++col;
        } else if (src[i] == '[') {
            tokens.emplace_back(TokenType::LBracket, "[", line, start_col);
            ++i;
            ++col;
        } else if (src[i] == ']') {
            tokens.emplace_back(TokenType::RBracket, "]", line, start_col);
            ++i;
            ++col;
        } else if (src[i] == ',') {
            tokens.emplace_back(TokenType::Comma, ",", line, start_col);
            ++i;
            ++col;
        } else if (src[i] == '.') {
            tokens.emplace_back(TokenType::Dot, ".", line, start_col);
            ++i;
            ++col;
        } else {
            tokens.emplace_back(TokenType::Unknown, std::string(1, src[i]), line, start_col);
            ++i;
            ++col;
        }
    }
    tokens.emplace_back(TokenType::EndOfFile, "", line, col);
    return tokens;
}
