# <img src="./assets/logo-icon.svg" width="10%"> Lamina

<img src="./assets/logo.svg" width="100%">

<div align="right">
  <a href="/docs/zh_TW/README.md"> 繁體中文</a> | <strong> 简体中文</strong> | <a href="/docs/en_US/README.md"> English</a>
</div>
<br>

[![GitHub issues](https://img.shields.io/github/issues/lamina-dev/Lamina)](https://github.com/Lamina-dev/Lamina/issues)
[![GitHub stars](https://img.shields.io/github/stars/lamina-dev/Lamina?style=flat)](https://github.com/Lamina-dev/Lamina/stargazers)
[![GitHub forks](https://img.shields.io/github/forks/lamina-dev/Lamina?style=flat)](https://github.com/Lamina-dev/Lamina/forks)
[![GitHub contributors](https://img.shields.io/github/contributors/lamina-dev/Lamina?style=flat)](https://github.com/Lamina-dev/Lamina/graphs/contributors)
![GitHub last commit](https://img.shields.io/github/last-commit/lamina-dev/Lamina?style=flat)
[![License](https://img.shields.io/badge/license-GNUv3-blue.svg)](LICENSE)
[![Language](https://img.shields.io/badge/language-C%2B%2B-orange.svg)](https://isocpp.org/)
[![Math](https://img.shields.io/badge/math-precise-green.svg)](#精确数学特性)
[![QQ](https://img.shields.io/badge/QQ-%E4%BA%A4%E6%B5%81%E7%BE%A4-red?logo=qq&logoColor=white)](https://qm.qq.com/q/QwPXCgsJea)

## 一种专注于精确数学计算的面向过程编程语言

[快速开始](#快速开始) • [语法指南](#基础语法) • [数学特性](#精确数学特性) • [示例代码](#示例代码) • [编译指南](/docs/zh_CN/Compile.md) • [贡献指南](/docs/zh_CN/CONTRIBUTING.md) • [Wiki](https://github.com/lamina-dev/Lamina/wiki) • [动态库插件开发](/docs/zh_CN/PLUGIN_GUIDE.md)

---

## 目录

- [概述](#概述)
- [精确数学特性](#精确数学特性)
- [快速开始](#快速开始)
- [基础语法](#基础语法)
- [数据类型](#数据类型)
- [运算符](#运算符)
- [内建函数库](#内建函数库)
- [示例代码](#示例代码)
- [设计原则](#设计原则)

---

## 概述

**Lamina** 是一种面向过程的数学计算语言，专注于数学问题的高效解决，支持图灵完备性。其设计目标是提供简洁、直观的语法，同时支持复杂数学运算。

### 主要特性

- **精确数学**：支持有理数（分数）和无理数（√、π、e）的精确表示和运算
- **精度保护**：避免浮点数精度丢失，多次循环运算保持精确
- **数学友好**：原生支持向量、矩阵运算和数学函数
- **递归支持**：可配置递归深度限制
- **大整数**：支持任意精度大整数运算
- **交互式 REPL**：支持交互式编程和脚本执行
- **智能终端**：自动检测终端色彩支持，避免乱码
- **栈追踪**：完整的错误栈追踪，便于调试

---

## 精确数学特性

Lamina 的核心优势在于其精确数学计算能力，解决了传统编程语言中浮点数精度丢失的问题。

### 有理数（Rational）

- **自动分数表示**：除法运算自动产生精确分数
- **多次运算保持精确**：避免累积误差
- **自动化简**：分数自动约简到最简形式

```lamina
var fraction = 16 / 9;           // 结果为 16/9，不是 1.777...
var result = fraction;

// 多次循环运算保持精确
var i = 1;
while (i <= 10) {
    result = result * 9 / 9;     // 始终保持为 16/9
    i = i + 1;
}
```

### 无理数（Irrational）

- **符号化表示**：√2、π、e 等保持符号形式
- **自动化简**：√8 → 2√2，√(a²) → a
- **精确运算**：符号运算避免近似误差

```lamina
var root2 = sqrt(2);             // 精确的 √2
var root8 = sqrt(8);             // 自动化简为 2√2
var pi_val = pi();               // 精确的 π
var result = root2 * root2;      // 精确结果：2
```

### 精度对比示例

```lamina
// 传统浮点数（其他语言）
// 0.1 + 0.2 = 0.30000000000000004

// Lamina 精确计算
var result = 1/10 + 2/10;        // 结果：3/10（完全精确）
```

---

## 快速开始

### 编译和运行

```bash
# 初始化子模块（首次构建或更新后需要）
git submodule update --init --recursive

# 编译
# 请参考编译指南

# 交互式模式
./lamina

# 执行脚本
./lamina script.lm
```

**注意**：Lamina 使用 libuv 库来支持网络功能，该库以 Git 子模块形式包含在项目中。首次构建前或更新代码后，请务必运行上述子模块初始化命令。

### 简单示例

```lamina
// 精确数学计算
print("Hello, Lamina!");
var fraction = 16 / 9;          // 精确分数，不是浮点数
print("16/9 =", fraction);      // 输出: 16/9

var root = sqrt(2);             // 精确无理数
print("√2 =", root);            // 输出: √2
print("√2 × √2 =", root * root);// 输出: 2（精确结果）

// 向量运算
var v1 = [1, 2, 3];
var v2 = [4, 5, 6];
print("v1 + v2 =", v1 + v2);
print("v1 · v2 =", dot(v1, v2));

// 精确常数
var pi_val = pi();
print("π =", pi_val);           // 输出: π（符号形式）
```

---

## 基础语法

### 语句终止符

**分号是必需的**：Lamina 要求所有语句都必须以分号 `;` 结束，这提高了解析效率和代码可读性。

```lamina
var x = 10;           //  正确
print(x);             //  正确
bigint big = 100!;    //  正确

var y = 20            //  错误：缺少分号
print(y)              //  错误：缺少分号
```

### 模块系统

Lamina 支持模块化编程，可以使用 `include` 语句包含其他文件。

#### include 语句

**强制使用引号**：include 语句必须使用引号包裹文件名，这确保了语法的一致性和清晰度。

```lamina
include "math_utils";     //  正确：使用引号包裹
include "lib/vectors";    //  正确：支持相对路径

include math_utils;       //  错误：必须使用引号
```

#### 模块搜索路径

Lamina 会按以下顺序搜索模块文件：

1. 当前目录
2. `./` 目录
3. `./include/` 目录

如果文件名不包含 `.lm` 扩展名，会自动添加。

#### 内置模块

Lamina 提供了一些特殊的内置模块：

- `include "splash";` - 显示 Lamina 启动画面
- `include "them";` - 显示开发者信息和致谢

### 关键字

以下为保留字，不可用作变量名或函数名：

```plaintext
var func if else while for return break continue
print true false null include define bigint input
```

### 注释

```lamina
// 单行注释
/* 块注释 */
```

### 变量声明

```lamina
var x = 10;
var name = "Alice";
var arr = [1, 2, 3];
bigint large_num = 999999999999;  // 大整数需显式声明
bigint factorial_result = 25!;    // 可直接从运算结果赋值
```

---

## 数据类型

### 基本类型

| 类型         | 描述                 | 示例                                    |
| ------------ | -------------------- | --------------------------------------- |
| `int`        | 整数                 | `42`, `-10`                             |
| `float`      | 浮点数               | `3.14`, `-0.5`                          |
| `rational`   | 有理数（精确分数）   | `16/9`, `1/3`                           |
| `irrational` | 无理数（精确符号）   | `√2`, `π`, `e`                          |
| `bool`       | 布尔值               | `true`, `false`                         |
| `string`     | 字符串               | `"Hello"`, `"world"`                    |
| `null`       | 空值                 | `null`                                  |
| `bigint`     | 大整数（需显式声明） | `bigint x = 999999;`, `bigint y = 30!;` |

### 复合类型

```lamina
// 数组
var arr = [1, 2, 3];
var matrix = [[1,2], [3,4]];     // 2x2矩阵

// 函数
func add(a, b) {
    return a + b;
}
```

---

## 运算符

### 算术运算符

| 运算符 | 功能             | 示例          |
| ------ | ---------------- | ------------- |
| `+`    | 加法             | `2 + 3 → 5`   |
| `-`    | 减法             | `5 - 2 → 3`   |
| `*`    | 乘法             | `4 * 3 → 12`  |
| `/`    | 除法（精确分数） | `5 / 2 → 5/2` |
| `%`    | 取模             | `7 % 3 → 1`   |
| `^`    | 幂运算           | `2^3 → 8`     |
| `!`    | 阶乘             | `5! → 120`    |

### 向量/矩阵运算

```lamina
var v1 = [1, 2, 3];
var v2 = [4, 5, 6];

print(v1 + v2);                 // 向量加法：[5, 7, 9]
print(dot(v1, v2));             // 点积：32
print(2 * v1);                  // 标量乘法：[2, 4, 6]
```

### 比较运算符

`==` `!=` `>` `<` `>=` `<=`

---

## 内建函数库

### 数学函数

| 函数      | 描述                 | 示例           |
| --------- | -------------------- | -------------- |
| `sqrt(x)` | 平方根（精确无理数） | `sqrt(2) → √2` |
| `pi()`    | 圆周率常数           | `pi() → π`     |
| `e()`     | 自然常数             | `e() → e`      |
| `abs(x)`  | 绝对值               | `abs(-5) → 5`  |
| `sin(x)`  | 正弦函数             | `sin(0) → 0`   |
| `cos(x)`  | 余弦函数             | `cos(0) → 1`   |
| `log(x)`  | 自然对数             | `log(e()) → 1` |

### 向量/矩阵函数

| 函数            | 描述         | 示例                                |
| --------------- | ------------ | ----------------------------------- |
| `dot(v1, v2)`   | 向量点积     | `dot([1,2], [3,4]) → 11`            |
| `cross(v1, v2)` | 三维向量叉积 | `cross([1,0,0], [0,1,0]) → [0,0,1]` |
| `norm(v)`       | 向量模长     | `norm([3,4]) → 5`                   |
| `det(m)`        | 矩阵行列式   | `det([[1,2],[3,4]]) → -2`           |

### 工具函数

| 函数             | 描述       | 示例                      |
| ---------------- | ---------- | ------------------------- |
| `print(...)`     | 打印输出   | `print("x =", 42)`        |
| `input(prompt)`  | 获取输入   | `input("Name: ")`         |
| `size(x)`        | 获取大小   | `size([1,2,3]) → 3`       |
| `fraction(x)`    | 小数转分数 | `fraction(0.75) → 3/4`    |
| `decimal(x)`     | 分数转小数 | `decimal(3/4) → 0.75`     |
| `visit()`        | 访问       | `visit(a, 3, 0)`          |
| `visit_by_str()` | 访问       | `visit_by_str(b, "name")` |

### 字符串处理函数

| 函数                                                 | 描述                                      | 示例                                                           |
| ---------------------------------------------------- | ----------------------------------------- | -------------------------------------------------------------- |
| `string_concat(...)`                                 | 拼接多个字符串                            | `string_concat("abc", "abc", "abc") → "abcabcabc"`             |
| `string_char_at(str, index)`                         | 获取字符串指定位置的字符，以 Int 类型返回 | `string_char_at("abc", 1) → 98`                                |
| `string_length(str)`                                 | 获取字符串长度                            | `string_length("abc") → 3`                                     |
| `string_find(str, start_index, sub_str)`             | 从指定位置开始查找子字符串                | `string_find("abcAAA123", 0, "AAA") → 3`                       |
| `string_sub_string(str, start_index, len)`           | 截取指定长度的子字符串                    | `string_sub_string("abcAAA123", 3, 3) → "AAA"`                 |
| `string_replace_by_index(str, start_index, sub_str)` | 从指定位置开始替换原字符串                | `string_replace_by_index("abcAAA123", 3, "CCC") → "abcCCC123"` |

---

## 示例代码

### 精确数学计算

```lamina
// 精确分数运算
print("=== 精确分数运算 ===");
var fraction = 16 / 9;           // 精确分数 16/9
print("16/9 =", fraction);

// 多次循环保持精确性
var result = fraction;
var i = 1;
while (i <= 5) {
    result = result * 9 / 9;     // 应该保持为 16/9
    print("第", i, "次循环:", result);
    i = i + 1;
}

// 精确无理数运算
print("\n=== 精确无理数运算 ===");
var root2 = sqrt(2);             // 精确的 √2
var root3 = sqrt(3);             // 精确的 √3
print("√2 =", root2);
print("√3 =", root3);
print("√2 + √3 =", root2 + root3);
print("√2 × √3 =", root2 * root3);
print("√2 × √2 =", root2 * root2);

// 精确常数
var pi_val = pi();
var e_val = e();
print("π =", pi_val);
print("e =", e_val);
print("π + e =", pi_val + e_val);
```

### 分数与小数转换

```lamina
// 小数转分数
print("=== 小数转分数 ===");
print("0.5 =", fraction(0.5));      // 1/2
print("0.25 =", fraction(0.25));    // 1/4
print("0.75 =", fraction(0.75));    // 3/4
print("0.125 =", fraction(0.125));  // 1/8
print("0.625 =", fraction(0.625));  // 5/8

// 分数转小数
print("\n=== 分数转小数 ===");
print("1/2 =", decimal(1/2));       // 0.5
print("3/4 =", decimal(3/4));       // 0.75
print("1/3 =", decimal(1/3));       // 0.333333
print("2/3 =", decimal(2/3));       // 0.666667

// 精确计算的优势
print("\n=== 精确计算 vs 浮点计算 ===");
var float_result = 0.1 + 0.2;
var exact_result = fraction(0.1) + fraction(0.2);
print("浮点计算: 0.1 + 0.2 =", float_result);
print("精确计算: 1/10 + 1/5 =", exact_result);
print("转为小数:", decimal(exact_result));
```

### 解二次方程

```lamina
func quadratic(a, b, c) {
    var discriminant = b^2 - 4*a*c;
    if (discriminant < 0) {
        print("Complex roots not supported");
        return null;
    } else {
        var root1 = (-b + sqrt(discriminant))/(2*a);
        var root2 = (-b - sqrt(discriminant))/(2*a);
        return [root1, root2];
    }
}

var roots = quadratic(1, -3, 2);
print("Roots:", roots);  // 输出 [2, 1]
```

### 大整数阶乘

```lamina
define MAX_RECURSION_DEPTH 30;

// 直接计算
bigint result = 25!;
print("25! =", result);

// 递归版本
func factorial_big(n) {
    if (n <= 1) return 1;
    return n * factorial_big(n - 1);
}

bigint fact10 = factorial_big(10);
print("10! =", fact10);
```

### 向量和矩阵运算

```lamina
// 向量运算
var v1 = [1, 2, 3];
var v2 = [4, 5, 6];
var v3 = [1, 0, 0];
var v4 = [0, 1, 0];

print("向量加法:", v1 + v2);
print("点积:", dot(v1, v2));
print("叉积:", cross(v3, v4));
print("模长:", norm(v1));

// 矩阵运算
var matrix = [[1, 2], [3, 4]];
print("行列式:", det(matrix));
```

### 模块化编程

```lamina
// 包含数学工具模块
include "math_utils";

// 包含向量计算模块
include "vectors";

// 使用模块中定义的函数
var result = advanced_calculation(10, 20);
var vec = create_vector(1, 2, 3);

// 显示启动画面
include "splash";
```

---

## 设计原则

1. **简洁性**：语法简洁，符合数学表达习惯
2. **数学友好**：直接支持向量、矩阵和常用数学运算
3. **精确计算**：支持有理数和无理数的精确表示，避免精度丢失
4. **面向过程**：通过函数组织代码，避免复杂对象模型
5. **图灵完备**：支持无限循环、递归和任意计算
6. **用户可配置**：递归深度限制、大整数类型等需用户显式设置

---

## 未来扩展

- **复数支持**：实现复数类型和相关运算
- **符号微积分**：支持符号化的微分和积分
- **语法糖**：支持分数字面量（如 `16/9`）和无理数字面量（如 `√5`）
- **并行计算**：支持多线程加速复杂运算
- **高级数学库**：统计函数、数值分析等

---

## 错误处理与调试

### 栈追踪支持

Lamina 提供完整的错误栈追踪：

```text
Traceback (most recent call last):
  File "script.lm", line 12, in level1
  File "script.lm", line 8, in level2
  File "script.lm", line 4, in level3
RuntimeError: Undefined variable 'unknown_variable'
```

### 智能终端支持

- 自动色彩检测：根据终端能力自动启用/禁用颜色
- NO_COLOR 支持：遵循 NO_COLOR 环境变量标准
- 重定向友好：输出重定向时自动禁用色彩避免乱码

---

## 运算符优先级

`! > ^ > * / % > + - > == != > && `
