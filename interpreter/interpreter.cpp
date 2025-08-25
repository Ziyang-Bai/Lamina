#include "interpreter.hpp"
#include "../extensions/standard/lstruct.hpp"
#include "bigint.hpp"
#include "lamina.hpp"
#include "lexer.hpp"
#include "module_loader.hpp"
#include "parser.hpp"

#include <cmath>
#include <cstdlib>// For std::exit
#include <cstring>// For strcmp
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#elif __linux__
#include <cstdlib>// For getenv
#include <dlfcn.h>
#include <limits.h>
#include <link.h>
#include <unistd.h>
#include <unistd.h>// For isatty
#else
#include <cstdlib>// For getenv
#include <dlfcn.h>
#include <limits.h>
#include <unistd.h>
#include <unistd.h>// For isatty
#endif


// 这些异常类已经移到了 interpreter.hpp

// Scope stack operations
void Interpreter::push_scope() {
    variable_stack.emplace_back();
}

void Interpreter::add_function(const std::string& name, FuncDefStmt* func) {
    functions[name] = func;
}

void Interpreter::save_repl_ast(std::unique_ptr<ASTNode> ast) {
    repl_asts.push_back(std::move(ast));
}

void Interpreter::pop_scope() {
    if (variable_stack.size() > 1) variable_stack.pop_back();
}

Value Interpreter::get_variable(const std::string& name) const {
    for (auto it = variable_stack.rbegin(); it != variable_stack.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return found->second;
    }

    // 如果变量找不到，检查是否是函数名
    auto func_it = functions.find(name);
    if (func_it != functions.end()) {
        // 返回一个函数类型的值（这里我们需要扩展Value类型）
        // 暂时返回一个特殊的字符串值表示函数
        return Value("__function_" + name);
    }

    RuntimeError error("Undefined variable '" + name + "'");
    error.stack_trace = get_stack_trace();
    throw error;
    return Value();// Unreachable, but suppress compiler warning
}

void Interpreter::set_variable(const std::string& name, const Value& val) {
    if (!variable_stack.empty()) {
        (*variable_stack.rbegin())[name] = val;
    }
}

void Interpreter::set_global_variable(const std::string& name, const Value& val) {
    if (!variable_stack.empty()) {
        variable_stack.front()[name] = val;
    }
}

void Interpreter::execute(const std::unique_ptr<Statement>& node) {
    if (!node) return;

    if (auto* v = dynamic_cast<VarDeclStmt*>(node.get())) {
        if (v->expr) {
            Value val = eval(v->expr.get());
            set_variable(v->name, val);
        } else {
            RuntimeError error("Variable '" + v->name + "' declaration has null expression");
            error.stack_trace = get_stack_trace();
            throw error;
        }
    } else if (auto* d = dynamic_cast<DefineStmt*>(node.get())) {
        if (!d->value) {
            error_and_exit("Null expression in define statement for '" + d->name + "'");
        }
        Value val = eval(d->value.get());
        // 删除递归限制
        /*if (d->name == "MAX_RECURSION_DEPTH" && val.is_int()) {
            int new_depth = std::get<int>(val.data);
            if (new_depth > 0 && new_depth <= 10000) {
                max_recursion_depth = new_depth;
                std::cout << "Recursion depth limit set to: " << new_depth << std::endl;
            } else {
                error_and_exit("Invalid recursion depth value: " + std::to_string(new_depth) + " (must be between 1 and 10000)");
            }
        } else {
            // 原：std::cerr << "Error: Unknown define constant: " << d->name << std::endl;
            // 已替换
        }*/
    } else if (auto* bi = dynamic_cast<BigIntDeclStmt*>(node.get())) {
        if (bi->init_value) {
            Value val = eval(bi->init_value.get());
            if (val.is_bigint()) {
                // 如果值已经是BigInt，直接使用
                set_variable(bi->name, val);
            } else if (val.is_int()) {
                // 将普通整数转换为BigInt
                ::BigInt big_val(std::get<int>(val.data));
                set_variable(bi->name, Value(big_val));
            } else if (val.is_string()) {
                // 从字符串创建BigInt
                try {
                    ::BigInt big_val(std::get<std::string>(val.data));
                    set_variable(bi->name, Value(big_val));
                } catch (const std::exception& e) {
                    error_and_exit("Invalid BigInt string '" + std::get<std::string>(val.data) + "' in declaration of " + bi->name);
                }
            } else {
                // 默认初始化为0
                error_and_exit("Cannot convert " + val.to_string() + " to BigInt in declaration of " + bi->name);
            }
        } else {
            set_variable(bi->name, Value(::BigInt(0)));
        }
    } else if (auto* a = dynamic_cast<AssignStmt*>(node.get())) {
        if (!a->expr) {
            error_and_exit("Null expression in assignment to '" + a->name + "'");
        }
        Value val = eval(a->expr.get());
        set_variable(a->name, val);
    }else if (auto* a = dynamic_cast<StructDeclStmt*>(node.get())) {
        std::vector<std::pair<std::string, Value>> struct_init_val{};
        for (const auto& [n,e]: a->init_vec) {
            auto val = eval(e.get());
            struct_init_val.emplace_back(n, val);
        }
        set_variable(a->name,new_lstruct(struct_init_val));
    } else if (auto* ifs = dynamic_cast<IfStmt*>(node.get())) {
        if (!ifs->condition) {
            error_and_exit("Null condition in if statement");
        }
        Value cond = eval(ifs->condition.get());
        bool cond_true = cond.as_bool();
        if (cond_true) {
            for (auto& stmt: ifs->thenBlock->statements) execute(stmt);
        } else if (ifs->elseBlock) {
            for (auto& stmt: ifs->elseBlock->statements) execute(stmt);
        }
    } else if (auto* ws = dynamic_cast<WhileStmt*>(node.get())) {
        if (!ws->condition) {
            RuntimeError error("Loop condition cannot be null");
            error.stack_trace = get_stack_trace();
            throw error;
        }

        // 更健壮的while循环实现

        try {
            while (true) {
                // 评估循环条件
                Value cond;
                try {
                    cond = eval(ws->condition.get());
                } catch (const std::exception& e) {
                    // 如果条件计算出错，优雅地退出循环
                    RuntimeError error("Loop condition error: " + std::string(e.what()));
                    error.stack_trace = get_stack_trace();
                    throw error;
                }

                if (!cond.as_bool()) {
                    // 条件为假，退出循环
                    break;
                }

                bool continue_encountered = false;

                // 执行循环体
                for (auto& stmt: ws->body->statements) {
                    if (continue_encountered) continue;

                    try {
                        execute(stmt);
                    } catch (const BreakException&) {
                        // 退出整个循环
                        return;
                    } catch (const ContinueException&) {
                        // 跳到下一次迭代
                        continue_encountered = true;
                    }
                }
            }
        } catch (const BreakException&) {
            // 捕获可能从嵌套块冒泡上来的break异常
            return;
        } catch (const ReturnException&) {
            // 函数返回，直接传递给上层
            throw;
        } catch (const std::exception& e) {
            // 所有其他异常都作为运行时错误处理
            RuntimeError error("Loop body execution error: " + std::string(e.what()));
            error.stack_trace = get_stack_trace();
            throw error;
        }
    } else if (auto* func = dynamic_cast<FuncDefStmt*>(node.get())) {
        functions[func->name] = func;
    } else if (auto* block = dynamic_cast<BlockStmt*>(node.get())) {
        for (auto& stmt: block->statements) execute(stmt);
    } else if (auto* ret = dynamic_cast<ReturnStmt*>(node.get())) {
        Value val = eval(ret->expr.get());
        throw ReturnException(val);
    } else if (auto* breakStmt = dynamic_cast<BreakStmt*>(node.get())) {
        (void) breakStmt;// 避免未使用变量警告
        throw BreakException();
    } else if (auto* contStmt = dynamic_cast<ContinueStmt*>(node.get())) {
        (void) contStmt;// 避免未使用变量警告
        throw ContinueException();
    } else if (auto* includeStmt = dynamic_cast<IncludeStmt*>(node.get())) {
        if (!load_module(includeStmt->module)) {
            error_and_exit("Failed to include module '" + includeStmt->module + "'");
        }
    } else if (auto* exprstmt = dynamic_cast<ExprStmt*>(node.get())) {
        if (exprstmt->expr) {
            try {
                // std::cerr << "DEBUG: Executing expression statement" << std::endl;
                Value result = eval(exprstmt->expr.get());
                // std::cerr << "DEBUG: Expression result: " << result.to_string() << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "ERROR: Exception in expression statement: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "ERROR: Unknown exception in expression statement" << std::endl;
            }
        } else {
            error_and_exit("Empty expression statement");
        }
    } else if (auto *nullstmt = dynamic_cast<NullStmt*>(node.get())) {
        (void) nullstmt;
    }
}


Value Interpreter::eval(const ASTNode* node) {
    if (!node) {
        error_and_exit("Attempted to evaluate null expression");
    }
    if (auto* lit = dynamic_cast<const LiteralExpr*>(node)) {
        if (lit->type == Value::Type::Int) {
            // Check if it contains scientific notation (e or E) or decimal point
            std::string value = lit->value;
            if (value.find('.') != std::string::npos ||
                value.find('e') != std::string::npos ||
                value.find('E') != std::string::npos) {
                // Parse as double for floating point numbers and scientific notation
                double d = std::stod(lit->value);
                return Value(d);
            } else {
                // 先尝试用 int 解析，只有溢出时才用 BigInt
                try {
                    int i = std::stoi(lit->value);
                    return Value(i);
                } catch (const std::out_of_range&) {
                    // int 溢出，使用 BigInt
                    ::BigInt big(lit->value);
                    return Value(big);
                }
            }
        } else {
            // Check for boolean literals
            if (lit->value == "true") return Value(true);
            if (lit->value == "false") return Value(false);
            if (lit->value == "null") return Value(nullptr);
            // Otherwise it's a string
            return Value(lit->value);
        }
    } else if (auto* id = dynamic_cast<const IdentifierExpr*>(node)) {
        return get_variable(id->name);
    } else if (auto* var = dynamic_cast<const VarExpr*>(node)) {
        return get_variable(var->name);
    } else if (auto* bin = dynamic_cast<const BinaryExpr*>(node)) {
        Value l = eval(bin->left.get());
        Value r = eval(bin->right.get());

        // Handle arithmetic operations
        if (bin->op == "+") {
            // String concatenation
            if (l.is_string() || r.is_string()) {
                return Value(l.to_string() + r.to_string());
            }
            // Vector addition
            else if (l.is_array() && r.is_array()) {
                return l.vector_add(r);
            }
            // 只要有一方是 Irrational 或 Symbolic，优先生成符号表达式
            else if ((l.is_irrational() || l.is_symbolic() || r.is_irrational() || r.is_symbolic()) && l.is_numeric() && r.is_numeric()) {
                std::shared_ptr<SymbolicExpr> leftExpr;
                std::shared_ptr<SymbolicExpr> rightExpr;
                if (l.is_symbolic()) {
                    leftExpr = std::get<std::shared_ptr<SymbolicExpr>>(l.data);
                } else if (l.is_irrational()) {
                    leftExpr = std::get<::Irrational>(l.data).to_symbolic();
                } else if (l.is_rational()) {
                    leftExpr = SymbolicExpr::number(std::get<::Rational>(l.data));
                } else if (l.is_bigint()) {
                    leftExpr = SymbolicExpr::number(std::get<::BigInt>(l.data));
                } else if (l.is_int()) {
                    leftExpr = SymbolicExpr::number(std::get<int>(l.data));
                } else if (l.is_float()) {
                    leftExpr = SymbolicExpr::number(::Rational::from_double(std::get<double>(l.data)));
                } else {
                    leftExpr = SymbolicExpr::number(0);
                }
                if (r.is_symbolic()) {
                    rightExpr = std::get<std::shared_ptr<SymbolicExpr>>(r.data);
                } else if (r.is_irrational()) {
                    rightExpr = std::get<::Irrational>(r.data).to_symbolic();
                } else if (r.is_rational()) {
                    rightExpr = SymbolicExpr::number(std::get<::Rational>(r.data));
                } else if (r.is_bigint()) {
                    rightExpr = SymbolicExpr::number(std::get<::BigInt>(r.data));
                } else if (r.is_int()) {
                    rightExpr = SymbolicExpr::number(std::get<int>(r.data));
                } else if (r.is_float()) {
                    rightExpr = SymbolicExpr::number(::Rational::from_double(std::get<double>(r.data)));
                } else {
                    rightExpr = SymbolicExpr::number(0);
                }
                return Value(SymbolicExpr::add(leftExpr, rightExpr));
            }
            // Numeric addition with irrational and rational number support
            else if (l.is_numeric() && r.is_numeric()) {
                // BigInt 优先：如果任一为 BigInt，结果为 BigInt
                if (l.is_bigint() || r.is_bigint()) {
                    ::BigInt lb = l.is_bigint() ? std::get<::BigInt>(l.data) : ::BigInt(l.as_number());
                    ::BigInt rb = r.is_bigint() ? std::get<::BigInt>(r.data) : ::BigInt(r.as_number());
                    return Value(lb + rb);
                }
                // If either operand is rational, use rational arithmetic
                if (l.is_rational() || r.is_rational()) {
                    ::Rational result = l.as_rational() + r.as_rational();
                    return Value(result);
                }

                double result = l.as_number() + r.as_number();// Return int if both operands are int and result is whole
                if (l.is_int() && r.is_int()) {
                    return Value(static_cast<int>(result));
                }
                return Value(result);
            } else {
                error_and_exit("Cannot add " + l.to_string() + " and " + r.to_string());
            }
        }// Arithmetic operations (require numeric operands or vector operations)
        if (bin->op == "-" || bin->op == "*" || bin->op == "/" ||
            bin->op == "%" || bin->op == "^") {

            // Special handling for multiplication
            if (bin->op == "*") {
                // Vector and matrix operations
                if (l.is_array() && r.is_array()) {
                    // Try dot product for same-size vectors
                    const auto& la = std::get<std::vector<Value>>(l.data);
                    const auto& ra = std::get<std::vector<Value>>(r.data);
                    if (la.size() == ra.size()) {
                        return l.dot_product(r);
                    }
                }
                // Matrix multiplication
                if (l.is_matrix() && r.is_matrix()) {
                    return l.matrix_multiply(r);
                }
                // Scalar multiplication for vectors
                if (l.is_array() && r.is_numeric()) {
                    return l.scalar_multiply(r.as_number());
                }
                if (l.is_numeric() && r.is_array()) {
                    return r.scalar_multiply(l.as_number());
                }
                // 只要有一方是 Irrational 或 Symbolic，优先生成符号表达式
                if ((l.is_irrational() || r.is_irrational() || l.is_symbolic() || r.is_symbolic()) && l.is_numeric() && r.is_numeric()) {
                    std::shared_ptr<SymbolicExpr> leftExpr;
                    std::shared_ptr<SymbolicExpr> rightExpr;
                    // 强制所有 Irrational 都转为 SymbolicExpr
                    if (l.is_symbolic()) {
                        leftExpr = std::get<std::shared_ptr<SymbolicExpr>>(l.data);
                    } else {
                        leftExpr = from_number_to_symbolic(l);
                    }
                    if (r.is_symbolic()) {
                        rightExpr = std::get<std::shared_ptr<SymbolicExpr>>(r.data);
                    } else {
                        rightExpr = from_number_to_symbolic(r);
                    }
                    return Value(SymbolicExpr::multiply(leftExpr, rightExpr)->simplify());
                }
                // Regular multiplication (both must be numeric)
                if (l.is_numeric() && r.is_numeric()) {
                    // BigInt 优先：如果任一为 BigInt，结果为 BigInt
                    if (l.is_bigint() || r.is_bigint()) {
                        ::BigInt lb = l.is_bigint() ? std::get<::BigInt>(l.data) : ::BigInt(l.as_number());
                        ::BigInt rb = r.is_bigint() ? std::get<::BigInt>(r.data) : ::BigInt(r.as_number());
                        return Value(lb * rb);
                    }
                    // If either operand is irrational, use irrational arithmetic
                    if (l.is_irrational() || r.is_irrational()) {
                        ::Irrational result = l.as_irrational() * r.as_irrational();
                        return Value(result);
                    }
                    // If either operand is rational, use rational arithmetic
                    if (l.is_rational() || r.is_rational()) {
                        ::Rational result = l.as_rational() * r.as_rational();
                        return Value(result);
                    }

                    double result = l.as_number() * r.as_number();
                    return (l.is_int() && r.is_int()) ? Value(static_cast<int>(result)) : Value(result);
                }
                // Error case
                error_and_exit("Cannot multiply " + l.to_string() + " and " + r.to_string());
            }

            // Special handling for minus
            if (bin->op == "-") {
                // Vector and matrix operations
                if (l.is_array() && r.is_array()) {
                    // Try minus for same-size vectors
                    const auto& la = std::get<std::vector<Value>>(l.data);
                    const auto& ra = std::get<std::vector<Value>>(r.data);
                    return l.vector_minus(r);                                // An exception can be raised inside
                }
                // Matrix multiplication
                if (l.is_matrix() && r.is_matrix()) {
                    error_and_exit("Arithmetic operation '-' requires numeric or vector operands");
                }
                // Scalar multiplication for vectors
                if ((l.is_array() && r.is_numeric()) || (l.is_numeric() && r.is_array())) {
                    error_and_exit("Arithmetic operation '-' requires same-type operands");
                }
                // 只要有一方是 Irrational 或 Symbolic，优先生成符号表达式
                if ((l.is_irrational() || r.is_irrational() || l.is_symbolic() || r.is_symbolic()) && l.is_numeric() && r.is_numeric()) {
                    std::shared_ptr<SymbolicExpr> leftExpr;
                    std::shared_ptr<SymbolicExpr> rightExpr;
                    // 强制所有 Irrational 都转为 SymbolicExpr
                    if (l.is_symbolic()) {
                        leftExpr = std::get<std::shared_ptr<SymbolicExpr>>(l.data);
                    } else {
                        leftExpr = from_number_to_symbolic(l);
                    }
                    if (r.is_symbolic()) {
                        rightExpr = std::get<std::shared_ptr<SymbolicExpr>>(r.data);
                    } else {
                        rightExpr = from_number_to_symbolic(r);
                    }
                    return Value(SymbolicExpr::multiply(leftExpr, rightExpr)->simplify());
                }
                // Regular multiplication (both must be numeric)
                if (l.is_numeric() && r.is_numeric()) {
                    // BigInt 优先：如果任一为 BigInt，结果为 BigInt
                    if (l.is_bigint() || r.is_bigint()) {
                        ::BigInt lb = l.is_bigint() ? std::get<::BigInt>(l.data) : ::BigInt(l.as_number());
                        ::BigInt rb = r.is_bigint() ? std::get<::BigInt>(r.data) : ::BigInt(r.as_number());
                        return Value(lb - rb);
                    }
                    // If either operand is irrational, use irrational arithmetic
                    if (l.is_irrational() || r.is_irrational()) {
                        ::Irrational result = l.as_irrational() - r.as_irrational();
                        return Value(result);
                    }
                    // If either operand is rational, use rational arithmetic
                    if (l.is_rational() || r.is_rational()) {
                        ::Rational result = l.as_rational() - r.as_rational();
                        return Value(result);
                    }

                    double result = l.as_number() - r.as_number();
                    return (l.is_int() && r.is_int()) ? Value(static_cast<int>(result)) : Value(result);
                }
                // Error case
                error_and_exit("Cannot multiply " + l.to_string() + " and " + r.to_string());
            }

            // Other arithmetic operations require both operands to be numeric
            if (!l.is_numeric() || !r.is_numeric()) {
                error_and_exit("Arithmetic operation '" + bin->op + "' requires numeric operands");
            }

            // For division, always use rational arithmetic for precise results
            if (bin->op == "/") {
                // 只要有一方是 Irrational 或 Symbolic，优先生成符号表达式
                if ((l.is_irrational() || l.is_symbolic() || r.is_irrational() || r.is_symbolic()) && l.is_numeric() && r.is_numeric()) {
                    std::shared_ptr<SymbolicExpr> leftExpr;
                    std::shared_ptr<SymbolicExpr> rightExpr;
                    if (l.is_symbolic()) {
                        leftExpr = std::get<std::shared_ptr<SymbolicExpr>>(l.data);
                    } else {
                        leftExpr = from_number_to_symbolic(l);
                    }
                    if (r.is_symbolic()) {
                        rightExpr = std::get<std::shared_ptr<SymbolicExpr>>(r.data);
                    } else {
                        rightExpr = from_number_to_symbolic(r);
                    }
                    auto divExpr = SymbolicExpr::multiply(leftExpr, SymbolicExpr::power(rightExpr, SymbolicExpr::number(-1)));
                    return Value(divExpr->simplify());
                }
                // BigInt 优先：如果任一为 BigInt，结果为 BigInt（如果整除）或 Rational
                if (l.is_bigint() || r.is_bigint()) {
                    ::BigInt lb = l.is_bigint() ? std::get<::BigInt>(l.data) : ::BigInt(l.as_number());
                    ::BigInt rb = r.is_bigint() ? std::get<::BigInt>(r.data) : ::BigInt(r.as_number());
                    if (rb.is_zero()) {
                        error_and_exit("Division by zero");
                    }
                    // 对于BigInt除法，如果能整除则返回BigInt，否则返回Rational
                    try {
                        ::BigInt quotient = lb / rb;
                        ::BigInt remainder = lb - (quotient * rb);
                        if (remainder.is_zero()) {
                            return Value(quotient);
                        } else {
                            // 不能整除，返回有理数
                            return Value(::Rational(lb.to_int(), rb.to_int()));
                        }
                    } catch (...) {
                        // 如果BigInt运算失败，回退到Rational
                        return Value(::Rational(lb.to_int(), rb.to_int()));
                    }
                }
                // If either operand is irrational, use irrational arithmetic
                if (l.is_irrational() || r.is_irrational()) {
                    ::Irrational lr = l.as_irrational();
                    ::Irrational rr = r.as_irrational();
                    if (rr.is_zero()) {
                        error_and_exit("Division by zero");
                    }
                    return Value(lr / rr);
                }

                ::Rational lr = l.as_rational();
                ::Rational rr = r.as_rational();
                if (rr.is_zero()) {
                    error_and_exit("Division by zero");
                }
                return Value(lr / rr);
            }

            // Use irrational arithmetic if either operand is irrational
            if (l.is_irrational() || r.is_irrational()) {
                ::Irrational lr = l.as_irrational();
                ::Irrational rr = r.as_irrational();

                if (bin->op == "-") {
                    return Value(lr - rr);
                }
                // Note: Other operations (%, ^) may fall back to double arithmetic
                // for irrational numbers as they're complex to handle exactly
            }

            // Use rational arithmetic if either operand is rational
            if (l.is_rational() || r.is_rational()) {
                ::Rational lr = l.as_rational();
                ::Rational rr = r.as_rational();

                if (bin->op == "-") {
                    return Value(lr - rr);
                }
                if (bin->op == "%") {
                    // For rational modulo, convert to double temporarily
                    double ld = lr.to_double();
                    double rd = rr.to_double();
                    if (rd == 0.0) {
                        error_and_exit("Modulo by zero");
                    }
                    return Value(static_cast<int>(ld) % static_cast<int>(rd));
                }
                if (bin->op == "^") {
                    // 只要有一方是 Irrational 或 Symbolic，优先生成符号表达式
                    std::shared_ptr<SymbolicExpr> leftExpr;
                    std::shared_ptr<SymbolicExpr> rightExpr;
                    if (l.is_symbolic()) {
                        leftExpr = std::get<std::shared_ptr<SymbolicExpr>>(l.data);
                    } else {
                        leftExpr = from_number_to_symbolic(l);
                    }
                    if (r.is_symbolic()) {
                        rightExpr = std::get<std::shared_ptr<SymbolicExpr>>(r.data);
                    } else {
                        rightExpr = from_number_to_symbolic(r);
                    }
                    auto exponentExpr = SymbolicExpr::power(leftExpr, rightExpr);
                    return Value(exponentExpr->simplify());
                }
                /*if (bin->op == "^") {
                    // For rational exponentiation, use integer exponent if possible
                    if (rr.is_integer() && rr.get_denominator() == 1) {
                        int exp = static_cast<int>(rr.get_numerator());
                        if (exp >= -1000 && exp <= 1000) {// Reasonable range
                            return Value(lr.pow(exp));
                        }
                    }
                    // Fall back to double arithmetic for non-integer or large exponents
                    return Value(std::pow(lr.to_double(), rr.to_double()));
                }*/
            }

            // BigInt 运算优先：如果任一为 BigInt，结果为 BigInt
            if (l.is_bigint() || r.is_bigint()) {
                ::BigInt lb = l.is_bigint() ? std::get<::BigInt>(l.data) : ::BigInt(l.as_number());
                ::BigInt rb = r.is_bigint() ? std::get<::BigInt>(r.data) : ::BigInt(r.as_number());

                if (bin->op == "-") {
                    return Value(lb - rb);
                }
                if (bin->op == "%") {
                    if (rb.is_zero()) {
                        error_and_exit("Modulo by zero");
                    }
                    // BigInt 模运算
                    return Value(lb % rb);
                }
                if (bin->op == "^") {
                    // BigInt 幂运算
                    return Value(lb.power(rb));
                }
            }

            // Fall back to double arithmetic
            double ld = l.as_number();
            double rd = r.as_number();

            if (bin->op == "-") {
                double result = ld - rd;
                return (l.is_int() && r.is_int()) ? Value(static_cast<int>(result)) : Value(result);
            }
            if (bin->op == "%") {
                if (rd == 0.0) {
                    // 原：std::cerr << "Error: Modulo by zero" << std::endl;
                    error_and_exit("Modulo by zero");
                }
                return Value(static_cast<int>(ld) % static_cast<int>(rd));
            }
            if (bin->op == "^") {
                return Value(std::pow(ld, rd));
            }
        }

        // Comparison operators
        if (bin->op == "==" || bin->op == "!=" || bin->op == "<" ||
            bin->op == "<=" || bin->op == ">" || bin->op == ">=") {

            // Handle different type combinations
            if (l.is_numeric() && r.is_numeric()) {
                // BigInt 比较优先
                if (l.is_bigint() || r.is_bigint()) {
                    ::BigInt lb = l.is_bigint() ? std::get<::BigInt>(l.data) : ::BigInt(l.as_number());
                    ::BigInt rb = r.is_bigint() ? std::get<::BigInt>(r.data) : ::BigInt(r.as_number());

                    // 使用字符串比较来判断大小（这是一个简化的实现）
                    std::string ls = lb.to_string();
                    std::string rs = rb.to_string();

                    if (bin->op == "==") return Value(ls == rs);
                    if (bin->op == "!=") return Value(ls != rs);

                    // 对于大小比较，需要考虑符号和长度
                    bool lb_neg = ls[0] == '-';
                    bool rb_neg = rs[0] == '-';

                    if (lb_neg && !rb_neg) {
                        // 左负右正
                        if (bin->op == "<") return Value(true);
                        if (bin->op == "<=") return Value(true);
                        if (bin->op == ">") return Value(false);
                        if (bin->op == ">=") return Value(false);
                    } else if (!lb_neg && rb_neg) {
                        // 左正右负
                        if (bin->op == "<") return Value(false);
                        if (bin->op == "<=") return Value(false);
                        if (bin->op == ">") return Value(true);
                        if (bin->op == ">=") return Value(true);
                    } else {
                        // 同号比较：比较绝对值的长度和字典序
                        std::string labs = lb_neg ? ls.substr(1) : ls;
                        std::string rabs = rb_neg ? rs.substr(1) : rs;

                        bool abs_less;
                        if (labs.length() != rabs.length()) {
                            abs_less = labs.length() < rabs.length();
                        } else {
                            abs_less = labs < rabs;
                        }

                        bool result_less = lb_neg ? !abs_less : abs_less;

                        if (bin->op == "<") return Value(result_less);
                        if (bin->op == "<=") return Value(result_less || ls == rs);
                        if (bin->op == ">") return Value(!result_less && ls != rs);
                        if (bin->op == ">=") return Value(!result_less);
                    }
                } else {
                    double ld = l.as_number();
                    double rd = r.as_number();

                    if (bin->op == "==") return Value(ld == rd);
                    if (bin->op == "!=") return Value(ld != rd);
                    if (bin->op == "<") return Value(ld < rd);
                    if (bin->op == "<=") return Value(ld <= rd);
                    if (bin->op == ">") return Value(ld > rd);
                    if (bin->op == ">=") return Value(ld >= rd);
                }
            } else if (l.is_string() && r.is_string()) {
                std::string ls = std::get<std::string>(l.data);
                std::string rs = std::get<std::string>(r.data);

                if (bin->op == "==") return Value(ls == rs);
                if (bin->op == "!=") return Value(ls != rs);
                if (bin->op == "<") return Value(ls < rs);
                if (bin->op == "<=") return Value(ls <= rs);
                if (bin->op == ">") return Value(ls > rs);
                if (bin->op == ">=") return Value(ls >= rs);
            } else if (l.is_bool() && r.is_bool()) {
                bool lb = std::get<bool>(l.data);
                bool rb = std::get<bool>(r.data);

                if (bin->op == "==") return Value(lb == rb);
                if (bin->op == "!=") return Value(lb != rb);
                // For booleans, false < true
                if (bin->op == "<") return Value(lb < rb);
                if (bin->op == "<=") return Value(lb <= rb);
                if (bin->op == ">") return Value(lb > rb);
                if (bin->op == ">=") return Value(lb >= rb);
            } else {
                // Type mismatch - only equality/inequality make sense
                if (bin->op == "==") return Value(false);// Different types are never equal
                if (bin->op == "!=") return Value(true); // Different types are always not equal

                error_and_exit("Cannot compare different types with operator '" + bin->op + "'");
                return Value();
            }
        }

        error_and_exit("Unknown binary operator '" + bin->op + "'");

    } else if (auto* unary = dynamic_cast<const UnaryExpr*>(node)) {
        Value v = eval(unary->operand.get());

        if (unary->op == "-") {
            if (v.type != Value::Type::Int && v.type != Value::Type::BigInt && v.type != Value::Type::Float) {
                RuntimeError error("Unary operator '-' requires integer, float or big integer operand");
                error.stack_trace = get_stack_trace();
                throw error;
            }
            if (v.type == Value::Type::Int) {
                int vi = std::get<int>(v.data);
                return Value(-vi);
            } else if (v.type == Value::Type::Float) {
                float vf = std::get<double>(v.data);
                return Value(-vf);
            } else {
                // For BigInt, negate directly
                ::BigInt big_val = std::get<::BigInt>(v.data);
                return Value(big_val.negate());
            }
        }

        if (unary->op == "!") {
            if (v.type != Value::Type::Int && v.type != Value::Type::BigInt) {
                RuntimeError error("Unary operator '!' requires integer or big integer operand");
                error.stack_trace = get_stack_trace();
                throw error;
            }
            int vi;
            if (v.type == Value::Type::Int) {
                vi = std::get<int>(v.data);
            } else {
                vi = std::get<::BigInt>(v.data).to_int();
            }

            if (vi < 0) {
                RuntimeError error("Cannot calculate factorial of negative number");
                error.stack_trace = get_stack_trace();
                throw error;
            }

            // Use BigInt for factorial if the number is large or result would be large
            if (vi > 20) {
                ::BigInt result(1);
                for (int j = 2; j <= vi; ++j) {
                    result = result * ::BigInt(j);
                }
                return Value(result);
            } else {
                // Use regular int for small factorials
                int res = 1;
                for (int j = 1; j <= vi; ++j) res *= j;
                return Value(res);
            }
        }

        std::cerr << "Error: Unknown unary operator '" << unary->op << "'" << std::endl;
        return Value("<unknown op>");
    }// Support function calls
    else if (auto* call = dynamic_cast<const CallExpr*>(node)) {
        std::string actual_callee = call->callee;
        //        std::cout << "DEBUG: Call expression with callee: '" << actual_callee << "'" << std::endl;

        // 检查调用的名称是否是一个参数，如果是，获取其实际值
        try {
            Value callee_value = get_variable(actual_callee);
            if (callee_value.is_string() && std::get<std::string>(callee_value.data).substr(0, 11) == "__function_") {
                // 这是一个函数参数，提取实际的函数名
                actual_callee = std::get<std::string>(callee_value.data).substr(11);
                //   std::cout << "DEBUG: Function parameter resolved to: '" << actual_callee << "'" << std::endl;
            }
        } catch (const RuntimeError&) {
            // 如果不是变量，保持原名称
            // std::cout << "DEBUG: Not a variable or parameter, using direct name: '" << actual_callee << "'" << std::endl;
        }

        // Check builtin functions first
        //   std::cout << "DEBUG: Looking for builtin function: '" << actual_callee << "'" << std::endl;
        //   std::cout << "DEBUG: Available builtin functions:" << std::endl;
        // for (const auto& pair : builtin_functions) {
        //     std::cout << "  - '" << pair.first << "'" << std::endl;
        // }

        auto builtin_it = builtin_functions.find(actual_callee);
        if (builtin_it != builtin_functions.end()) {
            //     std::cout << "DEBUG: Found builtin function: '" << actual_callee << "'" << std::endl;

            // 检查是否是模块函数
            bool is_module_func = actual_callee.find(".") != std::string::npos;
            if (is_module_func) {
                //       std::cout << "DEBUG: This is a module function with dot notation" << std::endl;
            }

            // Handle builtin call with stack frame and unified error handling
            push_frame(actual_callee, "<builtin>", 0);

            std::vector<Value> args;
            for (const auto& arg: call->args) {
                if (!arg) {
                    pop_frame();
                    RuntimeError err("Null argument in call to builtin function '" + actual_callee + "'");
                    err.stack_trace = get_stack_trace();
                    throw err;
                }
                args.push_back(eval(arg.get()));
            }

            Value result;
            try {
                result = builtin_it->second(args);
            } catch (...) {
                pop_frame();
                throw;
            }

            pop_frame();
            return result;
        }

        // Check user-defined functions
        auto it = functions.find(actual_callee);
        if (it != functions.end()) {
            FuncDefStmt* func = it->second;
            if (!func) {
                std::cerr << "Error: Function object for '" << actual_callee << "' is null" << std::endl;
                return Value("<func error>");
            }

            // Check recursion depth
            if (recursion_depth >= max_recursion_depth) {
                RuntimeError error("Maximum recursion depth exceeded (" + std::to_string(max_recursion_depth) + ")");
                error.stack_trace = get_stack_trace();
                throw error;
            }

            recursion_depth++;
            push_frame(actual_callee, "<script>", 0);// Add to call stack

            // Check parameter count
            if (call->args.size() > func->params.size()) {
                Interpreter::print_warning("Too many arguments provided to function '" + actual_callee +
                                                   "'. Expected " + std::to_string(func->params.size()) +
                                                   ", got " + std::to_string(call->args.size()),
                                           true);
            }

            // Calculate arguments
            std::vector<Value> arg_values(func->params.size());
            for (size_t j = 0; j < func->params.size(); ++j) {
                if (j < call->args.size()) {
                    if (call->args[j]) {
                        arg_values[j] = eval(call->args[j].get());
                    } else {
                        Interpreter::print_error("Null argument " + std::to_string(j + 1) + " in call to function '" + actual_callee + "'", true);
                        arg_values[j] = Value("<null arg>");
                    }
                } else {
                    Interpreter::print_warning("Missing argument '" + func->params[j] + "' in call to function '" + actual_callee + "'", true);
                    arg_values[j] = Value("<undefined>");
                }
            }

            push_scope();// Create scope here

            // Pass arguments
            for (size_t j = 0; j < func->params.size(); ++j) {
                set_variable(func->params[j], arg_values[j]);
            }

            // Execute function body, capture return
            try {
                for (const auto& stmt: func->body->statements) {
                    try {
                        execute(stmt);
                    } catch (const ReturnException& re) {
                        // Normal return handling
                        pop_frame();
                        pop_scope();
                        recursion_depth--;
                        return re.value;
                    } catch (const RuntimeError& re) {
                        // If the error doesn't have a stack trace yet, capture current state
                        RuntimeError enriched(re.message);
                        if (re.stack_trace.empty()) {
                            enriched.stack_trace = get_stack_trace();
                        } else {
                            enriched.stack_trace = re.stack_trace;// Preserve existing trace
                        }
                        pop_frame();
                        pop_scope();
                        recursion_depth--;
                        throw enriched;
                    } catch (const std::exception& e) {
                        // Wrap standard exception as RuntimeError
                        RuntimeError enriched("In function '" + actual_callee + "': " + std::string(e.what()));
                        enriched.stack_trace = get_stack_trace();// Get stack trace before cleanup
                        pop_frame();
                        pop_scope();
                        recursion_depth--;
                        throw enriched;
                    }
                }
            } catch (const ReturnException& re) {
                // Ensure cleanup in any case
                pop_frame();
                pop_scope();
                recursion_depth--;
                return re.value;
            }

            pop_frame();
            pop_scope();
            recursion_depth--;
            return Value();// Default value when no return
        }

        // Check if it's a module function before reporting undefined
        bool is_module_func = actual_callee.find(".") != std::string::npos;
        if (is_module_func) {
            // Prepare arguments for module function call
            std::vector<Value> args;
            for (const auto& arg: call->args) {
                if (!arg) {
                    std::cerr << "Error: Null argument in call to module function '" << actual_callee << "'" << std::endl;
                    args.push_back(Value());
                } else {
                    args.push_back(eval(arg.get()));
                }
            }

            // Try to call the module function
            Value result = call_module_function(actual_callee, args);
            if (result.to_string() != "null") {// 检查是否成功调用
                return result;
            }
            // If module function call failed, fall through to undefined function error
        }

        std::cerr << "Error: Call to undefined function '" << actual_callee << "'" << std::endl;
        return Value("<undefined function>");
    } else if (auto* arr = dynamic_cast<const ArrayExpr*>(node)) {
        std::vector<Value> elements;
        for (const auto& element: arr->elements) {
            if (element) {
                elements.push_back(eval(element.get()));
            } else {
                std::cerr << "Error: Null element in array literal" << std::endl;
                return Value();
            }
        }
        return Value(elements);
    }
    std::cerr << "Error: Unsupported expression type" << std::endl;
    return Value("<type error>");
}

// Load and execute module
bool Interpreter::load_module(const std::string& module_name) {
    // Check if already loaded to avoid circular imports
    if (loaded_modules.find(module_name) != loaded_modules.end()) {
        return true;// Already loaded
    }
    // 立即插入，防止递归 include
    loaded_modules.insert(module_name);

    bool is_shared_lib = false;
    std::string clean_name = module_name;
    if (clean_name.rfind("lib", 0) == 0) {
        clean_name = clean_name.substr(3);
    }

// 平台相关动态库扩展名
#if defined(_WIN32)
    const std::string lib_ext = ".dll";
#elif defined(__APPLE__)
    const std::string lib_ext = ".dylib";
#else
    const std::string lib_ext = ".so";
#endif

    // Handle built-in hidden library "splash"
    if (module_name == "splash") {
        // Output ASCII art logo
        std::cout << "   __                    _            \n";
        std::cout << "  / /   ____ _____ ___  (_)___  ____ _\n";
        std::cout << " / /   / __ `/ __ `__ \\/ / __ \\/ __ `/\n";
        std::cout << "/ /___/ /_/ / / / / / / / / / / /_/ / \n";
        std::cout << "/_____/\\__,_/_/ /_/ /_/_/_/ /_/\\__,_/  \n";
        std::cout << "                                       \n";

        // Mark as loaded to avoid multiple outputs
        loaded_modules.insert(module_name);
        return true;
    }

    // Handle built-in hidden library "them" (credits)
    if (module_name == "them") {
        // Output credits and developer information
        std::cout << "Credits\n";
        std::cout << "Lamina Interpreter\n";
        std::cout << "Developed by Ziyang-bai\n";
        std::cout << "Helper: Ange1PLSGreet\n";
        std::cout << "Special thanks to all contributors and users!\n";
        std::cout << "For more information, visit: https://github.com/Ziyang-bai/Lamina\n";
        std::cout << "This interpreter is open source and welcomes contributions.\n";
        std::cout << "Designed by Ziyang-Bai\n";
        std::cout << "\n";
        // Mark as loaded to avoid multiple outputs
        loaded_modules.insert(module_name);
        return true;
    }

    // Build possible file paths
    std::string filename = module_name;
    bool has_lm = filename.find(".lm") != std::string::npos;
    bool has_lib = filename.find(lib_ext) != std::string::npos;

    // Try different paths: current directory, examples directory, etc.
    std::vector<std::string> search_paths = {"", "./", "include/", "extensions/"};
    std::string full_path;

    // 如果指定了动态库扩展名，直接尝试加载动态库
    if (has_lib) {
        for (const auto& path: search_paths) {
            full_path = path + filename;
            std::ifstream testfile(full_path, std::ios::binary);
            if (testfile) {
                testfile.close();
                auto loader = std::make_unique<ModuleLoader>(full_path);
                if (loader->isLoaded()) {
                    if (loader->registerToInterpreter(*this)) {
                        // 保存模块加载器实例
                        module_loaders.push_back(std::move(loader));
                        loaded_modules.insert(module_name);
                        return true;
                    } else {
                        std::cerr << "Failed to register module: " << full_path << std::endl;
                    }
                } else {
                    std::cerr << "Failed to load shared library: " << full_path << std::endl;
                }
            }
        }
        // 如果指定了.dll/.so/.dylib但没找到，直接返回失败
        std::cerr << "Error: Cannot load module '" << module_name << "'" << std::endl;
        return false;
    }

    // 如果没有指定扩展名，默认添加.lm
    if (!has_lm && !has_lib) {
        filename += ".lm";
    }

    std::ifstream file;
    for (const auto& path: search_paths) {
        full_path = path + filename;
        file.open(full_path);
        if (file) break;
    }

    // 如果没找到脚本文件，尝试查找动态库（lib前缀和无前缀，自动适配扩展名）
    if (!file) {
        std::vector<std::string> lib_names = {
                "lib" + clean_name + lib_ext,
                clean_name + lib_ext};
        is_shared_lib = true;
        bool found = false;
        for (const auto& libfile: lib_names) {
            for (const auto& path: search_paths) {
                full_path = path + libfile;
                std::ifstream testfile(full_path, std::ios::binary);
                if (testfile) {
                    testfile.close();
                    auto loader = std::make_unique<ModuleLoader>(full_path);
                    if (loader->isLoaded()) {
                        if (loader->registerToInterpreter(*this)) {
                            // 保存模块加载器实例
                            module_loaders.push_back(std::move(loader));
                            loaded_modules.insert(module_name);
                            found = true;
                            break;
                        } else {
                            std::cerr << "Failed to register module: " << full_path << std::endl;
                        }
                    } else {
                        std::cerr << "Failed to load shared library: " << full_path << std::endl;
                    }
                }
            }
            if (found) break;
        }
        if (!found) {
            std::cerr << "Error: Cannot load module '" << module_name << "'\n";
            std::cerr << "  Searched in: ";
            for (const auto& libfile: lib_names) {
                for (const auto& path: search_paths) {
                    std::cerr << path + libfile << " ";
                }
            }
            std::cerr << "\n";
            return false;
        }
        return true;
    }


    // Read file into string
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    file.close();


    // Lexical and syntax analysis
    auto tokens = Lexer::tokenize(source);
    auto ast = Parser::parse(tokens);

    if (!ast) {
        std::cerr << "Error: Failed to parse module '" << module_name << "'" << std::endl;
        loaded_modules.erase(module_name);// Remove from loaded modules on failure
        return false;
    }// Execute module code (should be a block statement)
    auto* block = dynamic_cast<BlockStmt*>(ast.get());
    if (block) {
        // Execute module code in the current scope (not a new scope)
        // This allows variables and functions to be accessible after inclusion
        try {
            for (auto& stmt: block->statements) {
                execute(stmt);
            }
        } catch (const std::exception& e) {
            std::cerr << "Error: Exception while executing module '" << module_name << "': " << e.what() << std::endl;
            loaded_modules.erase(module_name);// 失败时移除，防止死锁
            return false;
        }

        // Store the AST to keep function pointers valid
        loaded_module_asts.push_back(std::move(ast));
        return true;
    }
    std::cerr << "Error: Module '" << module_name << "' does not contain valid code" << std::endl;
    return false;
}

// 使用函数内的静态变量来避免DLL导出/导入问题
static std::vector<Interpreter::EntryFunction>& get_entry_functions() {
    static std::vector<Interpreter::EntryFunction> entry_functions;
    return entry_functions;
}

void Interpreter::register_entry(EntryFunction func) {
    get_entry_functions().push_back(func);
}

// Register builtin mathematical functions
void Interpreter::register_builtin_functions() {
    for (auto entry: get_entry_functions()) {
        entry(*this);
    }
}

// 将Number转为Symbolic(如果可能)
std::shared_ptr<SymbolicExpr> Interpreter::from_number_to_symbolic(const Value& v) {
    std::shared_ptr<SymbolicExpr> expr;
    if (v.is_irrational()) {
        expr = std::get<::Irrational>(v.data).to_symbolic();
    } else if (v.is_rational()) {
        expr = SymbolicExpr::number(std::get<::Rational>(v.data));
    } else if (v.is_bigint()) {
        expr = SymbolicExpr::number(std::get<::BigInt>(v.data));
    } else if (v.is_int()) {
        expr = SymbolicExpr::number(std::get<int>(v.data));
    } else if (v.is_float()) {
        expr = SymbolicExpr::number(::Rational::from_double(std::get<double>(v.data)));
    } else {
        expr = SymbolicExpr::number(0);// 失败，返回0
    }

    return expr;
}


// 在文件顶端添加错误处理函数
LAMINA_EXPORT void error_and_exit(const std::string& msg) {
    std::cerr << "Error: " << msg << std::endl;
    std::exit(EXIT_FAILURE);
}

// 添加一个新的错误报告函数，只打印错误但不终止程序
// static void report_error(const std::string& msg) {
//     std::cerr << "Error: " << msg << std::endl;
// }

// 打印当前所有变量
void Interpreter::printVariables() const {
    if (variable_stack.empty()) {
        std::cout << "No variables defined." << std::endl;
        return;
    }

    std::cout << "\nCurrent variable list:" << std::endl;
    std::cout << "--------------------" << std::endl;

    // 从最内层作用域开始打印
    bool hasVars = false;
    for (auto it = variable_stack.rbegin(); it != variable_stack.rend(); ++it) {
        const auto& scope = *it;
        for (const auto& [name, value]: scope) {
            std::cout << name << " = " << value.to_string() << std::endl;
            hasVars = true;
        }
    }

    if (!hasVars) {
        std::cout << "No variables defined." << std::endl;
    }

    // 打印函数列表
    if (!functions.empty()) {
        std::cout << "\nDefined functions:" << std::endl;
        std::cout << "------------------" << std::endl;
        for (const auto& [name, _]: functions) {
            std::cout << name << "()" << std::endl;
        }
    }
}

// Stack trace management functions
void Interpreter::push_frame(const std::string& function_name, const std::string& file_name, int line_number) {
    call_stack.emplace_back(function_name, file_name, line_number);
}

void Interpreter::pop_frame() {
    if (!call_stack.empty()) {
        call_stack.pop_back();
    }
}

std::vector<StackFrame> Interpreter::get_stack_trace() const {
    return call_stack;
}

void Interpreter::print_stack_trace(const RuntimeError& error, bool use_colors) const {
    bool colors_enabled = supports_colors() && use_colors;

    // Use stack trace from error if available, otherwise use current call stack
    const auto& trace = error.stack_trace.empty() ? call_stack : error.stack_trace;

    // Print error header only if we have stack frames to show
    if (!trace.empty()) {
        if (colors_enabled) {
            std::cerr << "\033[1;31mTraceback (most recent call last):\033[0m\n";
        } else {
            std::cerr << "Traceback (most recent call last):\n";
        }

        // Print stack frames
        for (const auto& frame: trace) {
            if (colors_enabled) {
                std::cerr << "  File \"\033[1;34m" << frame.file_name << "\033[0m\", line "
                          << frame.line_number << ", in \033[1;33m" << frame.function_name << "\033[0m\n";
            } else {
                std::cerr << "  File \"" << frame.file_name << "\", line "
                          << frame.line_number << ", in " << frame.function_name << "\n";
            }
        }
    }

    // Print error message
    if (colors_enabled) {
        std::cerr << "\033[1;31mRuntimeError: " << error.message << "\033[0m\n";
    } else {
        std::cerr << "RuntimeError: " << error.message << "\n";
    }
}

bool Interpreter::supports_colors() {
    static bool checked = false;
    static bool colors_supported = false;

    if (!checked) {
        checked = true;

        // Check for NO_COLOR environment variable (universal standard)
        if (getenv("NO_COLOR") != nullptr) {
            colors_supported = false;
            return colors_supported;
        }

#ifdef _WIN32
        // On Windows, check if we're in a modern terminal that supports ANSI escape codes
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode;
        if (GetConsoleMode(hConsole, &mode)) {
            // Check if ENABLE_VIRTUAL_TERMINAL_PROCESSING is available (Windows 10+)
            colors_supported = (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
            if (!colors_supported) {
                // Try to enable it
                SetConsoleMode(hConsole, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
                if (GetConsoleMode(hConsole, &mode)) {
                    colors_supported = (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
                }
            }
        }

        // Also check if output is redirected
        if (colors_supported && _isatty(_fileno(stderr)) == 0) {
            colors_supported = false;// Output is redirected, don't use colors
        }
#else
        // On Unix-like systems, check if stderr is a terminal and TERM is set
        colors_supported = isatty(STDERR_FILENO) && getenv("TERM") != nullptr;

        // Additional check for specific terminal types that don't support colors
        const char* term = getenv("TERM");
        if (term && (strcmp(term, "dumb") == 0 || strcmp(term, "unknown") == 0)) {
            colors_supported = false;
        }
#endif
    }

    return colors_supported;
}

void Interpreter::print_error(const std::string& message, bool use_colors) {
    bool colors_enabled = supports_colors() && use_colors;

    if (colors_enabled) {
        std::cerr << "\033[1;31mError: " << message << "\033[0m\n";
    } else {
        std::cerr << "Error: " << message << "\n";
    }
}

// Call module function implementation
Value Interpreter::call_module_function(const std::string& func_name, const std::vector<Value>& args) {
    // Try each loaded module
    for (auto& loader: module_loaders) {
        if (loader && loader->isLoaded()) {
            // Check if this module has the function (by namespace)
            size_t dot_pos = func_name.find(".");
            if (dot_pos != std::string::npos) {
                std::string ns = func_name.substr(0, dot_pos);
                const auto* module_info = loader->getModuleInfo();
                if (module_info && std::string(module_info->namespace_name) == ns) {
                    return loader->callModuleFunction(func_name, args);
                }
            }
        }
    }

    std::cerr << "ERROR: Module function '" << func_name << "' not found in any loaded module" << std::endl;
    return Value();// null value
}

void Interpreter::print_warning(const std::string& message, bool use_colors) {
    bool colors_enabled = supports_colors() && use_colors;

    if (colors_enabled) {
        std::cerr << "\033[1;33mWarning: " << message << "\033[0m\n";
    } else {
        std::cerr << "Warning: " << message << "\n";
    }
}
