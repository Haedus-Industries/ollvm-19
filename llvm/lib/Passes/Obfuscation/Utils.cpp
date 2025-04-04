/**
 * @file Utils.cpp
 * @author SsageParuders
 * @brief 本代码参考原OLLVM项目:https://github.com/obfuscator-llvm/obfuscator
 *        感谢地球人前辈的指点
 * @version 0.1
 * @date 2022-07-14
 *
 * @copyright Copyright (c) 2022
 *
 */
#include "Utils.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/NoFolder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Local.h"
#include <algorithm>
#include <ctime>
#include <random>
#include <set>
#include <sstream>


using namespace llvm;
using std::vector;


LLVMContext *CONTEXT = nullptr;
bool obf_function_name_cmd = false;


namespace llvm {



static const char obfkindid[] = "MD_obf";

/**
 * @brief 参考资料:https://www.jianshu.com/p/0567346fd5e8
 *        作用是读取llvm.global.annotations中的annotation值 从而实现过滤函数
 * 只对单独某功能开启PASS
 * @param f
 * @return std::string
 */
std::string readAnnotate(Function *f) { // 取自原版ollvm项目
  std::string annotation = "";
  /* Get annotation variable */
  GlobalVariable *glob =
      f->getParent()->getGlobalVariable("llvm.global.annotations");
  if (glob != NULL) {
    /* Get the array */
    if (ConstantArray *ca = dyn_cast<ConstantArray>(glob->getInitializer())) {
      for (unsigned i = 0; i < ca->getNumOperands(); ++i) {
        /* Get the struct */
        if (ConstantStruct *structAn =
                dyn_cast<ConstantStruct>(ca->getOperand(i))) {
          if (ConstantExpr *expr =
                  dyn_cast<ConstantExpr>(structAn->getOperand(0))) {
            /*
             * If it's a bitcast we can check if the annotation is concerning
             * the current function
             */
            if (expr->getOpcode() == Instruction::BitCast &&
                expr->getOperand(0) == f) {
              ConstantExpr *note = cast<ConstantExpr>(structAn->getOperand(1));
              /*
               * If it's a GetElementPtr, that means we found the variable
               * containing the annotations
               */
              if (note->getOpcode() == Instruction::GetElementPtr) {
                if (GlobalVariable *annoteStr =
                        dyn_cast<GlobalVariable>(note->getOperand(0))) {
                  if (ConstantDataSequential *data =
                          dyn_cast<ConstantDataSequential>(
                              annoteStr->getInitializer())) {
                    if (data->isString()) {
                      annotation += data->getAsString().lower() + " ";
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  return (annotation);
}

/**
 * @brief 从注解中获取函数注解
 * 解决在llvm15+上读取不到注解 (测试版本在LLVM-18.0.1)
 * @param F
 * @return std::string
 */
std::string getFunctionAnnotation(Function *F) {
  Module *M = F->getParent();
  // 查找名为 "llvm.global.annotations" 的全局变量
  GlobalVariable *GA = M->getNamedGlobal("llvm.global.annotations");
  if (!GA)
    return ""; // 如果没有注解，返回空字符串
  // 解析 llvm.global.annotations
  if (ConstantArray *CA = dyn_cast<ConstantArray>(GA->getInitializer())) {
    for (unsigned i = 0; i < CA->getNumOperands(); ++i) {
      if (ConstantStruct *CS = dyn_cast<ConstantStruct>(CA->getOperand(i))) {
        // 第一个元素是被注解的函数
        if (Function *AnnotatedFunction =
                dyn_cast<Function>(CS->getOperand(0)->stripPointerCasts())) {
          if (AnnotatedFunction == F) {
            // 第二个元素是注解字符串的全局变量
            if (GlobalVariable *GV = dyn_cast<GlobalVariable>(
                    CS->getOperand(1)->stripPointerCasts())) {
              if (ConstantDataArray *Anno =
                      dyn_cast<ConstantDataArray>(GV->getInitializer())) {
                return Anno->getAsCString().str();
              }
            }
          }
        }
      }
    }
  }

  return ""; // 如果没有找到对应函数的注解，返回空字符串
}

/**
 * @brief 用于判断是否开启混淆
 *
 * @param flag
 * @param f
 * @param attribute
 * @return true
 * @return false
 */
bool toObfuscate(bool flag, Function *f,
                       std::string const &attribute) { // 取自原版ollvm项目
  std::string attr = attribute;
  std::string attrNo = "no" + attr;
  // Check if declaration
  if (f->isDeclaration()) {
    return false;
  }
  // Check external linkage
  if (f->hasAvailableExternallyLinkage() != 0) {
    return false;
  }
  // outs() << "[Soule] function: " << f->getName().str() << " # annotation: "
  // << readAnnotate(f) << "\n";
  //
  //  We have to check the nofla flag first
  //  Because .find("fla") is true for a string like "fla" or
  //  "nofla"
  if (getFunctionAnnotation(f).find(attrNo) !=
      std::string::npos) { // 是否禁止开启XXX
    return false;
  }
  // If fla annotations
  if (getFunctionAnnotation(f).find(attr) != std::string::npos) { // 是否开启XXX
    return true;
  }
  // 由于Visual Studio无法传入annotation,
  // 增加一个使用函数名匹配是否单独开关的功能
  if (obf_function_name_cmd == true) { // 开启使用函数名匹配混淆功能开关
    if (f->getName().find("_" + attrNo + "_") != StringRef::npos) {
      outs() << "[Soule] " << attrNo << ".function: " << f->getName().str()
             << "\n";
      return false;
    }
    if (f->getName().find("_" + attr + "_") != StringRef::npos) {
      outs() << "[Soule] " << attr << ".function: " << f->getName().str()
             << "\n";
      return true;
    }
  }
  // If fla flag is set
  if (flag == true) { // 开启PASS
    return true;
  }
  return false;
}

// Unlike O-LLVM which uses __attribute__ that is not supported by the ObjC
// CFE. We use a dummy call here and remove the call later Very dumb and
// definitely slower than the function attribute method Merely a hack
bool readFlag(Function *f, std::string attribute) {
  for (Instruction &I : instructions(f)) {
    Instruction *Inst = &I;
    if (CallInst *CI = dyn_cast<CallInst>(Inst)) {
      if (CI->getCalledFunction() != nullptr &&
#if LLVM_VERSION_MAJOR >= 18
          CI->getCalledFunction()->getName().starts_with("hikari_" +
                                                         attribute)) {
#else
          CI->getCalledFunction()->getName().startswith("hikari_" +
                                                        attribute)) {
#endif
        CI->eraseFromParent();
        return true;
      }
    }
    if (InvokeInst *II = dyn_cast<InvokeInst>(Inst)) {
      if (II->getCalledFunction() != nullptr &&
#if LLVM_VERSION_MAJOR >= 18
          II->getCalledFunction()->getName().starts_with("hikari_" +
                                                         attribute)) {
#else
          II->getCalledFunction()->getName().startswith("hikari_" +
                                                        attribute)) {
#endif
        BasicBlock *normalDest = II->getNormalDest();
        BasicBlock *unwindDest = II->getUnwindDest();
        BasicBlock *parent = II->getParent();
        if (parent->size() == 1) {
          parent->replaceAllUsesWith(normalDest);
          II->eraseFromParent();
          parent->eraseFromParent();
        } else {
          BranchInst::Create(normalDest, II);
          II->eraseFromParent();
        }
        if (pred_size(unwindDest) == 0)
          unwindDest->eraseFromParent();
        return true;
      }
    }
  }
  return false;
}

bool readAnnotationMetadataUint32OptVal(Function *f, std::string opt,
                                        uint32_t *val) {
  MDNode *Existing = f->getMetadata(obfkindid);
  if (Existing) {
    MDTuple *Tuple = cast<MDTuple>(Existing);
    for (auto &N : Tuple->operands()) {
      StringRef mdstr = cast<MDString>(N.get())->getString();
      std::string estr = opt + "=";
#if LLVM_VERSION_MAJOR >= 18
      if (mdstr.starts_with(estr)) {
#else
      if (mdstr.startswith(estr)) {
#endif
        *val = atoi(mdstr.substr(strlen(estr.c_str())).str().c_str());
        return true;
      }
    }
  }
  return false;
}

bool readFlagUint32OptVal(Function *f, std::string opt, uint32_t *val) {
  for (Instruction &I : instructions(f)) {
    Instruction *Inst = &I;
    if (CallInst *CI = dyn_cast<CallInst>(Inst)) {
      if (CI->getCalledFunction() != nullptr &&
#if LLVM_VERSION_MAJOR >= 18
          CI->getCalledFunction()->getName().starts_with("hikari_" + opt)) {
#else
          CI->getCalledFunction()->getName().startswith("hikari_" + opt)) {
#endif
        if (ConstantInt *C = dyn_cast<ConstantInt>(CI->getArgOperand(0))) {
          *val = (uint32_t)C->getValue().getZExtValue();
          CI->eraseFromParent();
          return true;
        }
      }
    }
    if (InvokeInst *II = dyn_cast<InvokeInst>(Inst)) {
      if (II->getCalledFunction() != nullptr &&
#if LLVM_VERSION_MAJOR >= 18
          II->getCalledFunction()->getName().starts_with("hikari_" + opt)) {
#else
          II->getCalledFunction()->getName().startswith("hikari_" + opt)) {
#endif
        if (ConstantInt *C = dyn_cast<ConstantInt>(II->getArgOperand(0))) {
          *val = (uint32_t)C->getValue().getZExtValue();
          BasicBlock *normalDest = II->getNormalDest();
          BasicBlock *unwindDest = II->getUnwindDest();
          BasicBlock *parent = II->getParent();
          if (parent->size() == 1) {
            parent->replaceAllUsesWith(normalDest);
            II->eraseFromParent();
            parent->eraseFromParent();
          } else {
            BranchInst::Create(normalDest, II);
            II->eraseFromParent();
          }
          if (pred_size(unwindDest) == 0)
            unwindDest->eraseFromParent();
          return true;
        }
      }
    }
  }
  return false;
}

bool toObfuscateBoolOption(Function *f, std::string option, bool *val) {
  std::string opt = option;
  std::string optDisable = "no" + option;
  if (readAnnotationMetadata(f, optDisable) || readFlag(f, optDisable)) {
    *val = false;
    return true;
  }
  if (readAnnotationMetadata(f, opt) || readFlag(f, opt)) {
    *val = true;
    return true;
  }
  return false;
}

bool toObfuscateUint32Option(Function *f, std::string option, uint32_t *val) {
  if (readAnnotationMetadataUint32OptVal(f, option, val) ||
      readFlagUint32OptVal(f, option, val))
    return true;
  return false;
}

static bool valueEscapes(const Instruction &Inst) {
  if (!Inst.getType()->isSized())
    return false;

  const BasicBlock *BB = Inst.getParent();
  for (const User *U : Inst.users()) {
    const Instruction *UI = cast<Instruction>(U);
    if (UI->getParent() != BB || isa<PHINode>(UI))
      return true;
  }
  return false;
}

/** LLVM\llvm\lib\Transforms\Scalar\Reg2Mem.cpp
 * @brief 修复PHI指令和逃逸变量
 *
 * @param F
 */
void fixStack(Function &F) {
  // Insert all new allocas into entry block.
  BasicBlock *BBEntry = &F.getEntryBlock();
  assert(pred_empty(BBEntry) &&
         "Entry block to function must not have predecessors!");

  // Find first non-alloca instruction and create insertion point. This is
  // safe if block is well-formed: it always have terminator, otherwise
  // we'll get and assertion.
  BasicBlock::iterator I = BBEntry->begin();
  while (isa<AllocaInst>(I))
    ++I;

  CastInst *AllocaInsertionPoint = new BitCastInst(
      Constant::getNullValue(Type::getInt32Ty(F.getContext())),
      Type::getInt32Ty(F.getContext()), "fix_stack_point", &*I);

  // Find the escaped instructions. But don't create stack slots for
  // allocas in entry block.
  std::list<Instruction *> WorkList;
  for (Instruction &I : instructions(F))
    if (!(isa<AllocaInst>(I) && I.getParent() == BBEntry) && valueEscapes(I))
      WorkList.push_front(&I);

  // Demote escaped instructions
  //NumRegsDemoted += WorkList.size();
  for (Instruction *I : WorkList)
    DemoteRegToStack(*I, false, AllocaInsertionPoint->getIterator()); //fix for llvm 19

  WorkList.clear();

  // Find all phi's
  for (BasicBlock &BB : F)
    for (auto &Phi : BB.phis())
      WorkList.push_front(&Phi);

  // Demote phi nodes
  //NumPhisDemoted += WorkList.size();
  for (Instruction *I : WorkList)
    DemotePHIToStack(cast<PHINode>(I), AllocaInsertionPoint->getIterator()); //fix for llvm19
}

/**
 * @brief
 *
 * @param Func
 */
void FixFunctionConstantExpr(Function *Func) {
  // Replace ConstantExpr with equal instructions
  // Otherwise replacing on Constant will crash the compiler
  for (BasicBlock &BB : *Func) {
    FixBasicBlockConstantExpr(&BB);
  }
}
/**
 * @brief
 *
 * @param BB
 */
void FixBasicBlockConstantExpr(BasicBlock *BB) {
  // Replace ConstantExpr with equal instructions
  // Otherwise replacing on Constant will crash the compiler
  // Things to note:
  // - Phis must be placed at BB start so CEs must be placed prior to current BB
  assert(!BB->empty() && "BasicBlock is empty!");
  assert((BB->getParent() != NULL) && "BasicBlock must be in a Function!");
  Instruction *FunctionInsertPt =
      &*(BB->getParent()->getEntryBlock().getFirstInsertionPt());
  // Instruction* LocalBBInsertPt=&*(BB.getFirstInsertionPt());
  for (Instruction &I : *BB) {
    if (isa<LandingPadInst>(I) || isa<FuncletPadInst>(I)) {
      continue;
    }
    for (unsigned i = 0; i < I.getNumOperands(); i++) {
      if (ConstantExpr *C = dyn_cast<ConstantExpr>(I.getOperand(i))) {
        Instruction *InsertPt = &I;
        IRBuilder<NoFolder> IRB(InsertPt);
        if (isa<PHINode>(I)) {
          IRB.SetInsertPoint(FunctionInsertPt);
        }
        Instruction *Inst = IRB.Insert(C->getAsInstruction());
        I.setOperand(i, Inst);
      }
    }
  }
}

/**
 * @brief 随机字符串
 *
 * @param len
 * @return string
 */
string rand_str(int len) {
  string str;
  char c = 'O';
  int idx;
  for (idx = 0; idx < len; idx++) {

    switch ((rand() % 3)) {
    case 1:
      c = 'O';
      break;
    case 2:
      c = '0';
      break;
    default:
      c = 'o';
      break;
    }
    str.push_back(c);
  }
  return str;
}

// LLVM-MSVC有这个函数, 官方版LLVM没有 (LLVM:17.0.6 | LLVM-MSVC:3.2.6)
void LowerConstantExpr(Function &F) {
  SmallPtrSet<Instruction *, 8> WorkList;

  for (inst_iterator It = inst_begin(F), E = inst_end(F); It != E; ++It) {
    Instruction *I = &*It;

    if (isa<LandingPadInst>(I) || isa<CatchPadInst>(I) ||
        isa<CatchSwitchInst>(I) || isa<CatchReturnInst>(I))
      continue;
    if (auto *II = dyn_cast<IntrinsicInst>(I)) {
      if (II->getIntrinsicID() == Intrinsic::eh_typeid_for) {
        continue;
      }
    }

    for (unsigned int i = 0; i < I->getNumOperands(); ++i) {
      if (isa<ConstantExpr>(I->getOperand(i)))
        WorkList.insert(I);
    }
  }

  while (!WorkList.empty()) {
    auto It = WorkList.begin();
    Instruction *I = *It;
    WorkList.erase(*It);

    if (PHINode *PHI = dyn_cast<PHINode>(I)) {
      for (unsigned int i = 0; i < PHI->getNumIncomingValues(); ++i) {
        Instruction *TI = PHI->getIncomingBlock(i)->getTerminator();
        if (ConstantExpr *CE =
                dyn_cast<ConstantExpr>(PHI->getIncomingValue(i))) {
          Instruction *NewInst = CE->getAsInstruction();
          NewInst->insertBefore(TI);
          PHI->setIncomingValue(i, NewInst);
          WorkList.insert(NewInst);
        }
      }
    } else {
      for (unsigned int i = 0; i < I->getNumOperands(); ++i) {
        if (ConstantExpr *CE = dyn_cast<ConstantExpr>(I->getOperand(i))) {
          Instruction *NewInst = CE->getAsInstruction();
          NewInst->insertBefore(I);
          I->replaceUsesOfWith(CE, NewInst);
          WorkList.insert(NewInst);
        }
      }
    }
  }
}

uint64_t getRandomNumber() {
  return (((uint64_t)rand()) << 32) | ((uint64_t)rand());
}
uint32_t getUniqueNumber(std::vector<uint32_t> &rand_list) {
  uint32_t num = getRandomNumber() & 0xffffffff;
  while (true) {
    bool state = true;
    for (auto n = rand_list.begin(); n != rand_list.end(); n++) {
      if (*n == num) {
        state = false;
        break;
      }
    }

    if (state)
      break;
    num = getRandomNumber() & 0xffffffff;
  }
  return num;
}

void getRandomNoRepeat(unsigned upper_bound, unsigned size,
                       std::vector<unsigned> &result) {
  assert(upper_bound >= size);
  std::vector<unsigned> list;
  for (unsigned i = 0; i < upper_bound; i++) {
    list.push_back(i);
  }

  std::shuffle(list.begin(), list.end(), std::default_random_engine());
  for (unsigned i = 0; i < size; i++) {
    result.push_back(list[i]);
  }
}
// ax = 1 (mod m)
void exgcd(uint64_t a, uint64_t b, uint64_t &d, uint64_t &x, uint64_t &y) {
  if (!b) {
    d = a, x = 1, y = 0;
  } else {
    exgcd(b, a % b, d, y, x);
    y -= x * (a / b);
  }
}
uint64_t getInverse(uint64_t a, uint64_t m) {
  assert(a != 0);
  uint64_t x, y, d;
  exgcd(a, m, d, x, y);
  return d == 1 ? (x + m) % m : 0;
}


void demoteRegisters(Function *f) {
  std::vector<PHINode *> tmpPhi;
  std::vector<Instruction *> tmpReg;
  BasicBlock *bbEntry = &f->getEntryBlock();

  BasicBlock::iterator I = bbEntry->begin();
  while (isa<AllocaInst>(I))
    ++I;

  CastInst *AllocaInsertionPoint = new BitCastInst(Constant::getNullValue(Type::getInt32Ty(f->getContext())),
                      Type::getInt32Ty(f->getContext()), "demote_reg", &*I);


  for (Function::iterator i = f->begin(); i != f->end(); i++) {
    for (BasicBlock::iterator j = i->begin(); j != i->end(); j++) {
      if (isa<PHINode>(j)) {
        PHINode *phi = cast<PHINode>(j);
        tmpPhi.push_back(phi);
        continue;
      }
      if (!(isa<AllocaInst>(j) && j->getParent() == bbEntry) &&
          j->isUsedOutsideOfBlock(&*i)) {
        tmpReg.push_back(&*j);
        continue;
      }
    }
  }
  for (unsigned int i = 0; i < tmpReg.size(); i++)
    DemoteRegToStack(*tmpReg.at(i), false,AllocaInsertionPoint->getIterator());
  for (unsigned int i = 0; i < tmpPhi.size(); i++)
    DemotePHIToStack(tmpPhi.at(i), AllocaInsertionPoint->getIterator());
}



void turnOffOptimization(Function *f) {
  f->removeFnAttr(Attribute::AttrKind::MinSize);
  f->removeFnAttr(Attribute::AttrKind::OptimizeForSize);
  if (!f->hasFnAttribute(Attribute::AttrKind::OptimizeNone) &&
      !f->hasFnAttribute(Attribute::AttrKind::AlwaysInline)) {
    f->addFnAttr(Attribute::AttrKind::OptimizeNone);
    f->addFnAttr(Attribute::AttrKind::NoInline);
  }
}

static inline std::vector<std::string> splitString(std::string str) {
  std::stringstream ss(str);
  std::string word;
  std::vector<std::string> words;
  while (ss >> word)
    words.emplace_back(word);
  return words;
}


void annotation2Metadata(Module &M) {
  GlobalVariable *Annotations = M.getGlobalVariable("llvm.global.annotations");
  if (!Annotations)
    return;
  auto *C = dyn_cast<ConstantArray>(Annotations->getInitializer());
  if (!C)
    return;
  for (unsigned int i = 0; i < C->getNumOperands(); i++)
    if (ConstantStruct *CS = dyn_cast<ConstantStruct>(C->getOperand(i))) {
      GlobalValue *StrC =
          dyn_cast<GlobalValue>(CS->getOperand(1)->stripPointerCasts());
      if (!StrC)
        continue;
      ConstantDataSequential *StrData =
          dyn_cast<ConstantDataSequential>(StrC->getOperand(0));
      if (!StrData)
        continue;
      Function *Fn = dyn_cast<Function>(CS->getOperand(0)->stripPointerCasts());
      if (!Fn)
        continue;

      // Add annotation to the function.
      std::vector<std::string> strs =
          splitString(StrData->getAsCString().str());
      for (std::string str : strs)
        writeAnnotationMetadata(Fn, str);
    }
}

bool readAnnotationMetadata(Function *f, std::string annotation) {
  MDNode *Existing = f->getMetadata(obfkindid);
  if (Existing) {
    MDTuple *Tuple = cast<MDTuple>(Existing);
    for (auto &N : Tuple->operands())
      if (cast<MDString>(N.get())->getString() == annotation)
        return true;
  }
  return false;
}

void writeAnnotationMetadata(Function *f, std::string annotation) {
  LLVMContext &Context = f->getContext();
  MDBuilder MDB(Context);

  MDNode *Existing = f->getMetadata(obfkindid);
  SmallVector<Metadata *, 4> Names;
  bool AppendName = true;
  if (Existing) {
    MDTuple *Tuple = cast<MDTuple>(Existing);
    for (auto &N : Tuple->operands()) {
      if (cast<MDString>(N.get())->getString() == annotation)
        AppendName = false;
      Names.emplace_back(N.get());
    }
  }
  if (AppendName)
    Names.emplace_back(MDB.createString(annotation));

  MDNode *MD = MDTuple::get(Context, Names);
  f->setMetadata(obfkindid, MD);
}



#if 0
std::map<GlobalValue *, StringRef> BuildAnnotateMap(Module &M) {
  std::map<GlobalValue *, StringRef> VAMap;
  GlobalVariable *glob = M.getGlobalVariable("llvm.global.annotations");
  if (glob != nullptr && glob->hasInitializer()) {
    ConstantArray *CDA = cast<ConstantArray>(glob->getInitializer());
    for (Value *op : CDA->operands()) {
      ConstantStruct *anStruct = cast<ConstantStruct>(op);
      /*
        Structure: [Value,Annotation,SourceFilePath,LineNumber]
        Usually wrapped inside GEP/BitCast
        We only care about Value and Annotation Here
      */
      GlobalValue *Value =
          cast<GlobalValue>(anStruct->getOperand(0)->getOperand(0));
      GlobalVariable *Annotation =
          cast<GlobalVariable>(anStruct->getOperand(1)->getOperand(0));
      if (Annotation->hasInitializer()) {
        VAMap[Value] =
            cast<ConstantDataSequential>(Annotation->getInitializer())
                ->getAsCString();
      }
    }
  }
  return VAMap;
}
#endif

}  //namespace llvm