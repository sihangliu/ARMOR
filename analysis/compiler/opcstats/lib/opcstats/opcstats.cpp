/*
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Analysis/ProfileInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/BranchProbabilityInfo.h"
#include <cassert>


using namespace llvm;

bool ifIAlu(int op) {
   return (op == Instruction::Add || op == Instruction::Sub || op == Instruction::Mul || 
        op == Instruction::UDiv || op == Instruction::SDiv || op == Instruction::URem ||
        op == Instruction::Shl || op == Instruction::LShr || op == Instruction::AShr || 
        op == Instruction::And || op == Instruction::Or || op == Instruction::Xor ||
        op == Instruction::ICmp || op == Instruction::SRem);
}

bool ifFAlu(int op) {
    return (op == Instruction::FAdd || op == Instruction::FSub || op == Instruction::FMul || 
        op == Instruction::FDiv || op == Instruction::FRem || op == Instruction::FCmp);
}

bool ifBr(int op) {
    return (op == Instruction::Br || op == Instruction::Switch ||
        op == Instruction::IndirectBr);
}

bool ifMem(int op) {
    return (op == Instruction::Alloca || op == Instruction::Load || 
        op == Instruction::Store || op == Instruction::GetElementPtr || 
        op == Instruction::Fence);
}

namespace {
    struct opcstats: public FunctionPass {
        static char ID;
        ProfileInfo* PI;
        opcstats() : FunctionPass(ID) { }
        bool runOnFunction(Function &F) {
            PI = &getAnalysis<ProfileInfo>();
            double TotInstr(0), IAlu(0), FAlu(0), BiasBr(0), UnbiasBr(0), Mem(0), other(0), Br(0);
            for (Function::iterator b = F.begin(); b != F.end(); b++) { //block
                bool isBias(false);
                bool isBranch(false);
                if (PI->getExecutionCount(b) <= 0)
                    continue;

                for (BasicBlock::iterator i = b->begin(); i != b->end(); i++) { //instr                   
                    if (ifIAlu(i->getOpcode())) {
                        IAlu+=PI->getExecutionCount(b);
                    }
                    else if (ifFAlu(i->getOpcode())) {
                        FAlu+=PI->getExecutionCount(b);
                    }
                    else if (ifMem(i->getOpcode())) {
                        Mem+=PI->getExecutionCount(b);
                    }
                    else if (ifBr(i->getOpcode())) { // no branch
                        isBranch = true;
                        Br+=PI->getExecutionCount(b);
                    }
                    else {
                    	other+=PI->getExecutionCount(b);     
                    }

                }
                if (isBranch) {
                    double totWeight(0);
                    for (Function::iterator bb = F.begin(); bb != F.end(); bb++) { //all other blocks
                        std::pair<BasicBlock*, BasicBlock*> E(b, bb);
                        if (PI->getEdgeWeight(E) > 0)
                        	totWeight += PI->getEdgeWeight(E);
                        // errs() << "@" <<totWeight << '\n';
                    }
                    // errs() << totWeight << '\n';
                    for (Function::iterator bb = F.begin(); bb != F.end(); bb++) { //all other blocks
                        std::pair<BasicBlock*, BasicBlock*> E(b, bb);
                        if (PI->getEdgeWeight(E)/totWeight > 0.8) {
                            isBias = true;
                            break;
                        }
                    }
                    if (isBias) BiasBr+=PI->getExecutionCount(b);
                    else UnbiasBr+=PI->getExecutionCount(b);
                }
            }
            TotInstr += (IAlu + FAlu + Mem + Br + other);
            
            assert(Br == (BiasBr + UnbiasBr));
            if (TotInstr != 0)
            	errs().write_escaped(F.getName())<< "," << TotInstr <<","<<IAlu/TotInstr<<","
		            <<FAlu/TotInstr<<","<<Mem/TotInstr<<","<<BiasBr/TotInstr<<","<<UnbiasBr/TotInstr
		            <<","<<other/TotInstr<<'\n';
			else
            	errs().write_escaped(F.getName())<<","<<0<<","<<0<<","<<0<<","<<0<<","<<0<<","<<0
		            <<","<<0<<'\n';
            return false; // return true if you modified the code
        }

        void getAnalysisUsage(AnalysisUsage &AU) const {
            AU.addRequired<ProfileInfo>();
        }
    };
}

char opcstats::ID = 0;
static RegisterPass<opcstats> X("opcstats", "operation status", false, false);
*/
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Analysis/ProfileInfo.h"
#include "llvm/Support/raw_ostream.h"
#include <string>

using namespace llvm;

namespace {
  struct Opcstats : public FunctionPass {
    static char ID;
    ProfileInfo *PI;
    Opcstats() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
      PI = &getAnalysis<ProfileInfo>();
      std::string name = F.getName();
      double dynOpCount   = 0;
      double iALUCount    = 0;
      double fALUCount    = 0;
      double memCount     = 0;
      double biasBrCount  = 0;
      double ubiasBrCount = 0;
      double othersCount  = 0;
      for (Function::iterator bb = F.begin(); bb != F.end(); ++bb) {
        double cc = PI->getExecutionCount(bb);
        if (cc < 0) {
          cc = 0;
        }
        double bias = 0;
        if (cc) {
          double max = 0;
          ProfileInfo::EdgeWeights ew = PI->getEdgeWeights(&F);
          for (std::map<ProfileInfo::Edge, double>::iterator ii = ew.begin(); ii != ew.end(); ++ii) {
            if (ii->first.first == &(*bb) && ii->second > max) {
              max = ii->second;
            }
          }
          bias = max / cc;
        }
        for (BasicBlock::iterator ii = bb->begin(); ii != bb->end(); ++ii) {
          dynOpCount += cc;
          switch (ii->getOpcode()) {
            case Instruction::Add:
            case Instruction::Sub:
            case Instruction::Mul:
            case Instruction::UDiv:
            case Instruction::SDiv:
            case Instruction::URem:
            case Instruction::Shl:
            case Instruction::LShr:
            case Instruction::AShr:
            case Instruction::And:
            case Instruction::Or:
            case Instruction::Xor:
            case Instruction::ICmp:
            case Instruction::SRem:
              iALUCount += cc;
              break;
            case Instruction::FAdd:
            case Instruction::FSub:
            case Instruction::FMul:
            case Instruction::FDiv:
            case Instruction::FRem:
            case Instruction::FCmp:
              fALUCount += cc;
              break;
            case Instruction::Alloca:
            case Instruction::Load:
            case Instruction::Store:
            case Instruction::GetElementPtr:
            case Instruction::Fence:
            case Instruction::AtomicCmpXchg:
            case Instruction::AtomicRMW:
              memCount += cc;
              break;
            case Instruction::Br:
            case Instruction::Switch:
            case Instruction::IndirectBr:
              if (bias > 0.8) {
                biasBrCount += cc;
              } else {
                ubiasBrCount += cc;
              }
              break;
            default:
              othersCount += cc;
          }
        }
      }

      if (dynOpCount) {
        outs().write_escaped(name)
          << ',' << dynOpCount
          << ',' << iALUCount / dynOpCount
          << ',' << fALUCount / dynOpCount
          << ',' << memCount / dynOpCount
          << ',' << biasBrCount / dynOpCount
          << ',' << ubiasBrCount / dynOpCount
          << ',' << othersCount / dynOpCount
          << '\n';
      } else {
        outs().write_escaped(name)
          << ',' << (double) 0
          << ',' << (double) 0
          << ',' << (double) 0
          << ',' << (double) 0
          << ',' << (double) 0
          << ',' << (double) 0
          << ',' << (double) 0
          << '\n';
      }

      return false;
    }

    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.addRequired<ProfileInfo>();
    }
  };
}
char Opcstats::ID = 0;
static RegisterPass<Opcstats> X("opcstats", "operation status", false, false);
