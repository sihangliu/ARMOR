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


#define NUM_ITER 3

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
			unsigned numIter;
		};

		struct NodeInfo {
			bool isInLoop;
			BasicBlock* BB;
			std::unordered_map<Value*, unsigned> ApproxVar2ErrorMap;
			std::vector<Instruction*> ArithInstrVec;
			std::unordered_map<Instruction*, std::vector<Instruction*> > 
												ArithInstr2SrcInstrVecMap;
			std::unordered_map<Instruction*, Value*> ArithInstr2StoreMap;
			std::unordered_map<Instruction*, Value*> CallInstr2StoreMap;
			std::unordered_map<Instruction*, std::vector<Instruction*> >
												CallInstr2SrcInstrVecMap;
		};


		std::unordered_map<BasicBlock*, NodeInfo*> BB2NodeInfoMap;
		std::unordered_map<BasicBlock*, LoopNodeInfo*> BB2LoopNodeInfoMap;
		
		std::unordered_set<BasicBlock*> processedBlock;
		
		void switchFunc(Function* func) {
			curr_func = func;
			LI = &getAnalysis<LoopInfo>(*func);
		}

		void switchFunc(BasicBlock* bb) {
			curr_func = bb->getParent();
			LI = &getAnalysis<LoopInfo>(*curr_func);
		}

		// update B1 with new node info from B2
		void merge(BasicBlock* B1, BasicBlock* B2) {
			NodeInfo *node_1 = BB2NodeInfoMap[B1];
			NodeInfo *node_2 = BB2NodeInfoMap[B2];

			//errs() << "Merge with: " <<  *B2->getFirstNonPHI() << " size=" << node_2->ApproxVar2ErrorMap.size() << "\n";
			for (auto &it : node_2->ApproxVar2ErrorMap) {
				//errs() << *it.first << ", error=" << it.second << "\n";
				if (node_1->ApproxVar2ErrorMap.find(it.first) == 
							node_1->ApproxVar2ErrorMap.end()) {
					// insert new var from incoming node
					//errs() << "@@@\n";
					node_1->ApproxVar2ErrorMap[it.first] = it.second;
				} else {
					// update existing var with larger error
					if (it.second > node_1->ApproxVar2ErrorMap[it.first]) {
						node_1->ApproxVar2ErrorMap[it.first] = it.second;
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
		
		bool isFloatFunc(const Function &F) {return (F.getReturnType()->isFloatTy());}
			
		bool isVoidFunc(const Function &F) {return (F.getReturnType()->isVoidTy());}


		bool isFloatArithmeticInstr(const Instruction &I) {return (
											I.getOpcode() == Instruction::FAdd || 
											I.getOpcode() == Instruction::FSub || 
											I.getOpcode() == Instruction::FMul ||
											I.getOpcode() == Instruction::FDiv);}

		bool isMemInstr(const Instruction &I) {return (
											I.getOpcode() == Instruction::Store || 
											I.getOpcode() == Instruction::Load);}

		bool isAllocaFloatInstr(const Instruction &I) {return (
											I.getOpcode() == Instruction::Alloca && 
											dyn_cast<AllocaInst>(&I)->
												getAllocatedType()->isFloatTy());}

		bool isFloatLoadInstr(const Instruction &I) {return (
											I.getOpcode() == Instruction::Load && 
											I.getType()->isFloatTy());}
		
		bool isPtrLoadInstr(const Instruction &I) {return (
											I.getOpcode() == Instruction::Load && 
											I.getType()->isPointerTy());}

		bool isFloatStoreInstr(const Instruction &I) {return (
											I.getOpcode() == Instruction::Store && 
											I.getOperand(0)->getType()->isFloatTy());}

		Instruction* getFirstInstr(Function &F) {return F.getEntryBlock().getFirstNonPHI();}

		std::unordered_map<Function*, unsigned> Func2MaxErrorMap;
		//unsigned max_error = 0;
	
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
			
			for (unsigned iter = 0; iter < BB2LoopNodeInfoMap[B]->numIter; ++iter) {
				
				bb_queue.push(B);
			
				// remove all loop blocks (including nested) from  processed block set
				// for next iteration
				for (auto victim = curr_loop->block_begin(); 
							victim != curr_loop->block_end(); ++victim) {
					//errs() << "@4" << "\n";
					if (processedBlock.find(*victim) != processedBlock.end()) {
						processedBlock.erase(*victim);
					}
				}	

				while (!bb_queue.empty()) {
					curr_bb = bb_queue.front();
					if (processedBlock.find(curr_bb) == processedBlock.end()) {
						if (LI->getLoopFor(curr_bb) != curr_loop && 
											!isNestedLoopEntryBlock(curr_bb, B)) {
							bb_queue.pop();
							continue;
						}

						//errs() << "Current: " << *curr_bb->getFirstNonPHI() << "\n";
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
						for (auto SB = succ_begin(curr_bb); SB != succ_end(curr_bb); ++SB) {
							
							if (!isBackedge(curr_bb, *SB)) { 
								bb_queue.push(*SB);
							}
						}
					} 
					bb_queue.pop();
				}
			}
			
			// entrance block merge with termiation block
			merge(loop_node->EntryBlock, loop_node->TerminateBlock);

			for (auto iter = curr_loop->block_begin(); 
					iter != curr_loop->block_end(); ++iter) {
				if (LI->getLoopFor(*iter) == curr_loop)
					processedBlock.insert(*iter);
			}
		}


		void updateError(BasicBlock* B) {
			NodeInfo *node = BB2NodeInfoMap[B];
			
			// update NodeInfo
			auto tempApproxVar2ErrorMap = node->ApproxVar2ErrorMap;
			Instruction* prev_instr = NULL;

			for (auto &I : *B) {
				if (I.getOpcode() == Instruction::FMul) {
					// arithmetic operation
					assert(node->ArithInstr2SrcInstrVecMap[&I].size() == 2);

					Value *op1 = NULL;
					Value *op2 = NULL;
					
					if (node->ArithInstr2SrcInstrVecMap[&I][0]->getOpcode() 
							== Instruction::Load) {
						op1 = node->ArithInstr2SrcInstrVecMap[&I][0]->getOperand(0);
					}
					else {
						op1 = node->ArithInstr2SrcInstrVecMap[&I][0];
					}
					errs() << "op1 " << *op1 << "\n";
					if (node->ArithInstr2SrcInstrVecMap[&I][1]->getOpcode() 
							== Instruction::Load)
						op2 = node->ArithInstr2SrcInstrVecMap[&I][1]->getOperand(0);
					else 
						op2 = node->ArithInstr2SrcInstrVecMap[&I][1];
					errs() << "op2 " << *op2 << "\n";

					unsigned err_op1 = tempApproxVar2ErrorMap[op1]; 
					unsigned err_op2 = tempApproxVar2ErrorMap[op2];
					unsigned new_error = 2 * ((err_op1 > err_op2) ? err_op1 : err_op2);
					errs() << new_error << "\n";	
					if (new_error > Func2MaxErrorMap[curr_func])
						Func2MaxErrorMap[curr_func] = new_error;

					tempApproxVar2ErrorMap[&I] = new_error;
					
					if (node->ArithInstr2StoreMap.find(&I) != node->ArithInstr2StoreMap.end()) {
						if (tempApproxVar2ErrorMap[node->ArithInstr2StoreMap[&I]] < new_error) { 
							tempApproxVar2ErrorMap[node->ArithInstr2StoreMap[&I]] = new_error;
							//errs() << "new error " << *node->ArithInstr2StoreMap[&I] << ": "  << new_error << "\n";
						}
					}
				} else if (prev_instr && isFloatStoreInstr(I) && 
								isFloatLoadInstr(*prev_instr) &&
							I.getOperand(0)	==  prev_instr) {
					// direct assign value of one var to another var
					tempApproxVar2ErrorMap[I.getOperand(1)] 
										= tempApproxVar2ErrorMap[prev_instr->getOperand(0)];					
					// also add new entry to err map
					node->ApproxVar2ErrorMap[I.getOperand(1)] 
										= tempApproxVar2ErrorMap[prev_instr->getOperand(0)];					
					//errs() << *I.getOperand(1) << "\n";	
					//errs() << tempApproxVar2ErrorMap[prev_instr->getOperand(0)] << "\n";
					//errs() << tempApproxVar2ErrorMap[I.getOperand(1)] << "\n";
				} else if {
					// direct assign value of *ptr to another var
				} else if {
					// direct assign value of a var to *ptr 
				} else if (I.getOpcode() == Instruction::Call && 
						!dyn_cast<CallInst>(&I)->getCalledFunction()->isDeclaration()) {
					//TODO do not process external function for now
					
					errs() << "Call Func: " << I << "\n";
					errs() << "current max error: " << Func2MaxErrorMap[curr_func] << "\n";
					
					CallInst* call_instr = dyn_cast<CallInst>(&I);
					Function* called_func = call_instr->getCalledFunction();

					// get list of arguments
					unsigned arg_cnt = 0;
					for (auto AI = called_func->arg_begin(); AI != called_func->arg_end(); ++AI) {
						Value* arg = dyn_cast<Value>(AI);
						Value* caller_var = node->CallInstr2SrcInstrVecMap[&I][arg_cnt]->getOperand(0);
						// errs() << "@@" << " " << *caller_var << " : " << tempApproxVar2ErrorMap[caller_var]  << "\n";
						// get use of arguments
						// - vars that depend on function args in that function
						for (auto UI = arg->use_begin(); UI != arg->use_end(); ++UI) {
							Instruction* arg_use = dyn_cast<Instruction>(*UI);
							// replace errors of dependent variables with errors from caller
							for (auto OP = arg_use->op_begin(); OP != arg_use->op_end(); ++OP) {
								Value* operand_var = dyn_cast<Value>(*OP);
								//errs() << "operand_var " << *operand_var << "\n";
								
								for (auto &called_func_bb : *called_func) {
									NodeInfo* called_func_node = BB2NodeInfoMap[&called_func_bb];
									if (called_func_node->ApproxVar2ErrorMap.find(operand_var) 
											!= called_func_node->ApproxVar2ErrorMap.end()) {

										called_func_node->ApproxVar2ErrorMap[operand_var] = 
																tempApproxVar2ErrorMap[caller_var];
										//errs() << "## " << called_func_node->ApproxVar2ErrorMap[operand_var] << "\n"; 
									}
								}
							}
						}
						arg_cnt++;
					}

					//TODO only process float type for now
					if (isFloatFunc(*called_func)) {
						// returns a float
						switchFunc(called_func);
						traverseFunc(called_func);
						switchFunc(B->getParent());
						
						unsigned new_error = Func2MaxErrorMap[called_func];
						errs() << "Call Func returns float with new error: "  << new_error << "\n";
						tempApproxVar2ErrorMap[&I] = new_error;
						tempApproxVar2ErrorMap[node->CallInstr2StoreMap[&I]] = new_error;
						if (new_error > Func2MaxErrorMap[curr_func])
							Func2MaxErrorMap[curr_func] = new_error;
					} else if (isVoidFunc(*called_func)) {
						errs() << "Call Func returns void\n";
						// use floats - get all the parameters passed in
						switchFunc(called_func);
						traverseFunc(called_func);
						switchFunc(B->getParent());
						unsigned new_error = Func2MaxErrorMap[called_func];
						errs() << "Call Func new error: " << new_error << "\n";
						for (auto &src : node->CallInstr2SrcInstrVecMap[&I]) {
							tempApproxVar2ErrorMap[src] = new_error;
						}
						if (new_error > Func2MaxErrorMap[curr_func])
							Func2MaxErrorMap[curr_func] = new_error;
					}
				}
				prev_instr = &I;
			}

			//update original err map
			//errs() << "size of error map: " << tempApproxVar2ErrorMap.size() << "\n";
			for (auto &it : node->ApproxVar2ErrorMap) {
				it.second =	tempApproxVar2ErrorMap[it.first];
				//errs() << "\t >> " << *it.first << " " << it.second << "\n";
			}
		}

		//std::unordered_map<Value*, unsigned> ApproxVar2ErrorMap;
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
			processedBlock.clear();

			// Traverse the tree
			BasicBlock* curr_bb = &(F->getEntryBlock());	
			std::queue<BasicBlock*> bb_queue;
			bb_queue.push(curr_bb);

			while (!bb_queue.empty()) {
				curr_bb = bb_queue.front();
				// errs() << "Current: " << *curr_bb->getFirstNonPHI() << "\n";
				if (processedBlock.find(curr_bb) == processedBlock.end()) {
					
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
						processedBlock.insert(curr_bb);
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
						/*
						if (I.getOpcode() == Instruction::Call) {
							CallInst* call_instr = dyn_cast<CallInst>(&I);
							Function* called_func = call_instr->getCalledFunction();
							if (!called_func->isDeclaration()) {
								errs() << "Called Function " << *called_func << "\n";
								errs() << "Return Type " << *called_func->getReturnType() << "\n";
								errs() << "Function Type " << *called_func->getFunctionType() 
																						<< "\n";
							}
						}
						*/
						if (isFloatArithmeticInstr(I)) {
							node_info->ArithInstrVec.push_back(&I);
							for (auto UI = I.use_begin(); UI != I.use_end(); ++UI) {
								Instruction* use_instr = dyn_cast<Instruction>(*UI);
								Value* store_val = use_instr->getOperand(1);
								Instruction* store_src = dyn_cast<Instruction>(store_val);
								errs() << "store src " <<  *store_src << "\n";
								if (dyn_cast<Instruction>(store_val)->getOpcode() == 
											Instruction::GetElementPtr) {
									store_val = getPrevInstr(dyn_cast<Instruction>(store_val))
																				->getOperand(0);
								} else if (isPtrLoadInstr(*store_src)) {
									store_val = store_src->getOperand(0);
								}	
								//node_info->ApproxVar2ErrorMap[use_instr->getOperand(1)] = 1;
								if (isFloatStoreInstr(*use_instr)) {
									node_info->ApproxVar2ErrorMap[store_val] = 1;
									errs() << F.getName() << " " << *store_val << "\n";
									node_info->ArithInstr2StoreMap[&I] = store_val;
								}
							}
						}
						if (isFloatLoadInstr(I) || isFloatArithmeticInstr(I)) {

							Instruction* src_instr = &I;
							//errs() << "@ " << F.getName() << "\n";
							if (I.getOperand(0) == getPrevInstr(&I) && 
										getPrevInstr(&I)->getOpcode() == 
											Instruction::GetElementPtr) {
								//handle ptr
								src_instr = getPrevInstr(getPrevInstr(&I));
								//errs() << *src_instr << "\n";
							} else if (isFloatLoadInstr(I) && getPrevInstr(&I) && 
										isPtrLoadInstr(*getPrevInstr(&I)) &&
										(I.getOperand(0) == dyn_cast<Value>(getPrevInstr(&I)))) {
								src_instr = getPrevInstr(&I);
							}

							//errs() << "@" << *src_instr << "\n";
							for (auto UI = I.use_begin(); UI != I.use_end(); ++UI) {
								Instruction* use_instr = dyn_cast<Instruction>(*UI);
																
								if (isFloatArithmeticInstr(*use_instr)) {
									node_info->ArithInstr2SrcInstrVecMap[use_instr].
																	push_back(src_instr);
									//errs() <<  *src_instr << " op1:" << *src_instr->getOperand(0)<< "\n";
								}

							}
						}
						if (I.getOpcode() == Instruction::Call) {
							for (auto OP = I.op_begin(); OP != I.op_end(); ++OP) {
								Instruction* src_instr = dyn_cast<Instruction>(OP);
								
								if (src_instr) {
									//handle ptr
									if (src_instr->getOperand(0) == getPrevInstr(src_instr) && 
											getPrevInstr(src_instr)->getOpcode() == 
														Instruction::GetElementPtr) {
										src_instr = getPrevInstr(getPrevInstr(src_instr));
									}
									//errs() << "src=" << *src_instr << "\n";
									node_info->CallInstr2SrcInstrVecMap[&I].
																	push_back(src_instr);
								}
							}
							
							if (!isVoidFunc(*dyn_cast<CallInst>(&I)->getCalledFunction())) {
								for (auto UI = I.use_begin(); UI != I.use_end(); ++UI) {
									Instruction* use_instr = dyn_cast<Instruction>(*UI);
									if (isFloatStoreInstr(*use_instr)) {
										node_info->CallInstr2StoreMap[&I] = use_instr->getOperand(1);	
									}
								}
							}
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
				
				//traverseFunc(&F);

			}
		
			errs() << "===========================================\n";

			for (auto &F : M) {
				// traverse main function
				if (F.getName().str() == "main") {
					if (F.isDeclaration())
						continue;
					//errs() << "Function Name: " << F.getName().str() << " \n";
					switchFunc(&F);
					traverseFunc(&F);
					errs() << Func2MaxErrorMap[&F] << "\n";
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
