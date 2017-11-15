#pragma once

#include <set>

#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"


using namespace llvm;

// LLVM passes are normally defined in the anonymous namespace, as they should
// only ever be exposed via their superclass interface
namespace {
/// SimplePass is a very simple example of an LLVM pass.  This runs on every
/// function and so can not change global module state.  If you want to create
/// or modify globals, then inherit from ModulePass instead.
///

const std::string ADD_FUNCTION_NAME = "mysoreScriptAdd";
const std::string MUL_FUNCTION_NAME = "mysoreScriptMul";
const std::string SUB_FUNCTION_NAME = "mysoreScriptSub";
const std::string DIV_FUNCTION_NAME = "mysoreScriptDiv";

Value *getAsSmallInt2(IRBuilder<>& b, Type* ObjIntTy, Value *i)
{
	if (i->getType()->isPointerTy())
	{
		return b.CreatePtrToInt(i, ObjIntTy);
	}
	return b.CreateBitCast(i, ObjIntTy);
}

Value *compileSmallInt2(IRBuilder<>& b, Type* ObjIntTy, Value *i)
{
  i = b.CreateShl(i, ConstantInt::get(ObjIntTy, 3));
  return b.CreateOr(i, ConstantInt::get(ObjIntTy, 1));
}

Value *getAsObject2(IRBuilder<>& b, Type* ObjPtrTy, Value *i)
{
  if (i->getType()->isPointerTy())
  {
    return b.CreateBitCast(i, ObjPtrTy);
  }
  return b.CreateIntToPtr(i, ObjPtrTy);
}

struct IntSpecialisationPass : FunctionPass, InstVisitor<IntSpecialisationPass>
{
  /// The module that we're currently working on
  Module *M = 0;
  /// The data layout of the current module.
  const DataLayout *DL = 0;
  /// Unique value.  Its address is used to identify this class.
  static char ID;
  /// Call the superclass constructor with the unique identifier as the
  /// (by-reference) argument.
  IntSpecialisationPass() : FunctionPass(ID) {}

  /// Return the name of the pass, for debugging.
  StringRef getPassName() const override {
    return "Simple example pass";
  }

  /// doInitialization - called when the pass manager begins running this
  /// pass on a module.  A single instance of the pass may be run on multiple
  /// modules in sequence.
  bool doInitialization(Module &Mod) override {
    M = &Mod;
    DL = &Mod.getDataLayout();
    // Return false on success.
    return false;
  }

  /// doFinalization - called when the pass manager has finished running this
  /// pass on a module.  It is possible that the pass will be used again on
  /// another module, so reset it to its initial state.
  bool doFinalization(Module &Mod) override {
    assert(&Mod == M);
    M = nullptr;
    DL = nullptr;
    // Return false on success.
    return false;
  }

  bool isAddition(const Function* func) {
    return ADD_FUNCTION_NAME == func->getName();
  }

  bool isMultiplication(const Function* func) {
    return MUL_FUNCTION_NAME == func->getName();
  }

  bool isSubtraction(const Function* func) {
    return SUB_FUNCTION_NAME == func->getName();
  }

  bool isDivision(const Function* func) {
    return DIV_FUNCTION_NAME == func->getName();
  }

  bool isBinaryOp(const Function* func) {
    if(func == nullptr) return false;
    return isAddition(func) ||
           isMultiplication(func) ||
           isSubtraction(func) ||
           isDivision(func);
  }

  bool runOnFunction(Function &F) override {

    visit(F);

    // In this vector of vectors, we will stall contiguous sequences of calls
    // to addition operations.
    // that we can "specialise"
    std::vector<std::vector<CallInst*>> callSequences;

    // Iterate through every basic block and find contiguous sequences of binary operations
    for (BasicBlock& basicBlock: F) {
      std::vector<CallInst*> callSequence;
      bool inContiguousSequence = false;
      for(Instruction& instruction: basicBlock) {
        bool isBinaryOps = false;
        if(CallInst* functionCall = dyn_cast<CallInst>(&instruction)) {
          Function* arithmeticFunc = functionCall->getCalledFunction();
          if(isBinaryOp(arithmeticFunc)) {
            inContiguousSequence = true;
            isBinaryOps = true;
            callSequence.push_back(functionCall);
          }
        }

        if(!isBinaryOps && inContiguousSequence) {
          inContiguousSequence = false;
          callSequences.push_back(callSequence);
          callSequence.clear();
        }
      }
      if(inContiguousSequence) {
        callSequences.push_back(callSequence);
      }
    }

    Type* ObjIntTy = Type::getInt64Ty(F.getContext());
    Type* ObjPtrTy = Type::getInt8PtrTy(F.getContext());

    for(std::vector<CallInst*>& callSequence: callSequences) {
      CallInst* firstBinaryOpCall = callSequence.front();
      CallInst* lastBinaryOpCall = callSequence.back();
      IRBuilder<> irBuilder(firstBinaryOpCall);
      // Symbolically keep track of which results map to which symbolic values.
      std::map<Value*, int> resultVals;
      int symbolicInt = 0;
      for(CallInst* arithmeticInst: callSequence) {
          if(resultVals.count(arithmeticInst) < 1) {
            resultVals.insert(std::pair<Value*, int>(arithmeticInst, symbolicInt++));
          }
      }
      ///
      /// Find all the integers that need to be checked, and create the LLVM IR
      /// that performs the checks
      ///
      Value* onlyIntegerAdditions = nullptr;
      {
        std::set<Value*> intsToCheck;
        for(auto rit = callSequence.rbegin(); rit != callSequence.rend(); ++rit) {
          CallInst* arithmeticInst = *rit;
          Value* leftArg = arithmeticInst->getArgOperand(0);
          Value* rightArg = arithmeticInst->getArgOperand(1);

          if(intsToCheck.count(arithmeticInst) > 0) {
            intsToCheck.erase(arithmeticInst);
          }
          intsToCheck.insert(leftArg);
          intsToCheck.insert(rightArg);
        }
        Value* prevInt = nullptr;
        for(Value* intToCheck: intsToCheck) {
          intToCheck = getAsSmallInt2(irBuilder, ObjIntTy, intToCheck);
          if(prevInt == nullptr) {
            prevInt = intToCheck;
            continue;
          }
          // And them together.  If they are both small ints, then the low bits of the
          // result will be 001.
          Value *isSmallInt = irBuilder.CreateAnd(intToCheck, prevInt);
          // Now mask off the low 3 bits.
          isSmallInt = irBuilder.CreateAnd(isSmallInt, ConstantInt::get(ObjIntTy, 7));
          // If the low three bits are 001, then it is a small integer
          isSmallInt = irBuilder.CreateICmpEQ(isSmallInt, ConstantInt::get(ObjIntTy, 1));

          if(onlyIntegerAdditions == nullptr) {
            onlyIntegerAdditions = isSmallInt;
          } else {
            onlyIntegerAdditions = irBuilder.CreateAnd(isSmallInt, onlyIntegerAdditions);
          }
        }
      }
      ///
      /// End Integer Checking
      ///


      TerminatorInst* rawIntegerBlockTerm = nullptr;
      TerminatorInst* safeArithmeticBlockTerm = nullptr;

      // Split the block
      SplitBlockAndInsertIfThenElse(onlyIntegerAdditions, firstBinaryOpCall,
                                    &rawIntegerBlockTerm,
                                    &safeArithmeticBlockTerm,
                                    nullptr);

      std::vector<Value*> intResults;
      for(CallInst* binaryOpCall: callSequence) {
        PHINode* phiNode = PHINode::Create(binaryOpCall->getFunctionType()->getReturnType(), 2, "binaryOpResultPhi");
        phiNode->insertAfter(lastBinaryOpCall);

        // We want to replace all future instances of the use of the result of this binary operation call
        // that aren't in the current sequence of binary operation calls with the phi node
        // which collects the raw integer result and the safe one.
        for(Use& use: binaryOpCall->materialized_uses()) {
          User* user = use.getUser();
          if(CallInst* call = dyn_cast<CallInst>(user)) {
            if(std::find(callSequence.begin(), callSequence.end(), call) != callSequence.end()) {
              continue;
            }
          }
          user->setOperand(use.getOperandNo(), phiNode);
        }
        binaryOpCall->moveBefore(safeArithmeticBlockTerm);

        irBuilder.SetInsertPoint(rawIntegerBlockTerm);

        // The searches here need to be done so that we ge the calculated results
        // from our arithmetic if we need to, not those the other basic block.
        Value* lhsInt = binaryOpCall->getArgOperand(0);
        {
          auto search = resultVals.find(lhsInt);
          if(search != resultVals.end()) {
            int resultIndex = search->second;
            lhsInt = intResults[resultIndex];
          } else {
            lhsInt = irBuilder.CreateAShr(getAsSmallInt2(irBuilder, ObjIntTy, lhsInt), ConstantInt::get(ObjIntTy, 3));
          }
        }
        Value* rhsInt = binaryOpCall->getArgOperand(1);
        {
          auto search = resultVals.find(rhsInt);
          if(search != resultVals.end()) {
            int resultIndex = search->second;
            rhsInt = intResults[resultIndex];
          } else {
            rhsInt = irBuilder.CreateAShr(getAsSmallInt2(irBuilder, ObjIntTy, rhsInt), ConstantInt::get(ObjIntTy, 3));
          }
        }


        const Function* binaryOpFunc = binaryOpCall->getCalledFunction();
        Value* intResult;
        if(isAddition(binaryOpFunc)) {
          intResult = irBuilder.CreateAdd(lhsInt, rhsInt);
        } else if(isSubtraction(binaryOpFunc)) {
          intResult = irBuilder.CreateSub(lhsInt, rhsInt);
        } else if(isMultiplication(binaryOpFunc)) {
          intResult = irBuilder.CreateMul(lhsInt, rhsInt);
        } else /*if(isDivision(binaryOpFunc))*/ {
          intResult = irBuilder.CreateSDiv(lhsInt, rhsInt);
        }

        Value* objIntResult = getAsObject2(irBuilder, ObjPtrTy, compileSmallInt2(irBuilder, ObjIntTy, intResult));

        intResults.push_back(intResult);

        phiNode->addIncoming(objIntResult, rawIntegerBlockTerm->getParent());
        phiNode->addIncoming(binaryOpCall, safeArithmeticBlockTerm->getParent());
      }
    }
    return true;
  }
}; // END STRUCT

char IntSpecialisationPass::ID;

/// This function is called by the PassManagerBuilder to add the pass to the
/// pass manager.  You can query the builder for the optimisation level and so
/// on to decide whether to insert the pass.
void addIntSpecialisationPass(const PassManagerBuilder &Builder, legacy::PassManagerBase &PM) {
  PM.add(new IntSpecialisationPass());
}

/// Register the pass with the pass manager builder.  This instructs the
/// builder to call the `addSimplePass` function at the end of adding other
/// optimisations, so that we can insert the pass.  See the
/// `PassManagerBuilder` documentation for other extension points.
RegisterStandardPasses SOpt(PassManagerBuilder::EP_OptimizerLast,
                            addIntSpecialisationPass);

/// Register the pass to run at -O0.  This is useful for debugging the pass,
/// though modifications to this pass will typically want to disable this, as
/// most passes don't make sense to run at -O0.
RegisterStandardPasses S(PassManagerBuilder::EP_EnabledOnOptLevel0,
                         addIntSpecialisationPass);
} // anonymous namespace
