# Lamina 模块开发指南

<div align="right">
  <a href="../zh_TW/PLUGIN_GUIDE.md">繁體中文</a> | <strong>简体中文</strong> | <a href="../en_US/PLUGIN_GUIDE.md">English</a>
</div>

## 概述
Lamina 支持通过动态库（DLL/SO）模块扩展功能。模块系统采用现代化设计，提供类型安全的跨平台API，支持命名空间管理和函数注册机制。

## 项目结构
```
Lamina/
├── interpreter/          # 核心解释器
│   ├── module_api.hpp    # 模块API定义
│   ├── module_loader.hpp # 模块加载器
│   └── ...               # 其他核心文件
├── extensions/           # 扩展模块源码
│   ├── minimal/          # 示例模块
│   │   ├── ultra_minimal.cpp  # 模块实现
│   │   └── CMakeLists.txt     # 构建配置
│   └── standard/         # 标准库模块
│       ├── array.cpp     # 数组操作
│       ├── math.cpp      # 数学函数
│       ├── stdio.cpp     # 输入输出
│       └── ...           # 其他标准库
├── modules/              # 编译后的模块(.dll/.so)
└── test/                 # 测试文件
```

## 模块API概述

### 核心特性
- **命名空间支持**：每个模块拥有独立的命名空间，避免函数名冲突
- **类型安全**：统一的 `LaminaValue` 类型系统，支持基本数据类型
- **跨平台兼容**：使用 `LAMINA_EXPORT` 和 `LAMINA_CALL` 宏确保不同平台兼容性
- **动态加载**：运行时加载模块，无需重新编译解释器

### 数据类型系统
```cpp
// 支持的值类型
typedef enum {
    LAMINA_TYPE_NULL = 0,
    LAMINA_TYPE_BOOL,
    LAMINA_TYPE_INT,      // 整数类型
    LAMINA_TYPE_NUMBER,   // 浮点数类型
    LAMINA_TYPE_STRING    // 字符串类型
} LaminaValueType;

// 统一的值结构
typedef struct {
    LaminaValueType type;
    union {
        bool bool_val;
        int int_val;
        double number_val;
        const char* string_val;
    } data;
} LaminaValue;
```

### 模块接口结构
```cpp
// 模块函数签名
typedef LaminaValue (LAMINA_CALL *LaminaFunction)(const LaminaValue* args, int argc);

// 函数注册条目
typedef struct {
    const char* name;        // 函数名
    LaminaFunction func;     // 函数指针
    const char* description; // 函数描述
} LaminaFunctionEntry;

// 模块信息
typedef struct {
    const char* namespace_name; // 命名空间名称
    const char* version;        // 版本号
    const char* description;    // 模块描述
} LaminaModuleInfo;

// 模块导出结构
typedef struct {
    LaminaModuleInfo info;           // 模块基本信息
    LaminaFunctionEntry* functions;  // 导出函数数组
    int function_count;              // 函数数量
} LaminaModuleExports;
```

## 创建第一个模块

### 1. 最简单的模块示例
以下是一个完整的最小模块实现（参考 `extensions/minimal/ultra_minimal.cpp`）：

```cpp
#include "../../interpreter/module_api.hpp"
#include <cstdio>

// 简单的测试函数
LAMINA_EXPORT LaminaValue LAMINA_CALL test_function(const LaminaValue* args, int argc) {
    LaminaValue result = {0};  // 零初始化
    result.type = LAMINA_TYPE_INT;
    result.data.int_val = 42;
    
    printf("test_function called successfully!\n");
    return result;
}

// 更复杂的函数示例
LAMINA_EXPORT LaminaValue LAMINA_CALL hello_function(const LaminaValue* args, int argc) {
    LaminaValue result = {0};
    result.type = LAMINA_TYPE_INT;
    result.data.int_val = 100;
    
    printf("Hello from minimal module function!\n");
    return result;
}

// 导出函数表
static LaminaFunctionEntry functions[] = {
    {"test", test_function, "Simple test function returning 42"},
    {"hello", hello_function, "Hello function returning 100"}
};

// 模块导出结构
static LaminaModuleExports exports = {
    {
        "minimal",                    // 命名空间名称
        "1.0.0",                     // 版本号
        "Ultra minimal test module"   // 描述
    },
    functions,                       // 函数数组
    2                               // 函数数量
};

// 必需的导出函数
LAMINA_EXPORT LaminaModuleExports* LAMINA_CALL lamina_module_init() {
    return &exports;
}
```

### 2. CMakeLists.txt 配置
```cmake
# 最小模块构建配置
cmake_minimum_required(VERSION 3.10)

# 设置模块名称
set(MODULE_NAME minimal)

# 创建共享库
add_library(${MODULE_NAME} SHARED
    ultra_minimal.cpp
)

# 设置输出目录
set_target_properties(${MODULE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Debug"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Debug"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Debug"
)

# 跨平台编译设置
if(WIN32)
    # Windows特定设置
    set_target_properties(${MODULE_NAME} PROPERTIES
        SUFFIX ".dll"
        PREFIX ""
    )
else()
    # Linux/macOS设置
    set_target_properties(${MODULE_NAME} PROPERTIES
        SUFFIX ".so"
        PREFIX "lib"
    )
endif()

# 编译标准
set_target_properties(${MODULE_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
```

### 3. xmake.lua 配置

```lua
add_rules("mode.debug", "mode.release")

package("lamina_core")
    set_urls("https://github.com/Lamina-dev/Lamina.git")
    on_install(function (package)
        import("package.tools.xmake").install(package)
    end)
package_end()

add_requires("lamina_core main")

target("minimal")
    set_kind("shared")
    set_languages("c++20")
    add_files("ultra_minimal.cpp")
    add_packages("lamina_core")
```

## 构建和使用模块

### 构建模块
#### 1. 使用 cmake 构建
```bash
# 配置构建
cmake -B build -DCMAKE_BUILD_TYPE=Debug .

# 编译项目（包括所有模块）
cmake --build build --config Debug --parallel

# 模块将输出到：build/Debug/minimal.dll (Windows) 或 build/Debug/libminimal.so (Linux)
```
#### 2. 使用 xmake 构建
```bash
xmake
```

### 在 Lamina 脚本中使用模块
```lamina
// 加载模块
include "minimal.dll";

// 调用模块函数
let result = minimal.test();
print("Test result: " + result);  // 输出: Test result: 42

let hello_result = minimal.hello();
print("Hello result: " + hello_result);  // 输出: Hello result: 100
```

### 运行示例
```bash
# 确保模块文件可访问
cd build/Debug

# 运行 Lamina 解释器
./Lamina.exe ../../test/test_minimal.lamina
```

## 高级模块开发

### 参数处理和类型检查
```cpp
LAMINA_EXPORT LaminaValue LAMINA_CALL advanced_function(const LaminaValue* args, int argc) {
    LaminaValue result = {0};
    
    // 参数数量检查
    if (argc < 2) {
        result.type = LAMINA_TYPE_STRING;
        result.data.string_val = "Error: Need at least 2 arguments";
        return result;
    }
    
    // 类型检查
    if (args[0].type != LAMINA_TYPE_INT || args[1].type != LAMINA_TYPE_INT) {
        result.type = LAMINA_TYPE_STRING;
        result.data.string_val = "Error: Arguments must be integers";
        return result;
    }
    
    // 执行计算
    int a = args[0].data.int_val;
    int b = args[1].data.int_val;
    
    result.type = LAMINA_TYPE_INT;
    result.data.int_val = a * b;
    return result;
}
```

### 字符串处理
```cpp
LAMINA_EXPORT LaminaValue LAMINA_CALL string_processor(const LaminaValue* args, int argc) {
    static char buffer[512];  // 静态缓冲区确保生命周期
    LaminaValue result = {0};
    
    if (argc < 1 || args[0].type != LAMINA_TYPE_STRING) {
        result.type = LAMINA_TYPE_STRING;
        result.data.string_val = "Error: Expected string argument";
        return result;
    }
    
    // 安全的字符串处理
    snprintf(buffer, sizeof(buffer), "Processed: %s", args[0].data.string_val);
    
    result.type = LAMINA_TYPE_STRING;
    result.data.string_val = buffer;
    return result;
}
```

### 错误处理模式
```cpp
// 返回错误信息的统一模式
static LaminaValue make_error(const char* message) {
    LaminaValue error = {0};
    error.type = LAMINA_TYPE_STRING;
    error.data.string_val = message;
    return error;
}

// 返回成功结果的统一模式
static LaminaValue make_success_int(int value) {
    LaminaValue result = {0};
    result.type = LAMINA_TYPE_INT;
    result.data.int_val = value;
    return result;
}
```

## 最佳实践

### 1. 模块设计原则
- **单一职责**：每个模块专注于特定功能领域
- **命名空间清晰**：使用简洁且描述性的命名空间名称
- **函数命名**：使用动词描述函数功能，保持一致性
- **版本管理**：使用语义化版本号（major.minor.patch）

### 2. 代码规范
```cpp
// 好的函数命名示例
{"calculate", math_calculate, "Calculate mathematical expression"},
{"format", string_format, "Format string with parameters"},
{"validate", data_validate, "Validate input data"}

// 避免的命名
{"func1", some_function, "Does something"},
{"process", unclear_function, "Process data"}
```

### 3. 错误处理策略
- 始终验证参数数量和类型
- 返回有意义的错误消息
- 使用一致的错误格式
- 避免程序崩溃，优雅处理异常情况

### 4. 内存安全
- 使用静态缓冲区或全局变量存储返回的字符串
- 避免返回局部变量指针
- 使用安全的字符串函数（`snprintf`、`strncpy`）
- 确保缓冲区大小足够且不会溢出

### 5. 跨平台兼容性
- 使用提供的 `LAMINA_EXPORT` 和 `LAMINA_CALL` 宏
- 避免平台特定的数据类型或函数
- 测试多个平台和编译器

## 故障排除

### 常见问题和解决方案

#### 1. 模块加载失败
**问题**：`ERROR: Module not loaded`
```cpp
// 检查项：
// - DLL文件是否存在于正确位置
// - 文件名是否与include语句匹配
// - 是否有必要的依赖库
```

**解决方案**：
- 确保模块文件在 `build/Debug/` 目录
- 检查文件扩展名（Windows: `.dll`, Linux: `.so`）
- 使用依赖工具检查缺失的动态库

#### 2. 函数未找到
**问题**：`ERROR: Function 'funcname' not found in module`
```cpp
// 检查项：
// - 函数是否正确注册在 functions 数组中
// - 函数名拼写是否正确
// - 命名空间是否匹配
```

**解决方案**：
- 验证 `LaminaFunctionEntry` 数组中的函数名
- 确认调用时使用正确的命名空间前缀
- 检查 `function_count` 是否正确

#### 3. 命名空间不匹配
**问题**：`ERROR: Namespace mismatch`
```cpp
// 在脚本中：include "minimal.dll"; minimal.test();
// 在模块中：namespace_name = "different_name"
```

**解决方案**：
- 确保模块的 `namespace_name` 与调用时使用的前缀一致
- 建议命名空间名与文件名保持一致

#### 4. 编译错误
**问题**：编译时出现链接错误或符号未定义
```cpp
// 常见原因：
// - 缺少 LAMINA_EXPORT 宏
// - 调用约定不匹配
// - 头文件路径错误
```

**解决方案**：
- 确保所有导出函数使用 `LAMINA_EXPORT` 和 `LAMINA_CALL`
- 检查 `module_api.hpp` 的包含路径
- 验证 CMake 配置正确

### 调试技巧

#### 1. 添加调试输出
```cpp
LAMINA_EXPORT LaminaValue LAMINA_CALL debug_function(const LaminaValue* args, int argc) {
    #ifdef _DEBUG
    printf("DEBUG: Function called with %d arguments\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("DEBUG: Arg[%d] type=%d\n", i, args[i].type);
    }
    #endif
    
    // 实际函数逻辑...
}
```

#### 2. 模块加载验证
```cpp
LAMINA_EXPORT LaminaModuleExports* LAMINA_CALL lamina_module_init() {
    printf("Module initializing: %s v%s\n", 
           exports.info.namespace_name, 
           exports.info.version);
    return &exports;
}
```

#### 3. 使用系统工具
- **Windows**: 使用 `dumpbin /exports module.dll` 查看导出符号
- **Linux**: 使用 `nm -D module.so` 或 `objdump -T module.so`
- **调试器**: 使用 GDB (Linux) 或 Visual Studio Debugger (Windows)

## 示例和参考

### 完整示例模块
查看项目中的示例模块获取完整实现：

#### minimal 模块 (`extensions/minimal/`)
- **文件**: `ultra_minimal.cpp`
- **功能**: 最基础的模块示例
- **函数**: `test()`, `hello()`
- **用途**: 学习模块开发基础

#### standard 模块 (`extensions/standard/`)
- **array.cpp**: 数组操作函数
- **math.cpp**: 数学计算函数
- **stdio.cpp**: 输入输出函数
- **times.cpp**: 时间处理函数
- **random.cpp**: 随机数生成
- **sockets.cpp**: 网络通信功能

### 快速开始模板
创建新模块的完整流程：

1. **创建目录结构**
```bash
mkdir extensions/mymodule
cd extensions/mymodule
```

2. **创建源文件 (mymodule.cpp)**
```cpp
#include "../../interpreter/module_api.hpp"

LAMINA_EXPORT LaminaValue LAMINA_CALL my_function(const LaminaValue* args, int argc) {
    LaminaValue result = {0};
    result.type = LAMINA_TYPE_STRING;
    result.data.string_val = "Hello from my module!";
    return result;
}

static LaminaFunctionEntry functions[] = {
    {"greet", my_function, "Greet the user"}
};

static LaminaModuleExports exports = {
    {"mymodule", "1.0.0", "My custom module"},
    functions,
    1
};

LAMINA_EXPORT LaminaModuleExports* LAMINA_CALL lamina_module_init() {
    return &exports;
}
```

3. **创建构建文件 (CMakeLists.txt)**
```cmake
add_library(mymodule SHARED mymodule.cpp)
set_target_properties(mymodule PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Debug"
    CXX_STANDARD 17
)
```

4. **添加到主构建系统**
在主 `CMakeLists.txt` 中添加：
```cmake
add_subdirectory(extensions/mymodule)
```

5. **构建和测试**
```bash
cmake --build build --config Debug
cd build/Debug
./Lamina.exe
```

### API 参考快查

#### 类型定义
```cpp
LAMINA_TYPE_NULL     // 空值
LAMINA_TYPE_BOOL     // 布尔值
LAMINA_TYPE_INT      // 整数
LAMINA_TYPE_NUMBER   // 浮点数
LAMINA_TYPE_STRING   // 字符串
```

#### 宏定义
```cpp
LAMINA_EXPORT        // 导出函数声明
LAMINA_CALL          // 调用约定
```

#### 必需函数
```cpp
lamina_module_init() // 模块初始化入口点
```

---

