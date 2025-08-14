#include "symbolic.hpp"
#include <algorithm>
#include <cmath>

// 符号表达式的化简实现
std::shared_ptr<SymbolicExpr> SymbolicExpr::simplify() const {
    switch (type) {
        case Type::Number:
        case Type::Variable:
            return std::make_shared<SymbolicExpr>(*this);
            
        case Type::Sqrt:
            return simplify_sqrt();
            
        case Type::Multiply:
            return simplify_multiply();
            
        case Type::Add:
            return simplify_add();
            
        default:
            return std::make_shared<SymbolicExpr>(*this);
    }
}

std::shared_ptr<SymbolicExpr> SymbolicExpr::simplify_sqrt() const {
    if (operands.empty()) return std::make_shared<SymbolicExpr>(*this);
    
    auto simplified_operand = operands[0]->simplify();
    
    // 如果操作数是数字，尝试化简
    if (simplified_operand->is_number()) {
        auto num_val = simplified_operand->get_number();
        
        // 处理不同的数字类型
        if (std::holds_alternative<int>(num_val)) {
            int n = std::get<int>(num_val);
            if (n < 0) {
                throw std::runtime_error("Square root of negative number");
            }
            if (n == 0 || n == 1) {
                return SymbolicExpr::number(n);
            }
            
            // 检查是否为完全平方数
            int sqrt_n = static_cast<int>(std::sqrt(n));
            if (sqrt_n * sqrt_n == n) {
                return SymbolicExpr::number(sqrt_n);
            }
            
            // 提取完全平方因子
            int factor = 1;
            int remaining = n;
            for (int i = 2; i * i <= remaining; ++i) {
                while (remaining % (i * i) == 0) {
                    factor *= i;
                    remaining /= (i * i);
                }
            }
            
            if (factor > 1) {
                if (remaining == 1) {
                    return SymbolicExpr::number(factor);
                } else {
                    // 返回 factor * √remaining
                    return SymbolicExpr::multiply(
                        SymbolicExpr::number(factor),
                        SymbolicExpr::sqrt(SymbolicExpr::number(remaining))
                    );
                }
            }
        }
        
        // 处理BigInt
        if (std::holds_alternative<::BigInt>(num_val)) {
            const auto& bi = std::get<::BigInt>(num_val);
            if (bi.is_perfect_square()) {
                return SymbolicExpr::number(bi.sqrt());
            }
            // 对于非完全平方的BigInt，保持符号形式
        }
    }
    
    // 无法进一步化简，返回 √(simplified_operand)
    return SymbolicExpr::sqrt(simplified_operand);
}

std::shared_ptr<SymbolicExpr> SymbolicExpr::simplify_multiply() const {
    if (operands.size() != 2) return std::make_shared<SymbolicExpr>(*this);
    
    auto left = operands[0]->simplify();
    auto right = operands[1]->simplify();
    
    // 如果两个操作数都是数字，直接相乘
    if (left->is_number() && right->is_number()) {
        auto left_num = left->get_number();
        auto right_num = right->get_number();
        
        // 简化：只处理整数乘法
        if (std::holds_alternative<int>(left_num) && std::holds_alternative<int>(right_num)) {
            int result = std::get<int>(left_num) * std::get<int>(right_num);
            return SymbolicExpr::number(result);
        }
    }
    
    return SymbolicExpr::multiply(left, right);
}

std::shared_ptr<SymbolicExpr> SymbolicExpr::simplify_add() const {
    if (operands.size() != 2) return std::make_shared<SymbolicExpr>(*this);
    
    auto left = operands[0]->simplify();
    auto right = operands[1]->simplify();
    
    // 如果两个操作数都是数字，直接相加
    if (left->is_number() && right->is_number()) {
        auto left_num = left->get_number();
        auto right_num = right->get_number();
        
        // 简化：只处理整数加法
        if (std::holds_alternative<int>(left_num) && std::holds_alternative<int>(right_num)) {
            int result = std::get<int>(left_num) + std::get<int>(right_num);
            return SymbolicExpr::number(result);
        }
    }
    
    return SymbolicExpr::add(left, right);
}

std::string SymbolicExpr::to_string() const {
    switch (type) {
        case Type::Number:
            if (std::holds_alternative<int>(number_value)) {
                return std::to_string(std::get<int>(number_value));
            } else if (std::holds_alternative<::BigInt>(number_value)) {
                return std::get<::BigInt>(number_value).to_string();
            } else if (std::holds_alternative<::Rational>(number_value)) {
                return std::get<::Rational>(number_value).to_string();
            }
            return "0";
            
        case Type::Variable:
            return identifier;
            
        case Type::Sqrt:
            if (operands.empty()) return "√()";
            return "√" + operands[0]->to_string();
            
        case Type::Multiply:
            if (operands.size() < 2) return "*(?)";
            // 特殊处理：如果左操作数是数字且右操作数是平方根，使用紧凑形式
            if (operands[0]->is_number() && operands[1]->type == Type::Sqrt) {
                return operands[0]->to_string() + operands[1]->to_string();
            }
            return operands[0]->to_string() + "*" + operands[1]->to_string();
            
        case Type::Add:
            if (operands.size() < 2) return "+(?)";
            return operands[0]->to_string() + "+" + operands[1]->to_string();
            
        case Type::Power:
            if (operands.size() < 2) return "^(?)";
            return operands[0]->to_string() + "^" + operands[1]->to_string();
            
        default:
            return "Unknown";
    }
}

double SymbolicExpr::to_double() const {
    switch (type) {
        case Type::Number:
            if (std::holds_alternative<int>(number_value)) {
                return static_cast<double>(std::get<int>(number_value));
            } else if (std::holds_alternative<::BigInt>(number_value)) {
                return std::get<::BigInt>(number_value).to_double();
            } else if (std::holds_alternative<::Rational>(number_value)) {
                return std::get<::Rational>(number_value).to_double();
            }
            return 0.0;
            
        case Type::Sqrt:
            if (!operands.empty()) {
                return std::sqrt(operands[0]->to_double());
            }
            return 0.0;
            
        case Type::Multiply:
            if (operands.size() >= 2) {
                return operands[0]->to_double() * operands[1]->to_double();
            }
            return 0.0;
            
        default:
            return 0.0;
    }
}
