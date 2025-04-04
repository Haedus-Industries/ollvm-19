#ifndef LLVM_UTILS_H
#define LLVM_UTILS_H
// LLVM libs
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/NoFolder.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
// System libs
#include <map>
#include <set>
#include <vector>
#include <stdio.h>
#include <sstream>
// 常用宏定义
#define INIT_CONTEXT(F) CONTEXT=&F.getContext()
#define TYPE_I32 Type::getInt32Ty(*CONTEXT)
#define CONST_I32(V) ConstantInt::get(TYPE_I32, V, false)
#define CONST(T, V) ConstantInt::get(T, V)
extern llvm::LLVMContext *CONTEXT;
// fla和bcf在混淆部分函数时会报错, 所以无法用命令行开启整体混淆
// 而且Visual Studio似乎没法把 annotate 传给LLVM, 只能用函数名控制
extern bool obf_function_name_cmd;
using namespace std;
namespace llvm{
    std::string readAnnotate(Function *f); // 读取llvm.global.annotations中的annotation值
    bool toObfuscate(bool flag, llvm::Function *f, std::string const &attribute); // 判断是否开启混淆
    bool toObfuscateBoolOption(Function *f, std::string option, bool *val); //获取混淆布尔变量，from hikari
    bool toObfuscateUint32Option(Function *f, std::string option, uint32_t *val); //获取混淆整型变量, from hikari
    void fixStack(Function &F); // 修复PHI指令和逃逸变量
    void FixBasicBlockConstantExpr(BasicBlock *BB);
    void FixFunctionConstantExpr(Function *Func);
    void turnOffOptimization(Function *f);  //from hikari
    string rand_str(int len);
    // LLVM-MSVC有这个函数, 官方版LLVM没有 (LLVM:17.0.6 | LLVM-MSVC:3.2.6)
    void LowerConstantExpr(Function &F);
    uint64_t getRandomNumber();
    unsigned int getUniqueNumber(std::vector<unsigned int> &rand_list);
    void getRandomNoRepeat(unsigned upper_bound, unsigned size,
                       std::vector<unsigned> &result);
    uint64_t getInverse(uint64_t a, uint64_t m);
    void demoteRegisters(Function *f);


    void annotation2Metadata(Module &M);
    bool readAnnotationMetadata(Function *f, std::string annotation);
    void writeAnnotationMetadata(Function *f, std::string annotation);
    #if 0
    std::map<GlobalValue*, StringRef> BuildAnnotateMap(Module& M);
    #endif

}
#endif // LLVM_UTILS_H