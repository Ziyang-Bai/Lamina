#include "interpreter.hpp"
#include "lexer.hpp"
#include "module_loader.hpp"
#include "parser.hpp"
#include "repl_input.hpp"
#include "trackback.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <typeinfo>

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char* argv[]) {
    try {
#ifdef _WIN32
        // 设置控制台输出编码为UTF-8
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
#endif
        if (argc < 2) {
            std::cout << "Lamina REPL. Press Ctrl+C or :exit to exit.\n";
            std::cout << "Type :help for help.\n";
            Interpreter interpreter;
            int lineno = 1;
            while (true) {
                try {
                    std::string prompt = Interpreter::supports_colors() ? "\033[1;32m>\033[0m " : "> ";
                    std::string line;
                    try {
                        line = repl_readline(prompt);
                    } catch (const CtrlCException&) {
                        break;
                    }
                    if (std::cin.eof()) break;
                    if (line.empty()) {
                        ++lineno;
                        continue;
                    }

                    if (line == ":exit") {
                        break;
                    }

                    if (line == ":help") {
                        std::cout << "Lamina Interpreter Commands:\n";
                        std::cout << "  :exit - Exit interpreter\n";
                        std::cout << "  :help - Show this help message\n";
                        std::cout << "  :vars - Show all variables\n";
                        std::cout << "  :clear - Clear screen\n";
                        //std::cout << "  :fns - Get Lamina Runtime Functions\n";
                        ++lineno;
                        continue;
                    }

                    if (line == ":vars") {
                        interpreter.printVariables();
                        ++lineno;
                        continue;
                    }

                    if (line == ":clear") {
#ifdef _WIN32
                        auto result = system("cls");
                        (void) result;// 明确忽略返回值
#else
                        auto result = system("clear");
                        (void) result;// 明确忽略返回值
#endif
                        ++lineno;
                        continue;
                    }

                    if (line == ":fns") {

                        ++lineno;
                        continue;
                    }

                    auto tokens = Lexer::tokenize(line);
                    auto ast = Parser::parse(tokens);
                    if (!ast) {
                        print_traceback("<stdin>", lineno);
                    } else {
                        // Only support BlockStmt
                        auto* block = dynamic_cast<BlockStmt*>(ast.get());
                        if (block) {
                            // Save AST to keep function pointers valid
                            interpreter.save_repl_ast(std::move(ast));

                            try {
                                for (auto& stmt: block->statements) {
                                    try {
                                        interpreter.execute(stmt);
                                    } catch (const RuntimeError& re) {
                                        interpreter.print_stack_trace(re, true);
                                        break;
                                    } catch (const ReturnException&) {
                                        // Catch and show meaningful info (return outside function causes this exception)
                                        Interpreter::print_warning("Return statement used outside function (line " + std::to_string(lineno) + ")", true);
                                        break;
                                    } catch (const BreakException&) {
                                        // Catch and show meaningful info
                                        Interpreter::print_warning("Break statement used outside loop (line " + std::to_string(lineno) + ")", true);
                                        break;
                                    } catch (const ContinueException&) {
                                        // Catch and show meaningful info
                                        Interpreter::print_warning("Continue statement used outside loop (line " + std::to_string(lineno) + ")", true);
                                        break;
                                    } catch (const std::exception& e) {
                                        Interpreter::print_error(e.what(), true);
                                        break;
                                    }
                                }
                            } catch (const RuntimeError& re) {
                                interpreter.print_stack_trace(re, true);
                            } catch (const ReturnException&) {
                                // Catch return exceptions that escape the inner loop
                                Interpreter::print_warning("Return statement used outside function", true);
                            } catch (...) {
                                Interpreter::print_error("Unknown error occurred", true);
                            }
                        }
                    }
                } catch (...) {
                    Interpreter::print_error("REPL environment caught exception, but recovered", true);
                }
                ++lineno;
            }
            return 0;
        }
        std::ifstream file(argv[1]);
        if (!file) {
            std::cerr << "Unable to open file: " << argv[1] << std::endl;
            return 1;
        }
        std::cout << "Executing file: " << argv[1] << std::endl;
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string source = buffer.str();
        auto tokens = Lexer::tokenize(source);
        auto ast = Parser::parse(tokens);
        Interpreter interpreter;

        // 注释掉自动加载minimal模块，改为按需加载
        // std::cout << "Loading minimal module..." << std::endl;
        // try {
        //     // 首先尝试从extensions目录加载
        //     auto moduleLoader = std::make_unique<ModuleLoader>("../extensions/minimal.dll");
        //     if (!moduleLoader->isLoaded()) {
        //         // 如果失败，尝试从当前目录加载
        //         moduleLoader = std::make_unique<ModuleLoader>("minimal.dll");
        //     }
        //     if (moduleLoader->isLoaded()) {
        //         std::cout << "Module loaded successfully, registering to interpreter..." << std::endl;
        //         if (moduleLoader->registerToInterpreter(interpreter)) {
        //             std::cout << "Module registered successfully!" << std::endl;
        //         } else {
        //             std::cerr << "Failed to register module to interpreter!" << std::endl;
        //         }
        //     } else {
        //         std::cerr << "Failed to load module!" << std::endl;
        //     }
        // } catch (const std::exception& e) {
        //     std::cerr << "Exception during module loading: " << e.what() << std::endl;
        // }

        if (!ast) {
            print_traceback(argv[1], 1);
            return 2;
        }

        // 只支持 BlockStmt
        auto* block = dynamic_cast<BlockStmt*>(ast.get());
        if (block) {
            // 每个语句独立执行并捕获异常，但保持解释器状态
            int currentLine = 0;
            for (auto& stmt: block->statements) {
                currentLine++;
                try {
                    interpreter.execute(stmt);
                } catch (const RuntimeError& re) {
                    // Use stack trace for runtime errors
                    interpreter.print_stack_trace(re, true);
                } catch (const ReturnException&) {
                    // Catch and show meaningful info (return outside function causes this exception)
                    Interpreter::print_warning("Return statement used outside function (line " + std::to_string(currentLine) + ")", true);
                } catch (const BreakException&) {
                    // Catch and show meaningful info
                    Interpreter::print_warning("Break statement used outside loop (line " + std::to_string(currentLine) + ")", true);
                } catch (const ContinueException&) {
                    // Catch and show meaningful info
                    Interpreter::print_warning("Continue statement used outside loop (line " + std::to_string(currentLine) + ")", true);
                } catch (const std::exception& e) {
                    // Other standard exceptions
                    Interpreter::print_error(std::string(e.what()) + " (line " + std::to_string(currentLine) + ")", true);
                    std::cerr << "Type: " << typeid(e).name() << std::endl;
                } catch (...) {
                    // Unknown exceptions
                    Interpreter::print_error("Unknown error (line " + std::to_string(currentLine) + ")", true);
                }
            }
            std::cout << "\nProgram execution completed." << std::endl;
        }
        return 0;
    } catch (const CtrlCException&) {
        std::cout << "\n程序收到 Ctrl+C 信号，已安全退出。" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "程序发生异常: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "程序发生未知异常。" << std::endl;
        return 1;
    }
}
