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

  bool runOnFunction(Function &F) override {

    visit(F);

    llvm::outs() << "Starting Type Specialisation Pass\n";

    llvm::SmallVector<int, 16> functionCount;

    std::set<const Value*> intsToCheck;
    // std::set<Value*> returnVals

    // Alternatively, we can loop over each basic block and then over each
    // instruction and inspect them individually:
    for (const llvm::BasicBlock& BB : F) {
      int numFunctions = 0;
      for(auto rit = BB.rbegin(); rit != BB.rend(); ++rit) {
        const llvm::Instruction& I = *rit;
      //for (const llvm::Instruction& I : BB) {
        if(const CallInst* arithmeticInst = dyn_cast<CallInst>(&I)) {
          const Function* arithmeticFunc = arithmeticInst->getCalledFunction();
          if(ADD_FUNCTION_NAME == arithmeticFunc->getName()) {
            const Value* leftArg = arithmeticInst->getArgOperand(0);
            const Value* rightArg = arithmeticInst->getArgOperand(1);
            // const Value* ret = arithmeticInst->getArgOperand(2);
            llvm::outs() << "args{\n";
            llvm::outs() << "  l: " << leftArg << "\n";
            llvm::outs() << "  r: " << rightArg<< "\n";
            llvm::outs() << "  v: " << arithmeticInst << "\n";
            llvm::outs() << "}\n";
            if(intsToCheck.count(arithmeticInst) > 0) {
              llvm::outs() << "HERE!!\n";
              intsToCheck.erase(arithmeticInst);
            }
            intsToCheck.insert(leftArg);
            intsToCheck.insert(rightArg);
            numFunctions++;
          }
        }
      }

      functionCount.push_back(numFunctions);
    }

    {
      llvm::outs() << "LLVM_PASS:";
      for(const int numFunctions: functionCount) {
        llvm::outs() << numFunctions << ",\n IntsToCheck: \n";
        for(auto val: intsToCheck)
          llvm::outs() << val << "\n";
      }
      llvm::outs() << "\n";
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
