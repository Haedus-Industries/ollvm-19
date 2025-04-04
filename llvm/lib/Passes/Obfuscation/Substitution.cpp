//===- Substitution.cpp - Substitution Obfuscation
// pass-------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements operators substitution's pass
//
//===----------------------------------------------------------------------===//

#include "Substitution.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/raw_ostream.h"
#include "Utils.h"
#include "llvm/IR/Intrinsics.h"

#define DEBUG_TYPE "substitution"

static cl::opt<int> ObfTimes("sub_loop",
         cl::desc("Choose how many time the -sub pass loops on a function"),
         cl::value_desc("number of times"), cl::init(1), cl::Optional);


// Stats
STATISTIC(Add, "Add substitued");
STATISTIC(Sub, "Sub substitued");
STATISTIC(Mul,  "Mul substitued");
// STATISTIC(Div,  "Div substitued");
// STATISTIC(Rem,  "Rem substitued");
// STATISTIC(Shi,  "Shift substitued");
STATISTIC(And, "And substitued");
STATISTIC(Or, "Or substitued");
STATISTIC(Xor, "Xor substitued");

PreservedAnalyses SubstitutionPass::run(Function &F, FunctionAnalysisManager &AM) {
   // Check if the percentage is correct
   if (ObfTimes <= 0) {
     errs() << "Substitution application number -sub_loop=x must be x > 0";
     return PreservedAnalyses::all();
   }

  Function *tmp = &F;
  // Do we obfuscate
  if (toObfuscate(flag, tmp, "sub")) {
    substitute(tmp);
    return PreservedAnalyses::none();
  }

  return PreservedAnalyses::all();
}

bool SubstitutionPass::substitute(Function *f) {
  Function *tmp = f;

  // Loop for the number of time we run the pass on the function
  int times = ObfTimes;
  do {
    for (Function::iterator bb = tmp->begin(); bb != tmp->end(); ++bb) {
      for (BasicBlock::iterator inst = bb->begin(); inst != bb->end(); ++inst) {
        if (inst->isBinaryOp()) {
          switch (inst->getOpcode()) {
          case BinaryOperator::Add:
            // case BinaryOperator::FAdd:
            // Substitute with random add operation
            (this->*funcAdd[llvm::cryptoutils->get_range(NUMBER_ADD_SUBST)])(
                cast<BinaryOperator>(inst));
            ++Add;
            break;
          case BinaryOperator::Sub:
            // case BinaryOperator::FSub:
            // Substitute with random sub operation
            (this->*funcSub[llvm::cryptoutils->get_range(NUMBER_SUB_SUBST)])(
                cast<BinaryOperator>(inst));
            ++Sub;
            break;
          case BinaryOperator::Mul:
          //case BinaryOperator::FMul:
            (this->*funcMul[llvm::cryptoutils->get_range(NUMBER_MUL_SUBST)])(
                cast<BinaryOperator>(inst));
            ++Mul;
            break;
          case BinaryOperator::UDiv:
          case BinaryOperator::SDiv:
          case BinaryOperator::FDiv:
            //++Div;
            break;
          case BinaryOperator::URem:
          case BinaryOperator::SRem:
          case BinaryOperator::FRem:
            //++Rem;
            break;
          case Instruction::Shl:
            //++Shi;
            break;
          case Instruction::LShr:
            //++Shi;
            break;
          case Instruction::AShr:
            //++Shi;
            break;
          case Instruction::And:
            (this->*
             funcAnd[llvm::cryptoutils->get_range(NUMBER_AND_SUBST)])(cast<BinaryOperator>(inst));
            ++And;
            break;
          case Instruction::Or:
            (this->*
             funcOr[llvm::cryptoutils->get_range(NUMBER_OR_SUBST)])(cast<BinaryOperator>(inst));
            ++Or;
            break;
          case Instruction::Xor:
            (this->*
             funcXor[llvm::cryptoutils->get_range(NUMBER_XOR_SUBST)])(cast<BinaryOperator>(inst));
            ++Xor;
            break;
          default:
            break;
          }              // End switch
        }                // End isBinaryOp
      }                  // End for basickblock
    }                    // End for Function
  } while (--times > 0); // for times
  return false;
}

// Implementation of a = b - (-c)
void SubstitutionPass::addNeg(BinaryOperator *bo) {
  BinaryOperator *op = NULL;

  // Create sub
  if (bo->getOpcode() == Instruction::Add) {
    op = BinaryOperator::CreateNeg(bo->getOperand(1), "", bo);
    op =
        BinaryOperator::Create(Instruction::Sub, bo->getOperand(0), op, "", bo);

    // Check signed wrap
    //op->setHasNoSignedWrap(bo->hasNoSignedWrap());
    //op->setHasNoUnsignedWrap(bo->hasNoUnsignedWrap());

    bo->replaceAllUsesWith(op);
  }/* else {
    op = BinaryOperator::CreateFNeg(bo->getOperand(1), "", bo);
    op = BinaryOperator::Create(Instruction::FSub, bo->getOperand(0), op, "",
                                bo);
  }*/
}

// Implementation of a = -(-b + (-c)) 
void SubstitutionPass::addDoubleNeg(BinaryOperator *bo) { 
  BinaryOperator *op, *op2 = NULL; 
  UnaryOperator *op3, *op4; 
  if (bo->getOpcode() == Instruction::Add) { 
    op = BinaryOperator::CreateNeg(bo->getOperand(0), "", bo); 
    op2 = BinaryOperator::CreateNeg(bo->getOperand(1), "", bo); 
    op = BinaryOperator::Create(Instruction::Add, op, op2, "", bo); 
    op = BinaryOperator::CreateNeg(op, "", bo); 
    bo->replaceAllUsesWith(op); 
    // Check signed wrap 
    //op->setHasNoSignedWrap(bo->hasNoSignedWrap()); 
    //op->setHasNoUnsignedWrap(bo->hasNoUnsignedWrap()); 
  } else { 
    op3 = UnaryOperator::CreateFNeg(bo->getOperand(0), "", bo); 
    op4 = UnaryOperator::CreateFNeg(bo->getOperand(1), "", bo); 
    op = BinaryOperator::Create(Instruction::FAdd, op3, op4, "", bo); 
    op3 = UnaryOperator::CreateFNeg(op, "", bo); 
    bo->replaceAllUsesWith(op3); 
  }   
}

// Implementation of  r = rand (); a = b + r; a = a + c; a = a - r
void SubstitutionPass::addRand(BinaryOperator *bo) {
  BinaryOperator *op = NULL;

  if (bo->getOpcode() == Instruction::Add) {
    Type *ty = bo->getType();
    ConstantInt *co =
        (ConstantInt *)ConstantInt::get(ty, llvm::cryptoutils->get_uint64_t());
    op =
        BinaryOperator::Create(Instruction::Add, bo->getOperand(0), co, "", bo);
    op =
        BinaryOperator::Create(Instruction::Add, op, bo->getOperand(1), "", bo);
    op = BinaryOperator::Create(Instruction::Sub, op, co, "", bo);

    // Check signed wrap
    //op->setHasNoSignedWrap(bo->hasNoSignedWrap());
    //op->setHasNoUnsignedWrap(bo->hasNoUnsignedWrap());

    bo->replaceAllUsesWith(op);
  }
  /* else {
      Type *ty = bo->getType();
      ConstantFP *co =
  (ConstantFP*)ConstantFP::get(ty,(float)llvm::cryptoutils->get_uint64_t());
      op = BinaryOperator::Create(Instruction::FAdd,bo->getOperand(0),co,"",bo);
      op = BinaryOperator::Create(Instruction::FAdd,op,bo->getOperand(1),"",bo);
      op = BinaryOperator::Create(Instruction::FSub,op,co,"",bo);
  } */
}

// Implementation of r = rand (); a = b - r; a = a + b; a = a + r
void SubstitutionPass::addRand2(BinaryOperator *bo) {
  BinaryOperator *op = NULL;

  if (bo->getOpcode() == Instruction::Add) {
    Type *ty = bo->getType();
    ConstantInt *co =
        (ConstantInt *)ConstantInt::get(ty, llvm::cryptoutils->get_uint64_t());
    op =
        BinaryOperator::Create(Instruction::Sub, bo->getOperand(0), co, "", bo);
    op =
        BinaryOperator::Create(Instruction::Add, op, bo->getOperand(1), "", bo);
    op = BinaryOperator::Create(Instruction::Add, op, co, "", bo);

    // Check signed wrap
    //op->setHasNoSignedWrap(bo->hasNoSignedWrap());
    //op->setHasNoUnsignedWrap(bo->hasNoUnsignedWrap());

    bo->replaceAllUsesWith(op);
  }
  /* else {
      Type *ty = bo->getType();
      ConstantFP *co =
  (ConstantFP*)ConstantFP::get(ty,(float)llvm::cryptoutils->get_uint64_t());
      op = BinaryOperator::Create(Instruction::FAdd,bo->getOperand(0),co,"",bo);
      op = BinaryOperator::Create(Instruction::FAdd,op,bo->getOperand(1),"",bo);
      op = BinaryOperator::Create(Instruction::FSub,op,co,"",bo);
  } */
}

// Implementation of a = b + c => a = b - ~c - 1
void SubstitutionPass::addSubstitution(BinaryOperator *bo) {
  BinaryOperator *op = NULL;

  if (bo->getOpcode() == Instruction::Add) {
      ConstantInt *co = (ConstantInt *)ConstantInt::get(bo->getType(), 1);
      BinaryOperator *op = BinaryOperator::CreateNot(bo->getOperand(1), "", bo);
      BinaryOperator *op1 = BinaryOperator::CreateNeg(co, "", bo);
      op = BinaryOperator::Create(Instruction::Sub, op, op1, "", bo);
      op = BinaryOperator::Create(Instruction::Sub, bo->getOperand(0), op, "", bo);
      bo->replaceAllUsesWith(op);
  }
}

// Implementation of a = b + c => a = (b | c) + (b & c)
void SubstitutionPass::addSubstitution2(BinaryOperator *bo) {
  BinaryOperator *op = NULL;

  if (bo->getOpcode() == Instruction::Add) {
      BinaryOperator *op = BinaryOperator::Create(
          Instruction::And, bo->getOperand(0), bo->getOperand(1), "", bo);
      BinaryOperator *op1 = BinaryOperator::Create(
          Instruction::Or, bo->getOperand(0), bo->getOperand(1), "", bo);
      op = BinaryOperator::Create(Instruction::Add, op, op1, "", bo);
      bo->replaceAllUsesWith(op);
  }
}

// Implementation of a = b + c => a = (b ^ c) + (b & c) * 2
void SubstitutionPass::addSubstitution3(BinaryOperator *bo) {
  BinaryOperator *op = NULL;

  if (bo->getOpcode() == Instruction::Add) {
      ConstantInt *co = (ConstantInt *)ConstantInt::get(bo->getType(), 2);
      BinaryOperator *op = BinaryOperator::Create(
          Instruction::And, bo->getOperand(0), bo->getOperand(1), "", bo);
      op = BinaryOperator::Create(Instruction::Mul, op, co, "", bo);
      BinaryOperator *op1 = BinaryOperator::Create(
          Instruction::Xor, bo->getOperand(0), bo->getOperand(1), "", bo);
      op = BinaryOperator::Create(Instruction::Add, op1, op, "", bo);
      bo->replaceAllUsesWith(op);
  }
}

// Implementation of a = b + (-c) 
void SubstitutionPass::subNeg(BinaryOperator *bo) { 
  BinaryOperator *op = NULL;   
  if (bo->getOpcode() == Instruction::Sub) { 
    op = BinaryOperator::CreateNeg(bo->getOperand(1), "", bo); 
    op = BinaryOperator::Create(Instruction::Add, bo->getOperand(0), op, "", bo); 
    // Check signed wrap 
    //op->setHasNoSignedWrap(bo->hasNoSignedWrap()); 
    //op->setHasNoUnsignedWrap(bo->hasNoUnsignedWrap()); 
  } else { 
    auto op1 = UnaryOperator::CreateFNeg(bo->getOperand(1), "", bo); 
    op = BinaryOperator::Create(Instruction::FAdd, bo->getOperand(0), op1, "", bo); 
  } 
  bo->replaceAllUsesWith(op); 
}

// Implementation of  r = rand (); a = b + r; a = a - c; a = a - r
void SubstitutionPass::subRand(BinaryOperator *bo) {
  BinaryOperator *op = NULL;

  if (bo->getOpcode() == Instruction::Sub) {
    Type *ty = bo->getType();
    ConstantInt *co =
        (ConstantInt *)ConstantInt::get(ty, llvm::cryptoutils->get_uint64_t());
    op =
        BinaryOperator::Create(Instruction::Add, bo->getOperand(0), co, "", bo);
    op =
        BinaryOperator::Create(Instruction::Sub, op, bo->getOperand(1), "", bo);
    op = BinaryOperator::Create(Instruction::Sub, op, co, "", bo);

    // Check signed wrap
    //op->setHasNoSignedWrap(bo->hasNoSignedWrap());
    //op->setHasNoUnsignedWrap(bo->hasNoUnsignedWrap());

    bo->replaceAllUsesWith(op);
  }
  /* else {
      Type *ty = bo->getType();
      ConstantFP *co =
  (ConstantFP*)ConstantFP::get(ty,(float)llvm::cryptoutils->get_uint64_t());
      op = BinaryOperator::Create(Instruction::FAdd,bo->getOperand(0),co,"",bo);
      op = BinaryOperator::Create(Instruction::FSub,op,bo->getOperand(1),"",bo);
      op = BinaryOperator::Create(Instruction::FSub,op,co,"",bo);
  } */
}

// Implementation of  r = rand (); a = b - r; a = a - c; a = a + r
void SubstitutionPass::subRand2(BinaryOperator *bo) {
  BinaryOperator *op = NULL;

  if (bo->getOpcode() == Instruction::Sub) {
    Type *ty = bo->getType();
    ConstantInt *co =
        (ConstantInt *)ConstantInt::get(ty, llvm::cryptoutils->get_uint64_t());
    op =
        BinaryOperator::Create(Instruction::Sub, bo->getOperand(0), co, "", bo);
    op =
        BinaryOperator::Create(Instruction::Sub, op, bo->getOperand(1), "", bo);
    op = BinaryOperator::Create(Instruction::Add, op, co, "", bo);

    // Check signed wrap
    //op->setHasNoSignedWrap(bo->hasNoSignedWrap());
    //op->setHasNoUnsignedWrap(bo->hasNoUnsignedWrap());

    bo->replaceAllUsesWith(op);
  }
  /* else {
      Type *ty = bo->getType();
      ConstantFP *co =
  (ConstantFP*)ConstantFP::get(ty,(float)llvm::cryptoutils->get_uint64_t());
      op = BinaryOperator::Create(Instruction::FSub,bo->getOperand(0),co,"",bo);
      op = BinaryOperator::Create(Instruction::FSub,op,bo->getOperand(1),"",bo);
      op = BinaryOperator::Create(Instruction::FAdd,op,co,"",bo);
  } */
}


// Implementation of a = b - c => a = (b & ~c) - (~b & c)
void SubstitutionPass::subSubstitution(BinaryOperator *bo) {
  BinaryOperator *op = NULL;

  if (bo->getOpcode() == Instruction::Sub) {
      BinaryOperator *op1 = BinaryOperator::CreateNot(bo->getOperand(0), "", bo);
      BinaryOperator *op =
          BinaryOperator::Create(Instruction::And, op1, bo->getOperand(1), "", bo);
      op1 = BinaryOperator::CreateNot(bo->getOperand(1), "", bo);
      BinaryOperator *op2 =
          BinaryOperator::Create(Instruction::And, bo->getOperand(0), op1, "", bo);
      op = BinaryOperator::Create(Instruction::Sub, op2, op, "", bo);
      bo->replaceAllUsesWith(op);
  }
}

// Implementation of a = b - c => a = (2 * (b & ~c)) - (b ^ c)
void SubstitutionPass::subSubstitution2(BinaryOperator *bo) {
  BinaryOperator *op = NULL;

  if (bo->getOpcode() == Instruction::Sub) {
      ConstantInt *co = (ConstantInt *)ConstantInt::get(bo->getType(), 2);
      BinaryOperator *op1 = BinaryOperator::Create(
          Instruction::Xor, bo->getOperand(0), bo->getOperand(1), "", bo);
      BinaryOperator *op = BinaryOperator::CreateNot(bo->getOperand(1), "", bo);
      op = BinaryOperator::Create(Instruction::And, bo->getOperand(0), op, "", bo);
      op = BinaryOperator::Create(Instruction::Mul, co, op, "", bo);
      op = BinaryOperator::Create(Instruction::Sub, op, op1, "", bo);
      bo->replaceAllUsesWith(op);
  }
}

// Implementation of a = b - c => a = b + ~c + 1
void SubstitutionPass::subSubstitution3(BinaryOperator *bo) {
  BinaryOperator *op = NULL;

  if (bo->getOpcode() == Instruction::Sub) {
      ConstantInt *co = (ConstantInt *)ConstantInt::get(bo->getType(), 1);
      BinaryOperator *op1 = BinaryOperator::CreateNot(bo->getOperand(1), "", bo);
      BinaryOperator *op =
          BinaryOperator::Create(Instruction::Add, bo->getOperand(0), op1, "", bo);
      op = BinaryOperator::Create(Instruction::Add, op, co, "", bo);
      bo->replaceAllUsesWith(op);
  }
}


// Implementation of a = b & c => a = (b^~c)& b
void SubstitutionPass::andSubstitution(BinaryOperator *bo) {
  BinaryOperator *op = NULL;

  // Create NOT on second operand => ~c
  op = BinaryOperator::CreateNot(bo->getOperand(1), "", bo);

  // Create XOR => (b^~c)
  BinaryOperator *op1 =
      BinaryOperator::Create(Instruction::Xor, bo->getOperand(0), op, "", bo);

  // Create AND => (b^~c) & b
  op = BinaryOperator::Create(Instruction::And, op1, bo->getOperand(0), "", bo);
  bo->replaceAllUsesWith(op);
}

// Implementation of a = a & b <=> ~(~a | ~b) & (r | ~r)
void SubstitutionPass::andSubstitutionRand(BinaryOperator *bo) {
  // Copy of the BinaryOperator type to create the random number with the
  // same type of the operands
  Type *ty = bo->getType();

  // r (Random number)
  ConstantInt *co =
      (ConstantInt *)ConstantInt::get(ty, llvm::cryptoutils->get_uint64_t());

  // ~a
  BinaryOperator *op = BinaryOperator::CreateNot(bo->getOperand(0), "", bo);

  // ~b
  BinaryOperator *op1 = BinaryOperator::CreateNot(bo->getOperand(1), "", bo);

  // ~r
  BinaryOperator *opr = BinaryOperator::CreateNot(co, "", bo);

  // (~a | ~b)
  BinaryOperator *opa =
      BinaryOperator::Create(Instruction::Or, op, op1, "", bo);

  // (r | ~r)
  opr = BinaryOperator::Create(Instruction::Or, co, opr, "", bo);

  // ~(~a | ~b)
  op = BinaryOperator::CreateNot(opa, "", bo);

  // ~(~a | ~b) & (r | ~r)
  op = BinaryOperator::Create(Instruction::And, op, opr, "", bo);

  // We replace all the old AND operators with the new one transformed
  bo->replaceAllUsesWith(op);
}



// Implementation of a = b & c => a = (b | c) & ~(b ^ c)
void SubstitutionPass::andSubstitution2(BinaryOperator *bo) {
  BinaryOperator *op1 = BinaryOperator::Create(
      Instruction::Xor, bo->getOperand(0), bo->getOperand(1), "", bo);
  op1 = BinaryOperator::CreateNot(op1, "", bo);
  BinaryOperator *op = BinaryOperator::Create(
      Instruction::Or, bo->getOperand(0), bo->getOperand(1), "", bo);
  op = BinaryOperator::Create(Instruction::And, op, op1, "", bo);
  bo->replaceAllUsesWith(op);
}

// Implementation of a = b & c => a = (~b | c) + (b + 1)
void SubstitutionPass::andSubstitution3(BinaryOperator *bo) {
  ConstantInt *co = (ConstantInt *)ConstantInt::get(bo->getType(), 1);
  BinaryOperator *op1 =
      BinaryOperator::Create(Instruction::Add, bo->getOperand(0), co, "", bo);
  BinaryOperator *op = BinaryOperator::CreateNot(bo->getOperand(0), "", bo);
  op = BinaryOperator::Create(Instruction::Or, op, bo->getOperand(1), "", bo);
  op = BinaryOperator::Create(Instruction::Add, op, op1, "", bo);
  bo->replaceAllUsesWith(op);
}

// Implementation of a = a & b => Nor(Nor(a, a), Nor(b, b))
void SubstitutionPass::andNor(BinaryOperator *bo) {
  BinaryOperator *noraa = buildNor(bo->getOperand(0), bo->getOperand(0), bo);
  BinaryOperator *norbb = buildNor(bo->getOperand(1), bo->getOperand(1), bo);
  bo->replaceAllUsesWith(buildNor(noraa, norbb, bo));
}

// Implementation of a = a & b => Nand(Nand(a, b), Nand(a, b))
void SubstitutionPass::andNand(BinaryOperator *bo) {
  BinaryOperator *nandab = buildNand(bo->getOperand(0), bo->getOperand(1), bo);
  BinaryOperator *nandab2 = buildNand(bo->getOperand(0), bo->getOperand(1), bo);
  bo->replaceAllUsesWith(buildNand(nandab, nandab2, bo));
}



// Implementation of a = b | c => a = (b & c) | (b ^ c)
void SubstitutionPass::orSubstitution(BinaryOperator *bo) {
  BinaryOperator *op = NULL;

  // Creating first operand (b & c)
  op = BinaryOperator::Create(Instruction::And, bo->getOperand(0),
                              bo->getOperand(1), "", bo);

  // Creating second operand (b ^ c)
  BinaryOperator *op1 = BinaryOperator::Create(
      Instruction::Xor, bo->getOperand(0), bo->getOperand(1), "", bo);

  // final op
  op = BinaryOperator::Create(Instruction::Or, op, op1, "", bo);
  bo->replaceAllUsesWith(op);
}

// Implementation of a = a | b =>
// a = (((~a & r) | (a & ~r)) ^ ((~b & r) | (b & ~r))) | (~(~a | ~b) & (r | ~r))
void SubstitutionPass::orSubstitutionRand(BinaryOperator *bo) {

  Type *ty = bo->getType();
  ConstantInt *co =
      (ConstantInt *)ConstantInt::get(ty, llvm::cryptoutils->get_uint64_t());

  // ~a
  BinaryOperator *op = BinaryOperator::CreateNot(bo->getOperand(0), "", bo);

  // ~b
  BinaryOperator *op1 = BinaryOperator::CreateNot(bo->getOperand(1), "", bo);

  // ~r
  BinaryOperator *op2 = BinaryOperator::CreateNot(co, "", bo);

  // ~a & r
  BinaryOperator *op3 =
      BinaryOperator::Create(Instruction::And, op, co, "", bo);

  // a & ~r
  BinaryOperator *op4 =
      BinaryOperator::Create(Instruction::And, bo->getOperand(0), op2, "", bo);

  // ~b & r
  BinaryOperator *op5 =
      BinaryOperator::Create(Instruction::And, op1, co, "", bo);

  // b & ~r
  BinaryOperator *op6 =
      BinaryOperator::Create(Instruction::And, bo->getOperand(1), op2, "", bo);

  // (~a & r) | (a & ~r)
  op3 = BinaryOperator::Create(Instruction::Or, op3, op4, "", bo);

  // (~b & r) | (b & ~r)
  op4 = BinaryOperator::Create(Instruction::Or, op5, op6, "", bo);

  // ((~a & r) | (a & ~r)) ^ ((~b & r) | (b & ~r))
  op5 = BinaryOperator::Create(Instruction::Xor, op3, op4, "", bo);

  // ~a | ~b
  op3 = BinaryOperator::Create(Instruction::Or, op, op1, "", bo);

  // ~(~a | ~b)
  op3 = BinaryOperator::CreateNot(op3, "", bo);

  // r | ~r
  op4 = BinaryOperator::Create(Instruction::Or, co, op2, "", bo);

  // ~(~a | ~b) & (r | ~r)
  op4 = BinaryOperator::Create(Instruction::And, op3, op4, "", bo);

  // (((~a & r) | (a & ~r)) ^ ((~b & r) | (b & ~r))) | (~(~a | ~b) & (r | ~r))
  op = BinaryOperator::Create(Instruction::Or, op5, op4, "", bo);
  bo->replaceAllUsesWith(op);
}


// Implementation of a = a | b => a = (b + (b ^ c)) - (b & ~c)
void SubstitutionPass::orSubstitution2(BinaryOperator *bo) {
  BinaryOperator *op1 = BinaryOperator::CreateNot(bo->getOperand(1), "", bo);
  op1 =
      BinaryOperator::Create(Instruction::And, bo->getOperand(0), op1, "", bo);
  BinaryOperator *op = BinaryOperator::Create(
      Instruction::Xor, bo->getOperand(0), bo->getOperand(1), "", bo);
  op = BinaryOperator::Create(Instruction::Add, bo->getOperand(0), op, "", bo);
  op = BinaryOperator::Create(Instruction::Sub, op, op1, "", bo);
  bo->replaceAllUsesWith(op);
}

// Implementation of a = a | b => a = (b + c + 1) + ~(c & b)
void SubstitutionPass::orSubstitution3(BinaryOperator *bo) {
  ConstantInt *co = (ConstantInt *)ConstantInt::get(bo->getType(), 1);
  BinaryOperator *op1 = BinaryOperator::Create(
      Instruction::And, bo->getOperand(1), bo->getOperand(0), "", bo);
  op1 = BinaryOperator::CreateNot(op1, "", bo);
  BinaryOperator *op = BinaryOperator::Create(
      Instruction::Add, bo->getOperand(0), bo->getOperand(1), "", bo);
  op = BinaryOperator::Create(Instruction::Add, op, co, "", bo);
  op = BinaryOperator::Create(Instruction::Add, op, op1, "", bo);
  bo->replaceAllUsesWith(op);
}


// Implementation of a = a | b => a = Nor(Nor(a, b), Nor(a, b))
void SubstitutionPass::orNor(BinaryOperator *bo) {
  BinaryOperator *norab = buildNor(bo->getOperand(0), bo->getOperand(1), bo);
  BinaryOperator *norab2 = buildNor(bo->getOperand(0), bo->getOperand(1), bo);
  BinaryOperator *op = buildNor(norab, norab2, bo);
  bo->replaceAllUsesWith(op);
}

// Implementation of a = a | b => a = Nand(Nand(a, a), Nand(b, b))
void SubstitutionPass::orNand(BinaryOperator *bo) {
  BinaryOperator *nandaa = buildNand(bo->getOperand(0), bo->getOperand(0), bo);
  BinaryOperator *nandbb = buildNand(bo->getOperand(1), bo->getOperand(1), bo);
  BinaryOperator *op = buildNand(nandaa, nandbb, bo);
  bo->replaceAllUsesWith(op);
}


// Implementation of a = a ^ b => a = (~a & b) | (a & ~b)
void SubstitutionPass::xorSubstitution(BinaryOperator *bo) {
  BinaryOperator *op = NULL;

  // Create NOT on first operand
  op = BinaryOperator::CreateNot(bo->getOperand(0), "", bo); // ~a

  // Create AND
  op = BinaryOperator::Create(Instruction::And, bo->getOperand(1), op, "",
                              bo); // ~a & b

  // Create NOT on second operand
  BinaryOperator *op1 =
      BinaryOperator::CreateNot(bo->getOperand(1), "", bo); // ~b

  // Create AND
  op1 = BinaryOperator::Create(Instruction::And, bo->getOperand(0), op1, "",
                               bo); // a & ~b

  // Create OR
  op = BinaryOperator::Create(Instruction::Or, op, op1, "",
                              bo); // (~a & b) | (a & ~b)
  bo->replaceAllUsesWith(op);
}

// implementation of a = a ^ b <=> (a ^ r) ^ (b ^ r) <=> 
// ((~a & r) | (a & ~r)) ^ ((~b & r) | (b & ~r))
// note : r is a random number
void SubstitutionPass::xorSubstitutionRand(BinaryOperator *bo) {
  BinaryOperator *op = NULL;

  Type *ty = bo->getType();
  ConstantInt *co =
      (ConstantInt *)ConstantInt::get(ty, llvm::cryptoutils->get_uint64_t());

  // ~a
  op = BinaryOperator::CreateNot(bo->getOperand(0), "", bo);

  // ~a & r
  op = BinaryOperator::Create(Instruction::And, co, op, "", bo);

  // ~r
  BinaryOperator *opr = BinaryOperator::CreateNot(co, "", bo);

  // a & ~r
  BinaryOperator *op1 =
      BinaryOperator::Create(Instruction::And, bo->getOperand(0), opr, "", bo);

  // ~b
  BinaryOperator *op2 = BinaryOperator::CreateNot(bo->getOperand(1), "", bo);

  // ~b & r
  op2 = BinaryOperator::Create(Instruction::And, op2, co, "", bo);

  // b & ~r
  BinaryOperator *op3 =
      BinaryOperator::Create(Instruction::And, bo->getOperand(1), opr, "", bo);

  // (~a & r) | (a & ~r)
  op = BinaryOperator::Create(Instruction::Or, op, op1, "", bo);

  // (~b & r) | (b & ~r)
  op1 = BinaryOperator::Create(Instruction::Or, op2, op3, "", bo);

  // ((~a & r) | (a & ~r)) ^ ((~b & r) | (b & ~r))
  op = BinaryOperator::Create(Instruction::Xor, op, op1, "", bo);
  bo->replaceAllUsesWith(op);
}



// Implementation of a = a ^ b => a = (b + c) - 2 * (b & c)
void SubstitutionPass::xorSubstitution2(BinaryOperator *bo) {
  ConstantInt *co = (ConstantInt *)ConstantInt::get(bo->getType(), 2);
  BinaryOperator *op1 = BinaryOperator::Create(
      Instruction::And, bo->getOperand(0), bo->getOperand(1), "", bo);
  op1 = BinaryOperator::Create(Instruction::Mul, co, op1, "", bo);
  BinaryOperator *op = BinaryOperator::Create(
      Instruction::Add, bo->getOperand(0), bo->getOperand(1), "", bo);
  op = BinaryOperator::Create(Instruction::Sub, op, op1, "", bo);
  bo->replaceAllUsesWith(op);
}

// Implementation of a = a ^ b => a = b - (2 * (c & ~(b ^ c)) - c)
void SubstitutionPass::xorSubstitution3(BinaryOperator *bo) {
  ConstantInt *co = (ConstantInt *)ConstantInt::get(bo->getType(), 2);
  BinaryOperator *op1 = BinaryOperator::Create(
      Instruction::Xor, bo->getOperand(0), bo->getOperand(1), "", bo);
  op1 = BinaryOperator::CreateNot(op1, "", bo);
  op1 =
      BinaryOperator::Create(Instruction::And, bo->getOperand(1), op1, "", bo);
  op1 = BinaryOperator::Create(Instruction::Mul, co, op1, "", bo);
  op1 =
      BinaryOperator::Create(Instruction::Sub, op1, bo->getOperand(1), "", bo);
  BinaryOperator *op =
      BinaryOperator::Create(Instruction::Sub, bo->getOperand(0), op1, "", bo);
  bo->replaceAllUsesWith(op);
}


// Implementation of a = a ^ b => a = Nor(Nor(Nor(a, a), Nor(b, b)), Nor(a, b))
void SubstitutionPass::xorNor(BinaryOperator *bo) {
  BinaryOperator *noraa = buildNor(bo->getOperand(0), bo->getOperand(0), bo);
  BinaryOperator *norbb = buildNor(bo->getOperand(1), bo->getOperand(1), bo);
  BinaryOperator *nornoraanorbb = buildNor(noraa, norbb, bo);
  BinaryOperator *norab = buildNor(bo->getOperand(0), bo->getOperand(1), bo);
  BinaryOperator *op = buildNor(nornoraanorbb, norab, bo);
  bo->replaceAllUsesWith(op);
}

// Implementation of a = a ^ b => a = Nand(Nand(Nand(a, a), b), Nand(a, Nand(b,
// b)))
void SubstitutionPass::xorNand(BinaryOperator *bo) {
  BinaryOperator *nandaa = buildNand(bo->getOperand(0), bo->getOperand(0), bo);
  BinaryOperator *nandnandaab = buildNand(nandaa, bo->getOperand(1), bo);
  BinaryOperator *nandbb = buildNand(bo->getOperand(1), bo->getOperand(1), bo);
  BinaryOperator *nandanandbb = buildNand(bo->getOperand(0), nandbb, bo);
  BinaryOperator *op = buildNand(nandnandaab, nandanandbb, bo);
  bo->replaceAllUsesWith(op);
}

// Implementation of a = b * c => a = (((b | c) * (b & c)) + ((b & ~c) * (c &
// ~b)))
void SubstitutionPass::mulSubstitution(BinaryOperator *bo) {
  BinaryOperator *op1 = BinaryOperator::CreateNot(bo->getOperand(0), "", bo);
  op1 =
      BinaryOperator::Create(Instruction::And, bo->getOperand(1), op1, "", bo);
  BinaryOperator *op2 = BinaryOperator::CreateNot(bo->getOperand(1), "", bo);
  op2 =
      BinaryOperator::Create(Instruction::And, bo->getOperand(0), op2, "", bo);
  BinaryOperator *op =
      BinaryOperator::Create(Instruction::Mul, op2, op1, "", bo);
  op1 = BinaryOperator::Create(Instruction::And, bo->getOperand(0),
                               bo->getOperand(1), "", bo);
  op2 = BinaryOperator::Create(Instruction::Or, bo->getOperand(0),
                               bo->getOperand(1), "", bo);
  BinaryOperator *op3 =
      BinaryOperator::Create(Instruction::Mul, op2, op1, "", bo);
  op = BinaryOperator::Create(Instruction::Add, op3, op, "", bo);
  bo->replaceAllUsesWith(op);
}

// Implementation of a = b * c => a = (((b | c) * (b & c)) + ((~(b | ~c)) * (b &
// ~c)))
void SubstitutionPass::mulSubstitution2(BinaryOperator *bo) {
  BinaryOperator *op1 = BinaryOperator::CreateNot(bo->getOperand(1), "", bo);
  BinaryOperator *op2 =
      BinaryOperator::Create(Instruction::And, bo->getOperand(0), op1, "", bo);
  BinaryOperator *op3 =
      BinaryOperator::Create(Instruction::Or, bo->getOperand(0), op1, "", bo);
  op3 = BinaryOperator::CreateNot(op3, "", bo);
  op3 = BinaryOperator::Create(Instruction::Mul, op3, op2, "", bo);
  BinaryOperator *op4 = BinaryOperator::Create(
      Instruction::And, bo->getOperand(0), bo->getOperand(1), "", bo);
  BinaryOperator *op5 = BinaryOperator::Create(
      Instruction::Or, bo->getOperand(0), bo->getOperand(1), "", bo);
  op5 = BinaryOperator::Create(Instruction::Mul, op5, op4, "", bo);
  BinaryOperator *op =
      BinaryOperator::Create(Instruction::Add, op5, op3, "", bo);
  bo->replaceAllUsesWith(op);
}


// Implementation of ~(a | b) and ~a & ~b
BinaryOperator *SubstitutionPass::buildNor(Value *a, Value *b,
                                           Instruction *insertBefore) {
  switch (cryptoutils->get_range(2)) {
  case 0: {
    // ~(a | b)
    BinaryOperator *op =
        BinaryOperator::Create(Instruction::Or, a, b, "", insertBefore);
    op = BinaryOperator::CreateNot(op, "", insertBefore);
    return op;
  }
  case 1: {
    // ~a & ~b
    BinaryOperator *nota = BinaryOperator::CreateNot(a, "", insertBefore);
    BinaryOperator *notb = BinaryOperator::CreateNot(b, "", insertBefore);
    BinaryOperator *op =
        BinaryOperator::Create(Instruction::And, nota, notb, "", insertBefore);
    return op;
  }
  default:
    llvm_unreachable("wtf?");
  }
}

// Implementation of ~(a & b) and ~a | ~b
BinaryOperator *SubstitutionPass::buildNand(Value *a, Value *b,
                                 Instruction *insertBefore) {
  switch (cryptoutils->get_range(2)) {
  case 0: {
    // ~(a & b)
    BinaryOperator *op =
        BinaryOperator::Create(Instruction::And, a, b, "", insertBefore);
    op = BinaryOperator::CreateNot(op, "", insertBefore);
    return op;
  }
  case 1: {
    // ~a | ~b
    BinaryOperator *nota = BinaryOperator::CreateNot(a, "", insertBefore);
    BinaryOperator *notb = BinaryOperator::CreateNot(b, "", insertBefore);
    BinaryOperator *op =
        BinaryOperator::Create(Instruction::Or, nota, notb, "", insertBefore);
    return op;
  }
  default:
    llvm_unreachable("wtf?");
  }
}


SubstitutionPass *llvm::createSubstitutionPass(bool flag) {
  return new SubstitutionPass(flag);
}
