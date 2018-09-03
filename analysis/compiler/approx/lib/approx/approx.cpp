#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/MemoryDependenceAnalysis.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ProfileInfo.h"
#include "llvm/Analysis/Interval.h"
#include "llvm/Support/raw_ostream.h"
#include <string>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <stack>


#define NUM_ITER 1

using namespace llvm;

namespace {

	struct Approx: public ModulePass {

		static char ID;

		Function* curr_func;
		
		LoopInfo* LI;

		struct LoopNodeInfo {
			//Loop* loop;
			BasicBlock* EntryBlock;
			BasicBlock* TerminateBlock;
			uint64_t numIter;
		};
		
		uint64_t max(uint64_t a, uint64_t b) {
			if (a > b) return a;
			else return b;
		}

		struct NodeInfo {
			bool isInLoop;
			BasicBlock* BB;
			//std::vector<Instruction*> ArithInstrVec;
			//std::unordered_map<Instruction*, std::vector<Instruction*> > 
			//									ArithInstr2SrcInstrVecMap;
			//std::unordered_map<Instruction*, Value*> ArithInstr2StoreMap;
			//std::unordered_map<Instruction*, Value*> CallInstr2StoreMap;
			//std::unordered_map<Instruction*, std::vector<Instruction*> >
			//									CallInstr2SrcInstrVecMap;

			// general support
			std::unordered_map<Value*, std::vector<Value*> > Dst2SrcVecMap;
			std::unordered_map<Value*, uint64_t> ApproxVar2ErrorMap;
		};


		std::unordered_map<BasicBlock*, NodeInfo*> BB2NodeInfoMap;
		std::unordered_map<BasicBlock*, LoopNodeInfo*> BB2LoopNodeInfoMap;
		std::unordered_map<Function*, std::unordered_set<BasicBlock*> > processedBlockMap;
		std::unordered_map<Function*, uint64_t> Func2MaxErrorMap;
		
		void switchFunc(Function* func) {
			curr_func = func;
			LI = &getAnalysis<LoopInfo>(*func);
		}

		void switchFunc(BasicBlock* bb) {
			curr_func = bb->getParent();
			LI = &getAnalysis<LoopInfo>(*curr_func);
		}

		void resetFunc(Function* func) {
			Func2MaxErrorMap.erase(func);
			for (auto &B : *func) {
				NodeInfo* node_info = BB2NodeInfoMap[&B];
				node_info->Dst2SrcVecMap.clear();
				auto approx_var_map = &(node_info->ApproxVar2ErrorMap);
				for (auto &var : *approx_var_map) {
					var.second = 1;
				}
			}
		}

		void resetBB(BasicBlock* B) {
			auto approx_var_map = BB2NodeInfoMap[B]->ApproxVar2ErrorMap;
			for (auto &iter : approx_var_map) {
				iter.second = 1;
			}
		}

		// update B1 with new node info from B2
		void merge(BasicBlock* B1, BasicBlock* B2) {
			NodeInfo *node_1 = BB2NodeInfoMap[B1];
			NodeInfo *node_2 = BB2NodeInfoMap[B2];

			//errs() << "Merge: " << *B1->getFirstNonPHI() << " with " << *B2->getFirstNonPHI() << "\n";
			for (auto &it : node_2->ApproxVar2ErrorMap) {
				//errs() << *it.first << ", error=" << it.second << "\n";
				if (node_1->ApproxVar2ErrorMap.find(it.first) == 
							node_1->ApproxVar2ErrorMap.end()) {
					// insert new var from incoming node
					node_1->ApproxVar2ErrorMap[it.first] = it.second;
				} else {
					// update existing var with larger error
					//errs() << *it.first << ", error=" << it.second << "\n";
					if (it.second > node_1->ApproxVar2ErrorMap[it.first]) {
						node_1->ApproxVar2ErrorMap[it.first] = it.second;
						//errs() << *it.first << "@"  << it.second << "\n";
					}
				}
			}
		}
		
		// returns true if B1 dominates B2
		//bool isDominatorBasicBlock(BasicBlock &B1, BasicBlock &B2) {return
		//									(DT->dominates(B1.getTerminator(), &B2));}
		// returns true if I1 dominates I2
		//bool isDominatorInstr(Instruction* I1, Instruction* I2) {return
		//									(DT->dominates(I1, I2));}
	

		bool isFloatRtnFunc(const Function &F) {return (F.getReturnType()->isFloatingPointTy());}
			
		// TODO need to handle all types of input and return
		bool isFloatPtrFunc(const Function &F) {
			//bool isFloatPtrInput = false;
			for (auto arg = F.arg_begin(); arg != F.arg_end(); ++arg) {
				if ((arg->getType()->isPointerTy() && 
						arg->getType()->getPointerElementType()->isFloatingPointTy()) || 
						arg->getName().str().find("approx_") != std::string::npos) {
					//isFloatPtrInput = true;
					return true;
				}
			}
			return false;
		}

		bool isFloatFunc(const Function &F) {return isFloatRtnFunc(F) || isFloatPtrFunc(F);}

		bool isFloatArithmeticInstr(const Instruction &I) {return (
											I.getOpcode() == Instruction::FAdd || 
											I.getOpcode() == Instruction::FSub || 
											I.getOpcode() == Instruction::FMul ||
											I.getOpcode() == Instruction::FDiv);}
/*
		bool isMemInstr(const Instruction &I) {return (
											I.getOpcode() == Instruction::Store || 
											I.getOpcode() == Instruction::Load);}
*/

		bool isAllocaFloatInstr(const Instruction &I) {return (
											I.getOpcode() == Instruction::Alloca && 
											dyn_cast<AllocaInst>(&I)->
												getAllocatedType()->isFloatingPointTy());}

		bool isFloatLoadInstr(const Instruction &I) {
			if (I.getOpcode() != Instruction::Load)
				return false;
			
			std::string type_str;
			llvm::raw_string_ostream rso(type_str);
			I.getType()->print(rso);
			//errs() << "XXXX" << rso.str().find("float") << "\n";
			if (rso.str().find("approx_") != std::string::npos || 
					rso.str().find("float") != std::string::npos) {
				return true;
			}

			//errs() << I << " @@@@ " << *I.getOperand(0) << "\n";	
			return (I.getType()->isFloatingPointTy() || 
							(I.getType()->isPointerTy() && 
								I.getType()->getPointerElementType()->isFloatingPointTy()) || 
							(I.getOperand(0)->getName().str().find("approx_") != std::string::npos));
		}

/*		
		bool isPtrLoadInstr(const Instruction &I) {return (
											I.getOpcode() == Instruction::Load && 
											I.getType()->isPointerTy());}
*/
		bool isFloatStoreInstr(const Instruction &I) {
			if (I.getOpcode() != Instruction::Store)
				return false;			
	
			std::string type_str;
			llvm::raw_string_ostream rso(type_str);
			I.getOperand(0)->getType()->print(rso);
			if (rso.str().find("approx_") != std::string::npos || 
					rso.str().find("float") != std::string::npos) {
				return true;
			}	

			return (I.getOperand(0)->getType()->isFloatingPointTy() ||
						 (I.getOperand(0)->getType()->isPointerTy() && 
							I.getOperand(0)->getType()->getPointerElementType()->isFloatingPointTy()));
		}

		bool isFloatConvInstr(const Instruction &I) {return(I.getOpcode() == Instruction::FPTrunc ||
																												I.getOpcode() == Instruction::FPExt);}

		bool isConstVal(Value* val) {
			std::string val_str = val->getName().str();
			if (val_str.find("%"))
				return false;
			else
				return true;	
		}

		bool isLlvmMemcpy(Instruction &I) {
			if (I.getOpcode() == Instruction::Call) {
				CallInst* call_instr = dyn_cast<CallInst>(&I);
				Function* called_func = call_instr->getCalledFunction();
				std::string called_func_name = called_func->getName().str();
				if (called_func_name.find("llvm.memcpy") != std::string::npos) {
					return true;
				}
			}
			return false;
		}


		//Instruction* getFirstInstr(Function &F) {return F.getEntryBlock().getFirstNonPHI();}

		//uint64_t max_error = 0;
	
		bool isLoopEntryBlock(BasicBlock* B) {
			
			if (BB2LoopNodeInfoMap.find(B) == BB2LoopNodeInfoMap.end())
				return false;

			return B == BB2LoopNodeInfoMap[B]->EntryBlock;
		}

		bool isLoopTerminateBlock(BasicBlock* B) {

			if (BB2LoopNodeInfoMap.find(B) == BB2LoopNodeInfoMap.end())
				return false;
			
			return B == BB2LoopNodeInfoMap[B]->TerminateBlock;
		}	

		
		// return true if B1 is inside B2 and B1 is the first block of its loop
		bool isNestedLoopEntryBlock(BasicBlock* B1, BasicBlock* B2) {
			Loop* L1 = LI->getLoopFor(B1);
			Loop* L2 = LI->getLoopFor(B2);
			assert(L2);
			if (L1 == NULL || *L1->block_begin() != B1 || L1 == L2)
				return false;

			std::unordered_set<BasicBlock*> LoopBlock2;

			for (auto B = L2->block_begin(); B != L2->block_end(); ++B) {
				LoopBlock2.insert(*B);
			}
			for (auto B = L1->block_begin(); B != L1->block_end(); ++B) {
				if (LoopBlock2.find(*B) == LoopBlock2.end())
					return false;
			}
			return true;
		}

		void updateLoopError(BasicBlock* B) {
			// update LoopNodeInfo
				
			// traverse the nodes in loop (same as function level)
			LoopNodeInfo* loop_node = BB2LoopNodeInfoMap[B];
			Loop* curr_loop = LI->getLoopFor(B);
			BasicBlock* curr_bb;	
			std::queue<BasicBlock*> bb_queue;
			
			for (uint64_t iter = 0; iter < BB2LoopNodeInfoMap[B]->numIter; ++iter) {
				
				bb_queue.push(B);
			
				// remove all loop blocks (including nested) from  processed block set
				// for next iteration
				curr_loop = LI->getLoopFor(B);
				for (auto victim = curr_loop->block_begin(); 
							victim != curr_loop->block_end(); ++victim) {
					//errs() << "@4" << "\n";
					if (processedBlockMap[curr_func].find(*victim) 
																		!= processedBlockMap[curr_func].end()) {
						processedBlockMap[curr_func].erase(*victim);
					}
				}	

				while (!bb_queue.empty()) {
					curr_bb = bb_queue.front();
					if (processedBlockMap[curr_func].find(curr_bb) 
																					== processedBlockMap[curr_func].end()) {
						
						//assert(curr_loop == LI->getLoopFor(B)); // This may fail!
						curr_loop = LI->getLoopFor(B);
						//errs() << "@Current: " << *curr_bb->getFirstNonPHI() << "\n";
						if (LI->getLoopFor(curr_bb) != curr_loop && 
											!isNestedLoopEntryBlock(curr_bb, B)) {
							//errs() << "@@@\n";
							bb_queue.pop();
							continue;
						}

						//errs() << "#Current: " << *curr_bb->getFirstNonPHI() << "\n";
						//clear existing errors
						resetBB(curr_bb);
						for (auto PB = pred_begin(curr_bb); PB != pred_end(curr_bb); ++PB) {
							//if (isBackedge(*PB, curr_bb))
							//	continue;
							merge(curr_bb, *PB);
						}
				
						//errs() << iter << " > " << *curr_bb->getFirstNonPHI()  << "\n";
						if (isNestedLoopEntryBlock(curr_bb, B)) {
							//errs() << "@ " << *curr_bb->getFirstNonPHI() 
							//				<< *B->getFirstNonPHI() << "\n";
							updateLoopError(curr_bb);
						} else {
							//errs() << "@ " << *curr_bb->getFirstNonPHI() << "\n";
							updateError(curr_bb);
						}
						//errs() << "update done\n";
					
					}
					if (!isLoopTerminateBlock(curr_bb)) {
						//errs() << "^^Current: " << *curr_bb->getFirstNonPHI() << "\n";
						
						for (auto SB = succ_begin(curr_bb); SB != succ_end(curr_bb); ++SB) {
						
							if (!isBackedge(curr_bb, *SB)) {
								//errs() << "\t>> " << *(*SB)->getFirstNonPHI() << "\n";	
								bb_queue.push(*SB);
							} 
						}
					} 
					bb_queue.pop();
				}
			}
			
			// entrance block merge with termiation block
			curr_bb = loop_node->EntryBlock;
			resetBB(curr_bb);
			merge(curr_bb, loop_node->TerminateBlock);
			for (auto PB = pred_begin(curr_bb); PB != pred_end(curr_bb); ++PB) {
				merge(curr_bb, *PB);
			}
			
			//errs() << "@@8" << LI->getLoopFor(B) << "\n";
			//assert(curr_loop == LI->getLoopFor(B));
			curr_loop = LI->getLoopFor(B);
			for (auto iter = curr_loop->block_begin(); 
					iter != curr_loop->block_end(); ++iter) {
				if (LI->getLoopFor(*iter) == curr_loop) {
					processedBlockMap[curr_func].insert(*iter);
				}
			}
		}


		void updateError(BasicBlock* B) {
			NodeInfo *node = BB2NodeInfoMap[B];
				
			// store all temp variables to temp map
			auto tempApproxVar2ErrorMap = node->ApproxVar2ErrorMap;

			for (auto &I : *B) {
				// insert all instructions to dependency map if not store
				// if store, update all dependent errors
				
				//errs() << "@@@@@"  << I << "\n";
				if (isFloatStoreInstr(I)) {
					//errs() << "@STORE" << I << "\n";
					Value* dst = I.getOperand(1);
					Value* src = I.getOperand(0);
					
					uint64_t src_err = tempApproxVar2ErrorMap[src]; 

					//errs() << *src << " : " << src_err << "\n";

					// find dep of dst 
					std::queue<Value*> depInstr;
					uint64_t dep_err;
					Value* curr;
					depInstr.push(dst);
					while (depInstr.size()) {
						curr = depInstr.front();
						//errs() << "curr dep: " << *curr << "\n";
						depInstr.pop();
						
						dep_err = tempApproxVar2ErrorMap[curr];
						if (src_err > dep_err) {
							tempApproxVar2ErrorMap[curr] = src_err;
						}
						for (auto &iter : node->Dst2SrcVecMap[curr]) {
							depInstr.push(iter);
						}
					}

				} else if (I.getOpcode() == Instruction::FMul || 
						   I.getOpcode() == Instruction::FDiv) {
					//errs() << "@FMUL/FDIV" << I << "\n";
					Value* dst = dyn_cast<Value>(&I);
					Value* src1 = I.getOperand(0);
					Value* src2 = I.getOperand(1);
					

					assert(I.getNumOperands() == 2 &&
								"Arithmetic instruction does not have two operands");
					
					uint64_t err1, err2;
					if (!isConstVal(src1)) {
						err1 = tempApproxVar2ErrorMap[src1];
						node->Dst2SrcVecMap[dst].push_back(src1);
					} else {
						err1 = 0;
					}

					if (!isConstVal(src2)) {
						err2 = tempApproxVar2ErrorMap[src2];
						node->Dst2SrcVecMap[dst].push_back(src2);
					} else {
						err2 = 0;
					}
					
					uint64_t dst_err = tempApproxVar2ErrorMap[dst];
/*
					node->Dst2SrcVecMap[dst].push_back(src1);
					node->Dst2SrcVecMap[dst].push_back(src2);
*/
					uint64_t larger_error; 
					if (err1 == 0 || err2 == 0)
						larger_error = max(dst_err, max(err1, err2));
					else
						larger_error = max(dst_err, err1 + err2);
					
					tempApproxVar2ErrorMap[dst] = larger_error;
					//errs() << "@FMUL : " << larger_error << "\n"; 	
					if (larger_error > Func2MaxErrorMap[curr_func])
						Func2MaxErrorMap[curr_func] = larger_error;	
				
				} else if ( //I.getOpcode() == Instruction::FDiv ||
						   I.getOpcode() == Instruction::FAdd ||
						   I.getOpcode() == Instruction::FSub) {
					//errs() << "@OTHER" << I << "\n";
					assert(I.getNumOperands() == 2 && 
								"Arithmetic instruction does not have two operands");
					Value* dst = dyn_cast<Value>(&I);
					Value* src1 = I.getOperand(0);
					Value* src2 = I.getOperand(1);
	
					uint64_t err1, err2;
					if (!isConstVal(src1)) {
						err1 = tempApproxVar2ErrorMap[src1];
						node->Dst2SrcVecMap[dst].push_back(src1);
					} else {
						err1 = 0;
					}

					if (!isConstVal(src2)) {
						err2 = tempApproxVar2ErrorMap[src2];
						node->Dst2SrcVecMap[dst].push_back(src2);
					} else {
						err2 = 0;
					}
					
					uint64_t dst_err = tempApproxVar2ErrorMap[dst];

/*		
					node->Dst2SrcVecMap[dst].push_back(src1);
					node->Dst2SrcVecMap[dst].push_back(src2);
*/
					uint64_t larger_error = max(err1, err2);
					larger_error = max(dst_err, larger_error);

					tempApproxVar2ErrorMap[dst] = larger_error;
					
					if (larger_error > Func2MaxErrorMap[curr_func])
						Func2MaxErrorMap[curr_func] = larger_error;

				}  else if (isLlvmMemcpy(I)) {
					Value* dst = I.getOperand(0); // first argument
					Value* src = I.getOperand(1); // second argument
				
					uint64_t src_err = tempApproxVar2ErrorMap[src]; 

					//errs() << *src << " : " << src_err << "\n";

					// find dep of dst 
					std::queue<Value*> depInstr;
					uint64_t dep_err;
					Value* curr;
					depInstr.push(dst);
					while (depInstr.size()) {
						curr = depInstr.front();
						//errs() << "curr dep: " << *curr << "\n";
						depInstr.pop();
						
						dep_err = tempApproxVar2ErrorMap[curr];
						if (src_err > dep_err) {
							tempApproxVar2ErrorMap[curr] = src_err;
						}
						for (auto &iter : node->Dst2SrcVecMap[curr]) {
							depInstr.push(iter);
						}
					}

					//errs() << "##"<< tempApproxVar2ErrorMap[src] << "\n";	
					//tempApproxVar2ErrorMap[dst] = tempApproxVar2ErrorMap[src];
					
				} else if (I.getOpcode() == Instruction::BitCast) {
					Value* dst = dyn_cast<Value>(&I);
					Value* src = I.getOperand(0);
					//errs() << "@src" << *src << "\n";
					//errs() << "@dst" << *dst << "\n";
					uint64_t src_err = tempApproxVar2ErrorMap[src];
					//errs() << "@@" << src_err << "\n";
					tempApproxVar2ErrorMap[dst] = src_err;
					node->Dst2SrcVecMap[dst].push_back(src);

				} else if (isFloatConvInstr(I)) { 
					
					Value* dst = dyn_cast<Value>(&I);
					Value* src = I.getOperand(0);
					uint64_t src_err = tempApproxVar2ErrorMap[src];

					node->Dst2SrcVecMap[dst].push_back(src);
					tempApproxVar2ErrorMap[dst] = src_err;

				} else if (I.getOpcode() == Instruction::GetElementPtr) {
					//TODO add constraints on pointer type
					//errs() << "@GetElementPtr" << I << "\n";
						
					Value* dst = dyn_cast<Value>(&I);
					Value* src = I.getOperand(0);

					node->Dst2SrcVecMap[dst].push_back(src);
					
					uint64_t src_err = tempApproxVar2ErrorMap[src];
					uint64_t dst_err = tempApproxVar2ErrorMap[dst];
					
					uint64_t larger_error = max(dst_err, src_err);

					tempApproxVar2ErrorMap[dst] = larger_error;
				
				} else if (isFloatLoadInstr(I)) { 
					//errs() << "@LOAD" << I << "\n";
					Value* dst = dyn_cast<Value>(&I);
					Value* src = I.getOperand(0);
				
					node->Dst2SrcVecMap[dst].push_back(src);

					uint64_t src_err = tempApproxVar2ErrorMap[src];
					uint64_t dst_err = tempApproxVar2ErrorMap[dst];
					
					uint64_t larger_error = max(dst_err, src_err);
					
					tempApproxVar2ErrorMap[dst] = larger_error;
					
				} else if ((I.getOpcode() == Instruction::Call && 
							(dyn_cast<CallInst>(&I)->getCalledFunction()->getName().find("ckSub") 
																		  != std::string::npos ||
							  dyn_cast<CallInst>(&I)->getCalledFunction()->getName().find("ckAdd") 
																		  != std::string::npos)) || 
						  (I.getOpcode() == Instruction::Invoke &&
							  (dyn_cast<InvokeInst>(&I)->getCalledFunction()->getName().find("ckSub") 
																		  != std::string::npos ||
							  dyn_cast<InvokeInst>(&I)->getCalledFunction()->getName().find("ckAdd")
																		  != std::string::npos))) {
				 

					Function* called_func;
					if (I.getOpcode() == Instruction::Call) {
						CallInst* call_instr = dyn_cast<CallInst>(&I);
						called_func = call_instr->getCalledFunction();
					}	else {
						InvokeInst* call_instr = dyn_cast<InvokeInst>(&I);
						called_func = call_instr->getCalledFunction();
					}
				
					Value* op_1 = I.getOperand(0);
					uint64_t op_err_1 = tempApproxVar2ErrorMap[op_1];
		
					Value* op_2 = I.getOperand(1);
					uint64_t op_err_2 = tempApproxVar2ErrorMap[op_2];
		  
					//errs() << *op_1 << "\t" << *op_2  << "\n";
					uint64_t larger_error = std::max(op_err_1, op_err_2);
					uint64_t new_err = larger_error * 4;

					tempApproxVar2ErrorMap[dyn_cast<Value>(&I)] = new_err;
					
					if (new_err > Func2MaxErrorMap[curr_func])
						Func2MaxErrorMap[curr_func] = new_err;
				
				} else if (I.getOpcode() == Instruction::Call && 
						dyn_cast<CallInst>(&I)->getCalledFunction()->isDeclaration() &&
						isFloatRtnFunc(*dyn_cast<CallInst>(&I)->getCalledFunction())) {
					
					// external library functions
					CallInst* call_instr = dyn_cast<CallInst>(&I);
					Function* called_func = call_instr->getCalledFunction();
				
					errs() << "(Ext) Call Func: " << I << "\n";
				
					std::string called_func_name = called_func->getName().str();
					
					Value* src = I.getOperand(0);
					Value* dst = dyn_cast<Value>(&I);

					uint64_t src_err = tempApproxVar2ErrorMap[src];
					uint64_t dst_err = tempApproxVar2ErrorMap[dst];

					uint64_t new_err = src_err;

					if (called_func_name == "exp") {
						new_err *= 1;
					} else if (called_func_name == "sqrt") {
						new_err *= 1;
					} else if (called_func_name == "pow") {
						new_err *= 4;
					} else if (called_func_name == "sin") {
						new_err *= 4;
					} else if (called_func_name == "cos") {
						new_err *= 4;
					} else if (called_func_name == "asin") {
						new_err *= 4;
					} else if (called_func_name == "acos") {
						new_err *= 4;
					} else if (called_func_name == "tan") {
						new_err *= 2;
					} else if (called_func_name == "atan") {
						new_err *= 2;
					} else if (called_func_name == "log" || called_func_name == "log2") {
						new_err *= 1;
					} else {
						continue;
					}
							
					uint64_t larger_error = max(dst_err, new_err);
					tempApproxVar2ErrorMap[dst] = larger_error;
					
					if (new_err > Func2MaxErrorMap[curr_func])
						Func2MaxErrorMap[curr_func] = new_err;
				
				} else if ((I.getOpcode() == Instruction::Call /*&& 
						!dyn_cast<CallInst>(&I)->getCalledFunction()->isDeclaration()*/) || 
						(I.getOpcode() == Instruction::Invoke /*&& 
						!dyn_cast<InvokeInst>(&I)->getCalledFunction()->isDeclaration()*/)) {
					
					Function* called_func;
					if (I.getOpcode() == Instruction::Call) {
						CallInst* call_instr = dyn_cast<CallInst>(&I);
						called_func = call_instr->getCalledFunction();
					}	else {
						InvokeInst* call_instr = dyn_cast<InvokeInst>(&I);
						called_func = call_instr->getCalledFunction();
					}

					// TODO only handle float functions for now
					if (!isFloatFunc(*called_func) || called_func->isDeclaration())
						continue;

					errs() << "Call Func: " << I << "\t" << isFloatFunc(*called_func) << "\t" << called_func->isDeclaration()<< "\n";
					// get list of arguments
					std::vector<Value*> arg_list;
					uint64_t num_arg = 0;
					for (auto AI = called_func->arg_begin(); AI != called_func->arg_end(); ++AI) {
						Value* arg = dyn_cast<Value>(AI);
						
						Value* op = I.getOperand(num_arg);
						uint64_t op_err = tempApproxVar2ErrorMap[op];

						errs() << "Arg error " << op_err  << "\n";
						arg_list.push_back(arg);

						for (auto &called_func_bb : *called_func) {
							NodeInfo* called_func_node = BB2NodeInfoMap[&called_func_bb];
							called_func_node->ApproxVar2ErrorMap[arg] = op_err; 
						}

						num_arg++;
					}

					// called function either returns float or takes pointer to float
					switchFunc(called_func);
					traverseFunc(called_func);
					switchFunc(B->getParent());

					//TODO only process float type for now
					if (isFloatRtnFunc(*called_func)) {
						// returns a float
						// switchFunc(called_func);
						// traverseFunc(called_func);
						// switchFunc(B->getParent());

						Value* dst = dyn_cast<Value>(&I);
						node->Dst2SrcVecMap[dst] = arg_list;

						uint64_t new_err = Func2MaxErrorMap[called_func];
						errs() << "Call Func returns float with new error: " << new_err << "\n";
						tempApproxVar2ErrorMap[&I] = new_err;
						//tempApproxVar2ErrorMap[node->CallInstr2StoreMap[&I]] = new_error;
						if (new_err > Func2MaxErrorMap[curr_func])
							Func2MaxErrorMap[curr_func] = new_err;
					}
					if (isFloatPtrFunc(*called_func)) {
						errs() << "Call Func" << called_func->getName()  << "returns void\n";
						// use floats - get all the parameters passed in
						// switchFunc(called_func);
						// traverseFunc(called_func);
						// switchFunc(B->getParent());
						uint64_t new_err = Func2MaxErrorMap[called_func];
						errs() << "Call Func new error: " << new_err << "\n";
						
						if (new_err > Func2MaxErrorMap[curr_func])
							Func2MaxErrorMap[curr_func] = new_err;
	
						for (auto op = I.op_begin(); op != I.op_end(); ++op) {

							// only update error for pointers
							// ignore pass by value
							if (!(*op)->getType()->isPointerTy())
								continue;

							// find dep of dst
							std::queue<Value*> depInstr;
							uint64_t dep_err;
							Value* curr;
							depInstr.push(*op);
							while (depInstr.size()) {
								curr = depInstr.front();
								depInstr.pop();
								
								dep_err = tempApproxVar2ErrorMap[curr];
								if (new_err > dep_err) {
									tempApproxVar2ErrorMap[curr] = new_err;
								}
								for (auto &iter : node->Dst2SrcVecMap[curr]) {
									depInstr.push(iter);
								}
							}
						} // end of argument update

					}				
					resetFunc(called_func);
				}// end of instruction type

			}

			//update original err map
			/*	
			for (auto &it : tempApproxVar2ErrorMap) {
				errs() << "\t >> " << *it.first << " " << it.second << "\n";
			}	
			*/
			for (auto &it : node->ApproxVar2ErrorMap) {
				it.second =	tempApproxVar2ErrorMap[it.first];
				//errs() << "\t >> " << *it.first << " " << it.second << "\n";
			}
		}

		//std::unordered_map<Value*, uint64_t> ApproxVar2ErrorMap;
		std::unordered_map<Loop*, std::vector<Instruction*> > Loop2ArithInstrVecMap;
		//std::unordered_map<Value*, std::vector<Instruction*> > ApproxVar2ArithInstrVecMap;
		std::unordered_map<Instruction*, std::vector<Instruction*> > 
															ArithInstr2SrcInstrVecMap;

		Approx() : ModulePass(ID) {}

		~Approx() {
			for (auto &BB : BB2NodeInfoMap) {
				delete BB.second;
			}
			
			for (auto &BB : BB2LoopNodeInfoMap) {
				delete BB.second;
			}
		}

		//LoopInfo* LI;
		//MemoryDependenceAnalysis* MDA;
		//DependenceAnalysis* DA;
		//DominatorTree* DT;
		//AliasAnalysis* AA;

		Instruction* getPrevInstr(Instruction* I) {
			BasicBlock* B = I->getParent();
			Instruction* prev = NULL;
			for (auto &iter : *B) {
				if (I == &iter) {
					return prev;
				}
				prev = &iter;
			}
			return 0;
		}

		bool isBackedge(BasicBlock* B1, BasicBlock*B2) {
			Loop* L1 = LI->getLoopFor(B1);
			Loop* L2 = LI->getLoopFor(B2);

			if (!L1 || !L2 || L1 != L2) 
				return false;

			return (B1 == BB2LoopNodeInfoMap[B1]->TerminateBlock) && 
					(B2 == BB2LoopNodeInfoMap[B2]->EntryBlock);
		}


		void traverseFunc(Function* F) {
			// reset
			processedBlockMap[curr_func].clear();

			// Traverse the tree
			BasicBlock* curr_bb = &(F->getEntryBlock());	
			std::queue<BasicBlock*> bb_queue;
			bb_queue.push(curr_bb);

			while (!bb_queue.empty()) {
				curr_bb = bb_queue.front();
				//errs() << "Current: " << *curr_bb->getFirstNonPHI() << "\n";
				if (processedBlockMap[curr_func].find(curr_bb) ==
													processedBlockMap[curr_func].end()) {
					
					//errs() << "Current: " << *curr_bb->getFirstNonPHI() << "\n";
					for (auto PB = pred_begin(curr_bb); PB != pred_end(curr_bb); ++PB) {
		//errs() << "isBackedge: " << *(*PB)->getFirstNonPHI() << " & " << *curr_bb->getFirstNonPHI() <<  ": " << isBackedge(*PB, curr_bb) <<  "\n";  
						if (isBackedge(*PB, curr_bb))
							continue;
						//errs() << *((*PB)->getFirstNonPHI()) << "\n";
						merge(curr_bb, *PB);
					}
					
					if (isLoopEntryBlock(curr_bb)) {
						//errs() << *curr_bb->getFirstNonPHI() << "\n";
						updateLoopError(curr_bb);
					} else {
						//errs() << *curr_bb->getFirstNonPHI() << "\n";
						updateError(curr_bb);
						processedBlockMap[curr_func].insert(curr_bb);
					}
				}
				for (auto SB = succ_begin(curr_bb); SB != succ_end(curr_bb); ++SB) {
		//errs() << "isBackedge: " << *curr_bb->getFirstNonPHI() << " & " << *(*SB)->getFirstNonPHI() <<  ": " << isBackedge(curr_bb, *SB) <<  "\n";
					if (isBackedge(curr_bb, *SB))
						continue;
					bb_queue.push(*SB);
				}
				bb_queue.pop();
			}

		}


		bool runOnModule(Module &M)  {

			for (auto &F : M) {
				if (F.isDeclaration())
					continue;
				
				//errs() << "Function Name: " << F.getName().str() << " \n";
				switchFunc(&F);
				//LI = &getAnalysis<LoopInfo>(F);
				//MDA = &getAnalysis<MemoryDependenceAnalysis>(F);
				//DA = &getAnalysis<DependenceAnalysis>(F);
				//DT = &getAnalysis<DominatorTree>(F);
				//AA = &getAnalysis<AliasAnalysis>(F);
				for (auto &B : F) {
					Loop* L =  LI->getLoopFor(&B);
					NodeInfo* node_info = new NodeInfo();
					node_info->isInLoop = (L != NULL);

					if (L) {
						LoopNodeInfo* loop_node_info = new LoopNodeInfo();
						//loop_node_info->loop = L;
						//TODO: assume fixed number of iteration for now
						loop_node_info->numIter = NUM_ITER;
						BB2LoopNodeInfoMap[&B] = loop_node_info;
						
					}

					for (auto &I : B) {

						// TODO keep track of all floats
						// including float, float*
						if ((I.getOpcode() == Instruction::Alloca && I.getType()->isPointerTy() &&
								(I.getType()->getPointerElementType()->isFloatingPointTy() || (
								I.getType()->getPointerElementType()->isPointerTy() &&
								I.getType()->getPointerElementType()->getPointerElementType()
																																->isFloatingPointTy()))) ||
								I.getName().str().find("approx_") != std::string::npos) {
							
							node_info->ApproxVar2ErrorMap[&I] = 1;
							//errs() << "@" << F.getName() << " declares " << I <<  " : " 
							//																				<<  *I.getType() << "\n"; 
						}
								
					}
					BB2NodeInfoMap[&B] = node_info;
				}
			
				// update LoopNodeInfo
				for (auto &iter : BB2LoopNodeInfoMap) {
					// skip BasicBlocks which do not belong to this function
					if (iter.first->getParent() != &F) 
						continue;
					LoopNodeInfo* loop_node_info = iter.second;
					Loop* L = LI->getLoopFor(iter.first);
						
					std::vector<BasicBlock*> LoopBB;

					for (auto B = L->block_begin(); B != L->block_end(); ++B) {
						if (L == LI->getLoopFor(*B)) {
							LoopBB.push_back(*B);
						}
					}
					
					loop_node_info->EntryBlock = LoopBB[0]; 
					loop_node_info->TerminateBlock = LoopBB[LoopBB.size() - 1]; 
				}
				
			}
		
			errs() << "=========================================================\n";

			for (auto &F : M) {
				// traverse main function
				if (F.getName().str() == "main") {
					if (F.isDeclaration())
						continue;
					//errs() << "Function Name: " << F.getName().str() << " \n";
					switchFunc(&F);
					traverseFunc(&F);
					//errs() << Func2MaxErrorMap[&F] << "\n";
					for (auto &iter : Func2MaxErrorMap) {
						errs() << iter.first->getName() << " : " << iter.second << "\n";
					}
				}
			}

			return false;
		}

		void getAnalysisUsage(AnalysisUsage &AU) const {
			//AU.addRequired<DominatorTree>();
			AU.addRequired<LoopInfo>();
			//AU.addRequired<MemoryDependenceAnalysis>();
			//AU.addRequired<DependenceAnalysis>();
			//AU.addRequired<AliasAnalysis>();
		}

	};

}

char Approx::ID = 1;
static RegisterPass<Approx> X("approx", "dram approx", false, false);
