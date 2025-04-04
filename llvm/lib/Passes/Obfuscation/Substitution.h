//===- SubstitutionIncludes.h - Substitution Obfuscation pass-------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains includes and defines for the substitution pass
//
//===----------------------------------------------------------------------===//

#ifndef _SUBSTITUTIONS_H_
#define _SUBSTITUTIONS_H_


// LLVM include
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/PassManager.h" //new Pass
#include "llvm/Transforms/IPO.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"
#include "CryptoUtils.h"

// Namespace
using namespace llvm;

#define NUMBER_ADD_SUBST 7
#define NUMBER_SUB_SUBST 6
#define NUMBER_AND_SUBST 6
#define NUMBER_OR_SUBST 6
#define NUMBER_XOR_SUBST 6
#define NUMBER_MUL_SUBST 2

namespace llvm {
    class SubstitutionPass : public PassInfoMixin<SubstitutionPass> {
        public:
          bool flag;
          void (SubstitutionPass::*funcAdd[NUMBER_ADD_SUBST])(BinaryOperator *bo);
          void (SubstitutionPass::*funcSub[NUMBER_SUB_SUBST])(BinaryOperator *bo);
          void (SubstitutionPass::*funcAnd[NUMBER_AND_SUBST])(BinaryOperator *bo);
          void (SubstitutionPass::*funcOr[NUMBER_OR_SUBST])(BinaryOperator *bo);
          void (SubstitutionPass::*funcXor[NUMBER_XOR_SUBST])(BinaryOperator *bo);
          void (SubstitutionPass::*funcMul[NUMBER_MUL_SUBST])(BinaryOperator *bo); //added for hikari

          SubstitutionPass(bool flag) {
            this->flag = flag;
            funcAdd[0] = &SubstitutionPass::addNeg;
            funcAdd[1] = &SubstitutionPass::addDoubleNeg;
            funcAdd[2] = &SubstitutionPass::addRand;
            funcAdd[3] = &SubstitutionPass::addRand2;
            funcAdd[4] = &SubstitutionPass::addSubstitution;
            funcAdd[5] = &SubstitutionPass::addSubstitution2;
            funcAdd[6] = &SubstitutionPass::addSubstitution3;

            funcSub[0] = &SubstitutionPass::subNeg;
            funcSub[1] = &SubstitutionPass::subRand;
            funcSub[2] = &SubstitutionPass::subRand2;
            funcSub[3] = &SubstitutionPass::subSubstitution;
            funcSub[4] = &SubstitutionPass::subSubstitution2;
            funcSub[5] = &SubstitutionPass::subSubstitution3;

            funcAnd[0] = &SubstitutionPass::andSubstitution;
            funcAnd[1] = &SubstitutionPass::andSubstitutionRand;
            funcAnd[2] = &SubstitutionPass::andSubstitution2;
            funcAnd[3] = &SubstitutionPass::andSubstitution3;
            funcAnd[4] = &SubstitutionPass::andNor;
            funcAnd[5] = &SubstitutionPass::andNand;

            funcOr[0] = &SubstitutionPass::orSubstitution;
            funcOr[1] = &SubstitutionPass::orSubstitutionRand;
            funcOr[2] = &SubstitutionPass::orSubstitution2;
            funcOr[3] = &SubstitutionPass::orSubstitution3;
            funcOr[4] = &SubstitutionPass::orNor;
            funcOr[5] = &SubstitutionPass::orNand;

            funcXor[0] = &SubstitutionPass::xorSubstitution;
            funcXor[1] = &SubstitutionPass::xorSubstitutionRand;
            funcXor[2] = &SubstitutionPass::xorSubstitution2;
            funcXor[3] = &SubstitutionPass::xorSubstitution3;
            funcXor[4] = &SubstitutionPass::xorNor;
            funcXor[5] = &SubstitutionPass::xorNand;


            funcMul[0] = &SubstitutionPass::mulSubstitution;
            funcMul[1] = &SubstitutionPass::mulSubstitution2;
          }

          PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
          bool substitute(Function *f);

          void addNeg(BinaryOperator *bo);
          void addDoubleNeg(BinaryOperator *bo);
          void addRand(BinaryOperator *bo);
          void addRand2(BinaryOperator *bo);
          void addSubstitution(BinaryOperator *bo);  //from hikari llvm15
          void addSubstitution2(BinaryOperator *bo);  //from hikari llvm15
          void addSubstitution3(BinaryOperator *bo);  //from hikari llvm15


          void subNeg(BinaryOperator *bo);
          void subRand(BinaryOperator *bo);
          void subRand2(BinaryOperator *bo);
          void subSubstitution(BinaryOperator *bo);  //from hikari llvm15
          void subSubstitution2(BinaryOperator *bo);  //from hikari llvm15
          void subSubstitution3(BinaryOperator *bo);  //from hikari llvm15

          void andSubstitution(BinaryOperator *bo);
          void andSubstitutionRand(BinaryOperator *bo);
          void andSubstitution2(BinaryOperator *bo);  //from hikari llvm15
          void andSubstitution3(BinaryOperator *bo);  //from hikari llvm15
          void andNor(BinaryOperator *bo);  //from hikari llvm15
          void andNand(BinaryOperator *bo);  //from hikari llvm15

          void orSubstitution(BinaryOperator *bo);
          void orSubstitutionRand(BinaryOperator *bo);
          void orSubstitution2(BinaryOperator *bo);  //from hikari llvm15
          void orSubstitution3(BinaryOperator *bo);  //from hikari llvm15
          void orNor(BinaryOperator *bo);  //from hikari llvm15
          void orNand(BinaryOperator *bo);  //from hikari llvm15

          void xorSubstitution(BinaryOperator *bo);
          void xorSubstitutionRand(BinaryOperator *bo);
          void xorSubstitution2(BinaryOperator *bo);  //from hikari llvm15
          void xorSubstitution3(BinaryOperator *bo);  //from hikari llvm15
          void xorNor(BinaryOperator *bo);  //from hikari llvm15
          void xorNand(BinaryOperator *bo);  //from hikari llvm15

          void mulSubstitution(BinaryOperator *bo);  //from hikari llvm15
          void mulSubstitution2(BinaryOperator *bo);  //from hikari llvm15

          BinaryOperator *buildNor(Value *a, Value *b, Instruction *insertBefore);

          BinaryOperator *buildNand(Value *a, Value *b,
                                 Instruction *insertBefore);

          static bool isRequired() { return true; } // 直接返回true即可
    };
    SubstitutionPass *createSubstitutionPass(bool flag); // 创建基本块分割
} // namespace llvm

#endif

