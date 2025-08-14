#pragma once
#include <algorithm>
#include <climits>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

class BigInt {
private:
    std::vector<int> digits;// 存储数字，低位在前
    bool negative;

public:
    // 友元类声明
    friend class Fraction;
    // 构造函数
    BigInt() : negative(false) { digits.push_back(0); }

    BigInt(int n) : negative(n < 0) {
        if (n == 0) {
            digits.push_back(0);
            return;
        }

        // 处理 INT_MIN 的特殊情况
        if (n == INT_MIN) {
            // INT_MIN 的绝对值超出 int 范围，直接处理
            long long ln = static_cast<long long>(n);
            ln = -ln;// 现在安全了
            while (ln > 0) {
                digits.push_back(ln % 10);
                ln /= 10;
            }
        } else {
            n = std::abs(n);
            while (n > 0) {
                digits.push_back(n % 10);
                n /= 10;
            }
        }
    }

    BigInt(const std::string& str) : negative(false) {
        if (str.empty() || str == "0") {
            digits.push_back(0);
            return;
        }

        int start = 0;
        if (str[0] == '-') {
            negative = true;
            start = 1;
        } else if (str[0] == '+') {
            start = 1;
        }

        for (int i = str.length() - 1; i >= start; i--) {
            if (str[i] >= '0' && str[i] <= '9') {
                digits.push_back(str[i] - '0');
            }
        }

        if (digits.empty()) {
            digits.push_back(0);
        }

        remove_leading_zeros();
    }

    // 移除前导零
    void remove_leading_zeros() {
        while (digits.size() > 1 && digits.back() == 0) {
            digits.pop_back();
        }
        if (digits.size() == 1 && digits[0] == 0) {
            negative = false;
        }
    }

    // 转换为字符串
    std::string to_string() const {
        if (digits.size() == 1 && digits[0] == 0) {
            return "0";
        }

        std::string result;
        if (negative) result += "-";

        for (int i = digits.size() - 1; i >= 0; i--) {
            result += char('0' + digits[i]);
        }

        return result;
    }

    // 乘法
    BigInt operator*(const BigInt& other) const {
        BigInt result;
        result.digits.assign(digits.size() + other.digits.size(), 0);
        result.negative = (negative != other.negative);

        for (size_t i = 0; i < digits.size(); i++) {
            for (size_t j = 0; j < other.digits.size(); j++) {
                result.digits[i + j] += digits[i] * other.digits[j];
                if (result.digits[i + j] >= 10) {
                    result.digits[i + j + 1] += result.digits[i + j] / 10;
                    result.digits[i + j] %= 10;
                }
            }
        }

        result.remove_leading_zeros();
        return result;
    }

    // 加法
    BigInt operator+(const BigInt& other) const {
        if (negative == other.negative) {
            // 同号相加
            BigInt result;
            result.negative = negative;
            result.digits.clear();
            int carry = 0;
            for (size_t i = 0; i < digits.size() || i < other.digits.size() || carry; ++i) {
                int sum = carry + (i < digits.size() ? digits[i] : 0) + (i < other.digits.size() ? other.digits[i] : 0);
                result.digits.push_back(sum % 10);
                carry = sum / 10;
            }
            result.remove_leading_zeros();
            return result;
        } else {
            // 异号相加，转换为减法
            // a + (-b) = a - b
            // (-a) + b = b - a
            BigInt other_copy = other;
            other_copy.negative = !other_copy.negative;
            return *this - other_copy;
        }
    }

    // 减法
    BigInt operator-(const BigInt& other) const {
        if (negative != other.negative) {
            // 异号相减
            // a - (-b) = a + b
            // (-a) - b = -(a + b)
            BigInt other_copy = other;
            other_copy.negative = !other_copy.negative;
            return *this + other_copy;
        }

        // 同号相减
        const BigInt* p1 = this;
        const BigInt* p2 = &other;
        bool result_negative = negative;

        if (abs_compare(*p1, *p2) < 0) {
            std::swap(p1, p2);
            result_negative = !result_negative;
        }

        BigInt result;
        result.negative = result_negative;
        result.digits.clear();
        int borrow = 0;
        for (size_t i = 0; i < p1->digits.size(); ++i) {
            int diff = p1->digits[i] - (i < p2->digits.size() ? p2->digits[i] : 0) - borrow;
            if (diff < 0) {
                diff += 10;
                borrow = 1;
            } else {
                borrow = 0;
            }
            result.digits.push_back(diff);
        }
        result.remove_leading_zeros();
        return result;
    }

    // 比较绝对值大小
    static int abs_compare(const BigInt& a, const BigInt& b) {
        if (a.digits.size() != b.digits.size()) {
            return a.digits.size() < b.digits.size() ? -1 : 1;
        }
        for (int i = a.digits.size() - 1; i >= 0; --i) {
            if (a.digits[i] != b.digits[i]) {
                return a.digits[i] < b.digits[i] ? -1 : 1;
            }
        }
        return 0;
    }

    // 转换为int（如果可能）
    int to_int() const {
        if (is_zero()) return 0;

        if (digits.size() > 10) {// 太大了
            return negative ? INT_MIN : INT_MAX;
        }

        long long result = 0;

        for (int i = digits.size() - 1; i >= 0; i--) {
            result = result * 10 + digits[i];
            if (!negative && result > INT_MAX) {
                return INT_MAX;
            }
            // For negative numbers, the check is against -(long long)INT_MIN
            if (negative && result > -(long long) INT_MIN) {
                return INT_MIN;
            }
        }

        if (negative) {
            long long neg_res = -result;
            if (neg_res < INT_MIN) return INT_MIN;
            return static_cast<int>(neg_res);
        }

        return static_cast<int>(result);
    }

    // 检查是否为零
    bool is_zero() const {
        return digits.size() == 1 && digits[0] == 0;
    }

    // 除法（整数除法）
    BigInt operator/(const BigInt& other) const {
        if (other.is_zero()) {
            throw std::runtime_error("Division by zero");
        }

        if (is_zero()) {
            return BigInt(0);
        }

        // 使用长除法算法
        BigInt dividend = *this;
        dividend.negative = false;
        BigInt divisor = other;
        divisor.negative = false;

        if (abs_compare(dividend, divisor) < 0) {
            return BigInt(0);
        }

        BigInt quotient;
        quotient.digits.clear();

        BigInt current(0);
        for (int i = dividend.digits.size() - 1; i >= 0; i--) {
            current.digits.insert(current.digits.begin(), dividend.digits[i]);
            current.remove_leading_zeros();

            int count = 0;
            while (abs_compare(current, divisor) >= 0) {
                current = current - divisor;
                count++;
            }
            quotient.digits.insert(quotient.digits.begin(), count);
        }

        quotient.negative = (negative != other.negative);
        quotient.remove_leading_zeros();
        return quotient;
    }

    // 取模运算
    BigInt operator%(const BigInt& other) const {
        if (other.is_zero()) {
            throw std::runtime_error("Modulo by zero");
        }

        BigInt quotient = *this / other;
        BigInt remainder = *this - (quotient * other);
        return remainder;
    }

    // 幂运算
    BigInt power(const BigInt& exponent) const {
        if (exponent.negative) {
            throw std::runtime_error("Negative exponent not supported for integer power");
        }

        if (exponent.is_zero()) {
            return BigInt(1);
        }

        if (is_zero()) {
            return BigInt(0);
        }

        BigInt result(1);
        BigInt base = *this;
        BigInt exp = exponent;

        while (!exp.is_zero()) {
            if (exp.digits[0] % 2 == 1) {// 如果指数是奇数
                result = result * base;
            }
            base = base * base;
            exp = exp / BigInt(2);
        }

        return result;
    }

    // 阶乘
    static BigInt factorial(const BigInt& n) {
        if (n.negative) {
            throw std::runtime_error("Factorial of negative number is undefined");
        }

        if (n.is_zero() || (n.digits.size() == 1 && n.digits[0] == 1)) {
            return BigInt(1);
        }

        BigInt result(1);
        BigInt current(1);

        while (abs_compare(current, n) <= 0) {
            result = result * current;
            current = current + BigInt(1);
        }

        return result;
    }

    // 比较运算符
    bool operator<(const BigInt& other) const {
        if (negative != other.negative) {
            return negative > other.negative;// 负数小于正数
        }

        if (negative) {
            // 两个都是负数，绝对值大的反而小
            return abs_compare(*this, other) > 0;
        } else {
            // 两个都是正数
            return abs_compare(*this, other) < 0;
        }
    }

    bool operator<=(const BigInt& other) const {
        return *this < other || *this == other;
    }

    bool operator>(const BigInt& other) const {
        return !(*this <= other);
    }

    bool operator>=(const BigInt& other) const {
        return !(*this < other);
    }

    bool operator==(const BigInt& other) const {
        return negative == other.negative && digits == other.digits;
    }

    bool operator!=(const BigInt& other) const {
        return !(*this == other);
    }

    // Mathematical functions
    
    // Absolute value - returns a copy with positive sign
    BigInt abs() const {
        BigInt result = *this;
        result.negative = false;
        return result;
    }
    
    // Negate - returns a copy with opposite sign
    BigInt negate() const {
        BigInt result = *this;
        if (!result.is_zero()) {
            result.negative = !result.negative;
        }
        return result;
    }
    
    // Square root using Newton's method for integer square root
    // Returns the floor of the square root
    BigInt sqrt() const {
        if (negative) {
            throw std::runtime_error("Square root of negative BigInt is undefined");
        }
        
        if (is_zero()) {
            return BigInt(0);
        }
        
        if (digits.size() == 1 && digits[0] == 1) {
            return BigInt(1);
        }
        
        // Newton's method for integer square root
        // Start with an initial guess
        BigInt x = *this;
        BigInt y = (*this + BigInt(1)) / BigInt(2);
        
        while (y < x) {
            x = y;
            y = (x + (*this / x)) / BigInt(2);
        }
        
        return x;
    }
    
    // Check if this BigInt is a perfect square
    bool is_perfect_square() const {
        if (negative) {
            return false;
        }
        
        BigInt root = sqrt();
        return (root * root) == *this;
    }
    
    // Power function for non-negative integer exponents (alias for existing power method)
    BigInt pow(const BigInt& exponent) const {
        return power(exponent);
    }
    
    // Greatest Common Divisor using Euclidean algorithm
    static BigInt gcd(const BigInt& a, const BigInt& b) {
        BigInt abs_a = a.abs();
        BigInt abs_b = b.abs();
        
        if (abs_b.is_zero()) {
            return abs_a;
        }
        
        return gcd(abs_b, abs_a % abs_b);
    }
    
    // Least Common Multiple
    static BigInt lcm(const BigInt& a, const BigInt& b) {
        if (a.is_zero() || b.is_zero()) {
            return BigInt(0);
        }
        
        BigInt gcd_val = gcd(a, b);
        return (a.abs() / gcd_val) * b.abs();
    }
    
    // Convert to double (with potential precision loss warning)
    double to_double() const {
        if (is_zero()) {
            return 0.0;
        }
        
        double result = 0.0;
        double multiplier = 1.0;
        
        for (size_t i = 0; i < digits.size(); ++i) {
            result += digits[i] * multiplier;
            multiplier *= 10.0;
            
            // Check for overflow - use a large but finite value
            if (multiplier > 1e308) {
                // Precision loss will occur
                break;
            }
        }
        
        return negative ? -result : result;
    }
};
